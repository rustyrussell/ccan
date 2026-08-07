# Audit: ccan/fdpass

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).

Scope: fdpass.h (23 lines), fdpass.c (84 lines), _info,
ccan/fdpass/test/run.c. No dependencies (_info "depends" is empty);
only libc/POSIX sockets. Suspects from the brief (msghdr/ctrl buffer
sizing, CMSG alignment, partial sends, error paths) were all checked;
two error-path defects confirmed, the rest disproved below.

Documented contract (fdpass.h): fdpass_send "On failure, sets errno and
returns false."; fdpass_recv "On failure, returns -1 and sets errno.
Otherwise returns fd."

## Mechanical checks

- ccanlint (before auditor additions): **43/44**, every check PASS
  (shortfall is partial credit on tests_coverage).
- After adding the two auditor regression tests: ccanlint **35/41
  FAIL** — exactly as designed: tests_pass fails because run-errno and
  run-multifd-leak fail against the current code (all other checks PASS).
- Existing test under clang 18.1.3 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; test #includes fdpass.c
  directly, linked with ccan/tap/tap.c only; `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  - run.c: **17/17 clean**, zero sanitizer diagnostics.
- -m32: skipped (module includes errno.h; 32-bit asm headers missing on
  this host, per audit brief).
- Reproducers/probes in /tmp/fdpass-asan/ (temporary, not committed):
  repro-errno.c, repro-eof-uninit.c, probe-controllen.c,
  repro-multifd-leak.c, probe-trunc.c, probe-trunc2.c, probe-trunc3.c.

## Findings

## F1 — CONFIRMED (FIXED in 6da9c78d): fdpass_recv() sets `errno = -EINVAL` (negative) on malformed-messages

- Location: ccan/fdpass/fdpass.c:78, function fdpass_recv:
  ```c
  errno = -EINVAL;
  return -1;
  ```
- Documented contract: fdpass.h:20 — "On failure, returns -1 and sets
  errno." errno values are positive; the idiom `errno = -EINVAL` is a
  kernel-internal convention leaking into user space.
- Reachable path: any message that is not a well-formed single-fd
  SCM_RIGHTS cmsg — a plain data byte with no ancillary data, EOF
  (recvmsg == 0), or an fd-carrying cmsg of the wrong length (see F2).
  All are normal events for a socket speaking to a non-fdpass peer.
- Observed output (/tmp/fdpass-asan/repro-errno, plain gcc):
  ```
  fdpass_recv returned -1, errno = -22 (Unknown error -22)
  EINVAL = 22; errno == -EINVAL (WRONG SIGN)
  ```
  strerror/perror print "Unknown error -22"; any caller testing
  `errno == EINVAL` misdiagnoses.
- Caller preconditions: none violated — failure handling is the
  documented interface.
- Concrete consequence: broken error reporting and broken caller-side
  errno comparisons on every malformed-message path.
- Regression test: ccan/fdpass/test/run-errno.c (alarm(10)-bounded,
  5 subtests covering the no-cmsg and EOF paths). Currently fails 2/5
  in plain and ASan builds; must pass after repair.
- Repair direction: `errno = EINVAL;` at fdpass.c:78.

## F2 — CONFIRMED (FIXED in f7062e12): fdpass_recv() leaks fds installed by the kernel when it rejects an SCM_RIGHTS message

- Location: ccan/fdpass/fdpass.c:73-80, function fdpass_recv:
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
  The reject path never closes the fds the message carried.
- Reachable path: the receiver's control buffer is
  CMSG_SPACE(sizeof(int)) = 24 bytes (LP64 glibc). A peer (anything but
  fdpass_send, which always sends exactly one fd) sends TWO fds in one
  SCM_RIGHTS cmsg: cmsg_len = CMSG_LEN(2*sizeof(int)) = 24, which FITS
  the buffer, so the kernel installs both fds into the receiving
  process during recvmsg. fdpass_recv then rejects
  (24 != CMSG_LEN(sizeof(int)) = 20) and returns -1 — both received
  fds are leaked. The same happens for >2 fds (kernel truncates,
  delivers what fits with MSG_CTRUNC; installed-but-rejected fds leak)
  and for two separate 1-fd cmsgs (the kernel coalesces adjacent
  SCM_RIGHTS cmsgs — probe-trunc3 shows a single delivered cmsg with
  cmsg_len=24).
- Observed output (/tmp/fdpass-asan/repro-multifd-leak, plain gcc;
  16 iterations of "send 2 fds, fdpass_recv rejects"):
  ```
  fdpass_recv returned -1 (errno=-22)
  fds before=7 after=39 (delta=32 over 16 rejected recvs)
  ```
  i.e. 2 fds leaked per rejected message. The regression test observes
  the same under clang ASan+UBSan (no sanitizer diagnostic — the leak
  is of kernel fd-table entries, not heap).
- Caller preconditions: none violated — fdpass_recv's contract
  (fdpass.h:17-21) is "receive a file descriptor from a socket";
  nothing restricts the peer to this module, and rejecting malformed
  messages is exactly what the code intends to do.
- Concrete consequence: file-descriptor exhaustion DoS by a malicious
  or merely non-conforming peer; each rejected message permanently
  consumes fds (they alias the sender-passed open file descriptions,
  also keeping those files/pipes alive).
- Regression test: ccan/fdpass/test/run-multifd-leak.c
  (alarm(10)-bounded, 5 subtests, counts /proc/self/fd before/after 16
  rejected 2-fd recvs). Currently fails 1/5 (leaked=32) in plain and
  ASan builds; must pass after repair.
- Repair direction: on the reject path, if a cmsg is present with
  cmsg_level == SOL_SOCKET, cmsg_type == SCM_RIGHTS and
  cmsg_len >= CMSG_LEN(0), close every fd in the payload before
  returning -1:
  ```c
  if (cmsg && cmsg->cmsg_level == SOL_SOCKET
      && cmsg->cmsg_type == SCM_RIGHTS
      && cmsg->cmsg_len >= CMSG_LEN(0)) {
      int *fds = (int *)CMSG_DATA(cmsg);
      size_t n = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
      for (size_t i = 0; i < n; i++)
          close(fds[i]);
  }
  ```
  (needs <unistd.h>; also consider rejecting MSG_CTRUNC explicitly).

## Rejected candidates (disproved)

- R1: fdpass_recv parses an UNINITIALIZED control buffer on EOF
  (recvmsg == 0): u.buf is never memset in fdpass_recv (unlike
  fdpass_send), and msg_controllen is set before the call. Disproved
  empirically: on AF_UNIX/SOCK_STREAM orderly shutdown the kernel
  zeroes msg_controllen (probe-controllen: "recvmsg returned 0,
  msg_controllen=0 (was 64)"), so CMSG_FIRSTHDR returns NULL and the
  buffer is never read; a stack-poisoning reproducer
  (repro-eof-uninit.c, -O0 and -O2) always returns -1, and MSan and
  valgrind --track-origins are clean. Rejected.
- R2: partial send (`sendmsg(...) == 1`, fdpass.c:41): a blocking
  AF_UNIX stream sendmsg of a single byte returns 1 or -1; 0/short
  counts are not reachable for 1-byte blocking sends (no signal
  partial-write semantics at this size). The hypothetical "return
  false without errno on 0" is unreachable. Rejected.
- R3: ctrl buffer sizing / CMSG alignment: the
  `union { char buf[CMSG_SPACE(sizeof(fd))]; struct cmsghdr align; }`
  idiom guarantees alignment for the cmsghdr and the int payload;
  msg_controllen = CMSG_SPACE (not cmsg_len) on send is harmless (the
  kernel walks cmsgs by cmsg_len and ignores the padding, which is
  zeroed by the memset). Matches the cmsg(3) manpage pattern. Rejected.
- R4: MSG_CTRUNC mishandling when fds DON'T fit: fds beyond what the
  control buffer holds are closed by the kernel itself (scm teardown);
  the accepted single-fd case transfers ownership to the caller, which
  is the documented happy path. Only the reject path leaks — that is
  F2, retained. Rejected as a separate defect.
- R5: fdpass_recv returning 0 (fd 0 is valid and indistinguishable
  from failure-by-sign alone): failure is -1 per the header, 0 is a
  legitimate fd; callers testing `< 0` would be wrong, but the
  documented test is `== -1`. Rejected.
- R6: fdpass_send error paths: sendmsg -1 propagates errno correctly;
  SIGPIPE on a dead peer is standard socket behavior the caller
  controls (signal disposition / MSG_NOSIGNAL is out of scope for this
  API). Rejected.
- R7: fdpass_recv accepting the first fd of a multi-fd message when
  cmsg_len DOES equal CMSG_LEN(sizeof(int)) but extra cmsgs follow:
  unreachable — extra fds change the (coalesced) cmsg_len (F2 path) or
  get truncated and kernel-closed. Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/fdpass/test/run-errno.c — proves F1. Fails 2/5 in plain and
  ASan builds (errno == -EINVAL on both the no-cmsg and EOF paths);
  alarm(10)-bounded. Must pass after repair.
- ccan/fdpass/test/run-multifd-leak.c — proves F2. Fails 1/5 in plain
  and ASan builds (32 fds leaked over 16 rejected 2-fd recvs, counted
  via /proc/self/fd); alarm(10)-bounded. Must pass after repair.
- Both #include ccan/fdpass/fdpass.c per run-test convention (ccanlint
  only links module objects into api tests).
- ccanlint with both present: 35/41 FAIL, solely because these tests
  fail against the current code (tests_pass).

No production files were modified (fdpass.h, fdpass.c, _info
untouched; verified by git status — the only new paths under
ccan/fdpass are the two tests above).
