#include "../shared/linux_process_handle.h"
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include "../shared/vm_windows.h"

int get_process_id(const char *process_name)
{
	int process_id = 0;

	DIR *dir = opendir("/proc/");
	if (dir == 0)
	{
		return 0;
	}

	struct dirent *a0;
	while ((a0 = readdir(dir)))
	{
		if (a0->d_type != DT_DIR && a0->d_type != DT_UNKNOWN)
		{
			continue;
		}
		
		char buffer[266]{};
		snprintf(buffer, sizeof(buffer), "/proc/%s/exe", a0->d_name);

		char executable[sizeof(buffer)]{};
		ssize_t link_length = readlink(buffer, executable, sizeof(executable) - 1);
		if (link_length < 0 || size_t(link_length) >= sizeof(executable) - 1) continue;
		executable[link_length] = 0;

		const char *name = strrchr(executable, '/');
		if (name == 0)
		{
			continue;
		}

		int pid = atoi(a0->d_name);
		if (pid < 1 || pid > 2147483647)
		{
			continue;
		}

		if (!strcmp(name + 1, process_name))
		{
			process_id = pid;
			break;
		}
	}
	closedir(dir);
	return process_id;
}

static size_t
read_line(int fd, char *buffer, size_t length)
{
	if (!buffer || length < 2) return 0;
	size_t pos = 0; bool truncated = false;
	for (;;) {
		char ch = 0;
		ssize_t result = read(fd, &ch, 1);
		if (result < 0 && errno == EINTR) continue;
		if (result < 0) { buffer[0] = 0; return 0; }
		if (!result || ch == '\n') {
			buffer[pos] = 0;
			if (!truncated) return pos;
			// Discard an overlong line entirely; never parse its partial fields.
			if (!result) return 0;
			pos = 0; truncated = false; continue;
		}
		if (pos + 1 < length) buffer[pos++] = ch;
		else truncated = true;
	}
}

QWORD get_module_base(int process_id, const char *module_name)
{
	QWORD base = 0;

	char buffer[512]{};
	snprintf(buffer, sizeof(buffer), "/proc/%d/maps", process_id);

	int fd = open(buffer, O_RDONLY);
	if (fd == -1)
	{
		return 0;
	}

	while (read_line(fd, buffer, sizeof(buffer)))
	{
		const char *name = strrchr(buffer, '/');

		if (name == 0)
		{
			continue;
		}

		if (!strcmpi_imp(name + 1, module_name))
		{
			name = buffer;
			base = strtoul(buffer, (char**)&name, 16);
			break;
		}
	}

	close(fd);

	return base;
}

int get_process_id2(const char *process_name, const char *module_name)
{
	int process_id = 0;

	DIR *dir = opendir("/proc/");
	if (dir == 0)
	{
		return 0;
	}

	struct dirent *a0;
	while ((a0 = readdir(dir)))
	{
		if (a0->d_type != DT_DIR && a0->d_type != DT_UNKNOWN)
		{
			continue;
		}
		
		char buffer[266]{};
		snprintf(buffer, sizeof(buffer), "/proc/%s/exe", a0->d_name);

		char executable[sizeof(buffer)]{};
		ssize_t link_length = readlink(buffer, executable, sizeof(executable) - 1);
		if (link_length < 0 || size_t(link_length) >= sizeof(executable) - 1) continue;
		executable[link_length] = 0;

		const char *name = strrchr(executable, '/');
		if (name == 0)
		{
			continue;
		}

		int pid = atoi(a0->d_name);
		if (pid < 1 || pid > 2147483647)
		{
			continue;
		}

		if (strcmp(name + 1, process_name))
		{
			continue;
		}

		QWORD base = get_module_base(pid, module_name);
		if (base == 0)
		{
			continue;
		}
		process_id = pid;
		break;
	}
	closedir(dir);
	return process_id;
}

VmOs vm::get_target_os(void)
{
	return VmOs::Windows;
}

BOOL vm::process_exists(PCSTR process_name)
{
	return get_process_id(process_name) != 0;
}

vm_handle vm::open_process(PCSTR process_name)
{
	//
	// we don't care about process_name really
	//
	int pid = get_process_id2("wine64-preloader", "EOSSDK-Win64-Shipping.dll"); // get_process_id(process_name);
	if (pid == 0)
	{
		return 0;
	}

	char dir[23];
	snprintf(dir, sizeof(dir), "/proc/%d/mem", pid);

	int fd = open(dir, O_RDWR);
	if (fd == -1)
	{
		return 0;
	}

	return make_linux_process_handle(fd, pid);
}

vm_handle vm::open_process_ex(PCSTR process_name, PCSTR dll_name)
{
	int pid = get_process_id2("wine64-preloader", "EOSSDK-Win64-Shipping.dll"); // get_process_id(process_name);

	if (pid == 0)
	{
		return 0;
	}

	char dir[23];
	snprintf(dir, sizeof(dir), "/proc/%d/mem", pid);

	int fd = open(dir, O_RDWR);
	if (fd == -1)
	{
		return 0;
	}

	return make_linux_process_handle(fd, pid);
}

inline void close_file(int fd)
{
	close(fd);
}

void vm::close(vm_handle process)
{
	if (!process) return;
	auto* handle = (linux_process_handle*)process;
	close_file(handle->fd); free(handle);
}

BOOL vm::running(vm_handle process)
{
	if (!process) return 0;
	auto* handle = (linux_process_handle*)process;
	return fcntl(handle->fd, F_GETFD) >= 0 && linux_start_ticks(handle->pid) == handle->start_ticks;
}

BOOL vm::read(vm_handle process, QWORD address, PVOID buffer, QWORD length)
{
	if (!process || !buffer || !length || length > SSIZE_MAX || address > LLONG_MAX || length > QWORD(LLONG_MAX) - address) return 0;
	return pread(((linux_process_handle*)process)->fd, buffer, length, address) == (ssize_t)length;
}

BOOL vm::write(vm_handle process, QWORD address, PVOID buffer, QWORD length)
{
	if (!process || !buffer || !length || length > SSIZE_MAX || address > LLONG_MAX || length > QWORD(LLONG_MAX) - address) return 0;
	return pwrite(((linux_process_handle*)process)->fd, buffer, length, address) == (ssize_t)length;
}

QWORD vm::get_peb(vm_handle process)
{
	/*
	QWORD peb[6]{};

	if (NtQueryInformationProcess(process, 0, &peb, 48, 0) != 0)
	{
		return 0;
	}

	return peb[1];
	*/
	return 0;
}

QWORD vm::get_wow64_process(vm_handle process)
{
	/*
	QWORD wow64_process = 0;

	if (process == 0)
		return wow64_process;

	if (NtQueryInformationProcess(process, 26, &wow64_process, 8, 0) != 0)
	{
		return 0;
	}

	return wow64_process;
	*/
	return 0;
}

QWORD vm::get_module(vm_handle process, PCSTR dll_name)
{
	return (QWORD)0x140000000; // return vmwin::get_module(process, dll_name);
}

QWORD vm::get_module_export(vm_handle process, QWORD base, PCSTR export_name)
{
	return vmwin::get_module_export(process, base, export_name);
}

PVOID vm::dump_module(vm_handle process, QWORD base, VM_MODULE_TYPE module_type)
{
	return vmwin::dump_module(process, base, module_type);
}

void vm::free_module(PVOID dumped_module)
{
	return vmwin::free_module(dumped_module);
}

QWORD vm::get_dump_export(PVOID dumped_module, PCSTR export_name)
{
	return vmwin::get_dump_export(dumped_module, export_name);
}

QWORD vm::scan_pattern(PVOID dumped_module, PCSTR pattern, PCSTR mask, QWORD length)
{
	return vmwin::scan_pattern(dumped_module, pattern, mask, length);
}

QWORD vm::scan_pattern_direct(vm_handle process, QWORD base, PCSTR pattern, PCSTR mask, DWORD length)
{
	return vmwin::scan_pattern_direct(process, base, pattern, mask, length);
}

