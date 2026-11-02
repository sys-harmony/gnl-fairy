#include "gnl_fairy.h"

/*
 * BUFFER_SIZE <= 0 is outside the subject, so run.sh turns any problem found
 * here into a warning. A sane get_next_line returns NULL, or still returns the
 * right lines if it falls back on a size of its own; it should not crash, hang
 * or overflow (this build runs under AddressSanitizer when available).
 */

#define STR(x) #x
#define XSTR(x) STR(x)

static void	degenerate_test(void) {
	const char	*content = "abc\ndef\n";
	int			fd = gnl_tmp_open(content);
	char		*line = get_next_line(fd);
	int			ko = 0;

	if (line && strcmp(line, "abc\n"))
		ko = gnl_fail("expected NULL or \"abc\\n\" as first line");
	else if (line)
		ko = gnl_check_fd(fd, content + 4);
	free(line);
	close(fd);
	if (ko)
		abort();
}

static const t_test	g_edge[] = {
	{"NULL or the right lines, no crash", .fn = degenerate_test},
};

int	main(void) {
	RUN_GROUP("get_next_line with BUFFER_SIZE=" XSTR(BUFFER_SIZE) " (outside the subject)", g_edge);
	return TESTS_STATUS;
}
