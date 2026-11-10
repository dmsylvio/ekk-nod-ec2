#ifndef EKK_LINUX_PROCESS_HANDLE_H
#define EKK_LINUX_PROCESS_HANDLE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

struct linux_process_handle { int fd, pid; unsigned long long start_ticks; };
inline unsigned long long linux_start_ticks(int pid)
{
    char path[64], data[4096]{};
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    ssize_t size = read(fd, data, sizeof(data) - 1);
    close(fd);
    if (size <= 0 || size == sizeof(data) - 1) return 0;
    // comm can contain spaces and parentheses; the last ')' closes it.
    char* end = strrchr(data, ')');
    if (!end || end[1] != ' ' || !end[2] || end[2] == 'Z' || end[2] == 'X') return 0;
    char* field = end + 2;
    for (int number = 3; number < 22; ++number) {
        field = strchr(field, ' ');
        if (!field) return 0;
        while (*field == ' ') ++field;
    }
    errno = 0; char* tail = nullptr;
    unsigned long long result = strtoull(field, &tail, 10);
    return errno || tail == field || (*tail && *tail != ' ' && *tail != '\n') ? 0 : result;
}
inline void* make_linux_process_handle(int fd, int pid)
{
    unsigned long long start = linux_start_ticks(pid);
    auto* result = static_cast<linux_process_handle*>(malloc(sizeof(linux_process_handle)));
    if (!result || !start) { free(result); close(fd); return nullptr; }
    result->fd = fd; result->pid = pid; result->start_ticks = start;
    return result;
}
#endif
