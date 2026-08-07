# Audit: ccan/tal

Date: 2026-08-04. Auditor: kimi-code (per AGENTS.md procedure).

Mechanical checks:
- ccanlint: 104/110 — all checks pass except tests_coverage (+0/6).
- Existing tests under clang 18 ASan+UBSan: 20/20 pass
  (with `-fno-sanitize=function`; see F5).
- Reproducers live in /tmp/tal-asan/repro-*.c (temporary, not committed).

## F1 — CONFIRMED (FIXED in 5b6fca2c): tal_steal() of an object being destroyed left a dangling child link (UAF)

- Location: ccan/tal/tal.c:538 `tal_steal_()`; freeing continues at
  tal.c:423 `del_tree()` / tal.c:456 `freefn(t)`.
- Reachable path: `tal_free(obj)` → `del_tree(obj)` → `notify(TAL_NOTIFY_FREE)`
  → user destructor calls `tal_steal(new_parent, obj)`.
  `tal_steal_()` does `list_del()` (idempotent, harmless) and
  `add_child(newpar, t)`, which links `obj` into `new_parent`'s child list
  and clears the destroying bit (tal.c:419). `del_tree()` then frees
  `obj`'s properties and `obj` itself regardless (tal.c:452-456).
  `new_parent` now owns a child node in freed memory; any later
  `tal_free(new_parent)` / `tal_first/tal_next` walks it.
- Preconditions: none documented against this — tal.h's tal_steal and
  destructor docs impose no restriction. (tal_free *does* guard re-entry
  via the destroying bit, tal.c:526; tal_steal_ does not.)
- Consequence: heap-use-after-free read/write in list_del_/del_tree.
- Reproducer: /tmp/tal-asan/repro-steal-self.c
- Observed: ASan heap-use-after-free, READ in list_del_
  (ccan/list/list.h:327) ← del_tree (tal.c:446) ← tal_free (tal.c:532);
  region freed by tal_free (tal.c:532).
- Repair direction: support rescue: del_tree() re-checks the destroying
  bit after notify() and spares a rescued object; notify() restores the
  blatted destructor callback; tal_steal_() refuses a new parent which
  is itself being destroyed (would re-link into the draining list).
  Implemented in commit 5b6fca2c.

## F2 — LIKELY (documented in ca38d33a): TAL_NOTIFY_ADD_CHILD notifier freeing the child makes tal_alloc_() return a dangling pointer

- Location: ccan/tal/tal.c:487-489 (`notify(parent, TAL_NOTIFY_ADD_CHILD,
  child)` then unconditional `return from_tal_hdr(child)`).
- Path: notifier on parent for TAL_NOTIFY_ADD_CHILD calls tal_free(info)
  on the announced child; tal_alloc_() returns the freed pointer and the
  caller uses it. No documented precondition forbids freeing in a
  notifier, but the usage is pathological — hence LIKELY, not CONFIRMED.
- Reproducer: /tmp/tal-asan/repro-addchild-free.c → ASan
  heap-use-after-free WRITE of the returned pointer.
- Repair direction: document that notifiers must not free the notified
  object, or re-validate linkage after notify() and return NULL.

## F3 — LIKELY (FIXED in ca38d33a): TAL_NOTIFY_DEL_CHILD notifier freeing the child causes unbounded recursion (stack exhaustion)

- Location: ccan/tal/tal.c:528-532 — `tal_free()` notifies the parent
  (TAL_NOTIFY_DEL_CHILD) *before* `list_del()` and before `del_tree()`
  sets the destroying bit.
- Path: parent's DEL_CHILD notifier calls tal_free(info); inner tal_free
  re-notifies before any state change → infinite mutual recursion.
  Same caveat as F2 (pathological notifier, undocumented).
- Reproducer: /tmp/tal-asan/repro-delchild-free.c → ASan stack-overflow.
- Repair direction: unlink the child and set the destroying bit before
  the DEL_CHILD notification (or document the restriction).

## F4 — LIKELY (FIXED in 328d96ff): tal_expand_() heap-buffer-overflow when bytelen is not a multiple of the element size

- Location: ccan/tal/tal.c:813 (`tal_resize_(ctxp, size, old_len/size +
  count, false)` truncates) and tal.c:816 (`memcpy(*ctxp + old_len, src,
  count*size)`).
- Path: `old_len % size != 0` ⇒ allocation is rounded down by
  `old_len % size` bytes while memcpy writes `old_len + count*size`,
  overflowing by up to `size-1` bytes. Only reachable when the tal
  object's allocation type differs from the pointer's element type
  (tal_expand's documented "same type" precondition is about @a2 and is
  met); that allocation-type mismatch is nowhere documented as invalid
  (tal_count()'s docs even acknowledge it), so LIKELY.
- Reproducer: /tmp/tal-asan/repro-expand-misalign.c → ASan
  heap-buffer-overflow WRITE of size 2 at tal.c:816, 0 bytes past a
  46-byte region from tal_resize_ (tal.c:755).
- Repair direction: round up: `(old_len + size - 1) / size + count`, or
  call_error() when `old_len % size != 0`.

## F5 — LIKELY (portability): callbacks invoked through incompatible function-pointer types

- Location: ccan/tal/tal.c:243, 246, 248 — destructors stored/called as
  `void (*)(tal_t *)` / `void (*)(tal_t *, void *)`, notifiers as
  `void (*)(tal_t *, enum tal_notify_type, void *)`, while user
  functions have e.g. `void fn(char *)`. UB per C11 6.3.2.3/8.
- Observed: clang UBSan `-fsanitize=function` fires on tal's own test
  suite (run-destructor, run-destructor2, run-free, run-notifier) on the
  first destructor/notifier call. Benign on all mainstream ABIs (same
  calling convention); breaks LTO/sanitizer-clean builds.
- Repair direction: generate a type-correct thunk per registration in
  the tal_add_destructor macro, or document and suppress
  -fsanitize=function.

## Rejected candidates (disproved)

- R1: destructor deleting another destructor later in the chain → notify()
  UAF. Disproved: del_notifier_property() unlinks the node, keeping
  p->next consistent; repro passes clean under ASan.
- R2: adjust_size() division by zero when *size==0. Unreachable: all
  callers pass sizeof(type) ≥ 1.
- R3: stale take() table entries after tal_resize_() moves a taken
  pointer. Disproved: taken() consumes the entry and take() re-adds the
  new pointer; balanced.
- R4: double list_del() during steal-in-destructor corrupts the old
  parent's list. Disproved: list_del is idempotent here (F1's UAF is
  from freeing, not list corruption).
- R5: tal_expand_() additive overflow check misses multiplication wrap
  of count*size. Disproved: tal_resize_() → adjust_size() catches the
  wrap before any memcpy.
