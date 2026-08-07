# Audit: ccan/pipecmd

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).

Scope: pipecmd.h, pipecmd.c, _info, ccan/pipecmd/test/ (run.c,
run-fdleak.c, run-preserve.c). ~259 LOC. Deps ccan/closefrom and
ccan/noerr used per their documented contracts (closefrom(4) in the
child; close_noerr/free_noerr in error paths). Suspects from the brief:
fd leaks on error paths, fork/exec error handling, signal handling in
child, double-close, waitpid EINTR — the fd-leak, exec-error-handling
and EINTR suspects all confirmed; double-close disproved (R1, R2).

## Mechanical checks

- ccanlint (before auditor additions): **43/49 PASS**, every check
  passes; partial credit only for tests_coverage (+1/6) and
  examples_exist (+1/2, pipecmd.h has no Example: section).
- After adding the three auditor regression tests: ccanlint **39/45
  FAIL** — exactly as designed: run-execfail-fdleak and
  run-execfail-flush fail against the current code (run-eintr fails in
  plain gcc -O0/-O2 and clang -O1 builds, 4/4 runs, but passes under
  ccanlint's default build and under ASan — timing, see F3).
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; each test #includes
  ccan/pipecmd/pipecmd.c directly; linked with ccan/closefrom,
  ccan/noerr, ccan/tap; per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  **3/3 pass cleanly, zero sanitizer diagnostics** — run (67 ok),
  run-fdleak (13 ok), run-preserve (25 ok). Total 105 ok.
- -m32: skipped; fails on missing 32-bit system headers via errno.h
  (known host limitation, same as earlier modules).
- Reproducers in /tmp/pipecmd-asan/ (temporary, not committed):
  repro-execfail-leak.c, repro-prefork-leak.c, repro-eintr.c,
  repro-waitpid-eintr.c, repro-exit-flush.c.

## Findings

## F1 — CONFIRMED (FIXED in 42486358): exec failure with pipes requested leaks all three parent-side pipe fds

- Location: ccan/pipecmd/pipecmd.c:170-188, function pipecmdarr,
  concretely the exec-failure branch pipecmd.c:175-180:
  ```c
  if (read(execfail[0], &err, sizeof(err)) == sizeof(err)) {
      close(execfail[0]);
      waitpid(childpid, NULL, 0);
      errno = err;
      return -1;
  }
  ```
- Reachable path: caller requests pipes (any of fd_tochild /
  fd_fromchild / fd_errfromchild non-NULL and not &pipecmd_preserve)
  and the exec fails — the module's own documented headline use case
  ("handling the case where the exec fails", _info:8), e.g.
  `pipecmd(&infd, &outfd, &errfd, "/doesnotexist", NULL)`. The pipes
  were created at pipecmd.c:61-94; the parent-side ends tochild[1],
  fromchild[0] and errfromchild[0] are only ever handed to the caller
  on the success path (pipecmd.c:182-187). On the exec-failure path
  they are closed by nobody: par_close[] (closed at pipecmd.c:171-172)
  contains only the *child-side* ends (tochild[0], fromchild[1],
  errfromchild[1], execfail[1]), and the child closed its copies of the
  parent-side ends via child_close[] — but the parent's own copies
  leak.
- Preconditions: none violated — passing a non-existent command with
  pipes requested is exactly the documented error contract ("The
  return value is the pid of the child, or -1", pipecmd.h:23-24, and
  run.c:141-143 tests errno == ENOENT for the all-NULL case).
- Concrete incorrect consequence, observed
  (/tmp/pipecmd-asan/repro-execfail-leak.c, plain clang build):
  ```
  fds before=4 after=19 leaked=15
  ```
  i.e. exactly 3 fds leaked per failed pipecmd(&in,&out,&err,...) call
  (5 iterations). A daemon probing for an optional helper binary, or
  retrying a failing command, exhausts RLIMIT_NOFILE over time.
  Existing test run.c does not catch this because its failing-exec case
  (run.c:141) passes all-NULL fds, so every opened fd is in par_close[].
- Regression test: ccan/pipecmd/test/run-execfail-fdleak.c tests 1-3
  (alarm(10)-bounded). Currently `not ok 3 - after == before`
  (3 fds leaked); after repair it must pass.
- Repair direction: track the parent-side ends too (e.g. add
  tochild[1]/fromchild[0]/errfromchild[0] to a second close-list) and
  close them on the exec-failure return at pipecmd.c:175-180, or close
  the three ends explicitly there before waitpid.

## F2 — CONFIRMED (FIXED in 2f04573d): the pre-fork `fail:` path leaks the parent-side ends of already-created pipes

- Location: ccan/pipecmd/pipecmd.c:190-193, function pipecmdarr:
  ```c
  fail:
      for (i = 0; i < num_par_close; i++)
          close_noerr(par_close[i]);
      return -1;
  ```
- Reachable path: any failure after the first command pipe succeeded —
  the second/third pipe() or open("/dev/null") failing
  (pipecmd.c:64-65, 78-79, 95-96, 99-100), pipe(execfail) failing
  (pipecmd.c:104-105), the fcntl(F_SETFD) failing (pipecmd.c:110-112),
  or fork() failing (pipecmd.c:114-116) — while at least one earlier
  pipe was created for a non-NULL fd out-parameter. child_close[] holds
  the parent-side ends precisely so the *child* closes them after
  fork; when fork never happens (or the child never runs), the parent's
  copies are leaked. If fork() itself fails, execfail[0] leaks as well.
- Preconditions: none violated; fd exhaustion (EMFILE/ENFILE) or fork
  failure (EAGAIN) are ordinary runtime conditions, not caller misuse.
- Concrete incorrect consequence, observed
  (/tmp/pipecmd-asan/repro-prefork-leak.c: RLIMIT_NOFILE capped at
  used+6 so the three command pipes succeed and pipe(execfail) fails
  with EMFILE):
  ```
  fds before=4 after=7 leaked=3
  ```
  Exactly when the process is already out of fds, each retry leaks 3
  more, so the condition becomes unrecoverable even after the caller
  frees fds elsewhere.
- Regression test: ccan/pipecmd/test/run-execfail-fdleak.c tests 4-5.
  Currently `not ok 5 - after == before` (3 fds leaked); after repair
  it must pass.
- Repair direction: same close-list as F1 — close the parent-side ends
  (and execfail[0], if created) on the `fail:` path as well.

## F3 — CONFIRMED (FIXED in 42486358): no EINTR handling on the parent synchronization path: interrupted read() reports false success; interrupted waitpid() leaves a zombie

- Location: ccan/pipecmd/pipecmd.c:175 (read of the execfail pipe) and
  pipecmd.c:177 (waitpid), function pipecmdarr.
- Reachable path: caller has any signal handler installed without
  SA_RESTART (timer signals, SIGCHLD from other children, SIGUSR1/2 in
  frameworks, etc.) and a signal arrives while the parent blocks in
  `read(execfail[0], &err, sizeof(err))`. read then returns -1/EINTR,
  which fails the `== sizeof(err)` test, so execution falls into the
  success tail at pipecmd.c:181-188: the caller is handed a pid (and
  pipe fds whose writer has already exited 127) for a command that
  never ran, with errno left stale. Similarly, a signal hitting the
  waitpid at pipecmd.c:177 makes it return -1/EINTR, so the
  exec-failed child is never reaped and remains a zombie (the -1/errno
  return to the caller is still correct in that sub-case).
- Preconditions: none violated — nothing in pipecmd.h requires callers
  to block signals or use SA_RESTART around the call.
- Concrete incorrect consequences, observed
  (/tmp/pipecmd-asan/repro-eintr.c: SIGALRM handler, sa_flags = 0,
  50 us interval timer, 2000 iterations of
  pipecmd(NULL, NULL, NULL, "/doesnotexist", NULL), plain clang build):
  ```
  correct_fail(ENOENT)=293 false_success(child exited 127)=146 other=1561
  ```
  146 calls returned a child pid ("success") although exec had failed
  (waitpid on that pid shows exit status 127 — the child_errno_fail
  path at pipecmd.c:161-167). With pipes requested, the caller would
  then write/read fds belonging to a dead child. And
  /tmp/pipecmd-asan/repro-waitpid-eintr.c (same setup, then
  waitpid(-1, NULL, WNOHANG) after each ENOENT return):
  ```
  enoent=917 zombies_reaped_late=362 other=1083
  ```
  i.e. in 362 of 917 ENOENT cases pipecmdarr's internal waitpid was
  EINTR'd and left the child as a zombie. (The "other" bucket is
  pipecmdarr returning -1 with errno == EINTR when the signal hits the
  pre-fork pipe()/open() syscalls — caller-visible and retryable, see
  R4; under ASan almost every call lands there instead, which is why
  the run-eintr regression test passes under ASan.)
- Regression test: ccan/pipecmd/test/run-eintr.c (alarm(60)-bounded,
  2000 iterations). Currently fails 1/1 in plain gcc -O0, gcc -O2 and
  clang -O1 builds (4/4 runs: `not ok 1 - false_success == 0`); passes
  under ASan only because ASan timing shifts the signals into the
  pre-fork syscalls. After repair it must pass everywhere.
- Repair direction: retry the read on EINTR (`do { } while (r < 0 &&
  errno == EINTR)`), and likewise loop the waitpid at pipecmd.c:177 on
  EINTR. For symmetry the child's error report write
  (pipecmd.c:164-166) could also loop on EINTR/partial write, though
  sizeof(int) <= PIPE_BUF makes partial writes impossible and an
  EINTR'd write merely degrades to the same false-success already
  covered here.

## F4 — CONFIRMED (FIXED in b02c1147): child calls exit(127) instead of _exit(127) after a failed exec — flushes the parent's inherited stdio buffers a second time and runs atexit handlers

- Location: ccan/pipecmd/pipecmd.c:167, function pipecmdarr (child
  branch, child_errno_fail label):
  ```c
  child_errno_fail:
      err = errno;
      if (write(execfail[1], &err, sizeof(err))) {
          ;
      }
      exit(127);
  ```
- Reachable path: parent's stdio holds unflushed buffered output at
  pipecmd() time (fully-buffered stream: stdout redirected to a
  file/pipe, or any FILE with pending data), the exec fails, and the
  duplicated stream reaches the child — i.e. fd_tochild/fromchild/
  errfromchild is &pipecmd_preserve for that stream, or the stream
  writes to fd 0/1/2 while the corresponding pipe end coincides
  (preserve is the clean case). exit(127) runs the inherited stdio
  flush (and any atexit handlers registered by the parent or its
  libraries) in the child; the parent later flushes the same buffer
  again.
- Preconditions: none violated — the header explicitly offers
  pipecmd_preserve for leaving streams unchanged (pipecmd.h:21), and
  buffered stdio at fork time is normal.
- Concrete incorrect consequence, observed
  (/tmp/pipecmd-asan/repro-exit-flush.c: stdout dup2'ed to a file,
  printf("unflushed-data") without newline,
  pipecmd(NULL, &pipecmd_preserve, NULL, "/doesnotexist", NULL),
  fflush in parent):
  ```
  child=-1 file-content='unflushed-dataunflushed-data' len=28
  ```
  The 14-byte buffer was written twice (len should be 14). Besides
  corrupting redirected output/logs, the atexit execution in the child
  can re-run arbitrary parent cleanup (unlinking files, shutting down
  subsystems) while the parent is still using them. Note the bug is
  invisible with outfd == NULL only because the child then flushes into
  /dev/null — the flush still happens.
- Regression test: ccan/pipecmd/test/run-execfail-flush.c
  (alarm(10)-bounded, 4 subtests). Currently `not ok 4` (file contains
  the duplicated buffer); after repair it must pass.
- Repair direction: replace `exit(127)` with `_exit(127)` (or
  `_Exit(127)`) at pipecmd.c:167 — the standard idiom for post-fork
  error exits (popen/system implementations do exactly this).

## Rejected candidates (disproved)

- R1: double-close for the shared errfd == outfd case
  (pipecmd.c:89-91, 134-136): errfromchild[] copies fromchild[]'s fds
  but adds nothing to par_close[]/child_close[], so each fd is closed
  exactly once in parent and child; the child uses
  dup2(STDOUT_FILENO, STDERR_FILENO) instead of touching
  errfromchild[1]. Rejected.
- R2: fd-number juggling collisions in the child (a pipe end landing on
  0/1/2 when the caller closed a standard stream, execfail[1] == 3,
  dup2 clobbering another needed fd): all pipes/opens happen before
  fork so their fds are pairwise distinct; the != STDIN_FILENO /
  STDOUT_FILENO / STDERR_FILENO guards (pipecmd.c:124, 129, 137) skip
  redundant dup2s; execfail[1] is mapped to fd 3 (pipecmd.c:144-154)
  only after the 0/1/2 dup2s, and closefrom(4) preserves 0-3. Walked
  through the closed-stdin / closed-stdout / closed-stderr and
  preserve-permutation cases; all sound. Rejected.
- R3: gather_args (pipecmd.c:12-30): growth `realloc(arr,
  sizeof(char *) * (n + 1))` always stays one slot ahead of the
  arr[n++] write; n+1 cannot overflow for any argv list passable
  through a va_list; realloc failure frees arr and pipecmdv frees arr
  via free_noerr after pipecmdarr (pipecmd.c:43). Rejected.
- R4: pipecmdarr returning -1 with errno == EINTR when a signal hits
  the pre-fork pipe()/open() syscalls (the "other" bucket in F3's
  repro): the -1/errno contract is honored, no state is corrupted and
  the call is retryable; indistinguishable from any other pre-fork
  resource failure. Retrying open("/dev/null")/pipe() on EINTR would be
  a nicety, not a defect fix. Rejected.
- R5: child inherits the parent's blocked-signal mask and (until exec)
  handlers: execvp resets caught handlers to default; blocked-mask
  inheritance matches popen(3)/posix_spawn defaults and nothing in
  pipecmd.h promises otherwise. Speculative hardening. Rejected.
- R6: short read on the execfail pipe (pipecmd.c:175): the child writes
  sizeof(int) <= PIPE_BUF atomically and then exits, so read can only
  return sizeof(int), 0 (EOF after successful exec or child death — both
  correctly treated as "no error to report"), or -1 (EINTR, F3). No
  partial-read misclassification exists. Rejected.
- R7: stale errno on the success path / errno clobbered by close():
  pipecmd.h documents only the pid return; errno is meaningful only on
  -1, where it is explicitly set (pipecmd.c:39, 178) or preserved by
  close_noerr. Rejected.
- R8: aliased out-parameters other than the documented errfd == outfd
  (e.g. fd_tochild == fd_fromchild): would double-store at
  pipecmd.c:182-187 and leak one end, but only errfd == outfd sharing
  is documented (pipecmd.h:20). Violation of documented preconditions.
  Rejected.
- R9: pipecmdarr with arr[0] == NULL / pipecmd with cmd == NULL:
  execvp(NULL, ...) fails with EFAULT and the error is correctly
  reported via the execfail pipe; "@arr: ... (first is program to run)"
  (pipecmd.h:43) makes a non-NULL command a documented precondition.
  Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/pipecmd/test/run-execfail-fdleak.c — proves F1 (tests 1-3) and
  F2 (tests 4-5). Currently fails 2/5 in plain and ASan builds
  (`not ok 3`, `not ok 5`: 3 fds leaked by each path);
  alarm(10)-bounded. After repairing F1/F2 it must pass.
- ccan/pipecmd/test/run-eintr.c — proves F3 (false-success half).
  Currently fails 1/1 in plain gcc -O0, gcc -O2 and clang -O1 builds
  (4/4 runs); passes under ASan due to the timing shift described in
  F3; alarm(60)-bounded. After repairing F3 it must pass.
- ccan/pipecmd/test/run-execfail-flush.c — proves F4. Currently fails
  1/4 in plain and ASan builds (`not ok 4`: inherited stdio buffer
  flushed twice); alarm(10)-bounded. After repairing F4 it must pass.
- ccanlint with these files present: 39/45 FAIL, solely because the new
  tests fail against the current code.

No production files were modified (pipecmd.h, pipecmd.c, _info
untouched; verified by git status — the only new paths under
ccan/pipecmd are the three tests above).
