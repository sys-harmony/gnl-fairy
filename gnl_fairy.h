#ifndef GNL_FAIRY_H
# define GNL_FAIRY_H

// =============================
// 📚 Libraries
// =============================
# include <ctype.h>
# include <fcntl.h>
# include <limits.h>
# include <signal.h>
# include <stdarg.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <sys/wait.h>

// =============================
// 🎨 Global define(s)
// =============================
# define GREEN			"\033[32m"
# define RED			"\033[31m"
# define YELLOW			"\033[33m"
# define BOLD			"\033[1m"
# define DIM			"\033[2m"
# define RESET			"\033[0m"
# define WIDTH			46		// as in run.sh
# define TEST_TIMEOUT	10		// seconds before a test counts as hanging
# ifndef VERBOSE
#  define VERBOSE		0
# endif

// =============================
// 🧩 Type(s)
// =============================
/* A test runs fn, or checks that content is read back line by line. */
typedef struct s_test {
	const char	*name;
	void		(*fn)(void);
	const char	*content;
}	t_test;

// =============================
// 🌍 Global variable(s)
// =============================
extern int		g_malloc_wrap_enabled;
extern int		g_malloc_count;
extern int		g_malloc_fail_at;
extern int		g_read_fail_fd;
extern int		g_read_short;
extern size_t	g_read_total;
extern size_t	g_read_bad_size;
extern int		g_lenient;

// =============================
// 🪄 Macro(s)
// =============================
# define STR(x) #x
# define XSTR(x) STR(x)
# define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*(arr)))
# define RUN_GROUP(title, tests) \
	run_group(title " · BUFFER_SIZE=" XSTR(BUFFER_SIZE), tests, ARRAY_SIZE(tests))

// =============================
// 📋 Function prototype(s)
// =============================
char		*get_next_line(int fd);

void		*__real_malloc(size_t size);
void		*__wrap_malloc(size_t size);
ssize_t		__real_read(int fd, void *buf, size_t count);
ssize_t		__wrap_read(int fd, void *buf, size_t count);
int			gnl_tmp_open(const char *content);
char		*gnl_expected_next(const char **cursor);
const char	*gnl_show(const char *s, char *out);
void		gnl_check_fd(int fd, const char *content);
void		gnl_check_content(const char *content);
void		gnl_drain(int fd);
char		*make_repeat(char c, size_t n, int trailing_nl);

void		die(const char *what) __attribute__((noreturn));
void		fail(const char *fmt, ...)
			__attribute__((noreturn, format(printf, 1, 2)));
void		warning(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void		section(const char *title);
void		run_group(const char *title, const t_test *tests, size_t n);
int			fairy_summary(void);

#endif //GNL_FAIRY_H
