How to audit a CCAN module.  Audit for actual correctness, security and portability defects.

First:
1. Read _info, headers, implementation, tests and direct dependencies.
2. Run the existing ccanlint target.
3. Run the tests under Clang ASan+UBSan.
4. Do not modify production files.
5. You may add temporary TAP regression tests.

Only report a defect when you can provide:
- exact file, line and function;
- reachable execution path;
- documented or inferred caller preconditions;
- concrete incorrect consequence;
- a reproducer or regression test;
- observed test, sanitizer or compiler output;
- minimal repair direction.

Actively try to disprove every candidate before retaining it.
Ignore style, speculative hardening and violations of documented preconditions.
Classify each result as CONFIRMED, LIKELY, or REJECTED.
