# Evaluation record

Problem: ACMOJ 1605. All five allowed submissions have been used.

| Submission | Commit | Repository URL form | Result | Score |
| --- | --- | --- | --- | --- |
| 879520 | 88557da | HTTPS without .git | Compile Error | 0 |
| 879521 | 88557da | SSH on port 443 | Compile Error | 0 |
| 879523 | 120dc7d | HTTPS with .git | Compile Error | 0 |
| 879528 | 24540fd | HTTPS with .git | Compile Error | 0 |
| 879531 | 24540fd | git@github.com SSH | Compile Error | 0 |

Every status response contained only `compile error`, with an empty test group
list and no compilation diagnostics. The underlying cause remains unknown.
No repository modifications or pushes occurred while submissions were pending.
Each push was verified by comparing local HEAD with `git ls-remote`.

Local validation succeeded with GCC 13.3:

- CMake build and direct Makefile build.
- A fresh HTTPS clone built with the documented in-source CMake/make procedure.
- Instruction, memory-width, hazard, branch-flush, loop, and jump regression tests.
- 100 deterministic randomized arithmetic programs checked against a Python oracle.

The GitHub repository was also confirmed publicly accessible without credentials.
The supplied README's test-data directory was absent. Local checks do not establish
correctness on the unavailable judge test cases.
