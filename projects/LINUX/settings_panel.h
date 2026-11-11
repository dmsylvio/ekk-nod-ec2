#pragma once

#include <cstdio>
#include <filesystem>
#include <unistd.h>

namespace settings_panel
{
inline int show()
{
	std::error_code error;
	const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
	if (!error)
	{
		const auto script = executable.parent_path() / "settings_panel.py";
		execl("/usr/bin/python3", "python3", script.c_str(), static_cast<char*>(nullptr));
	}
	std::fputs("Não foi possível abrir o painel de configurações.\n", stderr);
	return 1;
}
}
