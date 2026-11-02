#include "gnl_fairy.h"

/*
 * Every scenario writes a known buffer to a temp file and reads it back line
 * by line, comparing against the reference split (see gnl_check_fd). The
 * expected output is identical for every BUFFER_SIZE, so run.sh recompiles
 * this file for several sizes (1, 42, 9999, 10000000) and runs it each time.
 * A test given only a `content` is checked as is; the others run a function.
 * Debatable behaviours only warn (gnl_warn): a working get_next_line never
 * fails here.
 */

/* ************************************************************************** */
/*                              tiny inputs                                   */
/* ************************************************************************** */

static const t_test	g_tiny[] = {
	{"empty file", .content = ""},
	{"one char, no newline", .content = "a"},
	{"one char + newline", .content = "a\n"},
	{"lone newline", .content = "\n"},
};

/* ************************************************************************** */
/*                            basic multi-line                                */
/* ************************************************************************** */

static const t_test	g_basic[] = {
	{"three lines + trailing newline", .content = "line1\nline2\nline3\n"},
	{"three lines, no trailing newline", .content = "line1\nline2\nline3"},
	{"only empty lines", .content = "\n\n\n"},
	{"mixed empty and filled lines", .content = "a\n\nb\n\n\nc"},
	{"spaces and tabs preserved", .content = "  spaced  \n\ttabbed\t\n   "},
	{"CRLF line endings kept as is", .content = "dos\r\nline\r\n"},
};

/* ************************************************************************** */
/*                              long lines                                    */
/* ************************************************************************** */

static char	*make_repeat(char c, size_t n, int trailing_nl) {
	char	*s = malloc(n + 2);
	size_t	i;

	if (!s)
		error_exit("gnl-fairy: malloc failed");
	memset(s, c, n);
	i = n;
	if (trailing_nl)
		s[i++] = '\n';
	s[i] = '\0';
	return s;
}

static void	long_line_no_nl_test(void) {
	char	*c = make_repeat('x', 5000, 0);
	int		r = gnl_check_content(c);

	free(c);
	if (r)
		abort();
}

static void	long_line_nl_test(void) {
	char	*c = make_repeat('y', 5000, 1);
	int		r = gnl_check_content(c);

	free(c);
	if (r)
		abort();
}

static void	giant_line_test(void) {
	char	*c = make_repeat('z', 20000, 1);
	int		r = gnl_check_content(c);

	free(c);
	if (r)
		abort();
}

/*
 * Lines of BUFFER_SIZE - 1, BUFFER_SIZE and BUFFER_SIZE + 1 chars, then
 * nothing, a newline, or a newline and more text: the line end falls just
 * before, on and just after the end of a read() chunk (gnlTester's 41/42/43
 * files, for every BUFFER_SIZE).
 */
static void	boundaries_test(void) {
	const char	*tails[] = {"", "\n", "\nafter\n"};
	size_t		n;
	char		*s;
	int			ko = 0;

	/* Not above 100000: lines that long reach no new code path, but cost
	 * hours to an implementation that reads less than BUFFER_SIZE at once. */
	if (BUFFER_SIZE > 100000)
		return ;
	for (int delta = -1; !ko && delta <= 1; delta++) {
		for (size_t t = 0; !ko && t < ARRAY_SIZE(tails); t++) {
			n = BUFFER_SIZE + delta;
			s = make_repeat('b', n + strlen(tails[t]), 0);
			strcpy(s + n, tails[t]);
			ko = gnl_check_content(s);
			free(s);
		}
	}
	if (ko)
		abort();
}

static const t_test	g_long[] = {
	{"5000-char line, no newline", .fn = long_line_no_nl_test},
	{"5000-char line + newline", .fn = long_line_nl_test},
	{"20000-char giant line", .fn = giant_line_test},
	{"lines of BUFFER_SIZE-1, BUFFER_SIZE, BUFFER_SIZE+1 chars", .fn = boundaries_test},
};

/* ************************************************************************** */
/*                              many lines                                    */
/* ************************************************************************** */

static void	many_short_lines_test(void) {
	const char	*unit = "hello\n";
	size_t		ulen = 6;
	/* fewer lines with a huge BUFFER_SIZE: a call may zero a 10 MB buffer */
	size_t		count = BUFFER_SIZE <= 9999 ? 1000 : 100;
	char		*buf = malloc(ulen * count + 1);
	int			r;

	if (!buf)
		error_exit("gnl-fairy: malloc failed");
	for (size_t i = 0; i < count; i++)
		memcpy(buf + i * ulen, unit, ulen);
	buf[ulen * count] = '\0';
	r = gnl_check_content(buf);
	free(buf);
	if (r)
		abort();
}

static const t_test	g_many[] = {
	{"many identical short lines", .fn = many_short_lines_test},
	{"increasing length lines", .content = "a\nbb\nccc\ndddd\neeeee\nffffff\n"},
};

/* ************************************************************************** */
/*                          fd handling / stdin                               */
/* ************************************************************************** */

static void	negative_fd_test(void) {
	if (get_next_line(-1) != NULL)
		abort();
}

static void	closed_fd_test(void) {
	int	fd = open("/dev/null", O_RDONLY);

	close(fd);
	if (get_next_line(fd) != NULL)
		abort();
}

/* A per-fd array (bonus) indexed without an upper bound check crashes here. */
static void	huge_fd_test(void) {
	if (get_next_line(INT_MAX) != NULL)
		abort();
}

/*
 * fd 1023 is valid under the usual 1024-fd limit, but a per-fd array smaller
 * than that cannot serve it: a limitation more than a bug, hence a warning.
 */
static void	high_fd_test(void) {
	const char	*content = "high\nfd\n";
	int			fd = gnl_tmp_open(content);
	char		*line;
	int			ko = 0;

	if (dup2(fd, 1023) < 0)
		return ;
	close(fd);
	line = get_next_line(1023);
	if (!line)
		gnl_warn("returned NULL on fd 1023, a valid fd: is a per-fd array too small?");
	else if (strcmp(line, "high\n"))
		ko = gnl_fail("wrong first line on fd 1023");
	else
		ko = gnl_check_fd(1023, content + 5);
	free(line);
	if (ko)
		abort();
}

/* read() fails on a directory (EISDIR): an error, so NULL. */
static void	directory_fd_test(void) {
	int		fd = open(".", O_RDONLY);
	char	*got = get_next_line(fd);

	close(fd);
	if (got) {
		free(got);
		abort();
	}
}

static void	dev_null_test(void) {
	int		fd = open("/dev/null", O_RDONLY);
	char	*got = get_next_line(fd);

	close(fd);
	if (got) {
		free(got);
		abort();
	}
}

/*
 * stdin is usually a pipe or a terminal: read() returns what is available,
 * often less than BUFFER_SIZE, long before EOF. g_read_short emulates that,
 * so an implementation that takes a short read for EOF cuts the lines.
 */
static void	stdin_test(void) {
	const char	*content = "from\nstdin\n\nin small\npieces";
	int			p[2];

	if (pipe(p) < 0)
		error_exit("gnl-fairy: pipe failed");
	if (write(p[1], content, strlen(content)) < 0)
		error_exit("gnl-fairy: write failed");
	close(p[1]);
	dup2(p[0], STDIN_FILENO);
	close(p[0]);
	g_read_short = 1;
	if (gnl_check_fd(STDIN_FILENO, content))
		abort();
}

static const t_test	g_fd[] = {
	{"negative fd returns NULL", .fn = negative_fd_test},
	{"closed fd returns NULL", .fn = closed_fd_test},
	{"huge fd (INT_MAX) returns NULL", .fn = huge_fd_test},
	{"high valid fd (1023)", .fn = high_fd_test},
	{"directory fd returns NULL", .fn = directory_fd_test},
	{"empty source (/dev/null) returns NULL", .fn = dev_null_test},
	{"stdin as a pipe, data arriving in pieces", .fn = stdin_test},
};

/* ************************************************************************** */
/*                     state reset, read errors, read rules                   */
/* ************************************************************************** */

/* After EOF the state must be clean: a new file on the same fd number. */
static void	reuse_after_eof_test(void) {
	if (gnl_check_content("first\nfile") || gnl_check_content("second\nfile\n"))
		abort();
}

/*
 * read() fails while a partial line sits in the stash (BUFFER_SIZE 42, 9999).
 * The subject asks for NULL there, and francinette expects the stash to be
 * dropped so that the same fd number, reopened on the same file, starts over:
 * both are debatable, so they only warn. A crash, a hang, or anything else
 * than the file (stale data aside) after reopening fails. The stale data is
 * all or part of the 'x' line the stash held when read() failed.
 */
static void	read_error_test(void) {
	char	*content = make_repeat('x', 20005, 1);
	char	*line;
	size_t	len;
	int		fd;
	int		stale = 0;
	int		ko = 0;

	memcpy(content, "head\n", 5);
	fd = gnl_tmp_open(content);
	line = get_next_line(fd);
	if (!line || strcmp(line, "head\n"))
		ko = gnl_fail("wrong first line, before any read() error");
	free(line);
	g_read_fail_fd = fd;
	while (!ko && (line = get_next_line(fd)) != NULL) {
		if (!strchr(line, '\n'))
			gnl_warn("returned a partial line after read() failed (the subject asks for NULL on error)");
		free(line);
	}
	g_read_fail_fd = -1;
	close(fd);
	fd = gnl_tmp_open(content);
	line = get_next_line(fd);
	if (line && !strcmp(line, content + 5)) {
		stale = 1;
		free(line);
		line = get_next_line(fd);
	}
	len = line ? strlen(line) : 0;
	if (len > 5 && strspn(line, "x") == len - 5 && !strcmp(line + len - 5, "head\n"))
		stale = 1;
	else if (!ko && (!line || strcmp(line, "head\n")))
		ko = gnl_fail("wrong first line after reopening the file");
	if (stale && !ko)
		gnl_warn("kept its stash after a read() error: the next file on that fd starts with stale data");
	ko = ko || gnl_check_fd(fd, content + 5);
	free(line);
	close(fd);
	free(content);
	if (ko)
		abort();
}

/*
 * Subject rules seen through the read() wrapper, only warned about: read()
 * is asked for BUFFER_SIZE bytes, and the first line comes back without the
 * whole file being read ahead ("Don't read the whole file and then process
 * each line"). 24000 bytes, so that reading ahead shows up to BUFFER_SIZE
 * 9999; above, a file can't outgrow one read() and 10 lines are enough.
 */
static void	read_rules_test(void) {
	size_t	count = BUFFER_SIZE <= 9999 ? 4000 : 10;
	char	*content = malloc(6 * count + 1);
	char	*line;
	int		fd;

	if (!content)
		error_exit("gnl-fairy: malloc failed");
	for (size_t i = 0; i < count; i++)
		memcpy(content + 6 * i, "hello\n", 6);
	content[6 * count] = '\0';
	fd = gnl_tmp_open(content);
	g_read_total = 0;
	g_read_bad_size = 0;
	line = get_next_line(fd);
	if (line && g_read_total >= strlen(line) + BUFFER_SIZE)
		gnl_warn("read %zu bytes to return a %zu-char first line: the subject says "
			"not to read the whole file at once", g_read_total, strlen(line));
	while (line) {
		free(line);
		line = get_next_line(fd);
	}
	if (g_read_bad_size)
		gnl_warn("read() called with a size of %zu instead of BUFFER_SIZE (%d)",
			g_read_bad_size, BUFFER_SIZE);
	close(fd);
	free(content);
}

static const t_test	g_state[] = {
	{"new file after EOF (same fd number)", .fn = reuse_after_eof_test},
	{"read() error mid-line, then reopen", .fn = read_error_test},
	{"read() size and reading ahead (subject rules)", .fn = read_rules_test},
};

int	main(void) {
	RUN_GROUP("get_next_line tiny", g_tiny);
	RUN_GROUP("get_next_line basic", g_basic);
	RUN_GROUP("get_next_line long lines", g_long);
	RUN_GROUP("get_next_line many lines", g_many);
	RUN_GROUP("get_next_line fd handling", g_fd);
	RUN_GROUP("get_next_line state, errors & read rules", g_state);
	return TESTS_STATUS;
}
