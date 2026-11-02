#include "gnl_fairy.h"

/*
 * Bonus leak tests: several descriptors are read concurrently and each must be
 * drained to EOF so the per-fd stash is freed. Run under Valgrind by run.sh.
 */

#define NB_FD 3

static void	leak_multi_fd_interleaved(void) {
	const char	*content[NB_FD] = {
		"a\nb\nc\n",
		"d\ne\n",
		"f\ng\nh\ni\n"
	};
	int			fd[NB_FD];

	for (int i = 0; i < NB_FD; i++)
		fd[i] = gnl_tmp_open(content[i]);
	/* read one line from each before draining, to keep stashes live */
	for (int i = 0; i < NB_FD; i++)
		free(get_next_line(fd[i]));
	for (int i = 0; i < NB_FD; i++) {
		gnl_drain(fd[i]);
		close(fd[i]);
	}
}

/*
 * Each malloc failing in turn while three fds are read one after another, so
 * that it hits the state of a new fd while the others hold theirs.
 */
static void	leak_multi_fd_malloc_fail(void) {
	const char	*content[NB_FD] = {"11\n22\n", "33\n44\n", "55\n66\n"};
	int			fd[NB_FD];

	for (int k = 1; k < 1000; k++) {
		for (int i = 0; i < NB_FD; i++)
			fd[i] = gnl_tmp_open(content[i]);
		g_malloc_count = 0;
		g_malloc_fail_at = k;
		g_malloc_wrap_enabled = 1;
		for (int i = 0; i < NB_FD; i++)
			free(get_next_line(fd[i]));
		g_malloc_wrap_enabled = 0;
		for (int i = 0; i < NB_FD; i++) {
			gnl_drain(fd[i]);
			close(fd[i]);
		}
		if (g_malloc_count < k)
			break ;
	}
}

int	main(void) {
	leak_multi_fd_interleaved();
	leak_multi_fd_malloc_fail();
	return 0;
}
