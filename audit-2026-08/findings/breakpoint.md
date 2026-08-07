# Audit: ccan/breakpoint

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).

Scope: breakpoint.h (24 lines), breakpoint.c (32 lines), _info,
ccan/breakpoint/test/ (run.c). ~56 LOC. Dependency ccan/compiler (COLD
attribute only) treated as given. Suspects from the brief: signal
handling correctness, arch support gating, reentrancy — signal handling
and reentrancy confirmed (F1, F2, F3); arch gating disproved (R4).

Documented contract (_info:6-9): "This code allows you to insert
breakpoints within a program.  These will do nothing unless your
program is run under GDB."  breakpoint.h:15-16: "breakpoint - stop if
running under the debugger."  No documented preconditions on signal
masks, dispositions, threads, or fork.

Mechanism: breakpoint_init() (breakpoint.c:17-32) installs a temporary
SIGTRAP handler (`trap`), kill(getpid(), SIGTRAP), then restores the
old disposition.  If `trap` ran, no debugger is swallowing SIGTRAP; if
it did not run (gdb's default `handle SIGTRAP stop print nopass`), the
module concludes breakpoint_under_debug = true, and every later
breakpoint() (breakpoint.h:17-23) kill()s itself SIGTRAP.

## Mechanical checks

- ccanlint (before auditor additions): **38/43 PASS**, every check
  passes; partial credit only on tests_coverage (+2/6) and
  examples_exist (+1/2, no Example: section in breakpoint.h).
- After adding the three auditor regression tests: ccanlint **34/41
  FAIL** — exactly as designed: run-blocked, run-threads and run-fork
  all fail against the current code (tests_compile passes for all
  three; the failure is tests_pass).
- Existing test under clang 18.1.3 ASan+UBSan (`-fno-sanitize=function`
  -g -I.; run.c #includes breakpoint.c directly; linked with
  ccan/tap/tap.c; `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  run.c: **2/2 clean**, zero sanitizer diagnostics.
- -m32: unexpectedly available for this module (no errno.h chain);
  run.c builds and passes **2/2** under clang -m32.
- Reproducers in /tmp/bp-asan/ (temporary, not committed):
  repro-blocked.c, repro-threads.c, repro-threads-small.c,
  repro-fork.c, repro-ign.c.

## Findings

## F1 — CONFIRMED (FIXED in 339b359a): SIGTRAP blocked at first breakpoint() call: false "under debugger" detection, and a pending SIGTRAP that kills the process when the caller restores its mask

- Location: ccan/breakpoint/breakpoint.c:17-32, function
  breakpoint_init, concretely line 25 (`kill(getpid(), SIGTRAP);`) and
  lines 28-31:
  ```c
  if (!breakpoint_initialized) {
      breakpoint_initialized = true;
      breakpoint_under_debug = true;
  }
  ```
- Reachable path: caller's thread has SIGTRAP blocked (sigprocmask /
  pthread_sigmask — ordinary in daemons and libraries that block
  signals in critical regions) at the first breakpoint() call.  The
  kill at breakpoint.c:25 only *pends* the signal; `trap` can never run
  while the mask is in place, so breakpoint.c:28-31 concludes
  "under debugger" although none is attached.  Worse, the module leaves
  its own SIGTRAP pending behind the caller's back: when the caller
  later restores its signal mask, the pending SIGTRAP is delivered to
  the caller's *original* disposition — default: terminate + core dump.
  Every subsequent breakpoint() call re-sends SIGTRAP too
  (breakpoint.h:21-22).
- Preconditions: none violated — nothing in _info or breakpoint.h
  requires SIGTRAP to be unblocked; the contract is "do nothing unless
  run under GDB".
- Concrete incorrect consequence, observed
  (/tmp/bp-asan/repro-blocked.c, plain clang build; child blocks
  SIGTRAP, calls breakpoint(), prints the flags, restores the mask):
  ```
  child: breakpoint_initialized=1 breakpoint_under_debug=1 (expected 1/0)
  parent: child killed by signal 5 (Trace/breakpoint trap) [core dumped]
  ```
  Two distinct defects in one: (a) breakpoint_under_debug is true with
  no debugger present (misdetection, and breakpoint() now sends SIGTRAP
  on every call), (b) the process is terminated with a core dump by a
  signal the module pended and never consumed.
- Regression test: ccan/breakpoint/test/run-blocked.c
  (alarm(10)-bounded, 3 subtests).  Currently `not ok 2 -
  breakpoint_initialized && !breakpoint_under_debug`, then the process
  is killed by SIGTRAP when the mask is restored (exit 133); same under
  ASan.  Must pass after repair.
- Repair direction: in breakpoint_init, save the calling thread's
  signal mask, unblock SIGTRAP for the duration of the probe, and
  restore the mask afterwards (pthread_sigmask/sigprocmask around the
  sigaction/kill dance).  To avoid consuming a SIGTRAP the *caller* had
  pending, check sigpending() first and only swallow the probe signal
  when none was pending (or re-raise it before restoring the mask).

## F2 — CONFIRMED (documented in 6f0b1b03; regression test TODO-wrapped in 339b359a): breakpoint_init() is not thread-safe; concurrent first use terminates the process with SIGTRAP

- Location: ccan/breakpoint/breakpoint.c:17-32 (breakpoint_init) and
  breakpoint.h:19-20 (the unguarded `if (!breakpoint_initialized)
  breakpoint_init();` in the public breakpoint()).
- Reachable path: two threads call breakpoint() concurrently before
  initialization (the documented lazy-init usage).  Fatal interleaving:
  ```
  A: sigaction(trap, &oldA=orig)
  B: sigaction(trap, &oldB=trap)   /* B saves A's handler as "old" */
  A: kill -> trap runs -> initialized = true
  A: sigaction(oldA)               /* disposition back to orig */
  B: kill -> SIGTRAP with original (default) disposition
     -> process terminated + core dump
  ```
  Other interleavings leave `trap` installed permanently or misdetect;
  the underlying causes are the unsynchronized sigaction dance and the
  data race on the plain-bool globals.
- Preconditions: none violated — nothing documents breakpoint() as
  single-threaded-only.
- Concrete incorrect consequence, observed
  (/tmp/bp-asan/repro-threads.c, clang -O2: forked children each run 8
  threads hammering the uninitialized-then-breakpoint_init() cycle,
  200k iterations each):
  ```
  round 0: child killed by signal 5 (Trace/breakpoint trap) [core dumped]
  ```
  Crash within ~0.1 s on the first round (20-core host).  A smaller
  config (8 threads x 20k iterations) crashed in 4 of 6 runs.
- Regression test: ccan/breakpoint/test/run-threads.c
  (alarm(60)-bounded; up to 5 forked rounds of 8 threads x 20k
  iterations, resetting breakpoint_initialized each iteration to
  simulate repeated first-use races; fails if any child dies by
  SIGTRAP).  Currently `not ok 1 - !crashed` (plain and ASan builds).
  Must pass after repair.
- Repair direction: perform the whole detection under a lock or
  pthread_once (e.g. a static pthread_mutex_t around breakpoint_init's
  body, plus making the breakpoint_initialized check in breakpoint()
  use the same once/lock).  Any correct repair makes the test pass; the
  test only asserts no child dies by SIGTRAP.

## F3 — CONFIRMED (FIXED in 339b359a): debugger-detection state is stale across fork(); the untraced child kills itself with SIGTRAP

- Location: ccan/breakpoint/breakpoint.c:8-9 (the plain globals
  breakpoint_initialized / breakpoint_under_debug) consumed by
  breakpoint() at breakpoint.h:19-22.
- Reachable path: process runs under gdb, breakpoint() executes
  (detection sets breakpoint_under_debug = true), process forks.  With
  gdb's default follow-fork-mode=parent / detach-on-fork=on the child
  is NOT traced, but inherits breakpoint_under_debug = true, so its
  next breakpoint() does kill(getpid(), SIGTRAP) with the default
  disposition — terminated + core dump — although it is not running
  under the debugger.  Contract: "These will do nothing unless your
  program is run under GDB" (_info:8-9).  The same stale-state root
  cause applies after a debugger `detach` (detection is one-shot and
  never re-validated).
- Preconditions: none violated — fork after breakpoint() is normal
  program behavior; nothing documents otherwise.
- Concrete incorrect consequence, observed
  (/tmp/bp-asan/repro-fork.c under gdb 15.1,
  `gdb -batch -ex 'handle SIGTRAP nostop noprint nopass' -ex run`;
  nostop/nopass keeps detection working and the session scriptable):
  ```
  parent: initialized=1 under_debug=1
  [Detaching after fork from child process ...]
  parent: child killed by signal 5 (Trace/breakpoint trap) [core dumped]
  ```
  The same binary outside gdb: `parent: initialized=1 under_debug=0`,
  `child: survived breakpoint()`, `parent: child exited 0`.
- Regression test: ccan/breakpoint/test/run-fork.c (alarm(10)-bounded).
  Reproduces the exact post-fork state directly (sets
  breakpoint_initialized/under_debug = true, forks, child calls
  breakpoint()); the gdb run above proves the state arises naturally.
  Currently `not ok 1` — child killed by SIGTRAP (plain and ASan
  builds).  Must pass after repair.
- Repair direction: re-validate the debugger on each breakpoint() call
  (e.g. TracerPid from /proc/self/status, or ptrace(PTRACE_TRACEME)
  probing) instead of caching forever, or register a pthread_atfork
  child handler that resets breakpoint_initialized and
  breakpoint_under_debug so the child re-detects.

## Rejected candidates (disproved)

- R1: unchecked sigaction()/kill() return values (breakpoint.c:24-26):
  sigaction can only fail on an invalid signum (SIGTRAP is valid) and
  kill(getpid(), ...) only on ESRCH/EPERM, unreachable for self.
  No reachable failure.  Rejected.
- R2: `trap` writes plain `bool breakpoint_initialized`
  (breakpoint.c:12-15) instead of volatile sig_atomic_t: the signal is
  self-generated synchronously by the kill() at breakpoint.c:25, an
  external call the compiler must assume can modify any global, so the
  value is reloaded; no observable consequence on any real target.
  Formal-only issue.  Rejected.
- R3: caller disposition SIG_IGN for SIGTRAP: breakpoint_init installs
  `trap` over it before the kill and restores SIG_IGN afterwards, so
  detection works and nothing leaks.  Verified with
  /tmp/bp-asan/repro-ign.c: `initialized=1 under_debug=0 (no debugger
  present)`, `survived`.  Rejected.
- R4: arch support gating (suspect list): the module contains no
  arch-specific code at all — it probes via POSIX sigaction/kill, not
  int3/asm — so there is nothing to gate; ccanlint info_ported passes
  and the -m32 build passes 2/2.  Rejected.
- R5: kill(getpid(), SIGTRAP) in a multithreaded process may deliver
  the probe signal to a different unblocked thread: `trap` only sets a
  global, so detection is unaffected by which thread runs it; under gdb
  the whole process stops regardless.  Rejected.
- R6: dependence on gdb's default `handle SIGTRAP stop nopass` (a
  `pass` configuration, strace, or other tracers misdetects): the
  documented contract is specifically GDB ("do nothing unless your
  program is run under GDB", _info:8-9); non-default debugger
  configuration is out of contract.  Rejected.
- R7: an external SIGTRAP arriving during the init window would be
  swallowed by `trap` (and a genuine int3 would loop): requires an
  unrelated SIGTRAP source inside a three-call window; speculative, no
  demonstrated path.  Rejected.
- R8: data race on the plain-bool globals themselves: benign on its own
  (both writes go towards the same values); the harmful manifestation
  is the sigaction race, reported as F2.  Folded into F2.  Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/breakpoint/test/run-blocked.c — proves F1.  Currently fails
  (`not ok 2`, then the process is killed by SIGTRAP on mask restore,
  exit 133) in plain and ASan builds; alarm(10)-bounded, 3 subtests.
  Must pass after repairing F1.
- ccan/breakpoint/test/run-threads.c — proves F2.  Currently fails
  (`not ok 1 - !crashed`, child killed by SIGTRAP, usually in round 0)
  in plain and ASan builds; alarm(60)-bounded, 1 subtest.  Must pass
  after repairing F2.
- ccan/breakpoint/test/run-fork.c — proves F3.  Currently fails
  (`not ok 1`, child killed by SIGTRAP) in plain and ASan builds;
  alarm(10)-bounded, 1 subtest.  Must pass after repairing F3.
- All three #include ccan/breakpoint/breakpoint.c per run-test
  convention (ccanlint only links module objects into api tests).
- ccanlint with all three present: 34/41 FAIL, solely because these
  tests fail against the current code (tests_pass).

No production files were modified (breakpoint.h, breakpoint.c, _info
untouched; verified by git status — the only new paths under
ccan/breakpoint are the three tests above).
