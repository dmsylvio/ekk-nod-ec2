#include <cassert>
#include "../cs2/shared/cs2features.cpp"
static unsigned releases = 0, movements = 0;
void client::mouse1_up() { ++releases; }
void client::mouse_move(int, int) { ++movements; }
DWORD cs2::engine::get_current_ms() { return 7; }
int main() {
    cs2::features::mouse_down_ms = 100;
    cs2::features::trigger_target = 42;
    cs2::features::reset();
    assert(releases == 1 && !cs2::features::trigger_target && !cs2::features::mouse_down_ms);
    cs2::features::reset(); assert(releases == 1);
    cs2::features::reset(); assert(releases == 1);
    assert(random_number(20, 10) == 20);
    assert(random_number(0, ~DWORD(0)) == 7);
    cs2::features::standalone_rcs(2, {1, 1}, 0);
    cs2::features::standalone_rcs(2, {FLT_MAX, FLT_MAX}, 0.00001f);
    assert(movements == 0);
}
