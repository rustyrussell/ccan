# Audit: ccan/err

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: err.h (88 lines), err.c (65 lines), _info and ccan/err/test/
only. Conditional dependency ccan/compiler (audited separately) is used
only for NORETURN on the !HAVE_ERR_H path — no misuse.

The module has two build paths, selected by config.h HAVE_ERR_H:
- HAVE_ERR_H=1 (this host): err.h includes the system <err.h> and
  defines err_set_progname(name) as ((void)name); err.c compiles to
  nothing. Correctness is the platform libc's.
- HAVE_ERR_H=0: the CCAN replacement in err.c (err/errx/warn/warnx +
  err_set_progname, ~50 LOC) is compiled. This is the audit's real
  target; it was exercised by building with an override config.h
  (/tmp/err-asan/override/config.h, identical to the repo config.h
  except HAVE_ERR_H 0) ahead of -I. on the include path. No production
  file was modified to do this.

## Mechanical checks

- ccanlint: **44/44**, all checks pass.
- test/run.c (24 subtests) under clang 18 ASan+UBSan
  (`-fsanitize=address,undefined -fno-sanitize=function
  -fno-sanitize-recover=all -g -I.`, linked with ccan/tap/tap.c only —
  run.c #includes ccan/err/err.c directly; per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0):
  - system-err.h path (HAVE_ERR_H=1): **24/24 pass, rc=0, zero
    sanitizer diagnostics**.
  - CCAN replacement path (override config.h, HAVE_ERR_H=0):
    **24/24 pass, rc=0, zero sanitizer diagnostics**.
- -m32: skipped — fails on missing 32-bit asm headers via errno.h
  (`/usr/include/linux/errno.h: 'asm/errno.h' file not found`), the
  documented skip condition. The replacement is pure stdio with no
  word-size-sensitive arithmetic.
- Probes in /tmp/err-asan/ (temporary, not committed):
  repro-warn-errno.c, repro-null-fmt.c, override/config.h.

## Findings

None. Every candidate was disproved; see below. The replacement is a
straightforward fprintf/vfprintf/exit implementation: errno is sampled
before any stdio call in err() and warn() (err.c:20, :45), the format
string is only ever passed as the format argument with the caller's
va_list (standard printf contract), and there are no buffers, lengths,
or arithmetic anywhere in the module.

## Rejected candidates (disproved)

- R1: err_set_progname() stores the caller's pointer without copying
  (err.c:13-16) — dangling if the caller passes a temporary. Disproved
  as a defect: the docs direct the caller to pass argv[0] (err.h:22-25,
  _info example), which lives for the process lifetime, and BSD's
  setprogname() likewise stores a pointer (into argv[0]) without
  copying — same inferred precondition, no divergence. Caller passing a
  dead pointer is a precondition violation.
- R2: basename divergence — BSD prints the basename of argv[0]
  (getprogname()), the CCAN replacement prints the string as passed
  (full path). Disproved as a defect: glibc's err (the HAVE_ERR_H path
  on Linux) also prints the full argv[0] (program_invocation_name), so
  the replacement matches the platform it replaces; the module's own
  run.c explicitly tolerates either form (run.c:23-28, strstr for the
  basename). Cosmetic, undocumented either way.
- R3: format-string exposure — user-controlled text reaching
  vfprintf as the format. Disproved: fmt is the caller's format string
  by design (the API *is* printf-style); progname and the strerror
  text are only ever %s arguments (err.c:23, :27, :48, :52). No
  exposure beyond the documented printf contract.
- R4: warn() clobbers errno on return (fprintf failures leave e.g.
  EBADF), while FreeBSD's vwarn restores the entry errno. Verified
  with /tmp/err-asan/repro-warn-errno.c (stderr closed, errno=ENOENT
  at entry): errno after warn is EBADF (9) under BOTH the CCAN
  replacement AND the system glibc err.h — identical behavior on both
  build paths. Neither BSD's err(3) man page nor CCAN's docs promise
  errno preservation; the replacement matches the libc it stands in
  for. Rejected (undocumented implementation difference, no internal
  inconsistency).
- R5: NULL fmt tolerance — glibc and BSD skip a NULL fmt and print
  just "prog: strerror(errno)\n"; the replacement passes NULL to
  vfprintf (formally UB). Verified with
  /tmp/err-asan/repro-null-fmt.c: glibc prints `prog: No such file or
  directory`; the CCAN replacement (on glibc) prints
  `prog: : No such file or directory` (vfprintf(NULL) returns EOF
  without output) — no crash here, but on libcs where vfprintf(NULL)
  faults, warn(NULL) would segfault. Rejected: a NULL format violates
  the documented "@fmt: the printf-style format string" precondition;
  NULL tolerance is undocumented implementation leniency in the
  reference implementations, not part of the contract.
- R6: fork/stdio interplay — err()/errx() call exit() (err.c:28, :40),
  so a forked child flushes inherited stdio buffers. Disproved as a
  divergence: glibc and BSD err() also exit() via stdio; identical
  semantics. run.c itself forks around all four functions and passes
  clean under ASan on both paths.
- R7: missing v* variants (verr/verrx/vwarn/vwarnx). Feature gap vs
  BSD err.h, not a defect — _info promises only the four documented
  functions, and nothing in-tree needs the v* forms.
- R8: no PRINTF_FMT format attribute on the replacement declarations
  (err.h:42, :56, :71, :85), so -Wformat cannot check call sites when
  the replacement is active. Missed-warning opportunity; speculative
  hardening per AGENTS.md. Rejected.
- R9: err_set_progname macro on the HAVE_ERR_H path
  (err.h:10 `((void)name)`): single evaluation, no side-effect issue.
  Rejected.
- R10: doc nit — warn() and warnx() document a nonexistent
  `@eval: the exit code` parameter (err.h:60, :76). Copy-paste from
  err/errx docs; cosmetic only, ccanlint's documentation checks pass.
  Noted, rejected.
- R11: test-only nit — run.c's read loop indexes buffer[i-1]
  (run.c:46, :108) which is buffer[-1] if the child produced no output
  at all, and loops forever if read() keeps returning -1. Only
  reachable when the module under test is already broken (empty child
  output); test robustness, not a production defect. Noted, rejected.
- R12: progname default "unknown program" (err.c:11) matches the
  documented behavior (err.h:22, _info). No defect.
- R13: _info conditional dependency: "ccan/compiler" is printed only
  when !HAVE_ERR_H (_info:34-36), matching err.h:13's include of
  ccan/compiler/compiler.h on exactly that path. Consistent; no
  defect.

## Auditor-added test files

None — no defect survived disproof, so no regression test was
warranted. No production files were modified (err.h, err.c, _info
untouched; verified by `git status --short ccan/err/` — clean).
