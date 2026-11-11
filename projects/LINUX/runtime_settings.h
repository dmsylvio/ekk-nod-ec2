#pragma once
#include <array>
#include <filesystem>
#include <fstream>
#include <cstdlib>

namespace runtime_settings
{
struct Values
{
	int strength = 50;
	std::array<int, 3> regions{40, 50, 10};
	int wall = 1;
	int aim = 1;
	int trigger = 1;
	int aim_button = 5;
	int trigger_button = 4;
};

inline Values current;
inline bool game_focused = false;

inline std::filesystem::path directory()
{
	const char* root = std::getenv("XDG_CONFIG_HOME");
	if (root && std::filesystem::path(root).is_absolute())
		return std::filesystem::path(root) / "ekk";
	const char* home = std::getenv("HOME");
	return std::filesystem::path(home ? home : ".") / ".config" / "ekk";
}

inline bool valid(const Values& value)
{
	if (value.strength < 0 || value.strength > 100 ||
	    value.regions[0] < 0 || value.regions[0] > 100 ||
	    value.regions[1] < 0 || value.regions[1] > 100 ||
	    value.regions[2] < 0 || value.regions[2] > 100 ||
	    value.regions[0] + value.regions[1] + value.regions[2] != 100)
		return false;
	return (value.wall == 0 || value.wall == 1) &&
		(value.aim == 0 || value.aim == 1) && (value.trigger == 0 || value.trigger == 1) &&
		(value.aim_button == 1 || value.aim_button == 4 || value.aim_button == 5) &&
		(value.trigger_button == 1 || value.trigger_button == 4 || value.trigger_button == 5);
}

inline void reload()
{
	std::ifstream input(directory() / "painel-linux.conf");
	Values next;
	if (!(input >> next.strength >> next.regions[0] >> next.regions[1] >> next.regions[2]))
		return;
	input >> std::ws;
	if (!input.eof() && !(input >> next.wall >> next.aim >> next.trigger >> next.aim_button >> next.trigger_button))
		return;
	if (valid(next)) current = next;
}
}
