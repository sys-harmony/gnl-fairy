#include "gnl_fairy.h"

int		g_malloc_wrap_enabled = 0;
int		g_malloc_count;
int		g_malloc_fail_at;
int		g_read_fail_fd = -1;
int		g_read_short = 0;
size_t	g_read_total = 0;
size_t	g_read_bad_size = 0;

/* With -Wl,--wrap=malloc, the n-th malloc (g_malloc_fail_at) returns NULL. */
void	*__wrap_malloc(size_t size) {
	if (g_malloc_wrap_enabled && ++g_malloc_count == g_malloc_fail_at)
		return NULL;
	return __real_malloc(size);
}

/*
 * With -Wl,--wrap=read: fails on g_read_fail_fd; with g_read_short, delivers
 * 1 to 5 bytes at a time like a pipe or a terminal; records the bytes read
 * and any size asked other than BUFFER_SIZE (0, a common fd probe, aside).
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

/* ************************************************************************** */
/*                          files and expected lines                          */
/* ************************************************************************** */

/*
 * Returns a read-only fd on a fresh temp file holding content. The file is
 * unlinked at once: it lives as long as the fd, nothing is left in /tmp even
 * when a test crashes or hangs.
 */
int	gnl_tmp_open(const char *content) {
	char	path[] = "/tmp/gnl_fairy_XXXXXX";
	size_t	len = strlen(content);
	int		fd = mkstemp(path);

	if (fd < 0 || write(fd, content, len) != (ssize_t)len)
		die("gnl-fairy: temp file");
	close(fd);
	fd = open(path, O_RDONLY);
	if (fd < 0)
		die("gnl-fairy: temp file");
	unlink(path);
	return fd;
}

char	*make_repeat(char c, size_t n, int trailing_nl) {
	char	*s = malloc(n + 2);

	if (!s)
		die("gnl-fairy: malloc");
	memset(s, c, n);
	s[n] = '\n';
	s[n + !!trailing_nl] = '\0';
	return s;
}

/*
 * The reference: the next line of *cursor as a malloc'd string, with its
 * '\n' unless it ends the content without one. NULL at the end.
 */
char	*gnl_expected_next(const char **cursor) {
	const char	*nl = strchr(*cursor, '\n');
	size_t		n = nl ? (size_t)(nl + 1 - *cursor) : strlen(*cursor);
	char		*line;

	if (!n)
		return NULL;
	line = strndup(*cursor, n);
	if (!line)
		die("gnl-fairy: malloc");
	*cursor += n;
	return line;
}

/* Quotes s into out (96 bytes), '\n' escaped, cut after 32 characters. */
const char	*gnl_show(const char *s, char *out) {
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
		snprintf(out + o, 96 - o, "… (%zu chars)", strlen(s));
	return out;
}

/*
 * Reads fd to the end with get_next_line, comparing each line with the
 * reference split of content, then expects NULL twice: a finished fd stays
 * finished. Fails on the first difference.
 */
void	gnl_check_fd(int fd, const char *content) {
	char	*expected;
	char	*got;
	char	b1[96];
	char	b2[96];

	for (int n = 1, nulls = 0; nulls < 2; n++) {
		expected = gnl_expected_next(&content);
		nulls += !expected;
		got = get_next_line(fd);
		if (expected ? !got || strcmp(got, expected) : got != NULL)
			fail("line %d: expected %s, got %s", n, gnl_show(expected, b1), gnl_show(got, b2));
		free(got);
		free(expected);
	}
}

void	gnl_check_content(const char *content) {
	int	fd = gnl_tmp_open(content);

	gnl_check_fd(fd, content);
	close(fd);
}

/*
 * Reads and frees lines until NULL, so that a correct get_next_line frees its
 * stash. Capped: one that never returns NULL (a failure the functional tests
 * report) must not keep Valgrind spinning.
 */
void	gnl_drain(int fd) {
	char	*line;

	for (int n = 0; n < 1000 && (line = get_next_line(fd)) != NULL; n++)
		free(line);
}

static void	run_test(const t_test *t) {
	if (t->fn)
		t->fn();
	else
		gnl_check_content(t->content);
}

/* ═══════════════════════════════ fairy core ═══════════════════════════════ */
/* Identical in libft-fairy, printf-fairy and gnl-fairy: every test runs in a */
/* child process, so a crash or an infinite loop only fails that test. What   */
/* the child writes (fail and warning lines, sanitizer reports, glibc's own   */
/* "double free" messages) is shown under the test when it did not pass.      */

static int	g_count[3];	/* tests passed, failed, passed with warnings */
static int	g_warned;	/* in the child: warning() was called */
static int	g_timeout = TEST_TIMEOUT;	/* 1 second once a test hung */
int			g_lenient = 0;	/* outside the subject: a failure only warns */

void	die(const char *what) {
	perror(what);
	exit(EXIT_FAILURE);
}

static void	vreport(const char *tag, const char *fmt, va_list ap) {
	fprintf(stderr, "%s ", tag);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
}

/* Fails the running test: reason shown under it. */
void	fail(const char *fmt, ...) {
	va_list	ap;

	va_start(ap, fmt);
	vreport("↳", fmt, ap);
	va_end(ap);
	_exit(1);
}

/* Flags something debatable: the test still passes, the reason is shown. */
void	warning(const char *fmt, ...) {
	va_list	ap;

	va_start(ap, fmt);
	vreport("⚠", fmt, ap);
	va_end(ap);
	g_warned = 1;
}

/* Runs one test in a child: 0 passed, 1 failed, 2 warned. Its output in log. */
static int	fork_test(const t_test *t, char *log, size_t size) {
	static int	fd = -1;
	pid_t		pid;
	int			status;
	ssize_t		n;
	size_t		len;

	if (fd < 0) {
		FILE	*f = tmpfile();

		if (!f)
			die("fairy: tmpfile");
		fd = fileno(f);
	}
	if (ftruncate(fd, 0) < 0 || lseek(fd, 0, SEEK_SET) < 0)
		die("fairy: tmpfile");
	fflush(NULL);
	pid = fork();
	if (pid < 0)
		die("fairy: fork");
	if (!pid) {
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		alarm(g_timeout);
		run_test(t);
		_exit(g_warned ? 2 : 0);
	}
	if (waitpid(pid, &status, 0) < 0)
		die("fairy: waitpid");
	n = pread(fd, log, size - 1, 0);
	len = n > 0 ? (size_t)n : 0;
	log[len] = '\0';
	if (WIFEXITED(status))
		return WEXITSTATUS(status) == 2 ? 2 : WEXITSTATUS(status) != 0;
	if (WTERMSIG(status) != SIGALRM)
		snprintf(log + len, size - len, "↳ crashed: %s\n", strsignal(WTERMSIG(status)));
	else {
		snprintf(log + len, size - len, "↳ timeout after %ds (infinite loop?)%s\n", g_timeout,
			g_timeout == TEST_TIMEOUT ? "" : ", cut short since a test hung");
		g_timeout = 1;
	}
	return 1;
}

void	section(const char *title) {
	int	n = WIDTH - 4 - (int)strlen(title);

	printf("\n" DIM "──" RESET " " BOLD "%s" RESET " " DIM, title);
	for (int i = 0; i < (n > 3 ? n : 3); i++)
		fputs("─", stdout);
	puts(RESET);
}

/* Prints a test's output indented, without the "==pid==" prefixes, the "====="
 * rulers and the shadow memory map of sanitizer reports. */
static void	print_log(const char *log) {
	const char	*end;
	const char	*p;

	for (; *log; log = *end ? end + 1 : end) {
		end = strchr(log, '\n');
		if (!end)
			end = log + strlen(log);
		p = log + strspn(log, "=");
		if (p == end)
			continue ;
		if (p - log == 2 && isdigit(*p) && *(p += strspn(p, "0123456789")) == '=' && p[1] == '=')
			log = p + 2 + (p[2] == ' ');
		if (!strncmp(log, "Shadow bytes around", 19))
			return ;
		printf("     %.*s\n", (int)(end - log), log);
	}
}

/* Runs a group of tests; prints those that did not pass (all in VERBOSE). */
void	run_group(const char *title, const t_test *tests, size_t n) {
	static const char	*tags[] = {GREEN "[OK]", RED "[KO]", YELLOW "[OK]", YELLOW "[KO]"};
	static char			log[16384];
	int					shown = 0;
	int					res;

	for (size_t i = 0; i < n; i++) {
		res = fork_test(&tests[i], log, sizeof(log));
		if (res == 1 && g_lenient)
			res = 3;
		g_count[res == 3 ? 2 : res]++;
		if (res == 0 && !VERBOSE)
			continue ;
		if (!shown++)
			section(title);
		printf("%s" RESET " %s\n", tags[res], tests[i].name);
		print_log(log);
	}
}

/* Hands the counts to run.sh; returns the binary's exit status: 0, 1 or 2. */
int	fairy_summary(void) {
	const char	*path = getenv("FAIRY_STATS");
	FILE		*f = path ? fopen(path, "a") : NULL;

	if (f) {
		fprintf(f, "%d %d %d\n", g_count[0] + g_count[2], g_count[1], g_count[2]);
		fclose(f);
	}
	return g_count[1] ? 1 : g_count[2] ? 2 : 0;
}

/* ══════════════════════════════ end of core ═══════════════════════════════ */
