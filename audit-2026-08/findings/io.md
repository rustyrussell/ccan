# Audit: ccan/io

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: io.h, io_plan.h, io.c, poll.c, backend.h, SCENARIOS and
ccan/io/test/ only.  Submodule ccan/io/fdpass is a separate module (not
audited); benchmarks/ excluded.  Deps container_of/list/tal already
audited; time/timer taken as documented (io's use of them checked:
timers_expire/timer_earliest/time_to_msec usage in poll.c:396-415 is
correct).

SCENARIOS note: the file describes a long-obsolete API (struct
io_event, queue_read/queue_write — none of which exist any more).  Its
four scenarios (simple read→write→close, pass-through, pass-through-
and-connect, chatroom broadcast) were mapped onto the current state
machine and verified by inspection plus the existing test suite
(run-02, run-12, run-14, run-16, run-40 cover the equivalent flows);
no scenario-specific defect found.

Mechanical checks:
- ccanlint: 120/126.  The only failing check is tests_coverage (+0/6;
  "Module's tests cover all the code ... PASS (+0/6)").  All other
  checks pass.
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  linked with tap.c, timer.c, tal.c, list.c, time.c, ilog.c, take.c;
  per-test `timeout 60`):
  - 29/31 run-* tests pass cleanly, no sanitizer diagnostics:
    run-01 (10 ok), run-02 (10), run-03 (22), run-04 (11), run-05 (9),
    run-06 (13), run-07 (13), run-08-hangup-on-idle (6),
    run-08-read-after-hangup (3), run-09 (8), run-10 (105), run-12 (9),
    run-13 (3), run-14 (9), run-15 (21), run-16 (9), run-17 (13),
    run-18 (12), run-19 (9), run-20 (7), run-21 (15), run-22 (1),
    run-40 (3), run-41 (8), run-43 (5), run-44 (4), run-45 (2),
    run-46 (8), run-48 (8).
  - run-30-io_flush_sync: does not COMPILE under clang (typesafe_cb
    rejects `char *` arg vs `const char *` param — a typesafe_cb
    gcc/clang divergence, not an io defect).  Built with gcc instead:
    passes 9/9 under gcc ASan+UBSan, no diagnostics.
  - run-47-exclusive-duplex: FAILS under clang (test 5), passes under
    gcc (plain and ASan+UBSan).  Root cause is a test bug, not a module
    bug — see R1 below.  No sanitizer diagnostics either way.
  - Note: forked children of ASan binaries print LeakSanitizer
    "fatal error" hints (LSan-after-fork artifact); harmless, exit
    codes unaffected.
- Reproducers live in /tmp/io-asan/repro-*.c (temporary, not committed).

Auditor-added test files (temporary; keep or remove later):
- ccan/io/test/run-49-exclusive-always-spin.c — proves F1 (currently
  fails: test 3, io_loop returned the "spin detected" sentinel).
- ccan/io/test/run-50-listener-blocking-accept.c — proves F2 (currently
  fails: test 2, listener fd not O_NONBLOCK; test 4, child hung in
  accept() and was killed by SIGALRM).

## Findings

## F1 — CONFIRMED (FIXED in 4ceeef2b): exclusive conn + pending non-exclusive always plan makes io_loop busy-spin at 100% CPU

- Location: ccan/io/poll.c:417-419 (`if (num_always != 0) ms_timeout = 0;`)
  combined with ccan/io/poll.c:292-311 `handle_always()`, which skips
  non-exclusive always plans (`if (num_exclusive && !*exclusive(plan))
  continue;`) and returns false when none are runnable.
- Reachable path:
  1. conn A calls io_conn_exclusive(A, true) (documented API).
  2. conn B (non-exclusive) gets an always plan queued — e.g. A's
     callback io_wake()s B's io_wait() plan (io_do_wakeup → set_always,
     io.c:484-491), or B returned io_always() before A went exclusive.
  3. Every io_loop iteration: num_always != 0 forces ms_timeout = 0;
     poll() returns instantly; handle_always() skips B's plan (not
     exclusive) and returns false; repeat.  B's plan staying queued is
     correct per documentation ("no non-exclusive io_conn will be
     serviced"); the busy-polling is not.
- Preconditions: none violated — io_conn_exclusive() and io_wake() used
  exactly as documented in io.h:761-787 and io.h:582-595.
- Concrete consequence: io_loop spins at 100% CPU for as long as any
  exclusive conn exists while a non-exclusive always plan is pending
  (e.g. the entire duration of an "exclusive flush" phase), instead of
  blocking on the exclusive fds.  Denial of service / battery burn in
  any long-running server using io_conn_exclusive with io_wait/io_wake.
- Reproducer: /tmp/io-asan/repro-exclusive-always-spin.c — after arming
  the scenario, poll is invoked 100001 times with timeout == 0 and zero
  progress before the reproducer breaks out ("SPIN DETECTED").
  Regression test ccan/io/test/run-49-exclusive-always-spin.c fails as
  designed: `not ok 3 - ret == &good` (io_loop returned the
  spin-detected sentinel after >100 consecutive zero-timeout polls).
- Observed output: `poll_calls=100002 zero_timeout=100001 saw_spin=1`
  (clang 18 ASan+UBSan; sanitizers themselves report nothing — pure
  liveness/resource defect).
- Repair direction: only force ms_timeout = 0 when a *runnable* always
  plan exists, e.g. in io_loop replace the `num_always != 0` test with
  `num_always != 0 && (num_exclusive == 0 || <some always[i] is
  exclusive>)` (an O(num_always) scan at the same place, or an
  always-exclusive counter maintained by backend_set_exclusive/
  backend_new_always/remove_from_always).

## F2 — CONFIRMED (FIXED in 64822afc): io_new_listener() leaves the listen fd blocking; a vanished pending connection hangs io_loop in accept()

- Location: ccan/io/io.c:20-37 `io_new_listener_()` — never calls
  io_fd_block(fd, false), unlike io_new_conn_() (io.c:105-106).  The
  blocking accept() is at ccan/io/poll.c:271 `accept_conn()`.
- Reachable path (single process, no API misuse):
  1. Client connect()s; the completed connection sits in the accept
     queue; poll() reports the listener POLLIN.
  2. Before io_loop's rotation reaches the listener (the fairness
     rotation at poll.c:434-487 can run arbitrarily many other
     callbacks first), the client aborts: close with SO_LINGER {1,0}
     sends RST, and Linux removes the pending connection from the
     accept queue.
  3. accept_conn() calls accept() on the blocking listen fd with an
     empty queue → blocks forever; the whole event loop hangs (no
     other conn is serviced, timers never fire).
  The same hang occurs with a competing acceptor (prefork workers or
  SO_REUSEPORT peers sharing the fd): poll wakes both, the competitor
  accepts the single pending connection, this process's accept()
  blocks.  Reproduced both ways.
- Preconditions: io.h:106-118 documents only "When @fd becomes
  readable, we accept()" — no precondition that the fd must be
  O_NONBLOCK or exclusively owned.  Every other fd owned by io is
  forced non-blocking (io.c:106), so the asymmetry is clearly an
  oversight; non-blocking accept is also the universal event-loop
  idiom precisely because of this race.
- Concrete consequence: permanent hang of the entire io_loop (100%
  unresponsive, not even CPU-spinning) triggered by a remote peer
  aborting a just-completed connection at the wrong moment — a
  trivially triggerable remote DoS for any io-based server.
- Reproducers (both hang, killed by SIGALRM backstop, exit 142):
  - /tmp/io-asan/repro-listener-blocking.c — competing-acceptor variant;
    also prints `listener O_NONBLOCK after io_new_listener: NOT SET`.
  - /tmp/io-asan/repro-listener-rst.c — single-process RST variant.
  Both use io_poll_override() to deterministically place the
  theft/RST in the window between poll() and accept(); the window
  itself is real (rotation runs other callbacks first).
  Regression test ccan/io/test/run-50-listener-blocking-accept.c fails
  as designed: `not ok 2 - flags != -1 && (flags & O_NONBLOCK)` and
  `not ok 4` (child killed by SIGALRM while stuck in accept()).
- Repair direction: call io_fd_block(fd, false) in io_new_listener_()
  (mirroring io_new_conn_); accept_conn() already tolerates accept()
  failure (poll.c:273-277), so EAGAIN becomes a harmless no-op.

## F3 — LIKELY (FIXED in 897ff455) (portability/UB): fairness_counter signed int overflow in io_loop

- Location: ccan/io/poll.c:377 `static int fairness_counter;`,
  incremented once per io_loop iteration at poll.c:434, used at
  poll.c:440 `i = (rotation + fairness_counter) % num_fds;`.
- Path: after 2^31 poll iterations (a busy loop with always-plans —
  cf. F1 — or ~6 h at a sustained 100k events/s), the increment is
  signed-overflow UB; clang/gcc UBSan (-fsanitize=signed-integer-
  overflow) would fire.  Practical misbehavior is nil on mainstream
  targets: rotation is size_t, so a wrapped negative counter converts
  to a huge size_t and the modulo still yields a valid index — hence
  LIKELY, not CONFIRMED; not reproduced (2^31 iterations is
  impractical in a test).
- Repair direction: declare it `static size_t fairness_counter;`
  (well-defined wraparound, index math unchanged).

## Rejected candidates (disproved)

- R1: run-47-exclusive-duplex failure under clang.  Disproved as a
  module defect: the test shares one length variable (`&d->buflen`)
  between the read plan (io_read_partial, maxlen 30) and the write
  plan (io_write_partial, maxlen 1), and both are created as arguments
  of one io_duplex() call (run-47-exclusive-duplex.c:61) whose
  evaluation order is unspecified.  io_read_partial_ stores maxlen
  into *lenp eagerly (io.c:275); under clang's left-to-right order the
  write plan then clobbers it to 1, so the first read(2) requests 1
  byte (strace: `read(4, "1", 1) = 1`), splitting the first chunk and
  breaking the expected pattern string.  gcc's order masks it.
  Passes under gcc ASan+UBSan; no memory error involved.  Fix belongs
  in the test (separate length variables), not in io.
- R2: run-30 clang build failure.  typesafe_cb_preargs rejects
  `char *` arg vs `const char *` callback param under clang but not
  gcc — a typesafe_cb module portability divergence, not an io defect.
  Test passes 9/9 under gcc ASan+UBSan.
- R3: use-after-free when a callback frees its own or another
  connection.  Disproved: io_close → tal_free → destroy_conn removes
  the fd and both always-plans before finish runs (poll.c:232-248);
  do_plan/next_plan propagate the &io_conn_freed sentinel (io.c:60-63,
  415-416, 422) and never touch conn afterwards; io_do_always ignores
  next_plan's return but touches nothing after it (io.c:474-482);
  handle_always removes the plan from always[] *before* invoking it
  (poll.c:303-306).  Exercised by run-01/06/07/08/40/45 under ASan.
- R4: fds[]/pollfds[] mutation during io_loop's rotation (del_fd
  swap-removal, add_fd tal_resize).  Disproved: the rotation loop
  re-reads num_fds and re-indexes every iteration and caches no
  pointers across callbacks (poll.c:435-445); add_fd zeroes revents of
  new entries "In case we're iterating now" (poll.c:64).  A swapped-in
  fd may be skipped for one round, but poll is level-triggered and
  re-reports — no correctness issue.
- R5: io_wake() re-entrancy.  backend_wake (poll.c:210-230) only
  flips waiting plans to IO_ALWAYS via set_always; it runs no user
  callbacks, so no re-entrant mutation of fds[]/always[] is possible
  mid-scan.  Waking both plans of a duplex conn, waking an address
  with no waiters, and wake-before-wait (lost wakeup) are all
  consistent with the documented condvar-style semantics.
- R6: read()/write() EINTR closes the connection (do_read/do_write →
  do_plan → io_close, io.c:405-416).  Disproved as reachable: all
  conn fds are O_NONBLOCK, so read/write never sleep and cannot be
  interrupted; poll() EINTR is already handled (poll.c:426-431).
- R7: SIGPIPE from do_write on a closed socket kills the process.
  Standard POSIX write() semantics; the _info example sets
  signal(SIGPIPE, SIG_IGN) itself, establishing the documented
  caller-responsibility pattern.  Not a module defect.
- R8: OOM-only paths: io_new_conn_ leaks fd if add_conn fails before
  the destructor is registered (io.c:102-103); set_always returning
  NULL on backend_new_always failure leads to a NULL-plan assert in
  next_plan (io.c:159-161, 65); add_fd leaks the first pollfds array
  if the fds allocation fails (poll.c:39-44).  All require allocation
  failure; outside the audit's reachability bar.
- R9: io_flush_sync busy-spinning on a conn whose OUT plan is
  do_connect (case 0 → goto again on EINPROGRESS, io.c:605-613).
  Reachable only by calling io_flush_sync on a still-connecting
  socket, contrary to its documented purpose ("complete any
  outstanding *output*"); terminates when the connect completes.
  Caller misuse.
- R10: handle_always `int i = num_always - 1` when num_always == 0
  (poll.c:297): wraps to SIZE_MAX then converts to int —
  implementation-defined, yields -1 on all mainstream targets, loop
  body skipped.  Benign in practice; style-level.
- R11: `unsigned int i` vs size_t num_fds in backend_wake
  (poll.c:212-214) and `int` return of find_always (poll.c:122):
  truncation needs >4 billion fds/always plans.  Unrealistic.
- R12: io_always self-requeue starving fd events (handle_always runs
  before fd processing and breaks the rotation).  This is the
  documented meaning of "always" (highest priority, io.h:347-362); a
  callback that perpetually re-arms io_always is an explicit user
  busy-loop.  (Distinct from F1, where *no* always plan can run yet
  the loop still spins.)
- R13: POLLHUP on a fully idle conn (both plans waiting/unset) is
  never detected because setup_pfd negates pfd->fd (poll.c:192-194).
  By design: an io_wait()ing conn has no pending I/O to fail; when
  woken, its first read/write hits EOF/EPIPE and closes normally
  (covered by run-08-hangup-on-idle).
- R14: io_ready() calling do_plan on a plan whose status is
  IO_WAITING/IO_UNSET (assert at io.c:402-403).  Unreachable via real
  poll: setup_pfd only requests POLLIN/POLLOUT for polling plans, and
  the kernel only adds POLLERR/POLLHUP/POLLNVAL unrequested (those go
  to the close branch, poll.c:472-486).  A callback legally changing
  the *other* direction's plan mid-io_ready can only arm it
  (NOTSTARTED → optimistic do_plan is intended) or leave it; setting
  an already-active plan violates the io_plan_arg() assert
  (caller precondition).
- R15: `do_io_loop` declared in backend.h:88 but never defined or
  called anywhere.  Dead declaration; style only.
- R16: io_new_conn_ returns NULL (without setting errno) when the
  init callback returns io_close() (io.c:117-118), so the io.h:46-56
  example treats a successful self-close as an error.  Documentation
  nit, no code defect.
- R17: errno capture via `getsockopt(..., &errno, &errno_len)`
  (poll.c:481-484): &errno is a valid int lvalue on every mainstream
  libc; sizeof(errno)==sizeof(int).  Not a defect.
