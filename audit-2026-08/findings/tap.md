# Audit: ccan/tap

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: tap.c, tap.h, tap.3, _info and ccan/tap/test/ only.  Sole
dependency ccan/compiler (header-only PRINTF_FMT) taken as given.
This module is the TAP producer for ~420 other modules' tests, so the
producer/consumer contract was checked against the standard consumer
(perl5 TAP::Harness via prove(1)) wherever behavior is observable.

Mechanical checks:
- ccanlint (before adding the auditor test): 46/52.  Every check
  passes; the only check losing points is tests_coverage (+1/6).
  tests_pass, tests_pass_valgrind and tests_pass_valgrind_noleaks all
  pass.
- Existing test (test/run.c, 10 subtests + 1 outer) under clang 18
  ASan+UBSan (`-fno-sanitize=function`, -g, -I.; `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  passes cleanly, zero sanitizer diagnostics.
- Consumer checks with prove(1) (TAP::Harness) on captured output of
  each reproducer; transcripts quoted below.
- Reproducers live in /tmp/tap-asan/repro-*.c (temporary, not
  committed).  A fix-validation copy of tap.c with the one-line
  setbuf restoration lives in /tmp/tap-fix/ (production file
  untouched).
- After adding the auditor test: ccanlint 43/48 FAIL — tests_compile
  PASS, tests_pass FAIL (+1/2), exactly as designed (same pattern as
  the failtest audit).

Auditor-added test files (temporary; keep or remove later):
- ccan/tap/test/run-fork-flush.c — proves F1.  Currently FAILS as
  designed (plain gcc build and clang ASan+UBSan build): the captured
  TAP stream contains the pre-fork lines twice.  Passes against a
  /tmp copy of tap.c with the documented `setbuf(stdout, 0)`
  restored (see F1 repair direction).  alarm(10)-bounded; no
  production files modified.

## Findings

## F1 — CONFIRMED (FIXED in 6452564d): stdout is left fully buffered (documented `setbuf(stdout, 0)` is commented out), so a forked child's plain exit() duplicates every pre-fork TAP line — the harness rejects the stream

- Location: ccan/tap/tap.c:267-271 (_tap_init):
  ```
  /* stdout needs to be unbuffered so that the output appears
     in the same place relative to stderr output as it does
     with Test::Harness */
  //		setbuf(stdout, 0);
  ```
  The buffering side effect is part of the module's documented
  contract: tap.3:358-361 ("BUGS" section) states "stdout is set to
  unbuffered mode after calling any of the plan_* functions."  The
  code does not do it (the setbuf was disabled in commit b734bbea
  "tap: restore buffering to stdout" for performance — the per-char
  fputc loop in _gen_result produced 1-byte writes).
- Reachable path: any tap-based test that fork()s while stdout is a
  pipe or file (i.e. under every test harness; stdout to a pipe is
  fully buffered by default).  The child never touches tap; it does
  its work and calls plain exit(0).  exit() runs atexit handlers and
  flushes stdio: tap's _cleanup *is* correctly suppressed in the
  child (getpid() != test_pid guard, tap.c:202 — verified, the child
  emits no cleanup line), but the stdio flush is not suppressed, so
  the child's copy of the buffer — every TAP line the parent printed
  before the fork, including the plan line — is written to the
  shared stream a second time.
- Preconditions: none violated.  Nothing in tap.h or tap.3 forbids
  forking; tap contains explicit fork-tolerance code (the test_pid
  guard, tap.c:201-203), so forked children that exit() normally are
  an anticipated scenario — the guard is just incomplete.  In-tree
  confirmation that the trap is real: ccan/err/test/run.c:33-34,
  66-67, 97-98, 130-131 does `fflush(stdout);` immediately before
  every `fork()` — an in-tree user manually working around exactly
  this defect.
- Concrete consequence: corrupted TAP stream — duplicated plan line
  and duplicated test lines.  Reproducer /tmp/tap-asan/repro-fork2.c
  (plan_tests(2); ok1; fork; child exit(0) with no tap calls; parent
  ok1; exit_status).  Observed output (stdout=pipe):
  ```
  1..2
  ok 1 - 1
  1..2          <- child's flush of the inherited buffer
  ok 1 - 1
  ok 2 - 1
  ```
  prove(1) verdict on that stream:
  ```
  Parse errors: More than one plan found in TAP output
                Tests out of sequence.  Found (1) but expected (2)
                Tests out of sequence.  Found (2) but expected (3)
                Bad plan.  You planned 2 tests but ran 3.
  Result: FAIL
  ```
  So a suite in which every test passed and which exited 0 is failed
  by the standard consumer.  (Parent-and-child-both-run-tests after
  fork additionally diverges test_count; that part is inherent fork
  semantics and is the caller's problem — not part of this finding.)
- Regression test: ccan/tap/test/run-fork-flush.c captures the whole
  TAP stream through a pipe (style of test/run.c) and compares it
  byte-for-byte with the expected "1..2\nok 1 - true\nok 2 - true\n".
  Pre-fix it prints `not ok 1 - TAP stream duplicated by forked
  child: ...` and exits 1 (both plain gcc and ASan+UBSan builds).
  Against /tmp/tap-fix/tap-fixed.c (a copy of tap.c with only the
  setbuf line uncommented — production file untouched) the same test
  passes: `ok 1 - forked child's exit() adds nothing to the TAP
  stream`, rc=0.
- Repair direction: make the code match tap.3.  To keep the b734bbea
  performance win, prefer line buffering over full unbuffering:
  `setvbuf(stdout, NULL, _IOLBF, 0);` in _tap_init — every TAP
  statement ends in '\n', so output is still one write per line (no
  per-char writes from the '#' escaping loop at tap.c:151-157), and
  no complete TAP line can sit in a buffer across a fork.
  Alternatively fflush(stdout) at the end of _expected_tests,
  _gen_result, skip, plan_skip_all and diagv.  (Verified: restoring
  plain setbuf(stdout, 0) makes the regression test pass.)

## F2 — LIKELY (FIXED in bc70b226): plan_skip_all() neither checks nor sets have_plan — double planning via plan_skip_all is silently accepted, producing two plan lines (invalid TAP) and a wrong exit status

- Location: ccan/tap/tap.c:303-320 (plan_skip_all).  Contrast
  plan_no_plan (tap.c:286-291) and plan_tests (tap.c:333-338), which
  both do `if(have_plan != 0) { fprintf(stderr, "You tried to plan
  twice!\n"); test_died = 1; exit(255); }`.  plan_skip_all lacks the
  guard and also never sets have_plan, so a *later* plan_tests() /
  plan_no_plan() is accepted too.
- Reachable path: a test that calls two plan functions, one of them
  plan_skip_all — e.g. a fallback:
  `plan_tests(n); if (feature_missing) plan_skip_all("...");` or the
  reverse order.  tap.3:59-64 documents the "exit prematurely with a
  diagnostic" contract only for plan_tests/plan_no_plan, so this is
  caller misuse — hence LIKELY, not CONFIRMED — but the module's own
  established policy is that double planning is a fatal, loudly
  reported error, and plan_skip_all silently violates it.
- Concrete consequences (observed, /tmp/tap-asan/repro-doubleplan.c
  and repro-doubleplan2.c):
  - plan_tests(3); plan_skip_all("..."): output is
    ```
    1..3
    1..0 # Skip skipping anyway
    # Looks like you planned 3 tests but only ran 0.
    ```
    exit_status() == 3 (rc=3) — a skip-all that reports failure, plus
    two plan lines.
  - plan_skip_all("..."); plan_tests(2); two passing tests: output is
    ```
    1..0 # Skip no support
    1..2
    ok 1 - 1
    ok 2 - 1
    ```
    rc=0, but prove(1) rejects the stream:
    ```
    Parse errors: More than one plan found in TAP output
                  Bad plan.  You planned 0 tests but ran 2.
    Result: FAIL
    ```
- Repair direction: add the same have_plan guard to plan_skip_all
  (printing "You tried to plan twice!" and exiting 255), set
  have_plan = 1 in it, and have plan_tests/plan_no_plan treat a
  preceding skip_all the same way (their existing have_plan check
  covers that once skip_all sets it).

## F3 — LIKELY (FIXED in d8d9f9f8): tap_fail_callback fires for failing TODO tests, defeating its documented "exit on the first failure" purpose

- Location: ccan/tap/tap.c:186-187 (_gen_result):
  `if (!ok && tap_fail_callback) tap_fail_callback();` — no `!todo`
  condition, although the same function deliberately un-counts TODO
  failures (failures-- at tap.c:172-173) because they are expected.
- Reachable path: any suite that sets tap_fail_callback (documented
  at tap.h:245-247: "This can be used to ease debugging, or exit on
  the first failure" — added in fece6c23 "so we can abort on first
  failure") and also contains a todo_start()/todo_end() block with a
  failing test: the callback fires on the *expected* failure, so an
  abort-on-first-failure setup stops at a test that is supposed to
  fail.
- Preconditions: none violated.
- Concrete consequence: /tmp/tap-asan/repro-failcb-todo.c: one TODO
  failure + one real failure, callback count observed `cb_count=2`
  while exit_status() correctly reports 1 failure — the callback and
  the failure accounting disagree about what "a failure" is.
- LIKELY rather than CONFIRMED because the one-line doc ("function to
  call when we fail") is literally defensible for a TODO failure, and
  the only in-tree user (ccan/rbtree/test/run-many.c:107) has no TODO
  tests, so in-tree impact is nil today.
- Repair direction: gate the callback with `!todo` (matching the
  failures counter), or document at tap.h:245-247 that the callback
  also fires for expected (TODO) failures.

## Rejected candidates (disproved)

- R1: Test that never calls any plan_* function gets no trailing
  "1..N" and no diagnostic, although tap.c:223-227's comment ("No
  plan provided, but now we know how many tests were run, and can
  print the header at the end") and the `!have_plan` disjunct at
  tap.c:225 say it should.  Both are dead code: _cleanup is only
  registered with atexit() from _tap_init(), which is only called by
  plan_no_plan/plan_skip_all/plan_tests, each of which sets have_plan
  or skip_all — so the `!no_plan && !have_plan && !skip_all` branch
  at tap.c:210-214 ("Looks like your test died before it could
  output anything.") and the no-plan printing at :225 can never
  execute.  Reproduced (/tmp/tap-asan/repro-noplan.c): output is two
  bare "ok" lines, rc=0; prove(1) fails it ("No plan found in TAP
  output").  Rejected as a documented-precondition violation:
  tap.3:42 states "You must first specify a test plan."  The dead
  code itself is a maintenance observation, not a runtime defect.
- R2: Unbalanced todo_end() double-frees todo_msg (tap.c:417 frees
  without NULLing).  ASan confirms:
  `AddressSanitizer: attempting double-free` in
  /tmp/tap-asan/repro-todoend.c.  Rejected: pairing is the documented
  contract ("surround these tests by todo_start()/todo_end()",
  tap.h:181, tap.3:227-231).  Same category: todo_start() twice
  without todo_end() leaks the first todo_msg (tap.c:401 overwrites
  the pointer).
- R3: Format-string exposure through test names.  ok1() stringifies
  the expression and passes it as data with "%s" (tap.h:56-58) —
  safe, verified.  ok(), diag(), skip(), todo_start() take
  documented printf-style formats with PRINTF_FMT attributes
  (tap.h:118-119, 131, 154, 184) so the compiler checks literal
  calls; passing an untrusted string as the format is a caller
  precondition violation.  Rejected.
- R4: '#' in test names breaks TAP directive parsing.  Disproved:
  _gen_result backslash-escapes '#' in the name (tap.c:151-157).
  Newlines inside a caller-supplied name/diag/todo message do break
  TAP framing, but the string is caller-supplied and no multi-line
  handling is promised anywhere in tap.h/tap.3.  Rejected.
- R5: unsigned int counters (e_tests, test_count, failures) printed
  with %d throughout (tap.c:69, 143, 226, 230-249, 383).  Formally a
  variadic type mismatch (C11 7.16.1.1), benign on every ABI CCAN
  targets; observable wrong output needs >2^31 tests — speculative
  per AGENTS.md.  Portability nit only.  Rejected.
- R6: exit_status() arithmetic.  Verified correct for the reachable
  mismatch cases (/tmp/tap-asan/repro-exitstatus.c): plan 3 / 2 run /
  1 failure -> 2; plan 2 / 3 run -> 1 ("ran 1 extra"); plan_skip_all
  alone -> 0; >255 failures clamped to 255 (tap.c:455-456; commit
  cbca0212 fixed the exactly-256 case).  No unsigned wraparound is
  reachable: the e_tests < test_count case returns first, and
  failures <= test_count always (TODO failures are un-counted).
  Rejected.
- R7: test_count overflow at 2^32 tests ("ok 0", duplicate numbers).
  Speculative; unreachable in practice.  Rejected.
- R8: The fork child-cleanup guard (tap.c:201-203) itself: verified
  working — in repro-fork.c the child emits no plan/cleanup line;
  the single "Looks like you planned 3 tests but only ran 2" comes
  from the parent and is correct from the parent's perspective.  The
  stdio-duplication half of fork behavior is F1.  Rejected.
- R9: Portability of unconditional vasprintf()/flockfile()/unistd.h
  use (no configurator check for vasprintf exists).  Fine on
  glibc/musl/BSD; Windows is not a target for this module (unistd.h,
  getpid).  ccanlint reduce_features/objects_build_without_features
  pass.  Note only.  Rejected.
- R10: atexit(_cleanup) ordering vs. user atexit handlers: LIFO is
  standard C; _cleanup prints only diagnostics/plan lines and its
  output ordering relative to user handlers is not part of the TAP
  contract.  Rejected.
- R11: skip_if() dangling-else macro (tap.h:105-107): with the
  customary trailing semicolon the expansion parses and binds
  correctly (`if (a) if (b) skip(...); else ; else x();` — the inner
  else swallows the semicolon, the outer else binds to if(a)).
  Fragile but not observably wrong; the header itself comments "I
  don't find these to be useful."  Rejected.
- R12: plan_tests(0) exits 255 with a stderr diagnostic: deliberate
  and documented (tap.3:48-49).  plan printed before tests, TAP
  directives ("# skip", "# TODO") and the name_is_digits warning all
  match the TAP spec and the expectations encoded in test/run.c.
  Rejected.

No other findings.  The rest of the audit plan checked out clean under
the sanitizer suite, code inspection and the prove(1) consumer checks:
vasprintf failure paths fall back to fixed messages without leaking
(todo_msg_fixed / "libtap():malloc() failed"); LOCK/UNLOCK are no-ops
without WANT_PTHREAD and correct with it; the ok()/ok1() macros
evaluate their arguments exactly once; exit_status() is side-effect
free and idempotent; skip() counter arithmetic is consistent with the
plan; and the existing test/run.c exercises every public entry point
except plan_skip_all/plan_no_plan/diag under ccanlint without a
diagnostic.
