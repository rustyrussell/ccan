# Audit: ccan/noerr

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: noerr.h (41 lines, 4 prototypes), noerr.c (51 lines, 4 trivial
wrappers: close_noerr, fclose_noerr, unlink_noerr, free_noerr), _info and
ccan/noerr/test/ only. No dependencies (_info "depends" is empty).

## Mechanical checks

- ccanlint: **42/43**. Every check passes; the missing point is
  examples_exist (+1/2) because noerr.h has no Example: section — a
  documentation nit, not a correctness defect. tests_coverage reports
  87/87 lines covered; tests_pass, tests_pass_valgrind,
  examples_compile/examples_run all PASS.
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; test #includes ccan/noerr/noerr.c
  directly; linked with ccan/tap/tap.c; `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  **run.c passes 16/16, zero sanitizer diagnostics.**
- -m32: unavailable on this host (clang -m32 fails inside
  /usr/include/errno.h -> missing 32-bit asm headers, 2 errors, no
  binary). Skipped per procedure note; the module has no
  word-size-sensitive code (int errno values only).
- Reproducer/probe in /tmp/noerr-asan/ (temporary, not committed):
  repro-nested.c (nested/error-path errno preservation, incl. the exact
  _info example scenario; passes clean under ASan+UBSan, all asserts).

## Findings

None. No CONFIRMED or LIKELY defects.

The module is four functions, each with the identical, correct shape:

```c
int saved_errno = errno, ret;
if (fn(arg) != 0) ret = errno; else ret = 0;
errno = saved_errno;
return ret;
```

The save happens before the libc call on every path, the restore happens
after it on every path (there is exactly one path per function — no
branch can skip the restore), and `ret` is captured from errno before
the restore. errno is used strictly as an lvalue/rvalue once each, so
the `errno` macro form (`(*__errno_location())`) is fine. Thread-safety
is inherited from errno being thread-local.

## Rejected candidates (disproved)

- R1 (the audit suspect "missing restore on some path"): disproved by
  reading — each function is straight-line code with a single
  if/else that only selects `ret`; both arms fall through to
  `errno = saved_errno`. tests_coverage confirms 87/87 lines executed,
  and run.c asserts errno preservation after both succeeding and
  failing calls (ok 3-4, 7-8, 9-10, 11-12, 14-16).
- R2 (the audit suspect "nested use"): the wrappers never call each
  other, and caller-side nesting (cleanup path invoking close_noerr then
  unlink_noerr, as in the _info example) is LIFO-correct because each
  call saves and restores independently. Proven by
  /tmp/noerr-asan/repro-nested.c, which reproduces the _info example
  (failed write -> close_noerr + unlink_noerr) plus failing-close and
  failing-unlink nesting and free_noerr(NULL)/free_noerr(p): all errno
  values preserved, ASan+UBSan clean, rc=0.
- R3: close_noerr returning the errno value instead of -1 (so callers
  cannot use `ret < 0` and cannot read errno for the error). This is
  the documented contract: "if an error occurs, the resulting
  (non-zero) errno is returned" (noerr.h:10-11, :19-20, :28-29).
  Not a defect.
- R4: if close/fclose/unlink fails without setting errno, ret == 0 and
  the "non-zero errno" doc promise breaks. POSIX.1 requires these
  functions to set errno on failure; a libc that does not is
  non-conforming. Speculative; rejected.
- R5: close_noerr EINTR semantics — after close() returns EINTR the fd
  state is unspecified by POSIX (and on Linux the fd is already gone),
  so a caller retrying close_noerr could double-close. The wrapper does
  not retry; it faithfully reports close's result. This is close(2)'s
  documented semantics, not something the wrapper introduces or
  promises to handle. Rejected (caller precondition / out of scope).
- R6: fclose_noerr checks `!= 0` rather than `== EOF` — fclose returns
  0 on success and EOF on error, so the test is exact. close/unlink
  return 0 or -1, also exact. No defect.
- R7: free_noerr(NULL) — free(NULL) is a no-op per C17 7.22.3.3/2;
  save/restore still correct. Covered by repro-nested.c. No defect.
- R8: test artifact, not production: run.c:50-51 closes fileno(fp)
  under the FILE and then fcloses it. glibc's fclose frees the FILE
  even when the underlying close(2) fails with EBADF, so there is no
  leak or UAF; ASan reports nothing. run.c:23 uses the bare value 100
  as an errno stand-in; 100 is not assigned on Linux (max is ~133 but
  the exact value is irrelevant — the test only checks equality after
  restore). Test-only; no production consequence.
- R9: missing Example: section in noerr.h (the one ccanlint point
  lost). Documentation completeness, explicitly out of the audit's
  correctness/security/portability scope. Noted only to explain the
  42/43 score.

## Auditor-added test files

None. No defect survived disproof, so no regression test was warranted;
the only probe (repro-nested.c) lives in /tmp/noerr-asan/ and was not
committed.

No production files were modified (noerr.h, noerr.c, _info untouched;
verified by git status — no new or changed paths under ccan/noerr).
