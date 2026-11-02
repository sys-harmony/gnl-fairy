#include "gnl_fairy.h"

/*
 * Each scenario writes known content to a temp file and reads it back line by
 * line against the reference split (see gnl_check_fd). The expected lines do
 * not depend on BUFFER_SIZE, so run.sh builds this file for 1, 42, 9999 and
 * 10000000 and runs it each time. A test given a content only is checked as
 * is. What is debatable only warns: a working get_next_line never fails here.
 */

/* ************************************************************************** */
/*                           tiny and basic inputs                            */
/* ************************************************************************** */

static const t_test	g_basic[] = {
	{"empty file", .content = ""},
	{"one char, no newline", .content = "a"},
	{"one char + newline", .content = "a\n"},
	{"lone newline", .content = "\n"},
	{"three lines + trailing newline", .content = "line1\nline2\nline3\n"},
	{"three lines, no trailing newline", .content = "line1\nline2\nline3"},
	{"only empty lines", .content = "\n\n\n"},
	{"mixed empty and filled lines", .content = "a\n\nb\n\n\nc"},
	{"spaces and tabs preserved", .content = "  spaced  \n\ttabbed\t\n   "},
	{"CRLF line endings kept as is", .content = "dos\r\nline\r\n"},
	{"lines of increasing length", .content = "a\nbb\nccc\ndddd\neeeee\nffffff\n"},
};

/* ************************************************************************** */
/*                            long and many lines                             */
/* ************************************************************************** */

static void	check_repeat(char c, size_t n, int trailing_nl) {
	gnl_check_content(make_repeat(c, n, trailing_nl));
}

static void	long_line_no_nl_test(void) {
	check_repeat('x', 5000, 0);
}

static void	long_line_nl_test(void) {
	check_repeat('y', 5000, 1);
}

static void	giant_line_test(void) {
	check_repeat('z', 20000, 1);
}

/*
 * Lines of BUFFER_SIZE - 1, BUFFER_SIZE and BUFFER_SIZE + 1 chars, then
 * nothing, a newline, or a newline and more: the end of a line falls just
 * before, on and just after the end of a read() chunk. Not above 100000:
 * longer lines reach no new path, but cost hours to an implementation that
 * reads less than BUFFER_SIZE at once.
 */
static void	boundaries_test(void) {
	const char	*tails[] = {"", "\n", "\nafter\n"};
	char		*s;

	if (BUFFER_SIZE > 100000)
		return ;
	for (long n = BUFFER_SIZE - 1; n <= BUFFER_SIZE + 1; n++) {
		for (size_t t = 0; t < ARRAY_SIZE(tails); t++) {
			s = make_repeat('b', n + strlen(tails[t]), 0);
			strcpy(s + n, tails[t]);
			gnl_check_content(s);
			free(s);
		}
	}
}

/* Fewer lines with a huge BUFFER_SIZE: a valid call may clear a 10 MB buffer. */
static void	many_lines_test(void) {
	size_t	count = BUFFER_SIZE <= 9999 ? 1000 : 100;
	char	*s = malloc(6 * count + 1);

	if (!s)
		die("gnl-fairy: malloc");
	for (size_t i = 0; i < count; i++)
		memcpy(s + 6 * i, "hello\n", 6);
	s[6 * count] = '\0';
	gnl_check_content(s);
}

static const t_test	g_long[] = {
	{"5000-char line, no newline", long_line_no_nl_test},
	{"5000-char line + newline", long_line_nl_test},
	{"20000-char line", giant_line_test},
	{"lines of BUFFER_SIZE - 1, BUFFER_SIZE, + 1 chars", boundaries_test},
	{"1000 short lines", many_lines_test},
};

/* ************************************************************************** */
/*                              file descriptors                              */
/* ************************************************************************** */

static void	expect_null(int fd, const char *what) {
	char	*got = get_next_line(fd);
	char	b[96];

	if (got)
		fail("returned %s on %s, expected NULL", gnl_show(got, b), what);
}

static void	bad_fds_test(void) {
	int	fd = open("/dev/null", O_RDONLY);

	close(fd);
	expect_null(-1, "fd -1");
	expect_null(fd, "a closed fd");
	expect_null(INT_MAX, "fd INT_MAX (a per-fd array indexed without bound check crashes)");
}

/* read() fails on a directory (EISDIR): an error, hence NULL. */
static void	empty_sources_test(void) {
	expect_null(open(".", O_RDONLY), "a directory");
	expect_null(open("/dev/null", O_RDONLY), "/dev/null");
}

/*
 * fd 1023 is valid under the usual limit of 1024 fds, but a per-fd array
 * smaller than that cannot serve it: a limitation more than a bug.
 */
static void	high_fd_test(void) {
	const char	*content = "high\nfd\n";
	char		*line;

	if (dup2(gnl_tmp_open(content), 1023) < 0)
		return ;
	line = get_next_line(1023);
	if (!line) {
		warning("returned NULL on fd 1023, a valid fd: is a per-fd array too small?");
		return ;
	}
	if (strcmp(line, "high\n"))
		fail("wrong first line on fd 1023");
	gnl_check_fd(1023, content + 5);
}

/*
 * stdin is usually a pipe or a terminal: read() returns what is there, often
 * less than BUFFER_SIZE, long before the end. g_read_short emulates that: an
 * implementation that takes a short read for the end cuts its lines.
 */
static void	stdin_test(void) {
	const char	*content = "from\nstdin\n\nin small\npieces";
	int			p[2];

	if (pipe(p) < 0 || write(p[1], content, strlen(content)) < 0)
		die("gnl-fairy: pipe");
	close(p[1]);
	dup2(p[0], STDIN_FILENO);
	g_read_short = 1;
	gnl_check_fd(STDIN_FILENO, content);
}

static const t_test	g_fd[] = {
	{"fd -1, closed, INT_MAX: NULL", bad_fds_test},
	{"directory, /dev/null: NULL", empty_sources_test},
	{"high valid fd (1023)", high_fd_test},
	{"stdin as a pipe, data arriving in pieces", stdin_test},
};

/* ************************************************************************** */
/*                     state reset, read errors, read rules                   */
/* ************************************************************************** */

/* After the end, the state is clean: a new file on the same fd number. */
static void	reuse_after_eof_test(void) {
	gnl_check_content("first\nfile");
	gnl_check_content("second\nfile\n");
}

/*
 * read() fails while a partial line sits in the stash. The subject asks for
 * NULL there, and the stash should go, so that the same fd number reopened on
 * the same file starts over: both debatable, so they only warn. A crash, a
 * hang, or anything but the file (stale data aside) after reopening fails.
 */
static void	read_error_test(void) {
	char	*content = make_repeat('x', 20005, 1);
	char	*line;
	char	b[96];
	size_t	len;
	int		fd;
	int		stale = 0;

	memcpy(content, "head\n", 5);
	fd = gnl_tmp_open(content);
	line = get_next_line(fd);
	if (!line || strcmp(line, "head\n"))
		fail("first line, before any read() error: expected \"head\\n\", got %s", gnl_show(line, b));
	g_read_fail_fd = fd;
	while ((line = get_next_line(fd)) != NULL)
		if (!strchr(line, '\n') && !stale++)
			warning("returned a partial line after read() failed (the subject asks for NULL on error)");
	stale = 0;
	g_read_fail_fd = -1;
	close(fd);
	fd = gnl_tmp_open(content);
	line = get_next_line(fd);
	if (line && !strcmp(line, content + 5)) {
		stale = 1;
		line = get_next_line(fd);
	}
	len = line ? strlen(line) : 0;
	if (len > 5 && strspn(line, "x") == len - 5 && !strcmp(line + len - 5, "head\n"))
		stale = 1;
	else if (!line || strcmp(line, "head\n"))
		fail("first line after reopening the file: expected \"head\\n\", got %s", gnl_show(line, b));
	if (stale)
		warning("kept its stash after a read() error: the next file on that fd starts with stale data");
	gnl_check_fd(fd, content + 5);
}

/*
 * The subject's rules seen through the read() wrapper, warnings only: read()
 * gets BUFFER_SIZE, and the first line comes back without the whole file read
 * ahead. 24000 bytes, so that reading ahead shows up to BUFFER_SIZE 9999.
 */
static void	read_rules_test(void) {
	size_t	count = BUFFER_SIZE <= 9999 ? 4000 : 10;
	char	*content = malloc(6 * count + 1);
	char	*line;
	int		fd;

	if (!content)
		die("gnl-fairy: malloc");
	for (size_t i = 0; i < count; i++)
		memcpy(content + 6 * i, "hello\n", 6);
	content[6 * count] = '\0';
	fd = gnl_tmp_open(content);
	g_read_total = 0;
	line = get_next_line(fd);
	if (line && g_read_total >= strlen(line) + BUFFER_SIZE)
		warning("read %zu bytes to return a %zu-char first line: the subject says "
			"not to read the whole file at once", g_read_total, strlen(line));
	gnl_drain(fd);
	if (g_read_bad_size)
		warning("read() called with a size of %zu instead of BUFFER_SIZE (%d)",
			g_read_bad_size, BUFFER_SIZE);
}

static const t_test	g_state[] = {
	{"new file after the end (same fd number)", reuse_after_eof_test},
	{"read() error mid-line, then reopen", read_error_test},
	{"read() size and reading ahead (subject rules)", read_rules_test},
};

int	main(void) {
	RUN_GROUP("lines", g_basic);
	RUN_GROUP("long lines", g_long);
	RUN_GROUP("fds", g_fd);
	RUN_GROUP("state & read()", g_state);
	return fairy_summary();
}
