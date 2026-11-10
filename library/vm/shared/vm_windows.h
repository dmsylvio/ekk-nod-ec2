#ifndef VM_WINDOWS_H
#define VM_WINDOWS_H

#include "../vm.h"

//
// wmwin is private header used by vm.cpp only
//
namespace vmwin
{
	inline QWORD read_i32w(vm_handle process, QWORD address) { return (QWORD)vm::read_i32(process, address); }
	inline QWORD get_module(vm_handle process, PCSTR dll_name)
	{
		QWORD peb = vm::get_wow64_process(process);

		DWORD a0[6]{};
		QWORD a1, a2, a4;
		unsigned char a3[240]{};

		QWORD(*read_ptr)(vm_handle process, QWORD address) = 0;
		if (peb)
		{
			read_ptr = read_i32w;
			a0[0] = 0x04, a0[1] = 0x0C, a0[2] = 0x14, a0[3] = 0x28, a0[4] = 0x10, a0[5] = 0x20;
		}
		else
		{
			read_ptr = vm::read_i64;
			peb = vm::get_peb(process);
			a0[0] = 0x08, a0[1] = 0x18, a0[2] = 0x20, a0[3] = 0x50, a0[4] = 0x20, a0[5] = 0x40;
		}

		if (peb == 0)
		{
			return 0;
		}

		a1 = read_ptr(process, peb + a0[1]);
		if (a1 == 0)
		{
			return 0;
		}

		a1 = read_ptr(process, a1 + a0[2]);
		if (a1 == 0)
		{
			return 0;
		}


		//
		// return first list entry, if parameter is null
		//
		if (dll_name == 0)
			return read_ptr(process, a1 + a0[4]);


		int name_length = (int)strlen_imp(dll_name);
		if (name_length > 119)
			name_length = 119;


		a2 = read_ptr(process, a1 + a0[0]);
		while (a1 != a2)
		{
			a4 = read_ptr(process, a1 + a0[3]);

			//
			// if no module name, we skip the module
			//
			if (a4 == 0)
			{
				goto next_entry;
			}

			vm::read(process, a4, a3, (name_length*2)+2);
			wcs2str((short*)a3, name_length+1);
			a3[name_length+1] = 0;

			if (strcmpi_imp((PCSTR)a3, dll_name) == 0)
			{
				return read_ptr(process, a1 + a0[4]);
			}
		next_entry:
			a1 = read_ptr(process, a1);
			if (a1 == 0)
			{
				break;
			}
		}
		return 0;
	}

	// Resource ceiling for untrusted image metadata (512 MiB).
	static const QWORD max_image_size = 512ULL * 1024 * 1024;
	inline bool fits(QWORD size, QWORD offset, QWORD count)
	{
		return offset <= size && count <= size - offset;
	}
	template<class T> inline bool local_read(const BYTE* data, QWORD size, QWORD at, T& out)
	{
		if (!fits(size, at, sizeof(T))) return false;
		memcpy(&out, data + at, sizeof(T));
		return true;
	}
	struct pe_info { DWORD nt, image_size, headers_size; WORD sections, optional_size, magic; QWORD table; };
	inline bool parse_pe(const BYTE* data, QWORD size, pe_info& info)
	{
		WORD mz = 0; DWORD signature = 0;
		if (!local_read(data, size, 0, mz) || mz != 0x5A4D ||
		    !local_read(data, size, 0x3C, info.nt) ||
		    !local_read(data, size, info.nt, signature) || signature != 0x4550 ||
		    !local_read(data, size, QWORD(info.nt) + 6, info.sections) ||
		    !local_read(data, size, QWORD(info.nt) + 20, info.optional_size) ||
		    !local_read(data, size, QWORD(info.nt) + 24, info.magic)) return false;
		if ((info.magic != 0x10B && info.magic != 0x20B) ||
		    info.optional_size < (info.magic == 0x20B ? 240 : 224) ||
		    !info.sections || info.sections > 96 ||
		    !local_read(data, size, QWORD(info.nt) + 0x50, info.image_size) ||
		    !local_read(data, size, QWORD(info.nt) + 0x54, info.headers_size)) return false;
		info.table = QWORD(info.nt) + 24 + info.optional_size;
		return info.image_size && info.image_size <= max_image_size &&
		       info.headers_size <= info.image_size &&
		       fits(info.headers_size, info.table, QWORD(info.sections) * 40) &&
		       fits(size, info.table, QWORD(info.sections) * 40);
	}
	inline void release_image(BYTE* allocation)
	{
	#ifdef _KERNEL_MODE
		ExFreePoolWithTag(allocation, 'ofnI');
	#else
		free(allocation);
	#endif
	}
	inline PVOID dump_module(vm_handle process, QWORD base, VM_MODULE_TYPE module_type)
	{
		// Read a bounded header prefix, then the complete declared headers.
		BYTE dos[64]{}; DWORD nt = 0;
		if (!base || base > ~(QWORD)0 - max_image_size ||
		    !vm::read(process, base, dos, sizeof(dos))) return 0;
		memcpy(&nt, dos + 0x3C, sizeof(nt));
		if (dos[0] != 'M' || dos[1] != 'Z' || nt < 64 || nt > 1024 * 1024) return 0;
		BYTE prefix[88]{};
		if (!vm::read(process, base + nt, prefix, sizeof(prefix))) return 0;
		DWORD image_size = 0, headers_size = 0;
		memcpy(&image_size, prefix + 0x50, 4);
		memcpy(&headers_size, prefix + 0x54, 4);
		if (!image_size || image_size > max_image_size || headers_size > image_size ||
		    headers_size < QWORD(nt) + sizeof(prefix) || headers_size > 1024 * 1024) return 0;
	#ifdef _KERNEL_MODE
		BYTE* allocation = (BYTE*)ExAllocatePoolWithTag(NonPagedPoolNx, QWORD(image_size) + 16, 'ofnI');
	#else
		BYTE* allocation = (BYTE*)malloc(QWORD(image_size) + 16);
	#endif
		if (!allocation) return 0;
		BYTE* data = allocation + 16;
		memset(data, 0, image_size);
		pe_info info{};
		if (!vm::read(process, base, data, headers_size) ||
		    !parse_pe(data, headers_size, info) || info.image_size != image_size ||
		    info.headers_size != headers_size) { release_image(allocation); return 0; }
		for (WORD i = 0; i < info.sections; ++i)
		{
			QWORD section = info.table + QWORD(i) * 40;
			DWORD flags = 0, rva = 0, bytes = 0, raw = 0, raw_size = 0;
			local_read(data, headers_size, section + 36, flags);
			local_read(data, headers_size, section + 12, rva);
			local_read(data, headers_size, section + 8, bytes);
			local_read(data, headers_size, section + 20, raw);
			local_read(data, headers_size, section + 16, raw_size);
			if (module_type == VM_MODULE_TYPE::CodeSectionsOnly && !(flags & 0x20)) continue;
			if (module_type == VM_MODULE_TYPE::ReadOnly &&
			    (!(flags & 0x40000000) || (flags & 0xA2000000))) continue;
			QWORD destination = rva;
			if (module_type == VM_MODULE_TYPE::Raw) { destination = raw; if (raw_size < bytes) bytes = raw_size; }
			if (!bytes) continue;
			if (destination < headers_size || !fits(image_size, destination, bytes) ||
			    !fits(image_size, rva, bytes) ||
			    !vm::read(process, base + rva, data + destination, bytes))
			{ release_image(allocation); return 0; }
		}
		memcpy(allocation, &base, 8);
		QWORD stored_size = image_size; memcpy(allocation + 8, &stored_size, 8);
		return data;
	}

	inline void free_module(PVOID dumped_module)
	{
		if (!dumped_module)
			return;
		QWORD a0 = (QWORD)dumped_module;

		a0 -= 16;
	#ifdef _KERNEL_MODE
		ExFreePoolWithTag((void*)a0, 'ofnI');
	#else
		free((void*)a0);
	#endif
	}

	inline QWORD get_dump_export(PVOID dumped_module, PCSTR export_name)
	{
		if (!dumped_module || !export_name || !*export_name) return 0;
		BYTE* data = (BYTE*)dumped_module;
		QWORD size = 0, base = 0;
		memcpy(&size, data - 8, 8); memcpy(&base, data - 16, 8);
		pe_info info{};
		if (size > max_image_size || !parse_pe(data, size, info) || info.image_size != size) return 0;
		QWORD directory = QWORD(info.nt) + (info.magic == 0x20B ? 0x88 : 0x78);
		DWORD rva = 0, directory_size = 0;
		if (!local_read(data, size, directory, rva) || !rva ||
		    !local_read(data, size, directory + 4, directory_size) || directory_size < 40 ||
		    !fits(size, rva, directory_size)) return 0;
		DWORD functions = 0, names = 0, function_table = 0, name_table = 0, ordinal_table = 0;
		local_read(data, size, QWORD(rva) + 20, functions);
		local_read(data, size, QWORD(rva) + 24, names);
		local_read(data, size, QWORD(rva) + 28, function_table);
		local_read(data, size, QWORD(rva) + 32, name_table);
		local_read(data, size, QWORD(rva) + 36, ordinal_table);
		if (!fits(size, function_table, QWORD(functions) * 4) ||
		    !fits(size, name_table, QWORD(names) * 4) || !fits(size, ordinal_table, QWORD(names) * 2)) return 0;
		for (DWORD i = 0; i < names; ++i)
		{
			DWORD name = 0; WORD ordinal = 0;
			local_read(data, size, QWORD(name_table) + QWORD(i) * 4, name);
			if (name >= size) return 0;
			QWORD n = 0;
			while (n < size - name && data[name + n]) ++n;
			if (n == size - name) return 0;
			if (strcmpi_imp((PCSTR)data + name, export_name)) continue;
			local_read(data, size, QWORD(ordinal_table) + QWORD(i) * 2, ordinal);
			if (ordinal >= functions) return 0;
			DWORD target = 0;
			local_read(data, size, QWORD(function_table) + QWORD(ordinal) * 4, target);
			// Forwarded exports need a separate resolver, never return their string as code.
			if (!target || target >= size || (target >= rva && target - rva < directory_size) ||
			    base > ~(QWORD)0 - target) return 0;
			return base + target;
		}
		return 0;
	}
	inline QWORD get_module_export(vm_handle process, QWORD base, PCSTR export_name)
	{
		if (!export_name || !*export_name) return 0;
		PVOID dump = dump_module(process, base, VM_MODULE_TYPE::Full);
		if (!dump) return 0;
		QWORD result = get_dump_export(dump, export_name);
		free_module(dump);
		return result;
	}
	inline QWORD scan_pattern(PVOID dumped_module, PCSTR pattern, PCSTR mask, QWORD length)
	{
		if (!dumped_module || !pattern || !mask || !length) return 0;
		QWORD mask_size = 0; while (mask_size < length && mask[mask_size]) ++mask_size;
		if (mask_size != length || mask[mask_size]) return 0;
		BYTE* data = (BYTE*)dumped_module;
		QWORD size = 0, base = 0;
		memcpy(&size, data - 8, 8); memcpy(&base, data - 16, 8);
		pe_info info{};
		if (size > max_image_size || !parse_pe(data, size, info) || info.image_size != size) return 0;
		for (WORD i = 0; i < info.sections; ++i)
		{
			QWORD section = info.table + QWORD(i) * 40;
			DWORD flags = 0, rva = 0, bytes = 0;
			local_read(data, size, section + 36, flags);
			local_read(data, size, section + 12, rva);
			local_read(data, size, section + 8, bytes);
			if (!fits(size, rva, bytes)) return 0;
			if (!(flags & 0x20) || !bytes) continue;
			QWORD address = utils::FindPatternEx((QWORD)(data + rva), bytes, (BYTE*)pattern, (char*)mask);
			if (address) { QWORD offset = address - (QWORD)data; return base <= ~(QWORD)0 - offset ? base + offset : 0; }
		}
		return 0;
	}

	QWORD scan_pattern_direct(vm_handle process, QWORD base, PCSTR pattern, PCSTR mask, DWORD length)
	{
		if (base == 0)
		{
			return 0;
		}
	
		PVOID dumped_module = dump_module(process, base, VM_MODULE_TYPE::CodeSectionsOnly);
		if (dumped_module == 0)
		{
			return 0;
		}

		QWORD patt = scan_pattern(dumped_module, pattern, mask, length);

		free_module(dumped_module);
		return patt;
	}
}



#endif

