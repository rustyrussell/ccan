# Audit: ccan/take

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: take.h (136 lines), take.c (126 lines), _info and
ccan/take/test/ only.  Deps ccan/likely (header-only likely/unlikely)
and ccan/str (stringify/strends) already audited; take uses them only
for the unlikely() hint and the TAKE_LABEL stringification — no misuse
per their docs found.  Note: this take version is *not* tal-based; it
keeps its own realloc'd arrays.  The tal-side integration
(tal_resize/tal_free/tal_dup of take()n pointers) lives in ccan/tal
(already audited) and is only cross-checked here.

## Mechanical checks

- ccanlint (before auditor additions): **51/56**, every check passes;
  points lost only on tests_coverage (+2/6 — the labelarr
  realloc-failure path take.c:45-46 and the allocfailfn path are
  uncovered) and a trailing-whitespace note on the take.h:21 example
  comment.  tests_pass, tests_pass_valgrind, examples_compile all PASS.
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; each test #includes
  ccan/take/take.c directly; linked with ccan/tap/tap.c and
  ccan/str/str.c; per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  **2/2 pass cleanly, zero sanitizer diagnostics** — run (43 ok),
  run-debug (14 ok).  Total 57 ok.
- -m32: builds and passes (run.c, 43/43, ASan+UBSan clean).  The module
  has no word-size-sensitive arithmetic (sizes are pointer-count
  reallocations).
- ccanlint after adding the two auditor tests: **46/54 FAIL** — exactly
  as designed, because run-debug-labels.c fails 1/8 (F1) and
  run-allocfail-cleanup.c fails 2/4 (F2) against the current code.
- Reproducers/probes in /tmp/take-asan/ (temporary, not committed):
  repro-label-desync.c, repro-allocfail-cleanup.c.

## Findings

## F1 — CONFIRMED (FIXED in 018a4977): taken() does not keep labelarr in sync with takenarr; after consuming a non-last entry, taken_any() reports the label of an already-consumed take() and hides the real one (CCAN_TAKE_DEBUG only)

- Location: ccan/take/take.c:69-85, concretely the removal at
  take.c:82-83:
  ```c
  memmove(&takenarr[i-1], &takenarr[i],
          (--num_taken - (i - 1))*sizeof(takenarr[0]));
  ```
  which shifts takenarr but never applies the same shift to labelarr
  (allocated/maintained at take.c:17-18, 35-48, 51-52 whenever any
  take() carried a non-NULL label, i.e. under CCAN_TAKE_DEBUG).
- Reachable path:
  1. Build with CCAN_TAKE_DEBUG (e.g. c-lighting DEVELOPER=1), so
     TAKE_LABEL(p) (take.h:9) produces a non-NULL label per take().
  2. `take(&alpha); take(&beta);` — takenarr=[&alpha,&beta],
     labelarr=[Lalpha,Lbeta].
  3. The callee properly consumes the first: `taken(&alpha)` — takenarr
     becomes [&beta], num_taken=1, but labelarr stays [Lalpha,Lbeta].
  4. Any later leak check `taken_any()` (take.c:95-112) scans
     labelarr[0..num_taken) and returns labelarr[0] = Lalpha.
- Caller preconditions: none violated — this is exactly the documented
  take()/taken()/taken_any() usage (take.h:33, :57, :91).
- Concrete consequence: taken_any() violates its contract "returns the
  label where the pointer was passed to take() ... NULL if none are
  taken" (take.h:81-83): it returns the source location of a take()
  that was *properly consumed*, while the label of the genuinely
  still-taken pointer (Lbeta) is never reported.  Leak debugging under
  CCAN_TAKE_DEBUG therefore blames the wrong take site and hides the
  real one — the exact feature taken_any() exists for.  Not memory
  unsafe: labelarr indices stay within bounds (size >= num_taken is
  preserved), labels are string literals, and the ASan+UBSan reproducer
  run is diagnostically clean.
- Observed output (/tmp/take-asan/repro-label-desync.c, clang 18
  ASan+UBSan, rc=2):
  ```
  taken_any() = /tmp/take-asan/repro-label-desync.c:15:&alpha
  BUG: label of consumed take(&alpha) reported
  ```
  (line 15 is the consumed take(&alpha); line 16's take(&beta) is the
  one still taken.)
- History: labels were added in cbabfa8c ("take: add labels when
  CCAN_TAKE_DEBUG set") with the desync present from day one; 440efa55
  fixed only the labelarr realloc-failure desync in take_(), not the
  removal path.  run-debug.c never removes a non-last entry before
  querying taken_any(), so the suite cannot see it.
- Regression test: ccan/take/test/run-debug-labels.c (alarm(10)-bounded,
  8 subtests).  Currently fails 1/8 in a plain build:
  `not ok 8 - strstr(l, expect) != NULL` (the control case of removing
  the *last* entry passes today, showing the memmove omission is the
  whole bug).  After repair it must pass 8/8.
- Repair direction: in taken() (take.c:82-83) apply the identical
  memmove to labelarr when it is non-NULL:
  `if (labelarr) memmove(&labelarr[i-1], &labelarr[i], (num_taken - (i-1)) * sizeof(labelarr[0]));`
  using the already-decremented num_taken.

## F2 — LIKELY (FIXED in e30e011d): take_cleanup() leaves the allocfail counter behind, so a phantom taken NULL survives the cleanup

- Location: ccan/take/take.c:114-121 (take_cleanup resets max_taken,
  num_taken, takenarr, labelarr but not `allocfail`, take.c:11) in
  interaction with the allocfail handshake at take.c:25-28
  (`allocfail++` when take_() can't grow and an allocfail handler was
  registered) and taken()/is_taken() at take.c:73-76 and :89-90, which
  treat `!p && allocfail` as a taken NULL.
- Reachable path:
  1. take_allocfail(fn) registers a handler (take.h:126).
  2. take(p) hits the num_taken == max_taken growth path and realloc
     fails; take_() does allocfail++, calls fn(p), returns NULL — one
     phantom "taken NULL" is now pending, by design (take.h:111-113:
     the callee calls taken(NULL) to detect the failure).
  3. Before any taken(NULL) drains the counter, the program calls
     take_cleanup() — documented as "remove all taken pointers from
     list" (take.h:94-104; also invoked by tal_cleanup(), tal.h:484).
  4. Afterwards is_taken(NULL) is still true and taken(NULL) still
     returns true once, while taken_any() returns NULL (num_taken==0).
- Caller preconditions: none documented is violated; nothing says the
  allocfail handshake state is exempt from cleanup.
- Concrete consequence: after "remove all taken pointers", the module
  still reports a taken NULL — an inconsistent bookkeeping state
  (is_taken(NULL)==true vs taken_any()==NULL) and one spurious
  taken(NULL)==true consumed by whatever next asks.  Minor: no memory
  unsafety, requires allocation-failure injection plus continued
  take() use after take_cleanup(), and taken_any() (the documented
  leak-check API) is unaffected.
- Why LIKELY rather than CONFIRMED: the allocfail counter is an
  out-of-band representation of pending taken NULLs, and one can read
  take_cleanup()'s contract as covering only "pointers" in the array;
  the state is reachable only across an OOM event.  But the observable
  post-cleanup contradiction (is_taken vs taken_any) is real and
  reproduced.
- Observed output (/tmp/take-asan/repro-allocfail-cleanup.c, clang 18
  ASan+UBSan, rc=2, zero sanitizer diagnostics):
  ```
  before cleanup: is_taken(NULL)=1
  after cleanup:  is_taken(NULL)=1 taken_any()=(null)
  BUG: phantom taken NULL survived take_cleanup()
  and taken(NULL) consumed the phantom
  ```
- Regression test: ccan/take/test/run-allocfail-cleanup.c
  (alarm(10)-bounded, 4 subtests).  Currently fails 2/4 in a plain
  build (`not ok 3 - !is_taken(NULL)`, `not ok 4 - !taken(NULL)`).
  After repair it must pass 4/4.
- Repair direction: add `allocfail = 0;` to take_cleanup() (take.c:116).
  (allocfailfn is a registered handler and should intentionally stay.)

## Rejected candidates (disproved)

- R1: tal_resize() of a take()n pointer losing its taken status
  (audit suspect).  Disproved: tal_resize_ explicitly transfers the
  property on move — ccan/tal/tal.c:812-814
  `if (taken(from_tal_hdr(old_t))) take(from_tal_hdr(t));` — and
  from_tal_hdr() yields the *user* pointer, matching what take()
  recorded.  Documented at tal.h:117 ("if *p is take(), it will still
  be take() upon return, even if it moved") and covered by
  ccan/tal/test/run-take.c:47-50 ("tal_resize should return a taken
  pointer").  tal_dup_/tal_expand_ similarly consume-then-free
  (tal.c:864-865, 882-901).  This is tal-side code (already audited);
  the take module's value-based contract is honored.
- R2: Overflow in the tracking table (`sizeof(*takenarr) * (max_taken+1)`,
  take.c:23, :37).  max_taken grows by exactly 1 per successful take(),
  so overflow needs ~SIZE_MAX live taken pointers and a prior successful
  ~SIZE_MAX-byte realloc — impractical; size_t multiplication cannot
  wrap before the allocation itself is impossible.  Rejected.
- R3: O(n^2) growth (one realloc + full memmove per take, max_taken++
  at take.c:49).  1000-deep recursion in run.c passes instantly;
  deliberate simplicity ("Overallocate ... better than risking calloc
  returning NULL", take.c:16).  Performance nit, not a correctness
  defect.  Rejected.
- R4: take() macro double-evaluating p (take.h:33).  p appears once as
  a runtime argument to take_(); take_typeof(p) is __typeof__ (unevaluated)
  and TAKE_LABEL(p) only stringifies p.  Single evaluation.  Rejected.
- R5: take(NULL)/allocfail ambiguity: take(NULL) returns NULL and is
  indistinguishable from an allocfail return, so taken(NULL) drains the
  allocfail counter (take.c:73-76) before the real NULL entry in
  takenarr.  Both channels are independently drainable and the counts
  commute — run.c:38-56 and :85-87 exercise exactly this and pass; no
  reachable incorrect consequence beyond F2's stale counter.  Rejected.
- R6: labelarr calloc sizing/under-allocation (take.c:17-18,
  calloc(max_taken+1)) and the realloc paths (:23, :37): traced all
  growth orderings (label-first, grow-first, mixed labelled/unlabelled
  takes); the invariant labelarr size >= num_taken+1 (when non-NULL)
  holds at every read/write, and taken_any() reads only i < num_taken.
  ASan clean on the reproducers.  Rejected.
- R7: Silent label loss when calloc/realloc of labelarr fails
  (take.c:18, :41-47): best-effort-by-design (comment take.c:16;
  440efa55 made the failure path safe by dropping labelarr entirely);
  taken_any() then falls back to the pointer buffer.  OOM-only, no
  incorrect success reported.  Rejected.
- R8: Freed-but-still-taken pointer value matching a later unrelated
  allocation (value-based tracking false positive in find_taken,
  take.c:58-67).  Requires the caller to free a take()n pointer without
  the callee consuming it — the contract violation take() exists to
  prevent (take.h:29-31; tal enforces it with assert(!taken(...)) at
  tal.c:436).  Caller precondition.  Rejected.
- R9: Mixed CCAN_TAKE_DEBUG / non-debug translation units: the label is
  a runtime parameter of take_(), labelarr is allocated on first
  non-NULL label, and taken_any() explicitly allows "some with labels,
  some without" (take.c:102).  No ABI/ODR issue (take.c has no
  debug-conditional code).  Rejected.
- R10: taken_any()'s static pointer_buf (take.c:97, :110) — explicitly
  documented "returns a static char buffer" (take.h:82).  Not
  thread-safe, but the whole module is documented-global-state style
  with no thread-safety promise anywhere in _info/take.h.  Rejected.
- R11: take_() returning p unmarked on realloc failure without a
  handler (take.c:30-31, "Otherwise we leak p") — documented verbatim
  at take.h:110-113 ("the pointer won't be marked taken()").
  Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/take/test/run-debug-labels.c — proves F1.  Currently fails 1/8
  in plain and ASan builds (`not ok 8`: taken_any() reports the
  consumed take(&alpha)'s label); alarm(10)-bounded; no sanitizer
  diagnostics.  After repairing F1 it must pass 8/8.
- ccan/take/test/run-allocfail-cleanup.c — covers F2.  Currently fails
  2/4 (`not ok 3 - !is_taken(NULL)`, `not ok 4 - !taken(NULL)`);
  alarm(10)-bounded; ASan/UBSan clean.  After repairing F2 it must
  pass 4/4.
- ccanlint with both files present: 46/54 FAIL, solely because these
  two tests fail against the current code.

No production files were modified (take.h, take.c, _info untouched;
verified by git status — the only new paths under ccan/take are the
two tests above).
