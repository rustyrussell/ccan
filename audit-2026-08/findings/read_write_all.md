# Audit: ccan/read_write_all

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).

Scope: read_write_all.h (10 lines), read_write_all.c (38 lines), _info,
ccan/read_write_all/test/ (run-read_all.c, run-write_all.c). No
dependencies ("depends" empty in _info).

Documented contract (_info): "Successful read and write calls may only
partly complete if a signal is received or they are not operating on a
normal file. read_all() and write_all() do the looping for you." The
header declares `bool write_all(int fd, const void *data, size_t size)`
and `bool read_all(int fd, void *data, size_t size)`; the _info example
treats a false return as "could not read the requested characters".
Inferred contract: return true iff exactly `size` bytes were
transferred; false on error or EOF; retry on EINTR and short counts.

The implementation is two 8-line loops:

```c
while (size) {
    ssize_t done = write(fd, data, size);   /* or read */
    if (done < 0 && errno == EINTR)
        continue;
    if (done <= 0)
        return false;
    data = (const char *)data + done;
    size -= done;
}
return true;
```

## Mechanical checks

- ccanlint (before any auditor additions): **39/45**, every check PASS;
  shortfall is only partial credit on tests_coverage (+1/6) and
  examples_exist (+1/2, no Example: in the main header, only in _info).
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; both tests #include
  ccan/read_write_all/read_write_all.c directly, linked only with
  ccan/tap/tap.c; per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  - run-read_all.c: **6/6 clean** (forks a child, transfers 2 MiB over
    a pipe in two parts with a SIGUSR1 delivered mid-transfer).
  - run-write_all.c: **8/8 clean** (mocks write() via `#define write`
    to inject ENOSPC, EINTR, 1-byte partial writes and full writes).
  Zero sanitizer diagnostics.
- -m32: skipped (module includes errno.h; 32-bit asm headers missing on
  this host, per audit brief).
- Auditor edge-case probe /tmp/rwa-asan/probe-edge.c (temporary, not
  committed; mocks both read() and write(), built with clang ASan+UBSan,
  `timeout 60`): zero-length with NULL data and fd -1 (true, no syscall
  attempted); read EINTR-once-then-success; read in 7-byte partials;
  read immediate EOF -> false; write returning 0 -> false; write with
  1000 consecutive EINTRs then success -> true. **All pass, zero
  sanitizer diagnostics.**

## Findings

None. No CONFIRMED or LIKELY defects.

## Rejected candidates (disproved)

- R1 (suspect list: partial IO): short reads/writes are exactly what the
  loop handles — `size -= done` with `0 < done <= size` cannot underflow
  (done is a positive ssize_t bounded by the count just passed to the
  syscall), and `data + done` stays within (or one-past) the caller's
  size-byte buffer. Exercised by run-write_all.c (1-byte partials) and
  the probe (7-byte partial reads). Rejected.
- R2 (suspect list: EINTR handling): `done < 0 && errno == EINTR`
  retries correctly; a positive partial result followed by EINTR also
  works (state already advanced). Exercised by the mocked-EINTR subtest
  of run-write_all.c (line 13 of read_write_all.c taken per coverage),
  the real-signal run-read_all.c, and the probe (EINTR-once read, 1000x
  EINTR write). Note: run-read_all.c uses signal(), which on glibc
  installs SA_RESTART handlers, so the *read* EINTR branch may be
  covered only by auto-restart + looping rather than a literal EINTR
  return — a test-quality observation consistent with the +1/6
  tests_coverage partial credit, not a production defect. Rejected.
- R3 (suspect list: zero-length): `while (size)` skips the syscall
  entirely and returns true; read_all(-1, NULL, 0) and
  write_all(-1, NULL, 0) are safe (no pointer arithmetic on NULL is
  performed — the `data + done` line is unreachable when size == 0).
  Returning true for a zero-byte transfer matches read(2)/write(2)
  count==0 semantics. Verified by probe case 1. Rejected.
- R4 (suspect list: error returns): any error other than EINTR returns
  false with errno preserved from the failing syscall (asserted by
  run-write_all.c ok 1-2: false + errno == ENOSPC). EOF (read returns
  0) and write returning 0 both return false, matching the example's
  "Could not read N characters" usage. Rejected.
- R5: write() returning 0 with size > 0 leaves errno stale (no errno is
  set by the module on that path). Real write(2) with count > 0 does not
  return 0 on regular files/pipes; a driver that does would otherwise
  spin the loop forever, so returning false is the correct defensive
  choice; a stale errno on a path POSIX leaves unspecified is cosmetic.
  Speculative hardening. Rejected.
- R6: size > SSIZE_MAX passed to write(2)/read(2) is
  implementation-defined per POSIX (Linux caps the transfer at
  0x7ffff000 and returns a short count, which the loop then handles).
  No arithmetic in the module can overflow or underflow because every
  accepted `done` satisfies 0 < done <= remaining size. Requires a
  caller claiming a >SSIZE_MAX buffer — pathological precondition
  violation, no reachable incorrect consequence inside the module.
  Rejected.
- R7: errno left as EINTR after a *successful* call (the last retry
  succeeded but errno still holds EINTR): run-write_all.c lines 56-58
  explicitly assert `errno == EINTR` after a successful write_all, i.e.
  this is tested, intended behavior (errno is only meaningful on
  failure). Rejected.
- R8: signed/unsigned mixing `size -= done` (size_t -= ssize_t): done
  is guaranteed positive and <= size at that point (guards above), so
  the usual arithmetic conversions cannot wrap. Verified by UBSan-clean
  runs of both tests and the probe. Rejected.
- R9: infinite-loop hazard on repeated EINTR: correct per contract
  (EINTR is not an error); bounded in practice by signal delivery.
  Probe case 6 (1000 EINTRs) terminates. Rejected.

## Auditor-added test files

None committed. The only probe is /tmp/rwa-asan/probe-edge.c
(temporary); the module's two existing tests plus the probe already
cover every branch (ccanlint coverage output shows all lines of
read_write_all.c taken), and no defect was found to pin with a
regression test.

No production files were modified (read_write_all.h, read_write_all.c,
_info untouched; verified by git status — no new paths under
ccan/read_write_all).
