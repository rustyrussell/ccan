# Audit: ccan/list

Date: 2026-08-04. Procedure per AGENTS.md.

Mechanical checks:
- ccanlint: 101/105 (remainder: tests_coverage).
- All 9 tests pass under clang 18 ASan+UBSan (incl. CCAN_LIST_DEBUG
  variants).

## Findings

No defects in the module itself.  Manually verified: append/prepend of
empty lists, swap of single-node list, safe-iteration on empty lists,
list_del_from debug asserts, offset arithmetic in top/tail/pop.

## Finding in tal, surfaced by list's documented contract

- F1 CONFIRMED (FIXED 9da96c88): list_del() documents that a deleted
  node "can be added to another list, but not deleted again".  tal's
  destructor-rescue path violated this: tal_steal_() list_del()'d a node
  that tal_free()/del_tree() had already unlinked.  Harmless in default
  builds (verified idempotent), but with CCAN_LIST_DEBUG the first
  delete NULLs the node and the second crashes (UBSan: member access
  within null pointer at list.c:28).  tal_steal_() now skips the
  list_del() when the destroying bit is set.

## Rejected candidates

- list_check_node() count is int vs unsigned int (cosmetic); overflow
  needs >2^31 nodes: unreachable.
- list_for_each_safe deleting @nxt (not just @i): documented "@nxt is
  used to hold the next element, so you can delete @i" — precondition.
- list_append_list_(to, to) self-append: nonsensical usage,
  precondition.
