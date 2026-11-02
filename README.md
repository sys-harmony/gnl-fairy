# 🧚 gnl-fairy

A tester for 42's **get_next_line** (subject v14.3). Every scenario at `BUFFER_SIZE` 1, 42, 9999 and 10000000, under AddressSanitizer, Valgrind, fault injection on `read` and `malloc`, plus every check the subject asks for, in about 5 seconds.

Part of a trio with [libft-fairy](https://github.com/sys-harmony/libft-fairy) and [printf-fairy](https://github.com/sys-harmony/printf-fairy): same display, same verdicts, same engine.

## 🚀 Usage

Clone it inside your get_next_line repository, then run it:

```bash
git clone https://github.com/sys-harmony/gnl-fairy.git
./gnl-fairy/run.sh          # failing tests only
./gnl-fairy/run.sh -v       # every test, for every BUFFER_SIZE
```

The exit status is `0` when everything passed (warnings included), `1` otherwise, so it fits in a script or a CI.

## 📊 Output

```
╔════════════════════════════════════════════╗
║                GNL-FAIRY 🧚                ║
╚════════════════════════════════════════════╝

🔖 Version ............................. Bonus
📂 Files ................................ Done
📖 README ............................... Done
📝 Norm ................................. Done
📋 Prototype ............................ Done
🧱 Compilation .......................... Done
🔍 External functions ................... Done
🌍 Global variables ..................... Done
🔨 Test build ........................... Done
📋 Bonus prototype ...................... Done
🧱 Bonus compilation .................... Done
🔍 Bonus external functions ............. Done
🌍 Bonus global variables ............... Done
🧮 Bonus single static .................. Done
🔨 Bonus test build ..................... Done
🧪 Tests .............................. Failed

── lines · BUFFER_SIZE=1 ────────────────────
[KO] one char, no newline
     ↳ line 1: expected "a", got NULL
[KO] three lines, no trailing newline
     ↳ line 3: expected "line3", got NULL

── long lines · BUFFER_SIZE=1 ───────────────
[KO] 5000-char line, no newline
     ↳ line 1: expected "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"… (5000 chars), got NULL
…
🧪 Bonus tests .......................... Done
🧹 Cleanup .............................. Done

╔════════════════════════════════════════════╗
║            OH NO... YOU FAILED!            ║
╚════════════════════════════════════════════╝
   ✘ Tests · 31/213 tests failed · 4.60s
```

Only what did not pass is listed, each time with its reason: the first line that differs (expected vs got), a crash signal, a timeout, or the AddressSanitizer or Valgrind report. Tests are built with `-g`: reports point to your file and line.

## ⚖️ Failures and warnings

A **failure** is a bug or a clear subject violation. A **warning** is something debatable: worth a look, maybe a question from your evaluator, never a failure. A working get_next_line passes.

Warnings, for get_next_line:

- after a `read()` error: a partial line returned instead of `NULL`, or the stash kept (the next file on that fd starts with stale data)
- `read()` called with another size than `BUFFER_SIZE`, or the whole file read before the first line comes back
- a `static` variable declared outside any function
- memory still held at exit after reading to the end (Valgrind's *still reachable*), or leaks left unchecked because Valgrind cannot load over ~1 GB of static data
- a valid but high fd (1023) not served
- anything going wrong with `BUFFER_SIZE` 0 or -1, outside the subject
- a README that does not look like the subject's description, project files git does not track
- `norminette` or `valgrind` missing: that check did not run

## 🧪 What is checked

| Step | What |
|---|---|
| 🔖 Version | Bonus when `get_next_line_bonus.c` exists |
| 📂 Files | the subject's files at the root, and no other `.c` or `.h` among the graded ones (a local, untracked `main.c` is fine); project files not committed (warning) |
| 📖 README | there (failure), with the italic first line, *Description*, *Instructions*, *Resources*, how AI was used and the algorithm (warnings) |
| 📝 Norm | `norminette` on the graded files |
| 📋 Prototype | the compiler checks the header against `char *get_next_line(int fd)` |
| 🧱 Compilation | `-Wall -Wextra -Werror`, without `-D BUFFER_SIZE` (a default must exist) and with each tested size |
| 🔍 External functions | `read`, `malloc`, `free` only. `memset`, `memcpy` and `__stack_chk_fail` the compiler emits on its own are allowed, unless written in the sources |
| 🌍 Global variables | none (failure); a `static` outside any function warns |
| 🧮 Bonus single static | a single `static` variable |
| 🧪 Tests | below, at each `BUFFER_SIZE` |

**Lines:** empty file, one char with and without `\n`, lone `\n`, empty lines, no final `\n`, spaces and tabs, CRLF kept as is. Each file is read to the end against a reference split, then `NULL` is expected twice.

**Long lines:** 5000 and 20000 characters, 1000 short lines, and lines of `BUFFER_SIZE - 1`, `BUFFER_SIZE` and `BUFFER_SIZE + 1` characters: the end of a line falls just before, on and just after the end of a `read()`.

**fds:** -1, closed, `INT_MAX`, a directory and `/dev/null` give `NULL`; fd 1023 works; stdin as a pipe with `read()` delivering 1 to 5 bytes at a time, as pipes and terminals do.

**State and `read()`:** a new file on the same fd number after the end; a `read()` error in the middle of a line, then the file reopened; the size asked to `read()` and how far it reads ahead (warnings).

**Bonus:** three fds read interleaved, in turn to the end, two fds on the same content, a `read()` error on one fd sparing the others.

**Leaks:** Valgrind at `BUFFER_SIZE` 1, 42 and 9999: full reads, an 8 KB line, a `read()` error, and each `malloc` failing in turn until every allocation site has failed once; for the bonus, several fds at once.

## 🔍 How it works

- `BUFFER_SIZE` is a compile-time constant: every test binary is built for each size, all in parallel, then all run in parallel. Every core of the machine is used.
- AddressSanitizer runs with the functional tests up to `BUFFER_SIZE` 9999, so an overflow fails even without a crash; a binary it cannot start (huge static arrays) is rebuilt without it.
- Every test runs in its own process with a timeout: a crash or an infinite loop fails that test only, and the remaining timeouts shrink to 1 second.
- `-Wl,--wrap=malloc,--wrap=read` lets the tests make a `malloc` fail, make `read` fail on one fd or deliver a few bytes at a time, and watch what `read` is asked.
- Expected lines come from the content itself (`gnl_expected_next`): a scenario is one line in a table.

## 📋 Requirements

Linux with `bash` 4.3+, `cc` (AddressSanitizer used when available), `nm`, `valgrind`, `norminette`, `perl` and coreutils.

## 📁 Structure

```
gnl-fairy/
├── run.sh               # the steps; its "fairy core" is shared by the trio
├── utils.c              # malloc and read wrappers, reference split; the C fairy core
├── gnl_fairy.h
├── basic_tests.c        # mandatory scenarios
├── basic_tests_bonus.c  # several fds at once
├── edge_tests.c         # BUFFER_SIZE 0 and -1, warnings only
├── leak_tests.c         # under Valgrind
└── leak_tests_bonus.c   # under Valgrind, several fds
```

## 💖 Credits

Created with ✨ by **sys-harmony**. Issues and new edge cases are welcome.
