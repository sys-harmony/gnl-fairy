#include "gnl_fairy.h"
#include <signal.h>
#include <stdarg.h>
#include <sys/wait.h>

int		g_malloc_wrap_enabled = 0;
int		g_malloc_count;
int		g_malloc_fail_at;
int		g_read_fail_fd = -1;
int		g_read_short = 0;
size_t	g_read_total = 0;
size_t	g_read_bad_size = 0;
int		g_tests_failed = 0;
int		g_tests_warned = 0;

static const char	*g_test_name = "";
static int			g_warned = 0;

void	*__wrap_malloc(size_t size) {
	if (g_malloc_wrap_enabled && ++g_malloc_count == g_malloc_fail_at)
		return NULL;
	return __real_malloc(size);
}

/*
 * Besides forcing errors on one fd, the wrapper can deliver data in small
 * pieces (1 to 5 bytes) like a pipe or a terminal does, and records what
 * get_next_line asks for: any size other than BUFFER_SIZE (0, a common fd
 * probe, aside) and the bytes consumed, for the subject rules test.
 */
ssize_t	__wrap_read(int fd, void *buf, size_t count) {
	static size_t	calls;
	ssize_t			ret;

	if (count != (size_t)BUFFER_SIZE && count != 0)
		g_read_bad_size = count;
	if (fd == g_read_fail_fd)
		return -1;
	if (g_read_short && count > 1 + calls % 5)
		count = 1 + calls % 5;
	calls++;
	ret = __real_read(fd, buf, count);
	if (ret > 0)
		g_read_total += ret;
	return ret;
}

void	error_exit(char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

static void	vreport(const char *tag, const char *fmt, va_list ap) {
	fprintf(stderr, "  %s %s: ", tag, g_test_name);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
}

/* Prints why the running test failed, tagged with its name. Returns 1. */
int	gnl_fail(const char *fmt, ...) {
	va_list	ap;

	va_start(ap, fmt);
	vreport("↳", fmt, ap);
	va_end(ap);
	return 1;
}

/*
 * Reports a debatable behaviour (not a bug, but an evaluator may ask about
 * it) without failing the test. Returns 0.
 */
int	gnl_warn(const char *fmt, ...) {
	va_list	ap;

	va_start(ap, fmt);
	vreport("⚠", fmt, ap);
	va_end(ap);
	g_warned = 1;
	return 0;
}

/* ************************************************************************** */
/*                            GNL-specific helpers                            */
/* ************************************************************************** */

/*
 * Writes `content` to a fresh temp file and returns a read-only fd on it. The
 * file is unlinked at once: it lives as long as the fd, so nothing is left in
 * /tmp even when a test crashes or times out. The reference output of
 * get_next_line does not depend on BUFFER_SIZE, so the same contents validate
 * every buffer size.
 */
int	gnl_tmp_open(const char *content) {
	char	path[] = "/tmp/gnl_fairy_XXXXXX";
	int		fd;
	size_t	len;

	fd = mkstemp(path);
	if (fd < 0)
		error_exit("gnl-fairy: mkstemp failed");
	len = strlen(content);
	if (write(fd, content, len) != (ssize_t)len)
		error_exit("gnl-fairy: write failed");
	close(fd);
	fd = open(path, O_RDONLY);
	if (fd < 0)
		error_exit("gnl-fairy: open failed");
	unlink(path);
	return fd;
}

/*
 * Returns the next expected line as a freshly malloc'd string: every line
 * keeps its trailing '\n', except the final segment when `content` does not
 * end with one. Returns NULL once the cursor reached the end. Advances cursor.
 */
char	*gnl_expected_next(const char **cursor) {
	const char	*s = *cursor;
	const char	*nl;
	const char	*end;
	size_t		n;
	char		*line;

	if (!*s)
		return NULL;
	nl = strchr(s, '\n');
	if (nl)
		end = nl + 1;
	else
		end = s + strlen(s);
	n = (size_t)(end - s);
	line = malloc(n + 1);
	if (!line)
		error_exit("gnl-fairy: malloc failed");
	memcpy(line, s, n);
	line[n] = '\0';
	*cursor = end;
	return line;
}

/* Quotes `s` into `out` (96 bytes) with '\n' escaped, cut after 32 chars. */
static const char	*show(const char *s, char *out) {
	size_t	i;
	size_t	o = 0;

	if (!s)
		return "NULL";
	out[o++] = '"';
	for (i = 0; s[i] && i < 32; i++) {
		if (s[i] == '\n')
			out[o++] = '\\';
		out[o++] = s[i] == '\n' ? 'n' : s[i];
	}
	out[o++] = '"';
	out[o] = '\0';
	if (s[i])
		snprintf(out + o, 96 - o, "... (%zu chars)", strlen(s));
	return out;
}

/*
 * Reads `fd` line by line with get_next_line and compares each line to the
 * reference split of `content`, then asserts that get_next_line returns NULL
 * twice at EOF (idempotent end-of-file). Returns 0 on success, 1 on the first
 * divergence (reason printed on stderr).
 */
int	gnl_check_fd(int fd, const char *content) {
	const char	*cursor = content;
	char		*expected;
	char		*got;
	char		b1[96];
	char		b2[96];
	int			nulls = 0;
	int			ko = 0;

	for (int n = 1; !ko && nulls < 2; n++) {
		expected = gnl_expected_next(&cursor);
		nulls += !expected;
		got = get_next_line(fd);
		if (expected ? !got || strcmp(got, expected) : got != NULL)
			ko = gnl_fail("line %d: expected %s, got %s", n,
					show(expected, b1), show(got, b2));
		free(got);
		free(expected);
	}
	return ko;
}

/* Same as gnl_check_fd, on a temp file holding `content`. */
int	gnl_check_content(const char *content) {
	int	fd = gnl_tmp_open(content);
	int	ko = gnl_check_fd(fd, content);

	close(fd);
	return ko;
}

/*
 * Reads and frees lines until NULL, so a correct get_next_line releases its
 * stash. Capped: a get_next_line that never returns NULL (the functional tests
 * report it) must not keep Valgrind spinning until its timeout.
 */
void	gnl_drain(int fd) {
	char	*line;

	for (int n = 0; n < 1000 && (line = get_next_line(fd)) != NULL; n++)
		free(line);
}

/* ************************************************************************** */
/*                           forked test runner                               */
/* ************************************************************************** */

/*
 * Runs one test in a child process under a timeout, so a crash or an infinite
 * loop only fails that test. Returns 0 (pass), 1 (failure) or 2 (passed with
 * warnings). Crashes and timeouts are reported here; a wrong result is reported
 * by the test, which then aborts.
 */
static int	forked_test(const t_test *t) {
	pid_t	pid;
	int		status;
	int		sig;

	g_test_name = t->name;
	fflush(stdout);
	pid = fork();
	if (pid == -1)
		error_exit("gnl-fairy: fork failed");
	if (!pid) {
		alarm(GNL_TIMEOUT);
		if (t->fn)
			t->fn();
		else if (gnl_check_content(t->content))
			abort();
		exit(g_warned ? 2 : EXIT_SUCCESS);
	}
	if (waitpid(pid, &status, 0) == -1)
		error_exit("gnl-fairy: waitpid failed");
	if (!WIFSIGNALED(status))
		return WEXITSTATUS(status) == 2 ? 2 : WEXITSTATUS(status) != 0;
	sig = WTERMSIG(status);
	if (sig == SIGALRM)
		gnl_fail("timeout after %ds (infinite loop?)", GNL_TIMEOUT);
	else if (sig != SIGABRT)
		gnl_fail("crashed (%s)", strsignal(sig));
	return 1;
}

/* Runs a group of tests; prints it if one failed or warned (or in verbose mode). */
void	run_group(const char *title, const t_test *tests, size_t n) {
	int		res[n];
	int		fails = 0;
	int		warns = 0;

	for (size_t i = 0; i < n; i++) {
		res[i] = forked_test(&tests[i]);
		fails += res[i] == 1;
		warns += res[i] == 2;
	}
	g_tests_failed += fails;
	g_tests_warned += warns;
	if (!fails && !warns && !VERBOSE)
		return ;
	printf("\n========================================\n");
	printf("%s\n", title);
	printf("========================================\n");
	for (size_t i = 0; i < n; i++)
		printf("%s" RESET "  Test %s%s\n", res[i] == 1 ? RED "[KO]" : res[i] == 2
			? YELLOW "[OK]" : GREEN "[OK]", tests[i].name, res[i] == 2 ? " (warning)" : "");
}
