#include "gnl_fairy.h"

/*
 * BUFFER_SIZE <= 0 is outside the subject: whatever goes wrong here only
 * warns. A sane get_next_line returns NULL, or the right lines if it falls
 * back on a size of its own, without crashing, hanging or overflowing (this
 * build runs under AddressSanitizer when available).
 */

static void	degenerate_test(void) {
	const char	*content = "abc\ndef\n";
	int			fd = gnl_tmp_open(content);
	char		*line = get_next_line(fd);
	char		b[96];

	if (line && strcmp(line, "abc\n"))
		fail("expected NULL or \"abc\\n\" as first line, got %s", gnl_show(line, b));
	if (line)
		gnl_check_fd(fd, content + 4);
}

static const t_test	g_edge[] = {
	{"NULL or the right lines, no crash", degenerate_test},
};

int	main(void) {
	g_lenient = 1;
	RUN_GROUP("outside the subject", g_edge);
	return fairy_summary();
}
