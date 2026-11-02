#include "gnl_fairy.h"

/*
 * The bonus reads several fds at once: fd A, then B, then A again must not
 * lose A's state. "A single static variable" is checked by run.sh with nm;
 * here, the behaviour: each fd follows its own reference split.
 */

#define NB_FD 3

static int			g_fd[NB_FD];
static const char	*g_cur[NB_FD];

static void	open_all(void) {
	static const char	*contents[NB_FD] = {"A1\nA2\nA3\n", "B1\nB2\n", "C1\nC2\nC3\nC4"};

	for (int i = 0; i < NB_FD; i++) {
		g_fd[i] = gnl_tmp_open(contents[i]);
		g_cur[i] = contents[i];
	}
}

/* Reads the next line of fd #i: it must be that fd's own next line. */
static void	step(int i) {
	char	*expected = gnl_expected_next(&g_cur[i]);
	char	*got = get_next_line(g_fd[i]);
	char	b1[96];
	char	b2[96];

	if (expected ? !got || strcmp(got, expected) : got != NULL)
		fail("fd #%d: expected %s, got %s", i, gnl_show(expected, b1), gnl_show(got, b2));
	free(got);
	free(expected);
}

static void	interleaved_test(void) {
	const int	order[] = {0, 1, 2, 0, 1, 2, 0, 2, 1, 2, 0, 1, 2, 2, 1, 0};

	open_all();
	for (size_t i = 0; i < ARRAY_SIZE(order); i++)
		step(order[i]);
}

static void	round_robin_test(void) {
	open_all();
	for (int round = 0; round < 5; round++)
		for (int i = 0; i < NB_FD; i++)
			step(i);
}

static void	same_content_test(void) {
	const char	*content = "one\ntwo\nthree\n";

	g_fd[0] = gnl_tmp_open(content);
	g_fd[1] = gnl_tmp_open(content);
	g_cur[0] = content;
	g_cur[1] = content;
	step(0);
	step(1);
	step(0);
	step(0);
	step(1);
}

/*
 * A read() error on one fd leaves the others alone. fd #1 is read to its end
 * first, so that its next call really reads whatever BUFFER_SIZE; its own
 * state after the error is not checked (see the mandatory read error test).
 */
static void	read_error_one_fd_test(void) {
	open_all();
	step(0);
	step(2);
	step(1);
	step(1);
	g_read_fail_fd = g_fd[1];
	free(get_next_line(g_fd[1]));
	g_read_fail_fd = -1;
	for (int round = 0; round < 4; round++) {
		step(0);
		step(2);
	}
}

static const t_test	g_multi_fd[] = {
	{"interleaved reads keep each fd's state", interleaved_test},
	{"three fds read in turn to the end", round_robin_test},
	{"two fds on the same content stay apart", same_content_test},
	{"a read() error on one fd spares the others", read_error_one_fd_test},
};

int	main(void) {
	RUN_GROUP("multiple fds (bonus)", g_multi_fd);
	return fairy_summary();
}
