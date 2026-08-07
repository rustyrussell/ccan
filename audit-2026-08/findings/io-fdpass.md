# Audit: ccan/io/fdpass

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).

Scope: fdpass.h (70 lines), fdpass.c (73 lines), _info,
ccan/io/fdpass/test/run.c. Dependencies ccan/io (io.c, poll.c,
io_plan.h) and ccan/fdpass (fdpass.c, fdpass.h) — treated as given;
their internals were read to establish the exact contract the module
relies on (plan-arg lifetime, error dispatch, fd flags, fdpass
send/recv semantics). One cross-module observation is recorded as F1.

Documented contract (fdpass.h):
- io_send_fd: "Once that's sent, the @next function will be called: on
  an error, the finish function is called instead." "@fdclose: true to
  close fd after successful sending." Peers must pair io_send_fd with
  io_recv_fd, otherwise "the file descriptor will be lost".
- io_recv_fd: "On an error, if io_get_extended_errors() is true, then
  @next is called and @fd will be -1, otherwise the finish function is
  called."

Relevant io-core facts established (ccan/io/io.c, poll.c):
- io_new_conn sets the conn fd O_NONBLOCK (io.c:111), so the EAGAIN
  branches in do_fd_send/do_fd_recv are live, not speculative.
- plan->io is only invoked from io_ready on poll events
  (poll.c:481-483) and from io_flush_sync (io.c:611); do_plan treats
  -1-with-EAGAIN as EXHAUSTED (io.c:411-414), -1 otherwise as
  io_close(conn) (io.c:420), with a silent plan-drop on EPIPE when the
  other direction is still polling (io.c:415-419).
- struct io_plan_arg is embedded in the conn (io_plan_arg returns
  &conn->plan[dir].arg, io.c:146-152) and io_plan_arg asserts the
  direction is IO_UNSET, so a pending fdpass plan's scratch space
  cannot be overwritten by a second plan without tripping io's own
  API-misuse assertion.

## Mechanical checks

- ccanlint (before auditor additions): **42/46**, every check PASS;
  the only shortfall is partial credit on tests_coverage (+2/6,
  67/87 lines; the EAGAIN/extended-error branches of do_fd_send and
  do_fd_recv are uncovered by the shipped test).
- Existing test under clang 18.1.3 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; test includes
  ccan/io/fdpass/fdpass.c itself; linked with io.c, poll.c,
  fdpass/fdpass.c, tap, tal, take, list, str, noerr, timer, time,
  hash, ilog; `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  - run.c: **5/5 clean**, zero sanitizer diagnostics.
- Error-path probes (/tmp/iofdpass-asan/probe.c, temporary, not
  committed; same sanitizer build): **30/30 pass, exit 0**. Covered:
  - S1: send through a completely full socket buffer (fd O_NONBLOCK):
    send completes after the peer drains, received fd works
    (read "hello!"), fdclose=true closes the passed fd after the send
    (fcntl(F_GETFD) == -1 afterwards).
  - S2: conn destroyed before the send could run (in-direction
    io_recv_fd fails on a plain byte while the out buffer is full):
    the tal destructor closes the passed fd when fdclose=true, and
    leaves it open when fdclose=false. No double close.
  - S3: io_set_extended_errors(true), peer sends a plain byte:
    @next is called with *fd == -1 exactly as documented.
  - S4: send to a closed+shutdown peer (EPIPE, SIGPIPE ignored):
    finish called, @next not called, destructor closes the fd.
- Dependency probe (/tmp/iofdpass-asan/probe-multifd.c, plain gcc):
  see F1 — fdpass_recv on a 2-fd cmsg returns -1 with errno=-22 while
  /proc/self/fd grows by 2 (received fds leak in the dependency).
- gcc -Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes
  on fdpass.c: only -Wunused-parameter 'conn' in
  destroy_conn_close_send_fd (style; ignored per AGENTS.md).
- -m32: skipped (module includes errno.h; missing 32-bit system
  headers on this host, same as earlier modules).

## Findings

## F1 — (FIXED in ccan/fdpass f7062e12 + 6da9c78d): LIKELY (root cause in dependency ccan/fdpass): fdpass_recv leaks kernel-installed fds when the cmsg carries more than one fd, and sets a negative errno; both surface through io_recv_fd

- Location: root cause ccan/fdpass/fdpass.c:73-80 (fdpass_recv):
  ```c
  cmsg = CMSG_FIRSTHDR(&msg);
  if (!cmsg
      || cmsg->cmsg_len != CMSG_LEN(sizeof(fd))
      || cmsg->cmsg_level != SOL_SOCKET
      || cmsg->cmsg_type != SCM_RIGHTS) {
      errno = -EINVAL;
      return -1;
  }
  ```
  Surfacing point in this module: ccan/io/fdpass/fdpass.c:49
  (`int fdin = fdpass_recv(fd);` in do_fd_recv).
- Reachable path (through io/fdpass): receiver plans io_recv_fd; the
  peer (non-conforming or malicious — the socket peer is not bound by
  the documented pairing) sends one byte with an SCM_RIGHTS cmsg
  carrying 2+ fds. CMSG_SPACE(sizeof(int)) (24 bytes on LP64)
  physically holds the cmsg, recvmsg succeeds, and the kernel installs
  ALL payload fds into the receiver before fdpass_recv inspects
  cmsg_len. The cmsg_len != CMSG_LEN(sizeof(fd)) check rejects the
  message, but the extra fds are never closed.
- Observed output (/tmp/iofdpass-asan/probe-multifd.c, sends one cmsg
  with two /dev/null fds, then calls fdpass_recv):
  ```
  fdpass_recv=-1 errno=-22  open fds before=8 after=10 (delta=2)
  ```
  i.e. two descriptors leaked per rejected message, and errno is set
  to -EINVAL (negative — nonstandard; a caller of io_recv_fd running
  with io_get_extended_errors() that inspects errno after *fd == -1
  sees -22).
- Caller preconditions: none violated on the receiver side — io_recv_fd
  is used exactly as documented; the trigger is peer-controlled input.
- Concrete consequence: per-message fd leak driven by a remote peer;
  repeated messages exhaust the receiver's fd table (EMFILE,
  deny of service). In io/fdpass the conn is then closed (or *fd=-1
  with extended errors), but the leaked fds persist.
- Why LIKELY and scoped as cross-module: the defect and its repair
  live in ccan/fdpass (not yet separately audited; no fdpass.md in
  audit-2026-08/findings). io/fdpass adds no SCM_RIGHTS handling of its own —
  it delegates entirely to fdpass_send/fdpass_recv — so there is no
  independent io/fdpass defect here; this records what the async
  variant inherits. Severity also depends on whether the application
  treats the socket peer as untrusted.
- Repair direction (in ccan/fdpass/fdpass.c, not this module): on the
  rejection path, close any fds the cmsg actually carried before
  returning -1 (walk the payload with CMSG_NXTHDR/the cmsg_len byte
  count), or at minimum close the fds when cmsg_len indicates
  CMSG_LEN(n*sizeof(int)) for n >= 1; and change `errno = -EINVAL` to
  `errno = EINVAL`. No io/fdpass change is required once the
  dependency is repaired (do_fd_recv's -1 handling is correct).

No other findings. The module itself (143 LOC of plan glue) was
verified defect-free against its documented contract on every path the
probes could reach.

## Rejected candidates (disproved)

- R1: EAGAIN branches in do_fd_send (fdpass.c:17-19) and do_fd_recv
  (fdpass.c:53-54) are dead/wrong: they are live (io_new_conn sets
  O_NONBLOCK, io.c:111; POLLOUT is reported optimistically by poll
  whenever the socket is writable, so a racy full buffer yields
  EAGAIN) and returning 0 maps to do_plan's EXHAUSTED retry
  (io.c:422-425), identical to do_plan's own EAGAIN case. S1 exercised
  the full-buffer send path end to end. Rejected.
- R2: fdclose destructor double-close / closes the wrong fd
  (fdpass.c:7-11, 21-25, 41-42): add is balanced by
  tal_del_destructor2 on the success path before close can recur, and
  the destructor arg is the embedded plan arg (stable address, alive
  while conn is alive). arg->u1.s cannot be overwritten while a plan
  pends because io_plan_arg asserts IO_UNSET (io.c:148). Probes S2/S4
  show exactly one close; S2 with fdclose=false shows no close.
  Rejected.
- R3: "fdclose: true to close fd after successful sending" vs the
  module also closing the fd on send failure / conn close
  (fdpass.c:40 comment): closing on failure is the deliberate
  leak-avoidance net, matches the in-code contract, and harms no
  documented usage (with fdclose=true the caller has handed ownership
  to io). Rejected.
- R4: extended-errors recv path misbehaves: probe S3 shows @next
  called with *fd == -1 and the conn left open, exactly as
  fdpass.h:48-50 documents; errno is checked before
  io_get_extended_errors() is called, so no clobbering. Rejected.
- R5: EINTR from sendmsg/recvmsg kills the connection
  (do_fd_send/do_fd_recv return -1 for any non-EAGAIN error): the core
  do_read/do_write (io.c:193-233) behave identically — plan-level
  EINTR is fatal to the conn by io's own convention; not an
  io/fdpass-specific defect. Rejected.
- R6: EPIPE with the in-direction still polling silently drops the
  send plan (do_plan idle_on_epipe, io.c:415-419), so @next is never
  called and the fdclose fd is closed only at eventual conn
  destruction: this is core-io semantics for every write plan
  (do_write included); the destructor guarantees no leak at conn end.
  Delayed close at worst, consistent with the dependency's documented
  behavior. Rejected.
- R7: stale errno if sendmsg returns 0 (fdpass_send returns
  sendmsg(...) == 1 without setting errno on a 0): a 1-byte
  SOCK_STREAM sendmsg does not return 0 in practice, and "On failure,
  sets errno" is fdpass_send's own documented contract
  (fdpass/fdpass.h:12) — any such case is the dependency violating its
  contract, not io/fdpass misusing it. Rejected as speculative.
- R8: receiver-side stream-byte consumption when the peer is not
  protocol-conforming (fdpass_recv always reads 1 data byte; on S3's
  plain byte the "Z" is consumed before the error is detected):
  inherent to the SCM_RIGHTS-over-stream design and covered by the
  documented pairing requirement (fdpass.h:20-21). Rejected.
- R9: fd stored in size_t union member (arg->u1.s, fdpass.c:37) and
  passed back to int parameters (fdpass.c:15, 23): value-preserving
  for every valid fd; a negative fd is a caller precondition
  violation. Rejected.
- R10: destructor closing a reused fd number after io_flush_sync
  failure if the caller also closes the fd itself: requires the caller
  to close an fd it handed to io with fdclose=true — an ownership
  contract violation. With the contract honored, exactly one close
  occurs (probe-verified). Rejected.
- R11: cmsg buffer alignment/aliasing in the dependency: standard
  union-with-struct-cmsghdr idiom, memset before use; ASan+UBSan clean
  on every exercised path. Rejected.
- R12: POLLHUP/POLLERR-only events close the conn without invoking the
  io function (poll.c:484-497): with fdclose=true the destructor still
  closes the fd; with a pending readable fd-carrying message poll
  reports POLLIN, which is serviced first. No reachable loss beyond
  the documented pairing rules. Rejected.

## Auditor-added test files

- None added to the module: no in-module defect was confirmed, so
  there is nothing for a regression test to pin down (ccan/io/fdpass/
  is untouched — verified by git status).
- Temporary probes (not committed, /tmp/iofdpass-asan/): probe.c
  (30-check error-path suite: EAGAIN/full-buffer send, close-before-
  send destructor with fdclose true/false, extended-errors recv,
  EPIPE send failure), probe-multifd.c (F1 dependency leak demo),
  plus the ASan build of the shipped run.c.

No production files were modified (fdpass.h, fdpass.c, _info
untouched; verified by git status — no new paths under
ccan/io/fdpass).
