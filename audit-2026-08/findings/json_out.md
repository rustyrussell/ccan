# Audit: ccan/json_out

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: json_out.h (217 lines), json_out.c (364 lines), _info,
ccan/json_out/test/ only. Dependencies: ccan/compiler, ccan/json_escape,
ccan/membuf, ccan/tal, ccan/typesafe_cb — treated as given; only misuse
of their documented behavior is flagged (one cross-module note in F3).

Key documented contract (json_out.h): every operation that can allocate
promises graceful failure — "Returns true unless tal_resize() fails"
(json_out_start/end/add), "Returns a direct pointer ... or NULL if
tal_resize() fails" (json_out_member_direct, json_out_direct), "Returns
false if tal_resize() fails" (json_out_add_splice). membuf.h:151-165
documents that membuf_prepare_space can fail ("If this fails ... you
can check if membuf_num_space() is < num_extra"). tal only lets
tal_resize return false at all when the installed errorfn returns
instead of aborting (tal.h:170 note; tal.c:791-795) — the reproducers
below install such an errorfn via tal_set_backend, which is the
supported way to get the graceful-failure behavior json_out documents.

## Mechanical checks

- ccanlint (before auditor additions): **42/48**, every check PASS;
  shortfall is only partial credit on tests_coverage (+1/6) and
  examples_exist (+1/2, no Example: section in _info).
- After adding the three auditor regression tests: ccanlint **37/44
  FAIL** — exactly as designed: tests_pass +3/6 because run-oom,
  run-vsnprintf-error and run-oom-escape all fail against the current
  code (tests_compile passes for all three).
- Existing tests under clang 18.1.3 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; linked with
  json_escape.c, membuf.c, tal.c, tal/str/str.c, take.c, list.c, str.c,
  tap.c; per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  - run.c: **14689/14689 clean**, zero sanitizer diagnostics.
  - run-debugging.c (CCAN_JSON_OUT_DEBUG): **14689/14689 clean**.
  - run-move_cb.c: **3/3 clean**.
  So on the happy path (no allocation failure, no vsnprintf error) the
  module is sanitizer-clean, including the DEBUG state machine.
- -m32: not attempted (module pulls in tal/str -> errno.h chain; per
  audit brief, -m32 skipped on this host for such modules).
- Reproducers/probes in /tmp/jout-asan/ (temporary, not committed):
  repro-oom-addv.c, repro-oom-direct.c, repro-oom-splice.c,
  repro-oom-escape.c, repro-vsnprintf-neg.c, repro-vsnprintf-oob.c,
  patched/inc/ccan/json_out/json_out.c (mkroom-only fix), shadow/
  (full repair-direction patch used to validate the regression tests).

## Findings

## F1 — CONFIRMED (FIXED in f14769e3): mkroom() never checked membuf_prepare_space() success

- Location: ccan/json_out/json_out.c:119-127:
  ```c
  static char *mkroom(struct json_out *jout, size_t len)
  {
      ptrdiff_t delta = membuf_prepare_space(&jout->outbuf, len);
      if (delta && jout->move_cb)
          jout->move_cb(jout, delta, jout->cb_arg);
      return membuf_space(&jout->outbuf);
  }
  ```
  When the buffer must grow and tal_resize fails, membuf_prepare_space_
  leaves the buffer unchanged (membuf.c:44-50: expandfn NULL ->
  errno = ENOMEM, delta 0). membuf_space() then returns a non-NULL
  pointer into the old, too-small buffer. mkroom returns it; the
  `if (!dest)` guards in json_out_member_direct (json_out.c:166-168)
  and json_out_addv (json_out.c:247-248) are dead code — mkroom can
  never return NULL.
- Documented contract: see header quotes above; every allocating entry
  point promises false/NULL on tal_resize failure. membuf.h:151-165
  documents that prepare_space failure must be detected via
  membuf_num_space() < num_extra — the check mkroom omits.
- Reachable path: any growth failure. Reproducers install a tal backend
  whose resize_fn fails above a size threshold and whose errorfn
  returns (the supported non-aborting OOM mode), then:
  - json_out_add(jout, "f", true, "%s", <999-byte string>) with a
    64-byte pool: the first vsnprintf truncates (avail 57), so
    json_out.c:246 mkroom(fmtlen+3) fails silently and json_out.c:249
    re-runs vsnprintf into 58 bytes of space.
  - json_out_member_direct(jout, "f", 1000): json_out.c:179
    membuf_added(outbuf, 1004) with ~58 bytes of space.
  Same root cause covers json_out_direct, json_out_addstr,
  json_out_addstrn, json_out_add_splice, json_out_start/end.
- Observed output (/tmp/jout-asan/repro-oom-addv, clang ASan+UBSan,
  symbolized):
  ```
  ERROR: AddressSanitizer: heap-buffer-overflow
  WRITE of size 1000 at ... 0 bytes after 104-byte region
      #0 vsnprintf
      #1 json_out_addv ccan/json_out/json_out.c:249
      #2 json_out_add  ccan/json_out/json_out.c:288
  ```
  /tmp/jout-asan/repro-oom-direct (default build):
  ```
  membuf.h:146: membuf_added_: Assertion `num <= membuf_num_space_(mb)' failed.
  ```
  Plain gcc build of the regression test:
  `malloc(): corrupted top size` (glibc detects the overflow). Under
  NDEBUG the member_direct/direct paths corrupt membuf bookkeeping
  silently and the caller's own writes go past the buffer.
- Caller preconditions: none violated — plain documented API calls;
  allocation failure is a documented, anticipated event.
- Concrete consequence: heap buffer overflow / heap corruption /
  assertion abort on OOM instead of the documented false/NULL return.
  For a long-running process using tal with a non-aborting errorfn
  (exactly the setup that makes "Returns true unless tal_resize()
  fails" meaningful), a single failed resize is remote-corruption
  territory when the formatted content is attacker-influenced.
- Regression test: ccan/json_out/test/run-oom.c (alarm(10)-bounded,
  4 subtests: json_out_add, json_out_member_direct, json_out_direct,
  json_out_add_splice). Currently aborts (heap overflow / assert);
  must pass after repair.
- Repair direction: in mkroom, after membuf_prepare_space, add
  `if (membuf_num_space(&jout->outbuf) < len) return NULL;`
  (validated: with only this change the addv/member_direct/direct
  subtests of run-oom.c pass and the existing suite stays clean).

## F2 — CONFIRMED (FIXED in e9893f9e): json_out_add_splice() ignored json_out_member_direct() failure

- Location: ccan/json_out/json_out.c:325-337, concretely line 335:
  ```c
  memcpy(json_out_member_direct(jout, fieldname, len), p, len);
  ```
- Documented contract: json_out.h:183 — "Returns false if tal_resize()
  fails."
- Reachable path: any tal_resize failure while splicing. Today F1 masks
  the NULL (member_direct can never return NULL) and the same scenario
  heap-overflows/aborts via F1 instead. To demonstrate F2 independently
  the reproducer was built against a copy of json_out.c with ONLY
  mkroom fixed (/tmp/jout-asan/patched/inc/ccan/json_out/json_out.c;
  production untouched):
  ```
  /tmp/.../json_out.c:339:9: runtime error: null pointer passed as
  argument 1, which is declared to never be null   (memcpy(NULL, p, 13))
  ```
  (UBSan, clang 18; corresponds to json_out.c:335 in the pristine
  source.) Plain build: SIGSEGV in memcpy.
- Caller preconditions: none violated — src is finished and unconsumed
  per json_out.h:175-181.
- Concrete consequence: NULL-destination memcpy (crash / UB) on OOM
  instead of returning false.
- Regression test: the 4th subtest of ccan/json_out/test/run-oom.c.
- Repair direction: store member_direct's result, return false on NULL:
  ```c
  char *d = json_out_member_direct(jout, fieldname, len);
  if (!d)
      return false;
  memcpy(d, p, len);
  ```

## F3 — CONFIRMED (FIXED in d0401012): escape paths never checked json_escape_len() for NULL (root cause in json_escape escape())

- Location: ccan/json_out/json_out.c:261-262 (json_out_addv):
  ```c
  e = json_escape_len(NULL, dst + quote, fmtlen);
  fmtlen = strlen(e->s);
  ```
  and json_out.c:309-311 (json_out_addstrn):
  ```c
  e = json_escape_len(NULL, str, len);
  str = e->s;
  len = strlen(str);
  ```
  json_escape_len is a plain tal allocation and returns NULL on OOM
  (tal_arr at json_escape.c:63).
- Reachable path: json_out_add(jout, f, true, "%s", <string needing
  escape>) or json_out_addstr[n] with an escapable string, when the
  escape allocation fails. Reproducer (/tmp/jout-asan/repro-oom-escape.c)
  installs a failing alloc_fn only around the call, so the escape
  allocation is the one that fails.
- Observed output: the process crashes one frame *earlier* than
  json_out's own missing check, inside the dependency —
  ```
  ccan/json_escape/json_escape.c:123:9: runtime error: member access
  within null pointer of type 'struct json_escape'
  ```
  i.e. json_escape's escape() itself never checks its tal_arr() result
  either (json_escape.c:63, first dereferenced in the escape loop).
  Cross-module note for the orchestrator: ccan/json_escape has the same
  class of defect; json_escape.md's R5 audited the sizing arithmetic
  but not allocation failure. json_out's missing check is independently
  required: with a repaired json_escape (returns NULL), json_out would
  still strlen(NULL)/e->s-deref NULL.
- Caller preconditions: none violated.
- Concrete consequence: NULL dereference on OOM instead of the
  documented false return.
- Regression test: ccan/json_out/test/run-oom-escape.c (alarm(10)-
  bounded, 2 subtests: json_out_add and json_out_addstrn). Currently
  SIGSEGV/UBSan-abort; must pass after BOTH json_out's NULL check and
  json_escape's tal_arr NULL check are repaired (validated against the
  /tmp/jout-asan/shadow tree containing both fixes: 2/2 pass).
- Repair direction: check `if (!e)` in both places and return false
  (addv: `dst = NULL; goto out;` to keep the va_end; addstrn: plain
  `return false` before touching e->s).

## F4 — CONFIRMED (FIXED in c91ae47b): json_out_addv() stored vsnprintf() int return in size_t

- Location: ccan/json_out/json_out.c:218 (`size_t fmtlen`) and
  json_out.c:239:
  ```c
  fmtlen = vsnprintf(dst + quote, avail, fmt, ap);
  ```
  vsnprintf returns -1 on encoding error; fmtlen becomes SIZE_MAX.
- Reachable path: any fmt for which vsnprintf fails. Demonstrated with
  glibc: `snprintf(buf, 8, "%lc", (wint_t)0xD800)` returns -1 (EILSEQ)
  — probe printed by the reproducer. Then, quote=true:
  `fmtlen + quote*2 >= space` wraps (SIZE_MAX+2 = 1) so the reprint
  branch is skipped; vsnprintf NUL-terminates even on error, so
  json_escape_needed(dst+1, SIZE_MAX) stops at that '\0' and
  json_escape_len(NULL, dst+1, SIZE_MAX) is called; its `len*6+1`
  sizing overflows and tal aborts. With quote=false:
  `fmtlen >= space` is true, mkroom(jout, SIZE_MAX+1 = 0) "succeeds",
  and membuf_added(outbuf, SIZE_MAX) aborts on its assertion (or
  silently corrupts the membuf under NDEBUG -> later wild writes).
- Observed output (/tmp/jout-asan/repro-vsnprintf-neg.c, gdb backtrace):
  ```
  #5 call_error "allocation size overflow"  ccan/tal/tal.c:524
  #8 escape (len=18446744073709551615)      ccan/json_escape/json_escape.c:63
  #10 json_out_addv                          ccan/json_out/json_out.c:261
  #11 json_out_add                           ccan/json_out/json_out.c:288
  ```
  and the quote=false variant aborts in membuf_added_'s assertion.
  Regression test under ASan: exit 134 (abort) in both subtests.
- Caller preconditions: json_out.h documents only "@fmt...: the
  printf-style format"; nothing says the conversion must succeed. A
  caller formatting a wchar_t it did not validate (e.g. from a script
  or protocol message) hits this.
- Concrete consequence: process abort (tal errorfn) or membuf
  corruption instead of a clean false return — a crashable code path
  on bad-but-plausible input.
- Regression test: ccan/json_out/test/run-vsnprintf-error.c
  (alarm(10)-bounded, 2 subtests, self-skips on platforms where the
  %lc probe does not fail). Currently aborts; must pass after repair.
- Repair direction: keep the vsnprintf result in an int, and on
  negative goto out with dst = NULL (return false). Also recheck the
  second vsnprintf's return or assert it equals fmtlen.

## Rejected candidates (disproved)

- R1: json_out_addv truncation handling (json_out.c:241-250). The
  "vsnprintf still NUL-terminates" subtlety is handled correctly:
  avail excludes the 2 quote bytes, and the reprint condition
  fmtlen + 2*quote >= membuf_num_space() is exactly fmtlen >= avail;
  boundary lengths (fmtlen == avail-1, == avail, == 0) all verified by
  reasoning plus the run.c sweep over string lengths 1..63 crossing
  every buffer-growth boundary (14689 subtests, ASan-clean). Rejected.
- R2: escape-path buffer sizing (json_out.c:263-270): mkroom(fmtlen+2)
  for fmtlen escaped bytes plus two quotes, memcpy not vsnprintf so no
  NUL needed; dst[fmtlen+1] is the last reserved byte. Exact. Rejected.
- R3: `memcpy(dst + quote, e, fmtlen)` (json_out.c:266) copies from the
  struct json_escape pointer rather than e->s — correct because
  struct json_escape's first (and only) member is `char s[1]`
  (json_escape.h:8-11), so the addresses coincide. Obscure, not a
  defect. Rejected.
- R4: move_cb delta sign/type: membuf_prepare_space_ returns size_t,
  and memmove-down yields a negative delta as a wrapped size_t;
  `ptrdiff_t delta = membuf_prepare_space(...)` (json_out.c:121) is an
  implementation-defined (not undefined) conversion that is identity on
  every two's-complement target; run-move_cb.c exercises the expansion
  path and passes under UBSan. No reachable incorrect consequence.
  Rejected.
- R5: json_out_add_splice self-splice (jout == src) would UAF because
  member_direct can realloc the buffer p points into — but the
  documented contract is "copy a field from *another* json_out"
  (json_out.h:170-171), so self-splice is a precondition violation.
  Rejected.
- R6: finished/not-finished state machine (the suspect list): indent/
  unindent/empty transitions verified — run-debugging.c (with
  CCAN_JSON_OUT_DEBUG) passes 14689/14689 under ASan+UBSan; empty
  objects/arrays ("{}"), nesting 64 deep, json_out_finished's
  documented empty-flag reset, and comma placement after end/direct
  all produce byte-exact output. json_out_finished's DEBUG assert uses
  tal_count(jout->wrapping) with wrapping possibly NULL after
  indent-OOM; tal_bytelen(NULL) is defined as 0 (tal.c:726-732).
  Rejected.
- R7: json_out_member_direct setting `jout->empty = false` even when it
  fails (json_out.c:182): after a failed add the next successful add
  prepends a comma, which still yields well-formed JSON (the failed
  member was never added). Cosmetic at worst, only on the OOM path.
  Rejected.
- R8: size arithmetic overflows (json_out.c:159-164 extra++ and
  += strlen+3; json_out.c:315 len+2; membuf.c:45
  (max_elems+num_extra)*elemsize): all require caller-supplied sizes
  within a few bytes of SIZE_MAX — pathological inputs violating the
  implied "how many bytes to allocate" precondition, and the
  membuf-side one is the dependency's domain. Rejected.
- R9: vsnprintf(dst+quote, 0, ...) when quote && space < 2
  (json_out.c:229-239): dst+1 can be one or two past the buffer end —
  formally UB pointer formation, but size 0 means no dereference, no
  sanitizer diagnostic, no reachable consequence; the fmtlen it returns
  then drives the correct reprint path. Rejected as speculative.
- R10: number formatting (suspect list): the module does no number
  formatting of its own — everything goes through user-supplied
  printf formats; nothing to get wrong inside the module. The escaping
  suspect is similarly delegated to ccan/json_escape (separately
  audited); json_out's only escaping decision is
  json_escape_needed()/json_escape_len(), used correctly on the happy
  path. Rejected.
- R11: json_out_dup (json_out.c:66-86): num_elems-0 dup, move_cb/cb_arg
  copy, DEBUG wrapping re-dup (including src->wrapping == NULL) all
  checked by reasoning; tal_dup_arr handles NULL/0. No defect.
  Rejected.
- R12: json_out_end/json_out_start type asserts (json_out.c:190,203)
  restrict @type exactly as documented (json_out.h:53,64). Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/json_out/test/run-oom.c — proves F1 (and the F2 subtest).
  Fails against current code (ASan heap-buffer-overflow in vsnprintf
  from json_out_addv json_out.c:249; plain build: `malloc(): corrupted
  top size` / membuf_added assertion). alarm(10)-bounded, 4 subtests.
  Must pass after repair (validated against /tmp/jout-asan/shadow with
  the repair-direction patches applied: 4/4 pass, and the existing
  run.c still passes 14689/14689 under ASan+UBSan against the patched
  shadow).
- ccan/json_out/test/run-vsnprintf-error.c — proves F4. Aborts against
  current code (tal "allocation size overflow" / membuf_added assert);
  alarm(10)-bounded, 2 subtests, self-skipping probe. Passes 2/2
  against the patched shadow.
- ccan/json_out/test/run-oom-escape.c — proves F3. SIGSEGV / UBSan
  null-member-access against current code (first frame inside
  ccan/json_escape, see F3); alarm(10)-bounded, 2 subtests. Passes 2/2
  against the patched shadow (which also patches json_escape's
  unchecked tal_arr).
- All three #include ccan/json_out/json_out.c per run-test convention
  (ccanlint only links module objects into api tests).
- ccanlint with all three present: 37/44 FAIL, solely because these
  tests fail against the current code (tests_pass +3/6).

No production files were modified (json_out.h, json_out.c, _info
untouched; verified by git status — the only new paths under
ccan/json_out are the three tests above).
