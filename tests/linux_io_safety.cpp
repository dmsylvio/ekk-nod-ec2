#include <cassert>
#include "../library/vm/linux/vm.cpp"
int main() {
    int fds[2]; assert(pipe(fds) == 0);
    const char input[] = "012345678901234567890123456789\ngood\nlast";
    assert(write(fds[1], input, sizeof(input)-1) == sizeof(input)-1);
    close(fds[1]);
    char line[8]{};
    assert(read_line(fds[0], line, sizeof(line)) == 4 && !strcmp(line, "good"));
    assert(read_line(fds[0], line, sizeof(line)) == 4 && !strcmp(line, "last"));
    assert(read_line(fds[0], line, sizeof(line)) == 0);
    close(fds[0]);
    assert(read_line(-1, line, sizeof(line)) == 0);
    assert(read_line(-1, nullptr, 0) == 0);
    int owned_fd = open("/dev/null", O_RDONLY);
    assert(owned_fd >= 0);
    vm_handle handle = make_linux_process_handle(owned_fd, getpid());
    assert(handle && vm::running(handle));
    ++((linux_process_handle*)handle)->start_ticks;
    assert(!vm::running(handle));
    vm::close(handle);
    assert(fcntl(owned_fd, F_GETFD) < 0);
    vm::close(nullptr);
}
