#include "gnl_fairy.h"

/*
 * Leak tests are NOT forked: the whole process runs under Valgrind, which
 * inspects every allocation at exit. Each helper therefore reads its file to
 * EOF so that a correct get_next_line frees its static stash (a partial read
 * would legitimately leave a "still reachable" buffer and is not a leak).
 * We also exercise the two failure paths: read() error and malloc() failure,
 * where get_next_line must return NULL without leaking.
 */

static void	leak_file(const char *content) {
	int	fd = gnl_tmp_open(content);

	gnl_drain(fd);
	close(fd);
}

static void	leak_long_line(void) {
	char	big[8192];

	memset(big, 'a', sizeof(big) - 1);
	big[sizeof(big) - 1] = '\0';
	leak_file(big);
}

/* read() fails while a partial line sits in the stash (BUFFER_SIZE 42, 9999). */
static void	leak_read_error(void) {
	char	content[200];
	int		fd;

	memset(content, 'x', sizeof(content) - 1);
	content[sizeof(content) - 1] = '\0';
	memcpy(content, "head\n", 5);
	fd = gnl_tmp_open(content);
	free(get_next_line(fd));
	g_read_fail_fd = fd;
	free(get_next_line(fd));
	g_read_fail_fd = -1;
	gnl_drain(fd);
	close(fd);
}

/*
 * Fails each malloc of a full read in turn (1st, then 2nd, ...): get_next_line
 * may give up early but must neither crash nor leak. Stops once a full read no
 * longer reaches the k-th malloc, i.e. every allocation site has failed once.
 */
static void	leak_malloc_fail(void) {
	int	fd;

	for (int k = 1; k < 1000; k++) {
		fd = gnl_tmp_open("x\nyy\nzzz");
		g_malloc_count = 0;
		g_malloc_fail_at = k;
		g_malloc_wrap_enabled = 1;
		gnl_drain(fd);
		g_malloc_wrap_enabled = 0;
		gnl_drain(fd);
		close(fd);
		if (g_malloc_count < k)
			break ;
	}
}

int	main(void) {
	leak_file("alpha\nbeta\ngamma\n");
	leak_file("alpha\nbeta\ngamma");
	leak_long_line();
	leak_file("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n");
	leak_read_error();
	leak_malloc_fail();
	return 0;
}
