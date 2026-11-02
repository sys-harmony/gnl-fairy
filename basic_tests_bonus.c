#include "gnl_fairy.h"

/*
 * Bonus = managing several file descriptors at once with a single static
 * variable: reading from fd A then fd B then A again must not lose A's state.
 * "Only one static variable" is checked on the objects by run.sh (nm); here
 * we validate the behavioural half: per-fd independence.
 */

#define NB_FD 3

static int			g_fd[NB_FD];
static const char	*g_cur[NB_FD];
static const char	*g_content[NB_FD] = {
	"A1\nA2\nA3\n",
	"B1\nB2\n",
	"C1\nC2\nC3\nC4\n"
};

static void	open_all(void) {
	for (int i = 0; i < NB_FD; i++) {
		g_fd[i] = gnl_tmp_open(g_content[i]);
		g_cur[i] = g_content[i];
	}
}

static void	close_all(void) {
	for (int i = 0; i < NB_FD; i++)
		close(g_fd[i]);
}

/* Reads one line from fd #i and checks it matches that fd's own expectation. */
static int	step(int i) {
	char	*expected = gnl_expected_next(&g_cur[i]);
	char	*got = get_next_line(g_fd[i]);
	int		ok;

	if (expected == NULL)
		ok = (got == NULL);
	else
		ok = (got && strcmp(got, expected) == 0);
	if (!ok)
		gnl_fail("fd #%d returned a line that is not its own next line", i);
	free(got);
	free(expected);
	return ok;
}

static void	interleaved_test(void) {
	int	ok = 1;

	open_all();
	ok &= step(0); ok &= step(1); ok &= step(2);
	ok &= step(0); ok &= step(1); ok &= step(2);
	ok &= step(0); ok &= step(2); ok &= step(1);
	ok &= step(2);
	/* every fd is now at EOF: each must yield NULL independently */
	ok &= step(0);
	ok &= step(1);
	ok &= step(2);
	close_all();
	if (!ok)
		abort();
}

static void	round_robin_test(void) {
	int	ok = 1;

	open_all();
	/* fully drain the three fds in strict rotation */
	for (int round = 0; round < 5; round++)
		for (int i = 0; i < NB_FD; i++)
			ok &= step(i);
	close_all();
	if (!ok)
		abort();
}

static void	two_fd_same_file_test(void) {
	int		fda = gnl_tmp_open("one\ntwo\nthree\n");
	int		fdb = gnl_tmp_open("one\ntwo\nthree\n");
	char	*a1;
	char	*b1;
	char	*a2;
	int		ok;

	a1 = get_next_line(fda);
	b1 = get_next_line(fdb);
	a2 = get_next_line(fda);
	ok = a1 && !strcmp(a1, "one\n") && b1 && !strcmp(b1, "one\n")
		&& a2 && !strcmp(a2, "two\n");
	free(a1);
	free(b1);
	free(a2);
	close(fda);
	close(fdb);
	if (!ok)
		abort();
}

/*
 * A read() error on one fd must leave the others alone. fd #1 is emptied first
 * so that its next call really reads, whatever BUFFER_SIZE; its own state after
 * the error is not checked (see the mandatory read error test).
 */
static void	read_error_one_fd_test(void) {
	int	ok = 1;

	open_all();
	ok &= step(0);
	ok &= step(2);
	ok &= step(1);
	ok &= step(1);
	g_read_fail_fd = g_fd[1];
	free(get_next_line(g_fd[1]));
	g_read_fail_fd = -1;
	for (int round = 0; round < 4; round++) {
		ok &= step(0);
		ok &= step(2);
	}
	close_all();
	if (!ok)
		abort();
}

static const t_test	g_multi_fd[] = {
	{"interleaved reads keep per-fd state", .fn = interleaved_test},
	{"round-robin full drain", .fn = round_robin_test},
	{"two fds on identical content stay independent", .fn = two_fd_same_file_test},
	{"read() error on one fd leaves the others intact", .fn = read_error_one_fd_test},
};

int	main(void) {
	RUN_GROUP("get_next_line multi-fd (bonus)", g_multi_fd);
	return TESTS_STATUS;
}
