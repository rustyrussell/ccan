# Audit: ccan/lqueue

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: lqueue.h (238 lines, the entire implementation — header-only
module of macros + static inlines over a singly-linked circular list
storing only the back pointer), _info and ccan/lqueue/test/ only. Dep
ccan/tcon — used per its documented contract; verified directly that
tcon_container_of_() handles a NULL member pointer explicitly
(tcon.h:338-341: `member_ptr ? (char *)member_ptr - offset : NULL`),
which is load-bearing for lqueue_front/lqueue_back/lqueue_dequeue on an
empty queue (see R1). ccan/lqueue/info is a compiled _info binary
(build artifact), not source.

Note on the task's suspect list: this checkout's lqueue.h has no
iterator API and no lqueue_entry_from() wrapper — the module consists of
LQUEUE/LQUEUE_INIT, lqueue_entry, lqueue_init_from_back, lqueue_init,
lqueue_empty, lqueue_front, lqueue_back, lqueue_enqueue and
lqueue_dequeue only. Iterator invalidation and lqueue_entry_from were
therefore not auditable here.

## Mechanical checks

- ccanlint (before auditor additions): **45/46**. Every check passes;
  the single deduction is examples_compile (+9/10): gcc 13 emits
  `-Wmaybe-uninitialized` for the lqueue_entry() doc example
  (`struct waiter w;` used only via `&w.ql` and compared against `&w`,
  tcon.h:335 in the expansion). The example never reads w's value; the
  pointer arithmetic does not depend on it. Example-only gcc
  false-positive-class warning, not a production defect (see R7).
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; run.c includes only headers —
  header-only module — linked with ccan/tap/tap.c; per-test
  `timeout 60`, ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0),
  LP64: **1/1 pass cleanly, zero sanitizer diagnostics** — run (25 ok).
- -m32: builds and passes (run.c, 25 ok, ASan+UBSan). The module has no
  word-size-sensitive arithmetic at all (pointer fields only).
- Differential fuzz (/tmp/lqueue-asan/repro-fuzz.c, temporary): 200000
  random enqueue/dequeue/front-back-check steps against a reference
  FIFO model, run twice in parallel over two element layouts (link
  member at offset 0 and at non-zero offset after a `long` + `int`),
  under ASan+UBSan: **clean**, front/back/empty/NULL results matched
  the model at every step, including all transitions through the
  empty and single-element states.
- Reproducers/probes in /tmp/lqueue-asan/ (temporary, not committed):
  repro-fuzz.c, repro-init-from-back.c.

## Findings

None. No CONFIRMED or LIKELY defects retained.

## Rejected candidates (disproved)

- R1: lqueue_front/lqueue_back/lqueue_dequeue on an empty queue pass a
  NULL link into lqueue_entry() = tcon_container_of(); a naive
  container_of(NULL) computes (char*)NULL - offsetof(member), which for
  a link member at non-zero offset yields a non-NULL garbage pointer
  instead of the documented NULL (lqueue.h:150-153, :172-175).
  Disproved: tcon_container_of_() explicitly maps NULL to NULL
  (tcon.h:338-341), independent of the member offset. Verified
  empirically by the fuzz's offset-8 link layout: every empty-queue
  front/back/dequeue returned NULL (also run.c:23-24,65 with a link at
  offset 8). Rejected.
- R2: Macro double evaluation of q_/e_ arguments (lqueue.h:114-115,
  :129-130, :142-143, :161-162, :183-184, :197-198, :219-220). The
  extra mentions of q_ sit inside sizeof via tcon_check_ptr /
  tcon_offset (tcon.h:153-154, :311-312) and are never evaluated.
  Disproved empirically: /tmp/lqueue-asan/repro-init-from-back.c counts
  evaluations through side-effecting accessors —
  lqueue_enqueue(getq(), gete()) evaluated each argument exactly once;
  lqueue_front/getq(), lqueue_back, lqueue_dequeue, lqueue_empty each
  evaluated q_ exactly once. Rejected.
- R3: lqueue_init_from_back() with a non-back element or a middle
  element of another queue (lqueue.h:114-115). Suspicion: q2 built from
  an arbitrary element link could be malformed. Disproved: the list is
  circular, so any in-queue element is a valid back (front is
  back->next); initializing from a middle element yields the queue
  (el3, el4, el0, el1, el2), which drains correctly in order — verified
  in repro-init-from-back.c. init_from_back(NULL) yields an empty
  queue (tcon_member_of_ maps NULL to NULL, tcon.h:366-369), also
  verified. Rejected.
- R4: Single-element dequeue invariant assert (lqueue.h:230,
  `assert(front->next == front)`). A corrupted single-element queue
  would crash or loop. Disproved: enqueue of the first element sets
  e->next = e (lqueue.h:203), and dequeue of the last element sets
  q->back = NULL; the fuzz crossed the 1-element boundary in both
  directions thousands of times with model-checked results and no
  sanitizer output. Rejected.
- R5: Enqueueing an element already in the queue corrupts the circular
  list (e->next overwritten; observed in an early buggy probe as an
  infinite drain loop). Caller precondition violation — the element
  must not already be queued (lqueue.h:195: the link "will be
  overwritten"; dequeue docs, lqueue.h:216-217, describe re-adding a
  dequeued entry as the supported flow). Same family as every intrusive
  list in CCAN. Rejected.
- R6: Empty-queue lqueue_dequeue() behavior is not documented in the
  doc comment (lqueue.h:212-218 says only "remove and return the entry
  from the front"), yet returns NULL. The module's own run.c:65 pins
  the NULL return (`ok1(lqueue_dequeue(&q) == NULL)`), so it is
  tested, intended behavior; a doc omission at most, and the analogous
  front/back cases are explicitly documented. No incorrect consequence;
  rejected.
- R7: ccanlint examples_compile 9/10 — gcc -Wmaybe-uninitialized on the
  lqueue_entry() example (tcon.h:335 expansion over uninitialized
  `struct waiter w`). The example only takes &w.ql and compares the
  lqueue_entry() result against &w; no value of w is read, and clang
  does not warn. Example-only, compiler-noise class; no production
  code path affected. Rejected.
- R8: lqueue_dequeue() leaves the returned entry's link pointing into
  the queue it left (stale next). Explicitly documented (lqueue.h:
  216-217: "leaves the returned entry's link in an undefined state; it
  can be added to another queue, but not deleted again"). Documented
  contract; rejected.
- R9: Portability — no shifts, no integer arithmetic, no aliasing or
  alignment exposure beyond container_of-style pointer subtraction on
  valid in-queue links; TCON_WRAP union guarantees at least pointer
  size. -m32 build+test clean (25/25). Rejected.

## Auditor-added test files

None. No defect was retained, so no TAP regression test was added to
ccan/lqueue/test/. Probes stayed in /tmp/lqueue-asan/ (temporary).

No production files were modified (lqueue.h, _info untouched; verified
by `git status --short ccan/lqueue` — clean).
