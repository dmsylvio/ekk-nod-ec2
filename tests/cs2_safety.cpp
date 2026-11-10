#include <cassert>
#include <cstring>
#include "../cs2/shared/cs2.cpp"

static BYTE memory[16384]{};
static unsigned reads = 0, closes = 0;
BOOL vm::read(vm_handle, QWORD address, PVOID out, QWORD size)
{
    ++reads;
    if (!address || address > sizeof(memory) || size > sizeof(memory) - address)
        return 0;
    memcpy(out, memory + address, size);
    return 1;
}
void vm::close(vm_handle) { ++closes; }
template<typename T> void put(QWORD address, T value)
{
    memcpy(memory + address, &value, sizeof(value));
}
int main()
{
    cs2::game_handle = reinterpret_cast<vm_handle>(1);
    assert(cs2::get_interface_function(0, 16) == 0);
    put<QWORD>(64, 128);
    put<QWORD>(128 + 16 * 8, 512);
    assert(cs2::get_interface_function(64, 16) == 512);
    put<QWORD>(64, ~QWORD(0) - 4);
    assert(cs2::get_interface_function(64, 16) == 0);
    put<QWORD>(64, 128);
    cs2::interfaces::input = 64;
    put<DWORD>(514, 1024);
    put<DWORD>(64 + 1024, 42);
    assert(cs2::engine::get_current_ms() == 42);
    // The second displacement must be read again, rather than cached forever.
    put<DWORD>(514, 2048);
    put<DWORD>(64 + 2048, 99);
    assert(cs2::engine::get_current_ms() == 99);

    // Bounds-aware local dump reads reject one-past-end and unaligned truncation.
    auto* image = static_cast<BYTE*>(calloc(32, 1));
    QWORD image_size = 16; memcpy(image + 8, &image_size, 8);
    memcpy(image + 16, "field", 6);
    assert(cs2::dump_name_equals(image + 16, (PCSTR)image + 16, "field"));
    assert(!cs2::dump_name_equals(image + 16, (PCSTR)image + 32, "field"));
    assert(cs2::read_dump<QWORD>(image + 16, (QWORD)image + 31) == 0);
    free(image);
    assert(!cs2::entity::has_defuser(100));
    assert(!cs2::player::is_defusing(100));
    assert(!cs2::player::get_weapon_address(100));
    assert(!cs2::entity::get_client_entity(-1));
    assert(!cs2::node::get_bone_position(100, -1, nullptr));
    assert(cs2::node::get_dormant(0));

    cs2::interfaces::cvar = 3000;
    put<QWORD>(3064, 4000);
    put<DWORD>(3160, 1);
    put<QWORD>(4000, 5000);
    put<QWORD>(5000, 6000);
    char longest[120];
    memset(longest, 'a', 119); longest[119] = 0;
    memcpy(memory + 6000, longest, sizeof(longest));
    assert(cs2::engine::get_convar(longest) == 5000);
    char oversized[121];
    memset(oversized, 'a', 120); oversized[120] = 0;
    assert(cs2::engine::get_convar(oversized) == 0);
    assert(cs2::engine::get_convar(nullptr) == 0);
    memory[6000 + 119] = 'b';
    assert(cs2::engine::get_convar(longest) == 0);
    put<QWORD>(5000, sizeof(memory));
    assert(cs2::engine::get_convar(longest) == 0);
    put<DWORD>(3160, ~DWORD(0));
    reads = 0;
    assert(cs2::engine::get_convar(longest) == 0 && reads == 2);

    put<DWORD>(7000 + 0xD0, 1);
    put<QWORD>(7000 + 0xD8, 8000);
    put<DWORD>(8000, 1); put<DWORD>(8004, 5);
    assert(cs2::sdl::get_mouse_button(7000) == 5);
    put<DWORD>(7000 + 0xD0, ~DWORD(0));
    reads = 0;
    assert(cs2::sdl::get_mouse_button(7000) == 0 && reads == 1);
    put<DWORD>(7000 + 0xD0, 1); put<QWORD>(7000 + 0xD8, 0);
    assert(cs2::sdl::get_mouse_button(7000) == 0);

    cs2::netvars::m_iHealth = 77;
    cs2::direct::view_matrix = 100;
    cs2::convars::sensitivity = 200;
    cs2::sdl::sdl_window = 300;
    cs2::reset_session();
    assert(!cs2::game_handle && closes == 1);
    assert(!cs2::interfaces::input && !cs2::interfaces::cvar);
    assert(!cs2::netvars::m_iHealth && !cs2::direct::view_matrix);
    assert(!cs2::convars::sensitivity && !cs2::sdl::sdl_window);
    cs2::reset_session();
    assert(closes == 1);
}
