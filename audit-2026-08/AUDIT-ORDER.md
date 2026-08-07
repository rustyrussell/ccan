# CCAN module audit order

Scope: modules authored by David Gibson or Rusty Russell only.
Authorship determined from `_info` Author/Maintainer lines, falling back to
git first-committer where `_info` is silent. Ordering combines importance
(number of in-tree dependents, core-infrastructure role) with complexity
(implementation LOC, algorithmic subtlety). Production LOC (non-test) in
parentheses.

Excluded (not authored by Gibson/Russell): a_star, altstack, argcheck, avl,
base64, bdelta, block_pool, btree, cast, ccan_tokenizer, charset, closefrom,
cpuid, crc, crc32c, daemon_with_notify, darray, deque, edit_distance,
graphql, hash (Bob Jenkins), heap, idtree, ilog, isaac, iscsi, json, nfs,
pr_log, rbtree, rszshm, siphash, strgrp, stringbuilder, stringmap,
tal/stack, talloc (imported Samba code), tlist2, ttxml, version, xstring.

Borderline but included (derived/third-party algorithm, ccan code by
Russell/Gibson): md4 (RSA ref impl), ciniparser (iniparser port),
ogg_to_pcm, wwviaudio, crypto/* (public-domain algorithms), rfc822,
mem, tap.

## Tier 1 — core infrastructure (audit first)

1. tal (4445) — tree allocator; 35 in-tree dependents, foundation of most modern ccan code
2. tal/str (518), tal/path (733), tal/link (174), tal/grab_file (207), tal/autoptr (82), tal/talloc (579) — tal satellite helpers, audit right after tal itself
3. list (885) — intrusive doubly-linked list; used by tal, io, timer, etc.
4. str (755) — string helpers; 30 dependents
5. io (2890) — async I/O event loop; subtle state machine
6. htable (1859) — hash table underpinning strmap/strset/intmap users
7. opt (1949) — command-line option parsing; large input-validation surface
8. timer (895; 10892 test LOC) — timing wheel; heavy test suite hints at subtlety
9. failtest (2162) — failure-injection harness; 25 dependents' tests rely on it
10. tap (709) — TAP test harness; 420 dependents (audit relies on it being correct)

## Tier 2 — small but ubiquitous compile-time/runtime helpers

11. typesafe_cb (134)
12. container_of (145)
13. compiler (317)
14. build_assert (40)
15. check_type (64)
16. array_size (26)
17. alignof (20)
18. likely (249)
19. endian (363)
20. time (950) — 24 dependents
21. short_types (35)
22. structeq (46)
23. minmax (65)
24. order (147)
25. take (262)
26. err (153)
27. noerr (92)
28. foreach (275)
29. tcon (368)
30. autodata (189)
31. cppmagic (191)
32. mem (472)
33. ptrint (35)
34. bytestring (471)

## Tier 3 — data structures and algorithms

35. intmap (1087)
36. strmap (490)
37. strset (1296)
38. jmap (967)
39. jset (418)
40. tlist (293)
41. bitmap (370)
42. aga (1070) — abstract graph algorithms
43. agar (439)
44. dgraph (260)
45. ungraph (774)
46. lpq (216)
47. lqueue (238)
48. lstack (205)
49. asearch (94)
50. asort (487)
51. sparse_bsearch (95)
52. permutation (281)
53. invbloom (350)
54. eratosthenes (143)
55. objset (178)
56. coroutine (489)
57. generator (275)

## Tier 4 — crypto, encodings and parsing

58. crypto/sha256 (577)
59. crypto/sha512 (402)
60. crypto/ripemd160 (548)
61. crypto/hmac_sha256 (247)
62. crypto/hkdf_sha256 (119)
63. crypto/shachain (322)
64. crypto/xtea (81)
65. md4 (265)
66. rune (1330) — UTF-8 handling
67. utf8 (232)
68. str/base32 (237)
69. str/hex (139)
70. json_escape (263)
71. json_out (581)
72. ciniparser (1174)
73. rfc822 (728)
74. crcsync (363)

## Tier 5 — system, process and I/O utilities

75. antithread (3433)
76. antithread/alloc (1918)
77. net (512)
78. pipecmd (259)
79. daemonize (108)
80. fdpass (107)
81. io/fdpass (143)
82. read_write_all (49)
83. grab_file (126)
84. membuf (296)
85. rbuf (280)
86. lbalance (554)
87. pushpull (446)
88. ptr_valid (573)
89. breakpoint (56)
90. tally (631)
91. cdump (855)
92. jacobson_karels (35)

## Tier 6 — leaf/derived, lowest priority

93. asprintf (106)
94. ogg_to_pcm (168)
95. wwviaudio (647)
