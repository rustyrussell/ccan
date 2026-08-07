# Audit: ccan/strmap

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: strmap.h (238 lines, macros + inlines), strmap.c (252 lines,
critbit tree derived from ccan/strset.c which derives from agl/critbit),
_info and ccan/strmap/test/ only. Deps ccan/ilog, ccan/short_types,
ccan/str, ccan/tcon, ccan/typesafe_cb — used per their documented
contracts (ilog32_nz only on nonzero values; tcon_check/tcon_check_ptr
evaluate their expr argument zero times via sizeof — verified in
tcon.h:138-154, so strmap_add's double mention of `value` is not a
double evaluation).

## Mechanical checks

- ccanlint (before auditor additions): **59/65**, every check passes
  except tests_coverage (+1/6, "436 of 641 lines covered").
  tests_pass, tests_pass_valgrind, examples_compile, examples_run all
  PASS. (ccanlint prints only the total without -v; per-check output
  confirmed no other failures.)
- After adding the auditor test: ccanlint **53/59 FAIL** — exactly as
  designed, because test/run-getn-embedded-nul.c fails 4/6 subtests
  against the current code (F1).
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; each test #includes
  ccan/strmap/strmap.c directly; linked with ccan/tap/tap.c only;
  per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  **4/4 pass cleanly, zero sanitizer diagnostics** — run (42 ok),
  run-iterate-const (3 ok), run-order (7001 ok), run-prefix (231 ok).
  Total 7277 ok.
- -m32: skipped; fails on missing 32-bit system headers via errno.h
  (known host limitation, same as earlier modules).
- Differential fuzzing (/tmp/strmap-asan/repro-fuzz.c, temporary):
  200000 random add/del/get/prefix/full-iterate steps against a
  sorted-array reference model, keys of length 0-12 over a 6-byte
  alphabet (includes 0x01, 0x7f, 0xc3, 0xa9 and the empty string),
  under ASan+UBSan: **clean**, two independent seeds. Iteration order
  matched strcmp-sorted order at every check; prefix submaps yielded
  exactly the reference set (membership and count).
- Reproducers/probes in /tmp/strmap-asan/ (temporary, not committed):
  repro-getn-embedded-nul.c, repro-fuzz.c, repro-fuzz2.c,
  repro-submap-get.c, repro-deep.c.

## Findings

## F1 — CONFIRMED (FIXED in b1c71955): strmap_getn_() reads past the end of a shorter stored key and returns false matches when the counted member buffer contains a NUL byte before memberlen

- Location: ccan/strmap/strmap.c:45, function strmap_getn_:
  ```c
  if (!strncmp(member, n->u.s, memberlen) && !n->u.s[memberlen])
      return n->v;
  ```
- Reachable path: caller uses the documented strmap_getn()
  (strmap.h:88-103; added by d99e02dc "strmap: add strmap_getn for
  non-terminated string search") with a counted buffer of memberlen
  bytes that contains a NUL at offset k < memberlen, and the map
  contains the k-byte C-string key equal to the buffer's first k bytes.
  Example: map holds key "hi"; lookup buffer {'h','i',0,0,0} with
  memberlen 5.
- Why the preconditions are not violated: the only documented contract
  is "@member: the string to search for. @memberlen: the length of
  @member" (strmap.h:91-92). The function exists precisely to look up
  buffers that are not NUL-terminated strings, so a counted buffer may
  contain arbitrary bytes; nothing in the docs excludes embedded NULs.
  The lookup key is 5 bytes long; no 5-byte key equal to it can exist
  in the map (keys are NUL-terminated C strings, added via strlen in
  strmap_add_), so the only correct result is NULL/ENOENT.
- Mechanism: strncmp(member, key, memberlen) stops comparing at the
  common NUL ("characters that follow a NUL character are not
  compared"), returning 0 even though the key (k bytes) is shorter than
  memberlen. The guard then evaluates `n->u.s[memberlen]`, i.e. reads
  the stored key at index memberlen — past the end of its k+1-byte
  allocation.
- Concrete incorrect consequences, observed:
  1. Out-of-bounds read. /tmp/strmap-asan/repro-getn-embedded-nul.c,
     clang 18 ASan (key = strdup("hi"), 3-byte allocation):
     ```
     ==ERROR: AddressSanitizer: heap-buffer-overflow
     READ of size 1
     #0 strmap_getn_ /home/kimi/ccan/./ccan/strmap/strmap.c:45:47
     0x502000000015 is located 2 bytes after 3-byte region
     ```
     The read offset is caller-controlled via memberlen, so with a
     short key and a large memberlen (e.g. a 4096-byte zero-padded
     buffer) the read lands arbitrarily far past the allocation —
     potential segfault or info-dependent control flow.
  2. False match (logic error, plain builds). If the out-of-bounds
     byte happens to be 0, strmap_getn_ returns the value of a key
     that does not equal the counted lookup buffer, and does not set
     ENOENT. Demonstrated deterministically by the regression test
     with the key stored in a zero-padded 16-byte buffer:
     `not ok 3 - v == NULL`, `not ok 4 - errno == ENOENT` (the lookup
     wrongly returned the value for "hi"). On this host even the
     tightly allocated strdup("yo") case read a zero heap byte in the
     plain build (`not ok 5/6`).
- Regression test: ccan/strmap/test/run-getn-embedded-nul.c
  (alarm(10)-bounded, 6 subtests). Currently fails 4/6 in a plain
  build and aborts under ASan at strmap.c:45; after repair it must
  pass both ways.
- Repair direction: compare lengths before bytes, e.g. replace
  strmap.c:45 with
  `if (strlen(n->u.s) == memberlen && !memcmp(member, n->u.s, memberlen))`
  (the strlen check must come first; a bare memcmp would read the same
  out-of-bounds bytes). This also fixes the false match, since a key
  of exactly memberlen bytes never contains a NUL before memberlen.

## Note: recursion in strmap_iterate_/strmap_clear_ fixed in c127ff33 (same class as strset F1).

## Rejected candidates (disproved)

- R1: strmap_prefix_() `top` tracking (strmap.c:214-235). Suspicion:
  `top` is only advanced when the prefix byte c is nonzero, so a
  returned submap might contain keys not matching the prefix.
  Disproved: prefix bytes within [0, len) are nonzero by definition
  (only the terminator is NUL), so `if (c) top = n` fires exactly at
  nodes whose critbit byte is inside the prefix; at each such node the
  walk follows the prefix's own bit, so every excluded subtree differs
  from the prefix at that critbit. Below the final `top`, all nodes
  have byte_num >= len, hence all keys under top agree with the found
  leaf on bytes 0..len-1 (any disagreement would have created a node
  with byte_num < len), and the leaf itself is verified by
  strstarts(). Additionally validated by 2 x 200000-step differential
  fuzzing (prefix membership and counts matched the reference model,
  including empty-string keys and high-byte keys). Rejected.
- R2: Macro double evaluation in strmap_add (strmap.h:123-125): `value`
  appears in both tcon_check and the (void *) cast. Disproved:
  tcon_check's expr sits inside sizeof (tcon.h:138-139) and is never
  evaluated; `map` likewise. strmap_get/strmap_getn/strmap_del
  evaluate member/valuep exactly once. Rejected.
- R3: strmap_add_(..., value NULL) in NDEBUG builds stores v == NULL,
  making the leaf indistinguishable from an internal node
  (strmap.c:66-73). The header documents "@v: the (non-NULL) value"
  (strmap.h:109) and STRMAP() warns "you can't use 0 as a value!"
  (strmap.h:28-29); assert(value) guards debug builds. Documented
  precondition; rejected.
- R4: Unbounded recursion in iterate() (strmap.c:188-189) and clear()
  (strmap.c:241-242): a key set with long common prefixes ("", "a",
  "aa", ...) produces a tree of depth n-1, so iteration/clear recurse
  n-1 frames. Probe (/tmp/strmap-asan/repro-deep.c, 8 MB stack):
  depth-39999 tree (40000 such keys) iterates and clears fine; this is
  the same recursive shape as the upstream agl/critbit code the module
  is based on, and no documented contract bounds tree height.
  Speculative hardening; rejected. (A 100000-key probe did not finish
  building its O(n^2) key material within 280 s; not pursued further
  since the classification stands on design grounds.)
- R5: strmap_getn_ with memberlen == 0 or memberlen < strlen(member):
  strncmp(..., 0) == 0 plus !key[0] matches only the "" key (correct
  empty-key lookup); shorter memberlen is the documented counted
  lookup and the !n->u.s[memberlen] check correctly requires an exact
  length match. Both behave sensibly; only the embedded-NUL case (F1)
  is broken. Rejected.
- R6: strmap_get / strmap_empty on a strmap_prefix() submap
  (documented usage, strmap.h:212-214): a subtree pointer is a valid
  strmap (node-vs-leaf is per-slot, via v == NULL), and the shared
  static empty_map returned for absent prefixes (strmap.c:231-232) is
  const and behaves as an empty map for iterate/get/empty. Verified by
  /tmp/strmap-asan/repro-submap-get.c under ASan (lookup of in-prefix
  and out-of-prefix keys on submaps, all correct). Rejected.
- R7: bit_num computation (strmap.c:88-89): ilog32_nz is only called
  on the nonzero xor of two differing bytes (its documented
  precondition), yielding bit_num in [0,7]; assert(bit_num <
  CHAR_BIT) is consistent. u8/int promotions are value-preserving.
  Rejected.
- R8: Insertion-position loop bit ordering (strmap.c:105-121): the
  "bit numbers are backwards" comparison (break when an existing node
  has the same byte_num but a smaller/less-significant bit_num)
  matches the critbit invariant that more-significant critbits sort
  earlier. Validated by the differential fuzz (iteration order equaled
  strcmp order after every operation mix, including backward
  insertion). Rejected.
- R9: strmap_del_ parent/direction bookkeeping (strmap.c:143-176):
  `direction` always holds the branch taken from `parent` to the
  deleted leaf when parent != NULL, so `old->child[!direction]` is the
  correct sibling to raise; deleting the last member (parent == NULL)
  resets map->u.n. Covered by run.c and the fuzz deletions. Rejected.
- R10: closest() reading bytes[n->u.n->byte_num] (strmap.c:28-31,
  116-119, 150-154): guarded by byte_num < len everywhere, so the
  member buffer is never read past its (documented) length along the
  walk; the only over-read is the post-match check of F1. Rejected.
- R11: Empty-string keys: handled naturally by the v == NULL
  node/leaf discriminator (unlike strset, which needs a magic empty
  node); insert/delete/get/iterate/prefix of "" all verified by run.c,
  run-prefix.c and the fuzz (length-0 keys included). Rejected.
- R12: -m32 concerns: none testable (32-bit headers unavailable);
  the code has no word-size-sensitive arithmetic beyond size_t string
  lengths. Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/strmap/test/run-getn-embedded-nul.c — proves F1. Currently
  fails 4/6 in a plain gcc build (deterministic false match:
  `not ok 3..6`) and aborts under clang ASan (heap-buffer-overflow
  READ of size 1 at strmap.c:45); alarm(10)-bounded. After repairing
  F1 it must pass in both plain and ASan builds.
- ccanlint with this file present: 53/59 FAIL, solely because
  run-getn-embedded-nul.c fails against the current code.

No production files were modified (strmap.h, strmap.c, _info
untouched; verified by git status — the only new path under
ccan/strmap is the test above).
