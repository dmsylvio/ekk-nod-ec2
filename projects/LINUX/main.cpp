#include "../../cs2/shared/cs2game.h"
// #include "../../csgo/shared/csgogame.h"
#include "../../apex/shared/apexgame.h"

#include <sys/time.h>
#include <linux/types.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <random>
#include <signal.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../library/SDL3/include/SDL3/SDL.h"
#include "settings_panel.h"
#include "overlay.h"
#include "runtime_settings.h"

static OverlayBuffer overlay;
static unsigned long drawn_boxes = 0;


static int fd = -1;
static bool injected_click = false;

static volatile sig_atomic_t stop_requested = 0;
static void stop_handler(int) { stop_requested = 1; }
static unsigned char mouse_keys[(KEY_MAX + 8) / 8]{};
static bool button_down(int button)
{
	const int code = button == 1 ? BTN_LEFT : button == 4 ? BTN_SIDE : BTN_EXTRA;
	return (mouse_keys[code / 8] & (1U << (code % 8))) != 0;
}

static bool send_input(int device, __u16 type, __u16 code, __s32 value)
{
	if (device < 0) return false;
	input_event events[2]{};
	events[0].type = type;
	events[0].code = code;
	events[0].value = value;
	events[1].type = EV_SYN;
	events[1].code = SYN_REPORT;
	return write(device, events, sizeof(events)) == sizeof(events);
}

static int open_device(const char *name, size_t length)
{
	DIR* devices = opendir("/dev/input/by-id/");
	if (!devices) return -1;
	int selected = -1;
	while (auto* entry = readdir(devices))
	{
		const size_t size = strlen(entry->d_name);
		if (size < length || strcmp(entry->d_name + size - length, name) != 0) continue;
		char path[512]{};
		snprintf(path, sizeof(path), "/dev/input/by-id/%s", entry->d_name);
		const int candidate = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
		if (candidate < 0) continue;
		unsigned char keys[(KEY_MAX + 8) / 8]{};
		if (ioctl(candidate, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0)
		{
			close(candidate);
			continue;
		}
		if (selected >= 0) close(selected);
		selected = candidate;
		if ((keys[BTN_EXTRA / 8] & (1U << (BTN_EXTRA % 8))) &&
		    (keys[BTN_SIDE / 8] & (1U << (BTN_SIDE % 8)))) break;
	}
	closedir(devices);
	return selected;
}

namespace client
{
	bool aim_pressed()
	{
		return runtime_settings::game_focused && runtime_settings::current.aim &&
			runtime_settings::current.strength > 0 && button_down(runtime_settings::current.aim_button);
	}
	bool trigger_pressed()
	{
		return runtime_settings::game_focused && runtime_settings::current.trigger &&
			button_down(runtime_settings::current.trigger_button);
	}
	bool overlay_enabled() { return runtime_settings::current.wall != 0; }
	float aim_smoothing() { return 8.0f - 7.0f * runtime_settings::current.strength / 100.0f; }
	int choose_aim_bone()
	{
		static std::mt19937 generator(std::random_device{}());
		const auto& weights = runtime_settings::current.regions;
		const int region = std::discrete_distribution<int>(weights.begin(), weights.end())(generator);
		if (region == 0) return 6;
		if (region == 1) return 4;
		constexpr int limbs[]{8, 13, 23, 26};
		return limbs[std::uniform_int_distribution<int>(0, 3)(generator)];
	}

	bool get_overlay_size(int* width, int* height)
	{
		return overlay.size(width, height);
	}

	void mouse_move(int x, int y)
	{
		send_input(fd, EV_REL, 0, x);
		send_input(fd, EV_REL, 1, y);
	}

	void mouse1_down(void)
	{
		injected_click = send_input(fd, EV_KEY, BTN_LEFT, 1);
	}

	void mouse1_up(void)
	{
		if (injected_click && send_input(fd, EV_KEY, BTN_LEFT, 0)) injected_click = false;
	}

	void DrawRect(void *hwnd, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b)
	{
		++drawn_boxes;
		const unsigned long border = 0xff000000UL | (r << 16) | (g << 8) | b;
		const unsigned long fill = 0x32000000UL | ((r * 50 / 255) << 16) |
			((g * 50 / 255) << 8) | (b * 50 / 255);
		overlay.fill(x, y, w, h, border);
		overlay.fill(x + 2, y + 2, w - 4, h - 4, fill);
	}

	void DrawFillRect(void *hwnd, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b)
	{
		overlay.fill(x, y, w, h, 0xff000000UL | (r << 16) | (g << 8) | b);
	}
}

int main(int argc, char* argv[])
{
	if (argc == 2 && strcmp(argv[1], "--settings") == 0)
		return settings_panel::show();

	runtime_settings::reload();
	signal(SIGINT, stop_handler);
	signal(SIGTERM, stop_handler);
	fd = open_device("event-mouse", 11);
	if (fd < 0)
		LOG("mouse access unavailable; Aim and Trigger are paused\n");

	SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		LOG("failed to initialize SDL: %s\n", SDL_GetError());
		close(fd);
		return 1;
	}

	SDL_Rect bounds{};
	int display_count = 0;
	SDL_DisplayID* displays = SDL_GetDisplays(&display_count);
	if (!displays || display_count == 0 || SDL_GetDisplayBounds(displays[0], &bounds) != 0)
	{
		SDL_free(displays);
		SDL_Quit();
		close(fd);
		return 1;
	}
	SDL_free(displays);
	SDL_Window *window = SDL_CreateWindow("EKK", bounds.w, bounds.h,
		SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS |
		SDL_WINDOW_NOT_FOCUSABLE | SDL_WINDOW_ALWAYS_ON_TOP);
	if (window == NULL)
	{
		LOG("failed to create window: %s\n", SDL_GetError());
		SDL_Quit();
		close(fd);
		return 1;
	}

	if (!enable_overlay_input_passthrough(window))
	{
		LOG("transparent click-through overlay unavailable on this display\n");
		SDL_DestroyWindow(window);
		SDL_Quit();
		close(fd);
		return 1;
	}
	find_cs2_window(window, bounds);
	SDL_SetWindowSize(window, bounds.w, bounds.h);
	SDL_SetWindowPosition(window, bounds.x, bounds.y);
	if (!overlay.open(window, bounds.w, bounds.h))
	{
		LOG("failed to allocate the X11 overlay buffer\n");
		overlay.close();
		SDL_DestroyWindow(window);
		SDL_Quit();
		if (fd >= 0) close(fd);
		return 1;
	}

	SDL_ShowWindow(window);
	overlay.present();

	const bool diagnostics = std::getenv("EKK_DIAGNOSTICS") != nullptr;
	Uint64 next_diagnostic = 0;
	Uint64 next_window_check = 0;
	Uint64 next_mouse_check = 0;
	Uint64 next_connection_check = 0;
	BOOL quit = 0;
	while (!quit && !stop_requested)
	{
		const Uint64 now = SDL_GetTicks();
		const Uint64 frame_start = SDL_GetTicksNS();
		if (now >= next_window_check)
		{
			next_window_check = now + 250;
			runtime_settings::reload();
			runtime_settings::game_focused = false;
			SDL_Rect target{};
			if (find_cs2_window(window, target, &runtime_settings::game_focused) &&
			    (target.x != bounds.x || target.y != bounds.y ||
			     target.w != bounds.w || target.h != bounds.h))
			{
				if (!overlay.resize(target.w, target.h))
				{
					LOG("failed to resize the X11 overlay buffer\n");
					break;
				}
				bounds = target;
				SDL_SetWindowSize(window, bounds.w, bounds.h);
				SDL_SetWindowPosition(window, bounds.x, bounds.y);
			}
		}
		const Uint64 window_done = SDL_GetTicksNS();
		if (fd < 0 && now >= next_mouse_check)
		{
			next_mouse_check = now + 1000;
			fd = open_device("event-mouse", 11);
			if (fd >= 0) LOG("mouse access restored\n");
		}
		memset(mouse_keys, 0, sizeof(mouse_keys));
		if (fd >= 0 && ioctl(fd, EVIOCGKEY(sizeof(mouse_keys)), mouse_keys) < 0)
		{
			client::mouse1_up();
			close(fd);
			fd = -1;
			injected_click = false;
		}
		SDL_Event e;
		while ( SDL_PollEvent( &e ) )
		{
			if (e.type == SDL_EVENT_WINDOW_EXPOSED) overlay.invalidate();
			if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
				(e.type == SDL_EVENT_KEY_DOWN && e.key.keysym.sym == SDLK_ESCAPE))
			{
				quit = 1;
				break;
			}
		}

		if (quit)
			break;

		const Uint64 events_done = SDL_GetTicksNS();
		overlay.begin();

		if (cs2::game_handle)
		{
			if (cs2::running())
			{
				cs2::features::run();
			}
			else
			{
				cs2::features::reset();
			}
		}
		/*
		else if (csgo::game_handle)
		{
			if (csgo::running())
			{
				csgo::features::run();
			}
			else
			{
				csgo::features::reset();
			}
		}
		*/
		else if (apex::game_handle)
		{
			if (apex::running())
			{
				apex::features::run();
			}
			else
			{
				apex::features::reset();
			}
		}
		else if (now >= next_connection_check)
		{
			next_connection_check = now + 1000;
			if (vm::process_exists("cs2"))
				cs2::running();
			else if (vm::process_exists("r5apex.exe"))
				apex::running();
		}
		
		if (diagnostics && now >= next_diagnostic)
		{
			next_diagnostic = now + 2000;
			const QWORD controller = cs2::game_handle ? cs2::entity::get_local_player_controller() : 0;
			const QWORD pawn = controller ? cs2::entity::get_player(controller) : 0;
			LOG("overlay diagnostic: controller=%llx pawn=%llx drawn_boxes=%lu\n",
				(unsigned long long)controller, (unsigned long long)pawn, drawn_boxes);
			if (pawn)
			{
				LOG("input diagnostic: clock=%u weapon=%d aim=%d trigger=%d\n",
					cs2::engine::get_current_ms(), int(cs2::player::get_weapon_class(pawn)),
					int(client::aim_pressed()), int(client::trigger_pressed()));
				unsigned controllers = 0, players = 0, valid = 0, enemies = 0;
				for (int index = 1; index < 64; ++index)
				{
					const auto entity = cs2::entity::get_client_entity(index);
					if (!entity || !cs2::entity::is_player(entity)) continue;
					++controllers;
					const auto player = cs2::entity::get_player(entity);
					if (!player) continue;
					++players;
					const auto node = cs2::player::get_node(player);
					if (!node || !cs2::player::is_valid(player, node)) continue;
					++valid;
					if (cs2::player::get_team_num(player) != cs2::player::get_team_num(pawn))
					{
						++enemies;
						if (enemies == 1)
						{
							auto origin = cs2::player::get_origin(player);
							vec3 projected{};
							vec2 size{float(bounds.w), float(bounds.h)};
							bool visible = math::world_to_screen(size, origin, projected, cs2::engine::get_viewmatrix());
							LOG("overlay projection: origin=%f,%f,%f visible=%d screen=%f,%f\n",
								origin.x,origin.y,origin.z,int(visible),projected.x,projected.y);
						}
					}
				}
				LOG("overlay diagnostic: alpha=%d controllers=%u players=%u valid=%u enemies=%u\n",
					cs2::get_crosshairalpha(), controllers, players, valid, enemies);
			}
		}
		const Uint64 logic_done = SDL_GetTicksNS();
		overlay.present();
		const Uint64 present_done = SDL_GetTicksNS();
		if (diagnostics && present_done - frame_start > 100000000)
			LOG("frame timings ms: window=%llu events=%llu logic=%llu present=%llu\n",
				(unsigned long long)((window_done - frame_start) / 1000000),
				(unsigned long long)((events_done - window_done) / 1000000),
				(unsigned long long)((logic_done - events_done) / 1000000),
				(unsigned long long)((present_done - logic_done) / 1000000));
		SDL_Delay(8);
	}
	cs2::features::reset();
	client::mouse1_up();
	overlay.close();
	SDL_DestroyWindow(window);
	SDL_Quit();
	close(fd);
	return 0;
}

