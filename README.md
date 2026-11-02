# 🧚 GNL-fairy

A comprehensive tester for 42's **get_next_line** project, featuring memory leak and memory error detection, multi-buffer-size validation, forbidden-symbol checks, fault injection, and rigorous line-by-line comparison against a reference splitter.

## ✨ Features

- ✅ Complete functional tests (mandatory + bonus multi-fd)
- ⚖️ Two-level verdict: **failures** for real bugs and clear subject violations, **warnings** for debatable behaviours — a working `get_next_line` never fails
- 🧹 Clean execution (temporary files created and removed automatically, even on Ctrl+C)
- 🔍 Automatic verification with **norminette**
- 📋 Prototype validation in `get_next_line.h` / `get_next_line_bonus.h`
- 🎛️ Recompiles and replays the suite for several `BUFFER_SIZE` values (1, 42, 9999, 10000000), plus 0 and -1 (warnings only)
- 🔎 External function check (`read`, `malloc`, `free` only — catches `lseek` and libft calls)
- 🌍 Global-variable detection (forbidden by the subject)
- 🧮 Bonus: checks that a single `static` variable is used
- 🚰 Pipe/terminal emulation: `read()` delivering data in small pieces
- 🛡️ AddressSanitizer on the functional tests, **Valgrind** on the leak tests, including `read` errors and every `malloc` failure path
- 📖 Turn-in checks: extra files, uncommitted files, `README.md`
- ⏱️ Per-test timeout guard against infinite loops
- 📊 Clear and colorful output, with the reason of every failure or warning
- 🔇 Optional verbose mode for detailed results
- 🚦 Exit code `0` on success (warnings included), `1` on failure (usable in scripts)

## 📋 Prerequisites

Before using GNL-fairy, make sure you have installed:

- `cc` (with `--wrap` linker support; AddressSanitizer is used when available)
- `valgrind`
- `norminette` (the norm check fails if it is missing)
- `bash`, `perl`
- `nm` (usually pre-installed)

## 🚀 Installation

1. Clone the repository inside your get_next_line directory:

```bash
git clone https://github.com/sys-harmony/gnl-fairy.git
```

2. Navigate to the tester folder:

```bash
cd gnl-fairy
```

3. Make the script executable:

```bash
chmod +x run.sh
```

## 🎯 Usage

### Run all tests

```bash
./run.sh
```

### Verbose mode (displays all tests, even passing ones)

```bash
./run.sh --verbose
# or
./run.sh -v
```

## 📊 Output

```
╔════════════════════════════════════════╗
║              GNL-FAIRY 🧚              ║
╚════════════════════════════════════════╝

🔖 Checking version...               Bonus
📂 Checking files...                  Done
📖 Checking README...                 Done
📝 Checking norm...                   Done
📋 Checking prototype...              Done
🎛️  Checking compilation...           Done
🔍 Checking externals...              Done
🌍 Checking globals...                Done
🔨 Building tests...                  Done
🧪 Running tests...                   Done
📂 Checking bonus files...            Done
📋 Checking bonus prototype...        Done
🎛️  Checking bonus compilation...     Done
🔍 Checking bonus externals...        Done
🌍 Checking bonus globals...          Done
🧮 Checking bonus statics...          Done
🔨 Building bonus tests...            Done
🧪 Running bonus tests...             Done

╔════════════════════════════════════════╗
║           OH MY, YOU PASSED!           ║
╚════════════════════════════════════════╝
```

A step is `Done`, `Warning` or `Failed`. Under it, each failing scenario prints a `↳` line and each warning a `⚠` line, per buffer size, along with the AddressSanitizer or Valgrind report when there is one:

```
── basic_tests (BUFFER_SIZE=42) ──
  ↳ three lines, no trailing newline: line 3: expected "line3", got NULL
  ↳ huge fd (INT_MAX) returns NULL: crashed (Segmentation fault)
  ⚠ read() error mid-line, then reopen: returned a partial line after read() failed (the subject asks for NULL on error)
```

## ⚖️ Failures and warnings

A **failure** is a bug or a clear subject violation: wrong line, missing `NULL`, crash, timeout, leak (definitely or indirectly lost), memory error, forbidden function, global variable, norm error, missing or extra file, compilation error, more than one `static` in the bonus.

A **warning** flags what is debatable — not a bug, but an evaluator may ask about it. Warnings never fail the run:

- `read()` called with a size other than `BUFFER_SIZE`, or the whole file read ahead before the first line is returned
- after a `read()` error: a partial line returned instead of `NULL`, or the stash kept (the next file on that fd starts with stale data)
- a `static` variable declared outside any function
- memory still held at exit after reading to EOF (Valgrind's *still reachable* / *possibly lost*), or leaks left unchecked because Valgrind cannot load a program with over ~1 GB of static data
- a valid but high fd (1023) not served
- anything going wrong with `BUFFER_SIZE` 0 or -1, outside the subject
- a `README.md` missing or not shaped as the subject describes, project files never committed

## 🧪 Tests Covered

### Pre-compilation Checks

- **Files**: the mandatory (and bonus) files must sit at the root, and no other `.c`/`.h` file may be in the repository — in a git repository only tracked files count, so a local test `main.c` is fine; project files that exist but were never committed are reported (warning)
- **README**: the first line, the *Description*, *Instructions* and *Resources* sections and the algorithm explanation the subject asks for (warning)
- **Norminette**: validates the project files
- **Prototype**: the header must declare a `get_next_line` compatible with `char *get_next_line(int fd)`; the compiler checks it, so spacing, parameter name or `const` don't matter
- **Compilation**: `-Wall -Wextra -Werror`, without `-D BUFFER_SIZE` (a default must exist) and with each tested value, as the grading does
- **External functions**: each object is inspected with `nm -u`; only `read`, `malloc`, `free` and the functions defined by the project itself are allowed. Calls the compiler emits on its own (`memset` for an array initializer, `memcpy` for a struct copy, `__stack_chk_fail`) don't count: only calls written in the sources do
- **Global variables**: objects are scanned with `nm` for data symbols; a non-static global fails, a `static` declared outside any function warns, and a `static` declared inside a function (named `func.var` or `var.N` by the compiler) is what the subject expects
- **Bonus statics**: the bonus objects may contain only one (non-const) `static` variable

### Functional Scenarios (mandatory)

Each scenario writes a known buffer to a temp file and reads it back line by line, comparing every line to the reference split (each line keeps its trailing `\n`, except a final line with none), then asserts that `get_next_line` returns `NULL` twice at EOF. These tests run under AddressSanitizer for `BUFFER_SIZE` up to 9999, so an overflow fails even when it does not crash.

#### Tiny inputs
- Empty file, single char with/without newline, lone newline

#### Basic multi-line
- Three lines with/without trailing newline
- Only empty lines, mixed empty and filled lines
- Spaces and tabs preserved, CRLF line endings kept as is

#### Long lines
- 5000-char line with/without newline
- 20000-char giant line
- Lines of `BUFFER_SIZE - 1`, `BUFFER_SIZE` and `BUFFER_SIZE + 1` chars, followed by nothing, a newline, or a newline and more text: the line end falls just before, on and just after a `read()` chunk boundary (up to `BUFFER_SIZE` 100000)

#### Many lines
- Many identical short lines (1000, or 100 with a huge `BUFFER_SIZE`, where a valid implementation may zero a 10 MB buffer per call), increasing-length lines

#### Fd handling
- Negative fd, closed fd, huge fd (`INT_MAX`), directory fd, empty source (`/dev/null`) all return `NULL`
- A valid but high fd (1023) is read (warning if not)
- Reading from standard input as a pipe, with `read()` returning a few bytes at a time like a pipe or a terminal does

#### State, errors & read rules
- A new file opened after EOF (same fd number) is read from scratch
- A `read()` error with a partial line in the stash, then the file reopened on the same fd number: no crash, no hang, and the file read back (a partial line or stale data only warn)
- `read()` is called with `BUFFER_SIZE` and the first line comes back without the whole file being read ahead (warnings)

#### BUFFER_SIZE 0 and -1
- Outside the subject, so only warnings: `get_next_line` should return `NULL` (or the right lines) without crashing, hanging or overflowing

### Bonus Scenarios (multi-fd)

- Interleaved reads across three fds keep each fd's reading state
- Round-robin full drain of three fds
- Two fds on identical content stay independent
- A `read()` error on one fd leaves the others intact

### Memory Leak Tests

Run under **Valgrind** (`--leak-check=full`): definitely or indirectly lost memory and memory errors fail; memory still held at exit only warns. Every helper reads its file to EOF so a correct implementation frees its static stash (a partial read would legitimately leave a *still reachable* buffer and is not a leak).

- Full read with / without trailing newline
- Long single line (8 KB)
- Many lines
- `read` error with a partial line in the stash (forced via the read wrapper): must return without leaking
- `malloc` failure at **every** allocation site, one after the other (forced via the malloc wrapper): no crash, no leak

Bonus leak tests cover several descriptors read concurrently, including a `malloc` failure during interleaved reads.

## 📁 Project Structure

```
gnl-fairy/
├── run.sh                    # Main test orchestrator
├── basic_tests.c             # Mandatory functional tests
├── basic_tests_bonus.c       # Bonus multi-fd tests
├── edge_tests.c              # BUFFER_SIZE 0 and -1 (warnings only)
├── leak_tests.c              # Leak tests (mandatory)
├── leak_tests_bonus.c        # Leak tests (bonus)
├── utils.c                   # Helpers + malloc/read wrappers + scenario engine + forked runner
├── gnl_fairy.h               # Header file
└── README.md                 # This file
```

## 🔍 Advanced Features

### Buffer-size sweep

`BUFFER_SIZE` is a compile-time constant, so the tester recompiles the whole suite for `1`, `42`, `9999` and `10000000` and replays it each time. The expected output is independent of the buffer size, which is exactly the property the subject asks you to reason about — a single `BUFFER_SIZE`-specific bug (off-by-one around the buffer boundary) is caught on at least one size while the others may stay green.

### Reference splitter

Instead of hard-coding expected lines, the tester derives them from the file content with a small reference splitter (`gnl_expected_next`). The same engine drives the multi-fd bonus tests, where each descriptor keeps its own cursor into its own expected stream.

### Table-driven, forked tests with timeout

Each group is a table of `{name, function}` or `{name, content}` entries: adding a scenario is one line. Every test runs in its own process, so a segfault or double free in one case never takes the tester down, and each child arms an `alarm()` so a non-terminating `get_next_line` (a classic `BUFFER_SIZE` mistake) fails with a timeout instead of hanging the run. A test binary exits with `0`, `1` (failure) or `2` (passed with warnings). If a whole test binary times out, the remaining runs of that suite (Valgrind included) are skipped: they would only hang as well.

### Fault injection

`__wrap_malloc` and `__wrap_read` (via `-Wl,--wrap=malloc,--wrap=read`) let the tests:

- force a `malloc` failure at a chosen call — the leak tests fail the 1st, then the 2nd, … allocation of a full read until every site has failed once;
- force `read` to return `-1` on one fd;
- make `read` deliver 1 to 5 bytes at a time, like a pipe or a terminal: an implementation that takes a short read for EOF cuts its lines;
- record the requested sizes and the bytes consumed, for the subject's `read()` rules.

### Sanitizers without false alarms

AddressSanitizer is used only when the toolchain supports it, never with `BUFFER_SIZE` 10000000, and a test that cannot start under it (a huge static array leaves no room for its shadow memory) is rerun without it rather than failed. Likewise, when Valgrind cannot load a program (over ~1 GB of static data), the leaks are reported as unchecked, not failed.

### Reading to EOF in leak tests

Leak tests always drain to EOF so the static stash is freed by a correct implementation; this avoids false positives on the legitimate *still reachable* buffer a partial read would leave behind. The drain is capped, so a `get_next_line` that never returns `NULL` cannot keep Valgrind running until its timeout.

## 🐛 Debugging

If a test fails:

1. **Reasons**: each failing scenario prints a `↳` line — the first differing line (expected vs got, `\n` escaped), the crash signal, or the timeout — and each warning a `⚠` line
2. **Verbose mode**: run `./run.sh -v` to see every scenario, per buffer size
3. **Sanitizer and Valgrind logs**: printed in full on a memory error or a leak; tests are built with `-g`, so stacks point to your file and line
4. **Norminette**: specific errors shown with file and line
5. **Build errors**: captured per target and printed on failure

## ⚠️ Limitations

- Tested only on Linux (uses `fork`, `nm`, Valgrind, and `--wrap` linker support)
- Requires the tester to sit in a subfolder of the project (as cloned)
- The reference splitter assumes text input (binary files are undefined behavior per the subject)

## 🤝 Contributing

Contributions are welcome! Feel free to:

- Open an issue to report a bug
- Suggest new edge cases
- Improve test coverage
- Enhance documentation

## 📜 License

This project is free to use for educational purposes.

## 💖 Credits

Created with ✨ by **sys-harmony**.

---

*If GNL-fairy helped you validate your get_next_line, don't forget to leave a ⭐ on the repo!*
