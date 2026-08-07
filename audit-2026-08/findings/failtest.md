# Audit: ccan/failtest

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: failtest.c, failtest.h, failtest_override.h, failtest_proto.h,
failtest_undo.h, _info and ccan/failtest/test/ only.  Deps
ccan/build_assert, ccan/compiler, ccan/err, ccan/hash (excluded),
ccan/htable (audited), ccan/read_write_all, ccan/str (audited),
ccan/time, ccan/tlist: documented behavior taken as given; no misuse
found (htable used via HTABLE_DEFINE_NODUPS_TYPE per its contract;
read_write_all's read_all/write_all used exactly for the control
protocol they are meant for).

Mechanical checks:
- ccanlint (before adding auditor tests): 59/65.  The only check
  losing points is tests_coverage (+0/6).  All other checks pass,
  including tests_pass, tests_pass_valgrind (with the _info-documented
  exception for run-with-fdlimit.c) and examples_compile.
- Build recipe: every test #includes <ccan/failtest/failtest.c>
  directly (the module interposes libc, so this is intentional);
  linked with ccan/hash/hash.c, ccan/err/err.c,
  ccan/read_write_all/read_write_all.c, ccan/time/time.c,
  ccan/htable/htable.c, ccan/tap/tap.c.
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  -g, -I.; per-test `timeout 120`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  7/7 pass cleanly, zero sanitizer diagnostics:
  run-failpath (14 ok), run-history (69), run-locking (5835),
  run-malloc (3), run-open (12), run-with-fdlimit (2), run-write (5).
  Total 5940 ok.  Note: run-history must run with cwd=ccan/failtest
  (it opens test/run-history.c by relative path); run from elsewhere it
  segfaults in the *test* (run-history.c:141 dereferences
  opener_of(-1) == NULL) — test artifact, not a module defect.
- HAVE_FILE_OFFSET_BITS is 0 in this host's config.h, so failtest.h
  does not define _FILE_OFFSET_BITS=64 here (irrelevant on LP64; see
  F6 for the ILP32 build).
- Reproducers live in /tmp/failtest-asan/repro-*.c (temporary, not
  committed).

Auditor-added test files (temporary; keep or remove later).  All are
alarm()-bounded and modify no production files.  All currently FAIL as
designed (plain gcc and clang+ASan builds), except run-off-max which
fails everywhere and run-undo-close which fails to *compile*:
- ccan/failtest/test/run-errno.c — proves F1 (2 of 4 tests fail).
- ccan/failtest/test/run-pipe-real-fail.c — proves F2 (test 2 fails,
  plus a spurious "Leak at" line and forced exit status 1).
- ccan/failtest/test/run-getlk.c — proves F3 (test 2 fails).
- ccan/failtest/test/run-failure-race.c — proves F4 (both tests fail;
  self-checking via re-exec with output capture).
- ccan/failtest/test/run-wronly-write.c — proves F5 (file corruption
  observed).
- ccan/failtest/test/run-off-max.c — proves F6 (fails on LP64 *and*
  ILP32).
- ccan/failtest/test/run-undo-close.c — proves F7 (compile error).
Adding these deliberately breaks `ccanlint ccan/failtest` (tests_pass)
until the fixes land.

## Findings

## F1 — CONFIRMED (FIXED in 87c6c21a): add_history_() never initializes call->error; every wrapper sets `errno` to heap garbage on success, and clobbers the real errno on genuine syscall failures

- Location: ccan/failtest/failtest.c:162-186 (add_history_ assigns
  type/can_leak/file/line/cleanup/backtrace/backtrace_num and copies
  `u`, but never `error`); the uninitialized field is then stored into
  errno at the end of failtest_calloc (failtest.c:917),
  failtest_malloc (:943), failtest_realloc (:1015), failtest_open
  (:1123), failtest_mmap (:1179), failtest_pipe (:1224),
  failtest_add_read (:1268), failtest_add_write (:1370) and
  failtest_fcntl (:1645).
- Reachable path: any program using failtest_override.h (or calling the
  failtest_* wrappers directly) that
  (a) makes a successful wrapped call — `errno = p->error` copies an
      uninitialized heap int into errno; or
  (b) makes a wrapped call whose underlying syscall genuinely fails
      (not injected) — e.g. read() on an O_WRONLY fd, write() on an
      O_RDONLY fd, open() of a missing file, pipe() at the fd limit:
      the kernel sets the real errno, then failtest overwrites it with
      garbage (`p->error` is only ever assigned on *injected* failures
      and in the fcntl real-failure path at failtest.c:1631-1632).
- Preconditions: none violated; this hits the module's documented main
  use (overriding libc calls for arbitrary test programs, _info:8-16).
- Concrete consequences:
  (a) Undefined behavior (indeterminate-value read) and nondeterministic
      errno after successful calls — parent and replay children observe
      different errno values, undermining the reproducibility the module
      exists to provide; a tested program doing `errno = 0; p = malloc();
      if (errno) ...` misbehaves randomly.
  (b) Programs checking errno after a genuine failure (EBADF, ENOENT,
      EMFILE, ...) see garbage instead of the real error and take the
      wrong error path.  This is deterministic misbehavior, not just UB.
- Reproducer/regression test: ccan/failtest/test/run-errno.c (also
  /tmp/failtest-asan/repro-errno.c).  Observed, ASan build:
  ```
  not ok 2 - errno == 0
  # errno after successful failtest_malloc = -1094795586 (garbage)   # 0xBEBEBEBE, ASan malloc fill
  not ok 4 - errno == EBADF
  # errno after genuinely failing failtest_read = -1094795586, expected EBADF(9)
  ```
  Plain clang build with heap poisoning (freed block memset to 0x5A,
  reused for the history struct): errno = 1515870810 = 0x5A5A5A5A —
  proves the read of uninitialized heap, independent of ASan.
- Repair direction: initialize `call->error = 0` in add_history_(), and
  in each wrapper capture the real call's errno
  (`p->error = errno;` right after the real calloc/malloc/.../fcntl,
  success or failure) so the final `errno = p->error;` round-trips
  exactly what the real libc call produced; injected-failure paths keep
  their forced errno.  (failtest_lseek/failtest_close already return the
  real errno untouched and need no change.)

## F2 — CONFIRMED (FIXED in 5c921d59): failtest_pipe() does not handle a genuine pipe() failure — uninitialized fds closed at cleanup, spurious "Leak" report, forced exit status 1

- Location: ccan/failtest/failtest.c:1213-1215 (failtest_pipe):
  ```
  p->u.pipe.ret = pipe(p->u.pipe.fds);
  p->u.pipe.closed[0] = p->u.pipe.closed[1] = false;
  set_cleanup(p, cleanup_pipe, struct pipe_call);
  ```
  The return of the real pipe() is never checked (contrast
  failtest_open, which handles a genuine failure at failtest.c:1114-1116
  by setting closed=true and can_leak=false).  cleanup_pipe
  (failtest.c:1192-1201) then close()es both fds, and the leak check in
  failtest_cleanup (failtest.c:616-622) fires.
- Reachable path: fd exhaustion (RLIMIT_NOFILE) — exactly the scenario
  the module's own run-with-fdlimit.c explores for open().  Caller uses
  the pipe() override (failtest_override.h:40-42), the real pipe()
  fails with EMFILE, the caller handles -1 correctly.
- Preconditions: none violated.
- Concrete consequences (all three observed):
  (a) failtest_exit() calls close() on the *uninitialized* fds[] of the
      history record (arbitrary heap values — 0xBEBEBEBE under ASan;
      in a plain build these can coincidentally equal a live fd, which
      would then be closed from under the test program);
  (b) a spurious `Leak at <pipe call site>` report for a pipe that
      never existed, forcing exit status 1 — a correctly-written test
      is failed by the harness;
  (c) errno is garbage instead of EMFILE (F1 root cause).
- Reproducer/regression test: ccan/failtest/test/run-pipe-real-fail.c
  (also /tmp/failtest-asan/repro-pipe.c).  Observed:
  ```
  ok 1 - r == -1
  not ok 2 - errno == EMFILE
  # errno after genuine pipe() failure = -1094795586, expected EMFILE(24)
  Leak at run-pipe-real-fail.c:1: --failpath=p      <- spurious
  rc=1
  ```
- Repair direction: mirror failtest_open's handling —
  `if (p->u.pipe.ret == -1) { p->error = errno; p->can_leak = false; }
   else { p->u.pipe.closed[0] = p->u.pipe.closed[1] = false;
          set_cleanup(...); }`
  (and only memcpy the fds to the caller's pipefd[] on success, or
  keep the deliberate valgrind-bait behavior but skip cleanup).

## F3 — CONFIRMED (FIXED in ebfea397): failtest_fcntl(F_GETLK) discards the result — caller's struct flock is never updated

- Location: ccan/failtest/failtest.c:1599-1605:
  ```
  case F_GETLK:
      get_locks();
      va_start(ap, cmd);
      call.arg.fl = *va_arg(ap, struct flock *);   /* copy IN */
      va_end(ap);
      return fcntl(fd, cmd, &call.arg.fl);          /* result written to the copy, then dropped */
  ```
- Reachable path: any program under failtest_override.h calling
  fcntl(fd, F_GETLK, &fl) — the POSIX way to test whether a byte range
  is locked.  The kernel writes the conflicting-lock details (or
  l_type == F_UNLCK when free) into the *copy*; the caller's fl is left
  exactly as passed in.
- Preconditions: none violated.
- Concrete consequence: a program querying "is this locked?" gets a
  stale answer — with the customary fl.l_type = F_WRLCK query setup it
  is told "locked" even when the range is free.  Wrong control flow in
  the tested program, in both parent and children.
- Reproducer/regression test: ccan/failtest/test/run-getlk.c (also
  /tmp/failtest-asan/repro-getlk.c).  Observed:
  ```
  ok 1 - failtest_fcntl(fd, "run-getlk.c", 1, F_GETLK, &fl) == 0
  not ok 2 - fl.l_type == F_UNLCK
  # l_type = 1 after F_GETLK on unlocked file; caller's flock never updated
  ok 3 - fcntl(fd, F_GETLK, &fl) == 0 && fl.l_type == F_UNLCK   (direct fcntl works)
  ```
- Repair direction: F_GETLK is a pure query and needs no history entry;
  call the real fcntl on the *caller's* struct
  (`struct flock *fl = va_arg(ap, struct flock *);
    get_locks(); return fcntl(fd, cmd, fl);`).  Keep the get_locks()
  call first so the answer reflects locks currently held by the parent.

## F4 — CONFIRMED (FIXED in a8c9249c): a failing child's report races with the parent — buffered stdout lost, child killed by SIGPIPE, parent reports "Killed by signal 13" instead of the real failure

- Location: ccan/failtest/failtest.c:616-622 (failtest_cleanup prints
  "Leak at ..." with printf() to stdout, which in a child is a *fully
  buffered* pipe to the parent), failtest.c:629-633 (tell_parent sends
  FAILURE with a direct unbuffered write_all() on the control fd, and
  only the subsequent exit() flushes stdout); same ordering in
  child_fail (failtest.c:296-310: stderr prints are unbuffered and
  safe, the printf("To reproduce: ...") to stdout is not).  Parent
  side: failtest.c:860 (`} while (type != FAILURE);`) stops reading the
  child's output the moment FAILURE arrives, failtest.c:862-863 closes
  both pipe read ends, failtest.c:865-871 then reports the child as
  "Killed by signal %u".
- Reachable path: any failtest child that fails *and* has buffered
  stdout at exit — the canonical case being a leak detected in a
  (grand)child, but also any late stdout output before a child_fail.
  The window is the parent's poll-wakeup/read/close versus the child's
  exit-time stdio flush; unforced it fires nondeterministically
  (observed in the nested-fork run of repro-wronly: the grandchild's
  "Leak at" was lost and "Killed by signal 13" reported instead).
- Preconditions: none violated; leaking in a child is a normal,
  expected event for this harness (that is what the leak check is for).
- Concrete consequence: the module's central diagnostics (the "Leak at
  file:line: --failpath=..." line, the child's captured output) are
  silently lost, and the failure is misreported as "Killed by signal
  13", sending the user hunting for a nonexistent signal problem.
- Reproducer/regression test: ccan/failtest/test/run-failure-race.c
  (also /tmp/failtest-asan/repro-sigpipe.c).  The test's atexit()
  handler only widens the race window (atexit handlers run before stdio
  flushing in exit()); the race itself is entirely inside failtest.
  Observed, deterministic with the widener:
  ```
  [pid C] write(1, "Leak at /tmp/failtest-asan/repro"..., 61) = -1 EPIPE (Broken pipe)
  [pid C] --- SIGPIPE --- +++ killed by SIGPIPE +++
  [parent] write(2, "Killed by signal 13: ") 
  ```
  Regression test output (pre-fix): `not ok 1 - strstr(buf, "Leak at
  ...") != NULL`, `not ok 2 - strstr(buf, "Killed by signal") == NULL`;
  the captured output contains only "To reproduce: --failpath=M".
- Repair direction: fflush(stdout) (and stderr, harmless) immediately
  before every tell_parent() on a failure path (failtest_cleanup and
  child_fail); alternatively make the parent keep draining output until
  POLLHUP even after type == FAILURE, before close/waitpid.  The former
  is minimal and removes the SIGPIPE as well.

## F5 — CONFIRMED (FIXED in 96dd8cfd): writes to O_WRONLY files (and O_TRUNC opens of unreadable files) cannot be undone — the parent's file contents are silently corrupted

- Location: ccan/failtest/failtest.c:410-436 (save_contents reads the
  old contents with pread(fd, ...), which fails with EBADF on an
  O_WRONLY fd; it warns via fwarn and records s->count = 0),
  failtest.c:470-478 (restore_contents then pwrite()s 0 bytes and
  ftruncate()s to old_len — bytes the child overwrote inside old_len
  stay modified).  The O_TRUNC variant: save_file (failtest.c:1030-1043)
  open()s the path O_RDONLY, which fails for a write-only (mode 0200)
  file, so cleanup_open (failtest.c:1062-1075) restores nothing after
  the child's truncating open.  See also restore_contents' reopen with
  O_RDWR (failtest.c:452), which fails for O_WRONLY/O_RDONLY-permission
  files.
- Reachable path: tested program opens a file O_WRONLY through the
  override (a legitimate, common mode for output files); any failtest
  child then writes to the inherited fd.  The child's write really
  happens (fork shares the file); at the child's failtest_exit() the
  undo restores nothing but the length.
- Preconditions: none violated; the module documents no restriction to
  O_RDWR files, and run-write.c exercises exactly this undo machinery
  (with an O_RDWR file, where it works).
- Concrete consequence: after the failing child exits, the parent's
  copy of the file is corrupted — all later failure-path children and
  the parent's own checks see wrong contents, so test verdicts from
  that point on are meaningless (false passes or false failures).  Only
  a warning on stderr ("failed to save old contents!") hints at it.
- Reproducer/regression test: ccan/failtest/test/run-wronly-write.c
  (also /tmp/failtest-asan/repro-wronly.c).  Observed:
  ```
  failtest_write: failed to save old contents!: Bad file descriptor [oMw]
  not ok 1 - strcmp(buf, "Hello world!") == 0
  # file contents after child write: "XYZlo world!" (expected "Hello world!")
  ```
  (No in-repo failtest user currently opens O_WRONLY — rfc822/talloc/
  rbtree/deque/xstring and the ntdb junkcode tests all use O_RDWR — so
  in-tree reach is nil today; the defect is in the harness's contract,
  shown reachable by the reproducer.)
- Repair direction: when pread() on the fd fails, fall back to reading
  the old contents through a fresh O_RDONLY open of
  opener->u.open.pathname (or /proc/self/fd/<fd>); for genuinely
  unreadable files (perms) restoration is impossible — at minimum
  document that restriction in _info, and consider failing the write
  loudly instead of corrupting the parent's state.

## F6 — CONFIRMED (FIXED in 43c12175): off_max() constants are each missing one hex digit — wrong "end of file" sentinel on both ILP32 and LP64

- Location: ccan/failtest/failtest.c:371-374:
  ```
  if (sizeof(off_t) == 4)
      return (off_t)0x7FFFFFF;            /* = 0x07FFFFFF  = 2^27-1  = 134217727 */
  else
      return (off_t)0x7FFFFFFFFFFFFFFULL; /* = 0x07FFFFFFFFFFFFFF = 2^59-1 = 576460752303423487 */
  ```
  Correct values: 0x7FFFFFFF (2^31-1) and 0x7FFFFFFFFFFFFFFF (2^63-1).
- Reachable path: off_max() feeds end_of() (failtest.c:1569-1574), the
  "lock to end of file" (l_len == 0) sentinel, and get_locks()
  (failtest.c:397-398), which maps end == off_max() back to l_len = 0.
  Every fcntl(F_SETLK/F_SETLKW) with l_len == 0 through the override
  records its lock as ending at 2^59-1 (LP64) / 2^27-1 (ILP32) instead
  of the true off_t maximum.
- Preconditions: none violated; l_len == 0 is standard POSIX usage.
- Concrete consequence: the lock bookkeeping's overlap arithmetic
  (add_lock, failtest.c:1400-1478) treats bytes past 2^59-1 as
  uncovered by "to-EOF" locks, and a lock whose recorded end happens to
  equal the wrong sentinel is re-acquired as l_len=0 (to *real* EOF) in
  children.  Honest impact assessment: recording/re-acquisition
  round-trips consistently (both sides use off_max()), so divergence
  needs lock ranges at or beyond 2^59 bytes (~512 PiB) on LP64 —
  impractical today; on ILP32 the boundary is 128 MiB, reachable in
  principle (this tree's config.h has HAVE_FILE_OFFSET_BITS 0, so a
  -m32 build really has 4-byte off_t).  The constants are objectively
  wrong on every platform, demonstrated by the failing test.
- Reproducer/regression test: ccan/failtest/test/run-off-max.c (fails
  on LP64 and, verified with a -m32 build of
  /tmp/failtest-asan/repro-offmax.c, on ILP32):
  ```
  -m32: sizeof(off_t)=4  off_max()=134217727 (0x7ffffff)   expected 2147483647 (0x7fffffff)
  LP64: not ok 1 - off_max() == (off_t)0x7FFFFFFFFFFFFFFFLL
  ```
- Repair direction: add the missing F to both constants, or replace the
  whole function with a type-based expression, e.g.
  `return (off_t)((uint64_t)-1 >> (64 - sizeof(off_t) * 8));` guarded by
  the existing BUILD_ASSERT.

## F7 — CONFIRMED (FIXED in 36ef65f6): failtest_undo.h close() macro has wrong arity (any user fails to compile); header also fails to undo pread/pwrite/free/getpid; _info references a nonexistent file

- Location: ccan/failtest/failtest_undo.h:43
  `#define close(fd) failtest_close(fd)` — failtest_close() takes three
  arguments (failtest_proto.h:30); every other macro in the same header
  passes the (NULL, 0) suppression pair.  Also missing from the undo
  header: pread, pwrite (stay the failing versions with __FILE__/__LINE__
  — failures keep being injected after "undo"), free (stays
  failtest_free — benign), getpid (stays failtest_getpid — benign after
  init).  Doc bug: _info:11-12 tells users to include
  "failtest_restore.h", a file that does not exist (the header is
  failtest_undo.h).
- Reachable path: any translation unit doing what the header is for —
  `#include <ccan/failtest/failtest_undo.h>` then calling close().
- Preconditions: none violated; this is the header's documented purpose
  (_info:10-12, failtest_undo.h:4).
- Concrete consequence: compile error, observed:
  ```
  ccan/failtest/test/run-undo-close.c:29:6: error: too few arguments to function call, expected 3, have 1
        ok1(close(fd) == 0);
  ./ccan/failtest/failtest_undo.h:43:36: note: expanded from macro 'close'
   #define close(fd) failtest_close(fd)
  ```
  (No in-tree consumer includes failtest_undo.h, which is presumably
  why this survived.)
- Reproducer/regression test: ccan/failtest/test/run-undo-close.c
  (fails to compile pre-fix; /tmp/failtest-asan/repro-undo.c likewise).
- Repair direction: `#define close(fd) failtest_close((fd), NULL, 0)`;
  add the missing `#undef`/`#define` pairs for pread and pwrite (NULL,0
  style); fix the _info reference to failtest_undo.h.

## Rejected candidates (disproved)

- R1: failtest_fcntl(F_SETFL/F_SETFD) reads its variadic argument with
  va_arg(ap, long) (failtest.c:1590-1592) while fcntl(F_SETFL, ...) is
  specified to take an int and typical callers pass an int.
  Technically UB (C11 7.16.1.1), but benign on x86-64 and arm64
  (varargs ints occupy 64-bit slots; the low bits survive the
  long round-trip both into and out of the real fcntl).
  /tmp/failtest-asan/repro-setfl.c passes an int O_APPEND and behaves
  correctly under ASan+UBSan.  run-locking.c passes a long, matching
  the implementation.  Portability nit for exotic ABIs only.
- R2: move_fd_to_high() prints "Max is %i\n" to stdout on every call
  (failtest.c:204) — once in failtest_init and 4x per fork in
  should_fail's parent.  Leftover debug output polluting the TAP
  stream of every failtest-based test, but TAP verdicts are unaffected
  (all in-tree consumers pass, including under ccanlint's TAP parser).
  Cosmetic.
- R3: Parent poll loop (failtest.c:841-846): `out = realloc(out,
  outlen + 8192)` unchecked and `outlen += len` with len possibly -1.
  Disproved reachable: glibc signal() installs SIGUSR1 with SA_RESTART,
  so read() after poll(POLLIN) cannot return EINTR here, and realloc
  failure needs genuine OOM (speculative per AGENTS.md).  Unbounded
  buffering of a flooding child's output is by design (the parent must
  capture it to display on failure).
- R4: Writes to seekable fds not opened through failtest (inherited
  fds, sockets, dup'd fds) are executed by both parent and child
  (double write to the shared open file description).  Documented
  limitation: "FIXME: socket, dup, etc are untracked!"
  (failtest.c:566); the off == -1 mirroring covers non-seekable fds.
  Caller precondition: route the tested I/O through the override.
- R5: Unchecked malloc/realloc in add_history_, save_contents,
  read_write_info, failpath_string, get_backtrace.  OOM-only paths;
  speculative hardening per AGENTS.md.
- R6: failtest_init() with fds exhausted: dup(STDERR_FILENO) fails,
  move_fd_to_high(-1) scans the whole fd space warning per fd, and
  fdopen(-1) leaves warnf NULL for a later fwarn() to segfault on.
  Requires EMFILE before init; run-with-fdlimit.c deliberately halves
  the limit before init and passes.  Not reachable in normal use.
- R7: open() mode handling: for a two-argument open (no O_CREAT),
  call.mode is left uninitialized and passed to the real open()
  (failtest.c:1092, 1113) — harmless per POSIX (the kernel ignores the
  third argument without O_CREAT).  O_TMPFILE (which requires a mode
  but does not set O_CREAT) would get a garbage mode — no in-tree user,
  untested in practice.  Note only.
- R8: Reads from pipes/non-seekable fds cannot be undone (the bytes are
  consumed from the shared description), and cleanup_read's restoring
  lseek() on such fds fails, producing fwarn noise
  (failtest.c:1235-1237).  Inherent design limitation of offset-based
  undo; the seek-back is best-effort with a warning.  Rejected.
- R9: failtest_pipe's injected-failure path writes p->u.open.ret
  (failtest.c:1209) instead of p->u.pipe.ret — wrong union member name,
  but both structs have `int ret` as first member, so identical offset
  and type; works as intended.  The memcpy of uninitialized fds into
  the caller's pipefd[] on failure is deliberate valgrind bait
  (failtest.c:1222 comment).  Style only.
- R10: get_locks() abort()s if a re-acquired F_SETLKW fails
  (failtest.c:402-403).  The lock handoff protocol (child asks parent
  to RELEASE_LOCKS first) makes this unreachable absent kernel
  weirdness; if it ever fired, the parent reports "Killed by signal 6"
  — an adequate diagnosis.  Rejected.
- R11: failtest_close of an invalid or already-closed fd: recorded,
  opener_of() correctly returns NULL (close records are matched at
  failtest.c:544-548), real close() returns EBADF to the caller.  Pipe
  double-close is caught by assert (failtest.c:1509-1513).  Correct.
- R12: should_fail() pipe() failure before fork leaks the first pipe's
  fds if the second pipe() fails (failtest.c:746-747) — err(1) exits
  the process immediately anyway; fatal harness error path.  Rejected.

No other findings.  The rest of the audit plan checked out clean under
the sanitizer suite and code inspection: the parent/child control
protocol uses read_all/write_all throughout with no partial-read
hazards; nested-fork fd passing (high-fd dup2) is consistent; the
failtable duplicate suppression, --failpath/--debugpath replay
(follow_path validation), malloc/calloc/realloc failure semantics
(realloc failure keeps the old pointer tracked; fixup_ptr_history on
success), O_TRUNC save/restore for readable files, mmap save/restore,
lock split/merge bookkeeping (run-locking's 5835 cases), and the
poll/timeout/SIGUSR1 hand-down path all behave as designed.
