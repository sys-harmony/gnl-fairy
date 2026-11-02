#!/bin/bash
# gnl-fairy: a tester for 42's get_next_line (subject v14.3).
# Clone it inside your get_next_line repository, then: ./run.sh [-v|--verbose]

# ═════════════════════════════════ fairy core ═════════════════════════════════
# Identical in libft-fairy, printf-fairy and gnl-fairy: display, steps, the
# checks the three subjects share, and the parallel test runner.

set -u

RED=$'\033[31m' GREEN=$'\033[32m' YELLOW=$'\033[33m' PINK=$'\033[95m'
BOLD=$'\033[1m' DIM=$'\033[2m' RESET=$'\033[0m'
WIDTH=46                        # banner width; every status word ends there
JOBS=$(nproc 2> /dev/null || echo 4)
VERBOSE=0

for arg in "$@"; do
    case $arg in
        -v|--verbose) VERBOSE=1 ;;
        -h|--help)    echo "usage: $0 [-v|--verbose]"; exit 0 ;;
        *)            echo "usage: $0 [-v|--verbose]" >&2; exit 2 ;;
    esac
done

TESTER_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TESTER=$(basename "$TESTER_DIR")
cd "$TESTER_DIR/.." || exit 1
TMP=$(mktemp -d "/tmp/$TESTER.XXXXXX") || exit 1
export FAIRY_STATS=$TMP/stats   # each test binary appends "passed failed warned"
FAILED=0 WARNED=0 FAILED_STEPS="" START=$(date +%s%N)
declare -A PIDS

# ---- display -----------------------------------------------------------------
# box <color> <text> [emoji]: a WIDTH-wide double-line box, text centered
box() {
    local text=$2 w=${#2} bar l
    [[ -n ${3:-} ]] && text+=" $3" w=$((w + 3))
    printf -v bar '%*s' $((WIDTH - 2)) ''
    l=$(( (WIDTH - 2 - w) / 2 ))
    printf '%s╔%s╗\n║%*s%s%*s║\n╚%s╝%s\n' "$1" "${bar// /═}" $l '' "$text" \
        $((WIDTH - 2 - w - l)) '' "${bar// /═}" "$RESET"
}

# step <emoji> <label> opens a step line; ok, fail, warn or note closes it with
# a dot leader and a status word ending in column WIDTH. Details go below.
step() { STEP=$2 STEP_COL=$((${#2} + 4)); printf '%s %s ' "$1" "$2"; }
status() {
    local dots
    printf -v dots '%*s' $((WIDTH - STEP_COL - ${#2} - 1)) ''
    printf '%s%s%s %s%s%s\n' "$DIM" "${dots// /.}" "$RESET" "$1" "$2" "$RESET"
}
ok()   { status "$GREEN" Done; }
fail() { FAILED=1 FAILED_STEPS+="${FAILED_STEPS:+, }$STEP"; status "$RED" Failed; }
warn() { WARNED=1; status "$YELLOW" Warning; }
note() { status "$BOLD" "$1"; }
# verdict <issues...>: fails with one line per issue, or succeeds
verdict() { if [[ $# -eq 0 ]]; then ok; else fail; for i in "$@"; do say "$i"; done; fi; }

say()   { printf '   ↳ %s\n' "$*"; }          # one detail line under a step
quote() { sed 's/^/     /'; }                  # indents what it reads

# section <title>: the header of a block of results, as test binaries print it
section() {
    local line n=$((WIDTH - ${#1} - 4))
    printf -v line '%*s' $((n > 3 ? n : 3)) ''
    printf '\n%s──%s %s%s%s %s%s%s\n' "$DIM" "$RESET" "$BOLD" "$1" "$RESET" "$DIM" "${line// /─}" "$RESET"
}

# ---- end of run: cleanup and verdict -----------------------------------------
clean_project() { :; }
kill_tree() { local c; for c in $(pgrep -P "$1" 2> /dev/null); do kill_tree "$c"; done; kill "$1" 2> /dev/null; }

finish() {
    local rc=$? pid total=0 ko=0 secs
    for pid in $(pgrep -P $$ 2> /dev/null); do kill_tree "$pid"; done
    if [[ $rc -eq 130 ]]; then
        clean_project; rm -rf "$TMP"
        printf '\n%sInterrupted: no verdict.%s\n\n' "$YELLOW" "$RESET"
        return
    fi
    step 🧹 Cleanup; clean_project; ok
    [[ -f $FAIRY_STATS ]] && read -r total ko < <(awk '{p += $1; f += $2} END {print p + f, f + 0}' "$FAIRY_STATS")
    rm -rf "$TMP"
    secs=$(( ($(date +%s%N) - START) / 10000000 ))
    printf -v secs '%d.%02ds' $((secs / 100)) $((secs % 100))
    echo
    if [[ $FAILED -eq 0 ]]; then
        box "$GREEN" "OH MY, YOU PASSED!"
        printf '   %s✔ %d tests passed in %s%s' "$GREEN" "$total" "$secs" "$RESET"
        [[ $WARNED -eq 1 ]] && printf '  %s⚠ warnings above: not failures, worth a look%s' "$YELLOW" "$RESET"
    else
        box "$RED" "OH NO... YOU FAILED!"
        printf '   %s✘ %s' "$RED" "$FAILED_STEPS"
        [[ $ko -gt 0 ]] && printf ' · %d/%d tests failed' "$ko" "$total"
        printf ' · %s%s' "$secs" "$RESET"
    fi
    printf '\n\n'
}
trap finish EXIT
trap 'exit 130' INT TERM

# ---- background jobs ---------------------------------------------------------
# job <name> <cmd...>: runs cmd in the background, at most JOBS at once. Its
# output lands in $TMP/<name>.log, its exit status in $TMP/<name>.rc.
job() {
    local name=$1
    shift
    while [[ $(jobs -rp | wc -l) -ge $JOBS ]]; do wait -n; done
    { "$@" > "$TMP/$name.log" 2>&1; echo $? > "$TMP/$name.rc"; } &
    PIDS[$name]=$!
}
await() { local n; for n in "$@"; do wait "${PIDS[$n]}" 2> /dev/null; done; }
rc() { cat "$TMP/$1.rc" 2> /dev/null || echo 1; }

# ---- checks shared by the three subjects -------------------------------------
in_git() { git rev-parse --is-inside-work-tree > /dev/null 2>&1; }

# sources: the .c and .h files a grader gets. In a git repository that already
# tracks some: the tracked ones (a local main.c is fine). Otherwise all.
sources() {
    local files=""
    in_git && files=$(git ls-files -- '*.c' '*.h' | grep -v "^$TESTER/")
    [[ -z $files ]] && files=$(find . -path "./$TESTER" -prune -o -type f \( -name '*.c' -o -name '*.h' \) -print | sed 's|^\./||' | sort)
    [[ -n $files ]] && printf '%s\n' "$files"
}

# report_untracked <pathspecs...>: warns about the project files git would not
# hand to the grader. Returns 1 when there is none.
report_untracked() {
    local files
    files=$(in_git && git ls-files -o --exclude-standard -- "$@" | grep -v "^$TESTER/" | xargs)
    [[ -z $files ]] && return 1
    warn; say "not committed, hence not graded: $files"
}

# Norminette starts in the background at once: it is the slowest check.
norm_start() { [[ -n $(sources) ]] && job norm norminette $(sources); }
check_norm() {
    step 📝 Norm
    await norm
    if ! command -v norminette > /dev/null; then
        warn; say "norminette not found: the norm was NOT checked"
    elif [[ -z $(sources) ]]; then
        fail; say "no .c or .h file to check"
    elif [[ $(rc norm) -eq 0 ]]; then
        ok
    else
        fail
        grep -Ev 'OK!$|^Setting locale' "$TMP/norm.log" \
            | awk 'NR <= 20; END {if (NR > 20) print "… " NR - 20 " more lines"}' | quote
    fi
}

# check_readme [topic]: README.md as the subjects describe it. A missing one
# fails; its shape only warns, as a pattern cannot read prose.
check_readme() {
    local issues=() section
    step 📖 README
    if [[ ! -f README.md ]]; then
        fail; say "no README.md at the root of the repository"; return
    fi
    head -n 1 README.md | grep -qE '^\s*(\*|_|<i>|<em>)This project has been created as part of the 42 curriculum by ' \
        || issues+=("first line, in italics: *This project has been created as part of the 42 curriculum by <login>*")
    for section in Description Instructions Res+ources; do
        grep -qiE "^\s*(#+|[*_]{1,2}).*$section" README.md || issues+=("no \"${section/s+/s}\" section")
    done
    grep -qiE '\bAI\b|artificial intelligence|chatgpt|claude|copilot|gemini|LLM' README.md \
        || issues+=("Resources: say how AI was used, for which tasks and parts")
    [[ -n ${1:-} ]] && ! grep -qi "$1" README.md && issues+=("no explanation of the $1")
    if [[ ${#issues[@]} -eq 0 ]]; then
        ok
    else
        warn; for section in "${issues[@]}"; do say "$section"; done
    fi
}

# emitted <symbol>: a call the compiler emits on its own (array initializer,
# struct copy, stack protector), never written in the sources (comments aside)
emitted() {
    [[ -f $TMP/code ]] || sources | grep '\.c$' | xargs -r cat \
        | { perl -0777 -pe 's{/\*.*?\*/}{ }gs; s{//[^\n]*}{}g' 2> /dev/null || cat; } > "$TMP/code"
    [[ $1 =~ ^(memset|memcpy|memmove|__stack_chk_fail|_GLOBAL_OFFSET_TABLE_)$ ]] && ! grep -qw -- "$1" "$TMP/code"
}

# ---- test runs ---------------------------------------------------------------
# tests <title> <binary>: a test binary prints the sections that did not pass
# and exits 0, 1 (failure) or 2 (warnings). Anything else is reported here.
tests() {
    local rc
    (timeout --foreground 600 "$2" 2>&1) 2> /dev/null     # no "Segmentation fault" from bash
    rc=$?
    [[ $rc -le 2 ]] && return $rc
    section "$1"
    if [[ $rc -eq 124 ]]; then say "timed out"; else say "crashed: SIG$(kill -l $((rc - 128)) 2> /dev/null)"; fi
    if [[ -n ${LENIENT:-} ]]; then echo "1 0 1" >> "$FAIRY_STATS"; return 2; fi
    echo "0 1 0" >> "$FAIRY_STATS"
    return 1
}

# memcheck <title> <binary> [args...]: runs it under Valgrind, where VG_LEAKS
# are the leak kinds that fail. Prints the report if it found something.
memcheck() {
    local log=$TMP/vg.$BASHPID rc note=""
    if ! command -v valgrind > /dev/null; then
        echo "1 0 1" >> "$FAIRY_STATS"
        section "valgrind · $1"; say "valgrind not found: leaks NOT checked"
        return 2
    fi
    (timeout --foreground 30 valgrind --leak-check=full --show-leak-kinds=all \
        --errors-for-leak-kinds="$VG_LEAKS" --error-exitcode=42 --track-origins=yes \
        --log-file="$log" "${@:2}" > /dev/null 2>&1) 2> /dev/null
    rc=$?
    if [[ $rc -ne 0 ]] && grep -q "very large text, data or bss" "$log"; then
        rc=2 note="Valgrind cannot load it (huge static data): leaks NOT checked"
    elif [[ $rc -eq 0 ]] && grep -qE "(still reachable|possibly lost): [1-9]" "$log"; then
        rc=2 note="memory still held at exit: not a leak for Valgrind, worth freeing"
    elif [[ $rc -eq 124 ]]; then
        rc=1 note="timed out"
    elif [[ $rc -ne 0 && $rc -ne 42 ]]; then
        note="crashed: SIG$(kill -l $((rc - 128)) 2> /dev/null)" rc=1
    elif [[ $rc -eq 42 ]]; then
        rc=1
    fi
    [[ $rc -eq 1 && -n ${LENIENT:-} ]] && rc=2
    echo "$((rc != 1)) $((rc == 1)) $((rc == 2))" >> "$FAIRY_STATS"
    [[ $rc -eq 0 && $VERBOSE -eq 0 ]] && return 0
    section "valgrind · $1"
    [[ -n $note ]] && say "$note"
    # The report without its banner, pid prefixes and hints
    sed -E 's/^==[0-9]+== ?//' "$log" | awk '
        /^If you believe this happened/ {skip = 1} skip {skip = !/stack size used/; next}
        /^(Memcheck,|Copyright|Using Valgrind|Command:|Parent PID:)/ || /rerun with/ {next}
        /^$/ {if (!blank++ && shown) print; next} {blank = 0; shown = 1; print}' | quote
    return $rc
}

# lenient <cmd...>: runs tests or memcheck where a failure only warns
# (outside the subject, or debatable)
lenient() { LENIENT=1 "$@"; }

# queue <list> <name> <cmd...>: starts a run in the background, in a named
# list. run_queue <list> waits for the list, closes the step with the worst
# status and prints the reports in order.
queue() { local list=$1; shift; eval "Q_$list+=(\"\$1\")"; job "$@"; }
run_queue() {
    local -n runs=Q_$1
    local name worst=0 code
    await "${runs[@]}"
    for name in "${runs[@]}"; do
        code=$(rc "$name")
        if [[ $code -eq 2 ]]; then [[ $worst -eq 0 ]] && worst=2; elif [[ $code -ne 0 ]]; then worst=1; fi
    done
    case $worst in 0) ok ;; 2) warn ;; *) fail ;; esac
    for name in "${runs[@]}"; do cat "$TMP/$name.log"; done
}

# ═══════════════════════════════ end of core ══════════════════════════════════

BUFFER_SIZES="1 42 9999 10000000"
VG_SIZES="1 42 9999"
EDGE_SIZES="0 -1"       # outside the subject: what goes wrong there only warns
VG_LEAKS=definite,indirect  # memory held at exit after the end only warns
MANDATORY="get_next_line.c get_next_line_utils.c get_next_line.h"
BONUS_FILES="get_next_line_bonus.c get_next_line_utils_bonus.c get_next_line_bonus.h"

echo; box "$PINK" "GNL-FAIRY" 🧚; echo
norm_start

# AddressSanitizer catches overflows in every functional test, when the
# toolchain has it; never at BUFFER_SIZE 10000000, whose huge static arrays
# can leave no room for its shadow memory. Leaks stay Valgrind's job.
printf 'int main(void){return 0;}\n' > "$TMP/probe.c"
job asan sh -c "cc -fsanitize=address '$TMP/probe.c' -o '$TMP/probe' && '$TMP/probe'"
export ASAN_OPTIONS=detect_leaks=0

step 🔖 Version
BONUS=0
[[ -f get_next_line_bonus.c ]] && BONUS=1
if [[ $BONUS -eq 1 ]]; then note Bonus; else note Mandatory; fi

# The subject's files at the root, and no other .c or .h
step 📂 Files
EXPECTED=$MANDATORY
[[ $BONUS -eq 1 ]] && EXPECTED+=" $BONUS_FILES"
issues=()
for f in $EXPECTED; do [[ -f $f ]] || issues+=("missing at the root: $f"); done
for f in $(sources); do [[ " $EXPECTED " == *" $f "* ]] || issues+=("not in the subject's turn-in list: $f"); done
if [[ ${#issues[@]} -gt 0 ]]; then verdict "${issues[@]}"; elif ! report_untracked $EXPECTED README.md; then ok; fi
for f in $MANDATORY; do [[ -f $f ]] || exit 1; done

check_readme "algorithm"
check_norm

# label <step>: the step's label in the current part ("Bonus prototype")
label() { if [[ -n $pfx ]]; then echo "$pfx${1,}"; else echo "$1"; fi; }

# compile <tag> <src> [BUFFER_SIZE]: one object, as graded
compile() { cc -Wall -Wextra -Werror ${3:+-D BUFFER_SIZE=$3} -I. -c "$2" -o "$TMP/$1${2%.c}${3:+_bs$3}.o"; }

# check_part <prefix> <header> <sources...>: the static checks of one part
check_part() {
    local pfx=$1 hdr=$2 tag=${1:0:1} src bs sym objs="" issues=() globals statics
    shift 2

    # The prototype as the compiler sees it: undeclared or conflicting fails
    step 📋 "$(label Prototype)"
    if printf '#include <stdlib.h>\n#include "%s"\nchar *(*fairy)(int) = &get_next_line;\nchar *get_next_line(int fd);\n' \
            "$hdr" | cc -fsyntax-only -I. -x c - 2> "$TMP/proto.err"; then
        ok
    else
        fail; say "$hdr must declare: char *get_next_line(int fd);"
        grep -m 3 'error' "$TMP/proto.err" | quote
    fi

    # Graded with -Wall -Wextra -Werror, with and without -D BUFFER_SIZE
    step 🧱 "$(label Compilation)"
    for src in "$@"; do
        objs+=" $TMP/$tag${src%.c}.o"
        for bs in "" $BUFFER_SIZES; do job "cc_$tag$src$bs" compile "$tag" "$src" "$bs"; done
    done
    for src in "$@"; do
        for bs in "" $BUFFER_SIZES; do
            await "cc_$tag$src$bs"
            [[ $(rc "cc_$tag$src$bs") -eq 0 ]] && continue
            issues+=("$src does not compile ${bs:+with -D BUFFER_SIZE=$bs}${bs:-without -D BUFFER_SIZE (no default?)}")
            head -n 6 "$TMP/cc_$tag$src$bs.log" > "$TMP/cc.err"
        done
    done
    verdict "${issues[@]}"
    [[ ${#issues[@]} -eq 0 ]] || { quote < "$TMP/cc.err"; exit 1; }

    # Allowed: read, malloc, free, and what the part defines itself
    step 🔍 "$(label "External functions")"
    issues=()
    for sym in $(nm -u $objs | awk '{print $2}' | sort -u); do
        nm --defined-only $objs | awk '{print $3}' | grep -qx -- "$sym" && continue
        [[ $sym =~ ^(read|malloc|free)$ ]] || emitted "$sym" || issues+=("forbidden: $sym")
    done
    verdict "${issues[@]}"

    # Data symbols: uppercase is a global, forbidden. Lowercase without a '.'
    # is a static outside any function: a global for most evaluators, not all,
    # so a warning. Function-local statics (func.var for clang, var.N for gcc)
    # are what the subject expects.
    step 🌍 "$(label "Global variables")"
    globals=$(nm $objs | awk '$2 ~ /^[BCDGS]$/ {print $3}' | xargs)
    statics=$(nm $objs | awk '$2 ~ /^[bdgs]$/ && $3 !~ /\./ {print $3}' | xargs)
    if [[ -n $globals ]]; then
        fail; say "global variables are forbidden: $globals"
    elif [[ -n $statics ]]; then
        warn; say "static variables declared outside any function: $statics"
    else
        ok
    fi

    # The bonus: a single static variable (const tables aside, in .rodata)
    [[ -z $pfx ]] && return
    step 🧮 "$(label "Single static")"
    statics=$(nm $objs | awk '$2 ~ /^[bdgs]$/ {print $3}' | xargs)
    if [[ $(wc -w <<< "$statics") -le 1 ]]; then ok; else fail; say "one static variable allowed, found: $statics"; fi
}

# build <binary> <test file> <BUFFER_SIZE> <sources> [flags...]
build() {
    cc -g -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-result -DVERBOSE=$VERBOSE \
        -DBUFFER_SIZE="$3" "${@:5}" -I. "$TESTER/$2" "$TESTER/utils.c" $4 \
        -Wl,--wrap=malloc,--wrap=read -o "$1"
}

# asan_tests <title> <binary> <build args...>: runs a functional test binary;
# if AddressSanitizer cannot start (huge static arrays), rebuilds it without
asan_tests() {
    local title=$1 bin=$2 rc
    shift 2
    tests "$title" "$bin" > "$bin.out"
    rc=$?
    if [[ $rc -ne 0 ]] && grep -qE "ASan cannot proceed|ReserveShadowMemoryRange" "$bin.out"; then
        build "$bin" "$@" > /dev/null 2>&1 && { tests "$title" "$bin" > "$bin.out"; rc=$?; }
    fi
    cat "$bin.out"
    return $rc
}

# test_part <prefix> <list> <functional tests> <leak tests> <sources...>:
# builds every test binary in parallel, then queues their runs
test_part() {
    local pfx=$1 list=$2 basics=$3 leaks=$4 tag=${1:0:1} srcs="${*:5}" t bs bin kind asan builds=() issues=()
    step 🔨 "$(label "Test build")"
    await asan
    for t in $basics; do for bs in $BUFFER_SIZES; do builds+=("basic:$t:$bs"); done; done
    for bs in $EDGE_SIZES; do builds+=("edge:edge_tests.c:$bs"); done
    for t in $leaks; do for bs in $VG_SIZES; do builds+=("leak:$t:$bs"); done; done
    for b in "${builds[@]}"; do
        IFS=: read -r kind t bs <<< "$b"
        asan=""
        [[ $kind != leak && $bs -le 9999 && $(rc asan) -eq 0 ]] && asan="-fsanitize=address -fno-omit-frame-pointer"
        job "b_$tag${t%.c}$bs" build "$TMP/$tag${t%.c}_bs$bs" "$t" "$bs" "$srcs" $asan
    done
    for b in "${builds[@]}"; do
        IFS=: read -r kind t bs <<< "$b"
        await "b_$tag${t%.c}$bs"
        [[ $(rc "b_$tag${t%.c}$bs") -eq 0 || $kind == edge ]] && continue
        issues+=("$t does not build with BUFFER_SIZE=$bs")
        head -n 8 "$TMP/b_$tag${t%.c}$bs.log" > "$TMP/build.err"
    done
    verdict "${issues[@]}"
    [[ ${#issues[@]} -eq 0 ]] || { quote < "$TMP/build.err"; exit 1; }

    for b in "${builds[@]}"; do
        IFS=: read -r kind t bs <<< "$b"
        bin="$TMP/$tag${t%.c}_bs$bs"
        case $kind in
            basic) queue "$list" "r_$tag${t%.c}$bs" asan_tests "${t%.c}" "$bin" "$t" "$bs" "$srcs" ;;
            edge)  [[ -x $bin ]] && queue "$list" "r_${tag}edge$bs" lenient asan_tests "edge_tests" "$bin" "$t" "$bs" "$srcs" ;;
            leak)  queue "$list" "r_$tag${t%.c}$bs" memcheck "${t%.c} · BUFFER_SIZE=$bs" "$bin" ;;
        esac
    done
}

check_part "" get_next_line.h get_next_line.c get_next_line_utils.c
test_part "" main "basic_tests.c" "leak_tests.c" get_next_line.c get_next_line_utils.c

if [[ $BONUS -eq 1 ]]; then
    for f in $BONUS_FILES; do [[ -f $f ]] || exit 1; done
    check_part "Bonus " get_next_line_bonus.h get_next_line_bonus.c get_next_line_utils_bonus.c
    test_part "Bonus " bonus "basic_tests.c basic_tests_bonus.c" "leak_tests.c leak_tests_bonus.c" \
        get_next_line_bonus.c get_next_line_utils_bonus.c
fi

step 🧪 Tests
run_queue main
if [[ $BONUS -eq 1 ]]; then
    step 🧪 "Bonus tests"
    run_queue bonus
fi
exit $FAILED
