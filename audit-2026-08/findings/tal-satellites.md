# Audit: tal satellite modules (tal/str, tal/path, tal/link, tal/grab_file, tal/autoptr, tal/talloc)

Date: 2026-08-04. Procedure per AGENTS.md. tal/talloc excluded mid-audit
(maintainer: module should probably be removed).

## tal/str

- F1 CONFIRMED (FIXED 4122d490): tal_strreg_() counted capture groups by
  scanning regex text for unescaped '('; a '(' inside a bracket
  expression ("[(]") overestimated nmatch, so the va_arg loop read more
  char** args than passed and wrote through a garbage pointer (UBSan
  misaligned store + ASan SEGV at str.c:262). Fixed using re_nsub.
  Regression test: test/run-strreg-bracket.c.
- F2 thinko (FIXED 39d1a0a0): tal_strsplit_() `max *= 2 + 1` tripled the
  parts array (harmless; `num < max` invariant held either way).
- ccanlint 52/59 (coverage); all tests pass under ASan+UBSan.
- Rejected: tal_strjoin_/do_vfmt size overflow (needs unrepresentable
  strings); vsnprintf negative-return loop (fails safe via resize
  failure).

## tal/path

- F1 CONFIRMED (FIXED 6b4e62ec): path_simplify() nulled ret[j-1] before
  the lstat() check but only restored it on the ".." guard path; for a
  symlink or nonexistent prefix it dropped the ".." and everything after
  ("link/../x" -> "link"). Now keeps ".." literally unless the prefix is
  a proven real directory. Tests added to run-simplify.c (85 -> 95).
- F2 thinko (FIXED df66b082): path_readlink() `maxlen *= 2 + 1`.
- F3 (documented 0e926004): path_join(ctx, NULL, a) crashes in strlen();
  documented that args must be non-NULL except via take() chaining.
- F4 portability (FIXED 80416994): path_basename() could form a path-1
  pointer (UB, never dereferenced). Rewritten with index arithmetic.
- ccanlint 79/86 (coverage); all 14 tests pass under ASan+UBSan.

## tal/link

- No memory defects found. destroy_link()'s container_of on non-first
  nodes is safe only because links.n is at offset 0 (verified).
- F1 docs gap (not fixed): linkable_notifier abort()s when the object is
  tal_free()d or tal_steal()n while it has live links; link.h does not
  mention this.
- ccanlint 45/50; test passes under ASan+UBSan.

## tal/grab_file

- F1 CONFIRMED (FIXED 3217faca): regular files with st_size == 0
  (/proc, /sys) returned an empty buffer without calling read().
  Treated like the non-regular case now. Regression test:
  test/run-size0.c.
- ccanlint 42/47; tests pass under ASan+UBSan.

## tal/autoptr

- F1 LIKELY: autonull_set_ptr_() does not check tal() or the
  tal_add_destructor2/tal_add_destructor returns.  Only reachable with a
  returning custom errorfn (documented tal backend configuration):
  NULL deref on a->pp, or a half-registered autonull whose destructor2
  later fires with a freed `a` (UAF).  Not fixed (awaiting maintainer
  decision: add NULL checks + unwind, or document no-failure contract).
- ccanlint 44/44; test passes under ASan+UBSan.

## tal/talloc (audit aborted; removal candidate)

- F1 LIKELY: tal_talloc_destroy() never advances the bucket pointer on a
  non-matching entry, so freeing an object whose destructor-hash bucket
  still contains another object's destructor loops forever.
- F2 observation: tal_talloc_resize_() to count 0 runs the object's
  destructors (via talloc_free) even though the object "survives" as a
  fresh zero-size allocation.
- Could not run its tests: requires system talloc headers
  (talloc_set_abort_fn missing from the in-tree ccan/talloc snapshot).
