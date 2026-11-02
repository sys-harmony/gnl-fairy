#ifndef GNL_FAIRY_H
# define GNL_FAIRY_H

// =============================
// 📚 Libraries
// =============================
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <unistd.h>
# include <fcntl.h>
# include <stddef.h>
# include <limits.h>

// =============================
// 🎨 Global define(s)
// =============================
# define GREEN			"\033[0;32m"
# define RED			"\033[0;31m"
# define YELLOW			"\033[0;33m"
# define RESET			"\033[0m"
# define GNL_TIMEOUT	10
# ifndef VERBOSE
#  define VERBOSE		0
# endif

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
extern int		g_tests_failed;
extern int		g_tests_warned;

// =============================
// 🧩 Type(s)
// =============================
/*
 * A forked test: runs `fn`, or checks `content` when `fn` is NULL. It fails
 * through gnl_fail + abort() or a crash; gnl_warn only flags it. Test binaries
 * exit with 0 (pass), 1 (failure) or 2 (passed with warnings).
 */
typedef struct s_test {
	const char	*name;
	void		(*fn)(void);
	const char	*content;
}	t_test;

// =============================
// 🪄 Macro(s)
// =============================
# define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*(arr)))
# define RUN_GROUP(title, tests) run_group(title, tests, ARRAY_SIZE(tests))
# define TESTS_STATUS (g_tests_failed ? 1 : g_tests_warned ? 2 : 0)

// =============================
// 📋 Function prototype(s)
// =============================
void		*__real_malloc(size_t size);
void		*__wrap_malloc(size_t size);
ssize_t		__real_read(int fd, void *buf, size_t count);
ssize_t		__wrap_read(int fd, void *buf, size_t count);
void		error_exit(char *msg);
int			gnl_fail(const char *fmt, ...);
int			gnl_warn(const char *fmt, ...);
void		run_group(const char *title, const t_test *tests, size_t n);
int			gnl_tmp_open(const char *content);
char		*gnl_expected_next(const char **cursor);
int			gnl_check_fd(int fd, const char *content);
int			gnl_check_content(const char *content);
void		gnl_drain(int fd);
char		*get_next_line(int fd);

#endif //GNL_FAIRY_H
