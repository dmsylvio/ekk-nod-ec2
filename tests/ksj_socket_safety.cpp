#include <cassert>
#include <thread>
#include "../library/vm/ksj/src/vm.cpp"
int main() {
    vm::close(&vm::system_proc); // Never free static storage.
    assert(!client::pm::write(0, 0, nullptr, ~QWORD(0)));
    assert(!client::vm::write(0, 0, 0, nullptr, ~QWORD(0)));
    int sockets[2]; assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    std::thread writer([&] {
        assert(send(sockets[1], "ab", 2, MSG_NOSIGNAL) == 2);
        assert(send(sockets[1], "cd", 2, MSG_NOSIGNAL) == 2);
        close(sockets[1]);
    });
    char buffer[6]{};
    assert(socket_read(sockets[0], buffer, 6) == 4);
    assert(!memcmp(buffer, "abcd", 4));
    close(sockets[0]); writer.join();
    assert(protocol_failed);
    protocol_failed = false; // Explicitly create a fresh test session.
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    assert(socket_write(sockets[0], (PVOID)"hello", 5) == 5);
    assert(recv(sockets[1], buffer, 5, 0) == 5);
    assert(!memcmp(buffer, "hello", 5));
    close(sockets[0]); close(sockets[1]);
}
