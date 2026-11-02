#!/bin/bash

set -u

RED="\033[0;31m"
GREEN="\033[0;32m"
PINK='\033[0;95m'
YELLOW="\033[0;33m"
RESET="\033[0m"

VERBOSE=0
if [[ "${1:-}" == "-v" || "${1:-}" == "--verbose" ]]; then
    VERBOSE=1
fi

TESTER_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$( cd "$TESTER_DIR/.." && pwd )"
TMP_DIR=$(mktemp -d /tmp/gnl_fairy.XXXXXX) || exit 1

cd "$PROJECT_DIR" || exit 1

TESTER_NAME=$(basename "$TESTER_DIR")

BUFFER_SIZES="1 42 9999 10000000"
VG_SIZES="1 42 9999"
# Outside the subject: tested too, but what goes wrong there only warns
EDGE_SIZES="0 -1"

# Every failing step goes through fail(), every debatable finding through
# warn(). FAILED alone drives the verdict and the exit code: a warning never
# fails a working get_next_line.
FAILED=0
WARNED=0
BONUS_VERSION=0

# ---- alignment helpers ------------------------------------------------------
# Status words are right-aligned to a fixed column with \033[<n>G (absolute
# cursor column), so emoji width and label length never shift them. "Failed"
# (6 chars) starts at COL; the other words ("Done", "Warning", "Bonus",
# "Mandatory") start where they end at the same place — no tab arithmetic.
COL=37
ok()   { printf "\033[${COL}G  Done\n"; }
fail() { FAILED=1; printf "\033[${COL}G${RED}Failed${RESET}\n"; }
warn() { WARNED=1; printf "\033[$((COL - 1))G${YELLOW}Warning${RESET}\n"; }

cleanup() {
    rm -rf "$TMP_DIR"
    echo ""
    printf "🧹 Cleaning up..."
    ok
    echo ""
}

print_result() {
    echo ""
    if [[ $FAILED -eq 0 ]]; then
        echo -e "${GREEN}╔════════════════════════════════════════╗${RESET}"
        echo -e "${GREEN}║           OH MY, YOU PASSED!           ║${RESET}"
        echo -e "${GREEN}╚════════════════════════════════════════╝${RESET}"
        [[ $WARNED -eq 1 ]] && echo -e "${YELLOW}  ⚠ Warnings above: not failures, worth a look${RESET}"
    else
        echo -e "${RED}╔════════════════════════════════════════╗${RESET}"
        echo -e "${RED}║          OH NO... YOU FAILED!          ║${RESET}"
        echo -e "${RED}╚════════════════════════════════════════╝${RESET}"
    fi
}

# Ctrl+C or kill ends the run at once (status 130) without a verdict: a partial
# run proves nothing. Any other exit prints the verdict. Both clean up.
on_exit() {
    [[ $? -ne 130 ]] && print_result
    cleanup
}
trap on_exit EXIT
trap 'exit 130' INT TERM

echo ""
echo -e "${PINK}╔════════════════════════════════════════╗${RESET}"
echo -e "${PINK}║              GNL-FAIRY 🧚              ║${RESET}"
echo -e "${PINK}╚════════════════════════════════════════╝${RESET}"
echo

# ---- version ----------------------------------------------------------------
printf "🔖 Checking version..."
if [ -f "get_next_line_bonus.c" ]; then
    BONUS_VERSION=1
    printf "\033[38GBonus\n"
else
    printf "\033[34G${YELLOW}Mandatory${RESET}\n"
fi

# ---- locate & verify repository files ---------------------------------------
printf "📂 Checking files..."

# 1. Ensure all mandatory files are exactly at the root
if [ ! -f "get_next_line.c" ] || [ ! -f "get_next_line_utils.c" ] || [ ! -f "get_next_line.h" ]; then
    fail
    echo ""
    echo "Missing one or more mandatory files at the root of the repository:"
    [ ! -f "get_next_line.c" ] && echo "- get_next_line.c"
    [ ! -f "get_next_line_utils.c" ] && echo "- get_next_line_utils.c"
    [ ! -f "get_next_line.h" ] && echo "- get_next_line.h"
    echo ""
    exit 1
fi
MAND_SRCS="get_next_line.c get_next_line_utils.c"
PROJECT_FILES="get_next_line.c get_next_line_utils.c get_next_line.h"
if [[ $BONUS_VERSION -eq 1 ]]; then
    PROJECT_FILES="$PROJECT_FILES get_next_line_bonus.c get_next_line_utils_bonus.c get_next_line_bonus.h"
fi

# 2. No extra .c or .h file. In a git repository only tracked files count
#    (they are what gets graded): a local test main.c is fine.
IN_GIT=0
if git rev-parse --is-inside-work-tree > /dev/null 2>&1; then
    IN_GIT=1
    FOUND_FILES=$(git ls-files -- '*.c' '*.h' | grep -v "^$TESTER_NAME/")
else
    FOUND_FILES=$(find . -type d -name "$TESTER_NAME" -prune -o -type f \( -name "*.c" -o -name "*.h" \) -print)
fi
EXTRA_FILES=""
for file in $FOUND_FILES; do
    [[ " $PROJECT_FILES " == *" ${file#./} "* ]] || EXTRA_FILES+=" ${file#./}"
done

# 3. Files present but never committed would not be graded either
UNCOMMITTED=""
if [[ $IN_GIT -eq 1 ]]; then
    for file in $PROJECT_FILES README.md; do
        [[ -f "$file" ]] && ! git cat-file -e "HEAD:./$file" 2> /dev/null && UNCOMMITTED+=" $file"
    done
fi

if [[ -n "$EXTRA_FILES" ]]; then
    fail
    echo ""
    echo "Forbidden extra .c or .h files found in the repository:"
    for f in $EXTRA_FILES; do
        echo "- $f"
    done
    echo "Please remove them to match the subject's strict turn-in list."
    echo ""
elif [[ -n "$UNCOMMITTED" ]]; then
    warn
else
    ok
fi
[[ -n "$UNCOMMITTED" ]] && echo -e "\nNot committed, hence not graded:$UNCOMMITTED\n"

# ---- README -----------------------------------------------------------------
# Required by recent versions of the subject; older ones had none, and its
# wording is free, so only warn.
printf "📖 Checking README..."
README_ISSUES=""
if [[ ! -f README.md ]]; then
    README_ISSUES="\n- no README.md at the root of the repository"
else
    head -n 1 README.md | grep -qE '^(\*|_|<i>|<em>)This project has been created as part of the 42 curriculum by ' \
        || README_ISSUES+="\n- first line, in italics: This project has been created as part of the 42 curriculum by <login>"
    grep -qiE '^[#*_[:space:]]*Description' README.md || README_ISSUES+="\n- no \"Description\" section"
    grep -qiE '^[#*_[:space:]]*Instruction' README.md || README_ISSUES+="\n- no \"Instructions\" section"
    grep -qiE '^[#*_[:space:]]*Res+ource' README.md || README_ISSUES+="\n- no \"Resources\" section"
    grep -qi "algorithm" README.md || README_ISSUES+="\n- no explanation of the algorithm"
fi
if [[ -z "$README_ISSUES" ]]; then
    ok
else
    warn
    echo -e "\nREADME.md, as the subject describes it:$README_ISSUES\n"
fi

# ---- norm -------------------------------------------------------------------
printf "📝 Checking norm..."

# norminette only checks the project files: nothing else is graded
NORM_FILES=$(for f in $PROJECT_FILES; do [[ -f "$f" ]] && echo "$f"; done)
if ! command -v norminette > /dev/null; then
    fail
    echo -e "\nnorminette not found: the norm was NOT checked.\n"
elif NORM_OUTPUT=$(norminette $NORM_FILES 2>&1); echo "$NORM_OUTPUT" | grep -q "Error"; then
    fail
    echo ""
    echo "$NORM_OUTPUT" | grep "Error"
    echo ""
else
    ok
fi

# The include directory is simply the current directory (.)
IDIRS="-I."

# ---- static checks of one part (mandatory or bonus) -------------------------
EXTERN_ALLOWED="read malloc free"

# The header must declare get_next_line with the subject's type. Redeclaring it
# with that exact prototype is a hard error ("conflicting types") otherwise,
# whatever the spacing, the parameter name or a const on the parameter.
check_proto() {
    printf '#include <stdlib.h>\n#include <unistd.h>\n#include "%s"\n%s\n%s\n' "$1" \
        'int g_gnl_fairy_declared = sizeof(&get_next_line);' 'char *get_next_line(int fd);' \
        | cc -fsyntax-only $IDIRS -x c - 2> "$TMP_DIR/proto.err"
}

# $1 = label prefix ("" or "bonus "), $2 = header, $3.. = sources
check_part() {
    local pfx=$1 hdr=$2
    shift 2
    local tag=${pfx// /_} src obj objs="" bs how err=0 code defined sym forbidden="" globals
    local file_statics statics

    printf "📋 Checking ${pfx}prototype..."
    if check_proto "$hdr"; then
        ok
    else
        fail
        echo -e "\nMissing or malformed prototype in $hdr, expected:\nchar\t*get_next_line(int fd);"
        grep -m 3 "error" "$TMP_DIR/proto.err"
        echo ""
    fi

    # Graded with -Wall -Wextra -Werror, with and without -D BUFFER_SIZE: try
    # them all. The default build feeds the checks below.
    printf "🎛️  Checking ${pfx}compilation..."
    for src in "$@"; do
        objs+=" $TMP_DIR/${src%.c}.o"
        for bs in "" $BUFFER_SIZES; do
            how="without -D BUFFER_SIZE"
            [[ -n "$bs" ]] && how="with -D BUFFER_SIZE=$bs"
            cc -Wall -Wextra -Werror ${bs:+-D BUFFER_SIZE=$bs} $IDIRS -c "$src" \
                -o "$TMP_DIR/${src%.c}${bs:+_bs$bs}.o" 2>> "$TMP_DIR/${tag}cc.err" \
                || { err=1; echo "^ $src $how" >> "$TMP_DIR/${tag}cc.err"; }
        done
    done
    if [[ $err -eq 0 ]]; then
        ok
    else
        fail
        echo -e "\nMust compile with -Wall -Wextra -Werror, with and without -D BUFFER_SIZE (provide a default):"
        cat "$TMP_DIR/${tag}cc.err"
        echo ""
        for obj in $objs; do
            [[ -f "$obj" ]] || exit 1
        done
    fi

    # A call is the project's own only when written in its sources (comments
    # stripped): the compiler emits memset, memcpy or __stack_chk_fail by
    # itself for an array initializer, a struct copy or a stack protector.
    printf "🔍 Checking ${pfx}externals..."
    code=$(perl -0777 -pe 's{/\*.*?\*/}{ }gs; s{//[^\n]*}{}g' "$@" "$hdr" 2> /dev/null || cat "$@" "$hdr")
    defined=" $(nm --defined-only $objs | awk 'NF == 3 {print $3}' | tr '\n' ' ') "
    for obj in $objs; do
        for sym in $(nm -u "$obj" | awk '{print $2}'); do
            [[ " $EXTERN_ALLOWED$defined" == *" $sym "* ]] && continue
            grep -qw -- "$sym" <<< "$code" && forbidden+="$(basename "$obj" .o): forbidden: $sym\n"
        done
    done
    if [[ -z "$forbidden" ]]; then
        ok
    else
        fail
        echo -e "\n$forbidden"
    fi

    # Data symbols: uppercase = global variable, forbidden. Lowercase without
    # a '.' = static declared outside any function: a global variable for most
    # evaluators, not for all, so a warning. Function-local statics (named
    # func.var by clang, var.N by gcc) are what the subject expects.
    printf "🌍 Checking ${pfx}globals..."
    globals=$(nm $objs | awk '$2 ~ /^[BCDGS]$/ {print $3}' | tr '\n' ' ')
    file_statics=$(nm $objs | awk '$2 ~ /^[bdgs]$/ && $3 !~ /\./ {print $3}' | tr '\n' ' ')
    if [[ -n "$globals" ]]; then
        fail
        echo -e "\nGlobal variables are forbidden: $globals"
    elif [[ -n "$file_statics" ]]; then
        warn
    else
        ok
    fi
    [[ -n "$file_statics" ]] && echo -e "\nStatic variables declared outside any function: $file_statics"
    [[ -n "$globals$file_statics" ]] && echo ""

    # The bonus allows a single static variable (const tables aside)
    [[ -z "$pfx" ]] && return
    printf "🧮 Checking ${pfx}statics..."
    statics=$(nm $objs | awk '$2 ~ /^[bdgs]$/ {print $3}' | tr '\n' ' ')
    if [[ $(wc -w <<< "$statics") -le 1 ]]; then
        ok
    else
        fail
        echo -e "\nThe bonus must use only one static variable, found: $statics\n"
    fi
}

# AddressSanitizer catches overflows in every functional scenario, not only in
# the Valgrind ones. Used when the toolchain supports it, never at BUFFER_SIZE
# 10000000: a huge static array there can leave no room for its shadow memory.
# Leaks stay Valgrind's job.
ASAN=""
printf 'int main(void){return 0;}\n' | cc -fsanitize=address -x c - -o "$TMP_DIR/asan_probe" 2> /dev/null \
    && "$TMP_DIR/asan_probe" > /dev/null 2>&1 && ASAN="-fsanitize=address -fno-omit-frame-pointer"
export ASAN_OPTIONS="detect_leaks=0"

# Builds one test binary for the suite's $srcs: $1 = test file,
# $2 = BUFFER_SIZE, $3 = output, $4.. = extra flags
build_test() {
    local src=$1 bs=$2 bin=$3
    shift 3
    cc -g -Wall -Wextra -Wno-unused-result -DVERBOSE=$VERBOSE -DBUFFER_SIZE=$bs "$@" $IDIRS \
        "$TESTER_NAME/$src" "$TESTER_NAME/utils.c" $srcs \
        -Wl,--wrap=malloc,--wrap=read -o "$bin" 2> "$bin.err"
}

# ---- build + run a full suite (mandatory or bonus) --------------------------
# $1 = label prefix, $2 = functional test files, $3 = leak test files, $4.. = sources
run_suite() {
    local pfx=$1 basics=$2 leaks=$3
    shift 3
    local srcs="$*" tag=${pfx// /_}
    local runs="" run kind src bs bin rc asan note status build_fail=0 run_fail=0 run_warn=0 hung=0
    # Logs are collected in a file and printed after the status line; the test
    # output goes through cat, never echo -e, so its backslashes stay intact.
    local report="$TMP_DIR/${tag}report"
    : > "$report"

    # Runs are kind:file:BUFFER_SIZE. Test binaries exit with 0, 1 (failure)
    # or 2 (passed with warnings); "edge" runs (BUFFER_SIZE <= 0) only warn.
    for src in $basics; do
        for bs in $BUFFER_SIZES; do runs+=" basic:$src:$bs"; done
    done
    for bs in $EDGE_SIZES; do runs+=" edge:edge_tests.c:$bs"; done
    for src in $leaks; do
        for bs in $VG_SIZES; do runs+=" leak:$src:$bs"; done
    done

    printf "🔨 Building ${pfx}tests..."
    for run in $runs; do
        IFS=: read -r kind src bs <<< "$run"
        bin="$TMP_DIR/${tag}${src%.c}_bs${bs}"
        asan=""
        [[ $kind != leak && $bs -le 9999 ]] && asan=$ASAN
        build_test "$src" "$bs" "$bin" $asan && continue
        if [[ $kind == edge ]]; then
            echo "[$src BUFFER_SIZE=$bs] does not compile (outside the subject: warning only)" >> "$report"
            run_warn=1
        else
            echo "[$src BUFFER_SIZE=$bs]" >> "$report"
            build_fail=1
        fi
        cat "$bin.err" >> "$report"
    done
    if [[ $build_fail -ne 0 ]]; then
        fail
        cat "$report"
        return
    fi
    ok

    # --foreground keeps the tests in the terminal's process group, so that
    # Ctrl+C reaches them instead of waiting for the current run to finish.
    printf "🧪 Running ${pfx}tests..."
    for run in $runs; do
        IFS=: read -r kind src bs <<< "$run"
        bin="$TMP_DIR/${tag}${src%.c}_bs${bs}"
        [[ $hung -eq 1 || ! -x "$bin" ]] && continue
        note=""
        if [[ $kind == leak ]]; then
            note=" valgrind"
            timeout --foreground 60 valgrind --leak-check=full --show-leak-kinds=all \
                --errors-for-leak-kinds=definite,indirect --error-exitcode=42 --track-origins=yes \
                "$bin" > /dev/null 2> "$bin.log"
            rc=$?
            # Memory still held at exit after reading to EOF: worth freeing,
            # not a leak in Valgrind's own terms
            if [[ $rc -eq 0 ]] && grep -qE "(still reachable|possibly lost): [1-9]" "$bin.log"; then
                rc=2
                note=" valgrind, memory still held at exit: warning only"
            fi
            # Valgrind cannot load a program with static data over ~1 GB: a
            # limit of the tool, not a bug, so the leaks just go unchecked
            if [[ $rc -ne 0 ]] && grep -q "very large text, data or bss" "$bin.log"; then
                rc=2
                note=" valgrind cannot load it (huge static data): leaks NOT checked"
            fi
        else
            [[ $kind == edge ]] && note=", outside the subject: warning only"
            timeout --foreground 60 "$bin" > "$bin.log" 2>&1
            rc=$?
            # ASan may fail to map its shadow memory (huge static arrays): run
            # without it rather than fail a working get_next_line
            if [[ $rc -ne 0 ]] && grep -qE "ASan cannot proceed|ReserveShadowMemoryRange" "$bin.log"; then
                build_test "$src" "$bs" "$bin" && timeout --foreground 60 "$bin" > "$bin.log" 2>&1
                rc=$?
            fi
        fi
        status=fail
        [[ $rc -eq 0 ]] && status=ok
        [[ $rc -eq 2 || ( $kind == edge && $rc -ne 0 ) ]] && status=warn
        [[ $status == fail ]] && run_fail=1
        [[ $status == warn ]] && run_warn=1
        if [[ $status != ok || $VERBOSE -eq 1 ]]; then
            printf "\n${YELLOW}── %s (BUFFER_SIZE=%s)%s ──${RESET}\n" "${src%.c}" "$bs" "$note" >> "$report"
            cat "$bin.log" >> "$report"
        fi
        # A whole binary timing out means get_next_line hangs everywhere:
        # the remaining runs (Valgrind included) would only hang as well.
        if [[ $rc -eq 124 && $kind == basic ]]; then
            printf "${RED}(timed out: remaining ${pfx}runs skipped)${RESET}\n" >> "$report"
            hung=1
        elif [[ $rc -eq 124 ]]; then
            printf "${RED}(timed out)${RESET}\n" >> "$report"
        fi
    done
    if [[ $run_fail -ne 0 ]]; then
        fail
    elif [[ $run_warn -ne 0 ]]; then
        warn
    else
        ok
    fi
    [[ -s "$report" ]] && { cat "$report"; echo ""; }
}

check_part "" "get_next_line.h" $MAND_SRCS
run_suite "" "basic_tests.c" "leak_tests.c" $MAND_SRCS

# ---- bonus ------------------------------------------------------------------
if [[ $BONUS_VERSION -eq 1 ]]; then

    printf "📂 Checking bonus files..."
    # Ensure all required bonus files are exactly at the root of the repository
    if [ ! -f "get_next_line_bonus.c" ] || [ ! -f "get_next_line_utils_bonus.c" ] || [ ! -f "get_next_line_bonus.h" ]; then
        fail
        echo ""
        echo "Missing one or more bonus files at the root of the repository:"
        [ ! -f "get_next_line_bonus.c" ] && echo "- get_next_line_bonus.c"
        [ ! -f "get_next_line_utils_bonus.c" ] && echo "- get_next_line_utils_bonus.c"
        [ ! -f "get_next_line_bonus.h" ] && echo "- get_next_line_bonus.h"
        exit 1
    fi
    ok

    BONUS_SRCS="get_next_line_bonus.c get_next_line_utils_bonus.c"
    check_part "bonus " "get_next_line_bonus.h" $BONUS_SRCS
    run_suite "bonus " "basic_tests.c basic_tests_bonus.c" "leak_tests.c leak_tests_bonus.c" $BONUS_SRCS
fi

exit $FAILED
