#ifndef CHARM_POSIX_USER_CRT_H
#define CHARM_POSIX_USER_CRT_H

#ifndef CHARM_POSIX_HEADER_SKIP_STDDEF
#include <stddef.h>
#endif

#ifdef __cplusplus
extern "C" {
#define CHARM_POSIX_NOEXCEPT noexcept
#else
#define CHARM_POSIX_NOEXCEPT
#endif

typedef long long charm_posix_ssize_t;

#define CHARM_POSIX_EBADF 9
#define CHARM_POSIX_EINVAL 22
#define CHARM_POSIX_ENOSYS 38

int charm_posix_argc(void) CHARM_POSIX_NOEXCEPT;
char** charm_posix_argv(void) CHARM_POSIX_NOEXCEPT;
char** charm_posix_envp(void) CHARM_POSIX_NOEXCEPT;
char** charm_posix_environ(void) CHARM_POSIX_NOEXCEPT;
const char* charm_posix_getenv(const char* key) CHARM_POSIX_NOEXCEPT;
int* charm_posix_errno_location(void) CHARM_POSIX_NOEXCEPT;
charm_posix_ssize_t charm_posix_read(int fd, void* buf, size_t count) CHARM_POSIX_NOEXCEPT;
charm_posix_ssize_t charm_posix_write(int fd, const void* buf, size_t count) CHARM_POSIX_NOEXCEPT;
int charm_posix_getpid(void) CHARM_POSIX_NOEXCEPT;
int charm_posix_sleep(unsigned seconds) CHARM_POSIX_NOEXCEPT;
void charm_posix_exit(int code) CHARM_POSIX_NOEXCEPT;
void charm_posix_abort(void) CHARM_POSIX_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#undef CHARM_POSIX_NOEXCEPT

#endif
