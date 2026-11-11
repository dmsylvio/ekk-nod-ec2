#ifndef CS2GAME_H
#define CS2GAME_H

#ifdef _KERNEL_MODE
extern "C" int _fltused;
#endif

#include "cs2.h"

//
// features.cpp
//
namespace cs2
{
	namespace features
	{
		void run(void);
		void reset(void);
	}
}

//
// implemented by application/driver
//
namespace client
{
#ifdef __linux__
    bool get_overlay_size(int* width, int* height);
    float aim_smoothing();
#endif
	#if defined(EC_UM_CLOCK) || defined(__linux__)
    int choose_aim_bone();
    bool aim_pressed();
    bool trigger_pressed();
    bool overlay_enabled();
#endif
    extern void mouse_move(int x, int y);
	extern void mouse1_down(void);
	extern void mouse1_up(void);
	extern void DrawRect(void *hwnd, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
	extern void DrawFillRect(void *hwnd, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
}

namespace cs2
{
	inline void run(void)
	{
		if (cs2::running())
		{
			features::run();
		}
		else
		{
			features::reset();
		}
	}
}


#endif // CS2GAME_H
