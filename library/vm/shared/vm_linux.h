#ifndef VM_LINUX_H
#define VM_LINUX_H

#include "../vm.h"
#include <elf.h>
#include <byteswap.h>

//
// wmwin is private header used by vm.cpp only
//
namespace vmlinux
{
	static const QWORD max_image_size = 512ULL * 1024 * 1024;
	inline bool add_ok(QWORD address, QWORD bytes) { return address <= ~(QWORD)0 - bytes; }
	template<class T> inline bool read_value(vm_handle process, QWORD at, T& value)
	{
		value = {};
		return add_ok(at, sizeof(T)) && vm::read(process, at, &value, sizeof(T));
	}
	inline bool elf_class(vm_handle process, QWORD base, BYTE& kind)
	{
		BYTE ident[EI_NIDENT]{};
		if (!base || !add_ok(base, sizeof(ident)) || !vm::read(process, base, ident, sizeof(ident)) ||
		    ident[0] != 0x7f || ident[1] != 'E' || ident[2] != 'L' || ident[3] != 'F' ||
		    ident[EI_DATA] != ELFDATA2LSB || ident[EI_VERSION] != EV_CURRENT) return false;
		kind = ident[EI_CLASS];
		return kind == ELFCLASS32 || kind == ELFCLASS64;
	}
	template<class E, class P> inline QWORD find_segment(vm_handle process, QWORD base, DWORD tag, P* found)
	{
		E header{};
		if (!read_value(process, base, header) || header.e_ehsize != sizeof(E) ||
		    header.e_phentsize != sizeof(P) || !header.e_phnum || header.e_phnum > 1024 ||
		    header.e_phoff > max_image_size || !add_ok(base, header.e_phoff) ||
		    !add_ok(base + header.e_phoff, QWORD(header.e_phnum) * sizeof(P))) return 0;
		for (WORD i = 0; i < header.e_phnum; ++i)
		{
			P segment{}; QWORD at = base + header.e_phoff + QWORD(i) * sizeof(P);
			if (!read_value(process, at, segment)) return 0;
			if (segment.p_type == tag) { if (found) *found = segment; return at; }
		}
		return 0;
	}
	inline QWORD get_elf_address(vm_handle process, QWORD base, int tag)
	{
		BYTE kind = 0; if (!elf_class(process, base, kind)) return 0;
		return kind == ELFCLASS32 ? find_segment<Elf32_Ehdr, Elf32_Phdr>(process, base, tag, 0) :
			find_segment<Elf64_Ehdr, Elf64_Phdr>(process, base, tag, 0);
	}
	template<class E, class P, class D> inline QWORD dynamic_value(vm_handle process, QWORD base, QWORD tag)
	{
		P segment{};
		if (!find_segment<E, P>(process, base, PT_DYNAMIC, &segment) ||
		    !segment.p_memsz || segment.p_memsz > max_image_size ||
		    !add_ok(base, segment.p_vaddr) || !add_ok(base + segment.p_vaddr, segment.p_memsz)) return 0;
		QWORD count = segment.p_memsz / sizeof(D);
		if (count > 65536) return 0;
		for (QWORD i = 0; i < count; ++i)
		{
			D entry{};
			if (!read_value(process, base + segment.p_vaddr + i * sizeof(D), entry) || entry.d_tag == DT_NULL) return 0;
			if (QWORD(entry.d_tag) == tag) return entry.d_un.d_val;
		}
		return 0;
	}
	inline QWORD get_dyn_address(vm_handle process, QWORD base, QWORD tag)
	{
		BYTE kind = 0; if (!elf_class(process, base, kind)) return 0;
		return kind == ELFCLASS32 ? dynamic_value<Elf32_Ehdr, Elf32_Phdr, Elf32_Dyn>(process, base, tag) :
			dynamic_value<Elf64_Ehdr, Elf64_Phdr, Elf64_Dyn>(process, base, tag);
	}
	template<class S> inline QWORD export_value(vm_handle process, QWORD base, PCSTR name)
	{
		QWORD strings = get_dyn_address(process, base, DT_STRTAB);
		QWORD symbols = get_dyn_address(process, base, DT_SYMTAB);
		QWORD string_size = get_dyn_address(process, base, DT_STRSZ);
		QWORD entry_size = get_dyn_address(process, base, DT_SYMENT);
		if (!strings || !symbols || !string_size || string_size > max_image_size ||
		    entry_size != sizeof(S) || !add_ok(strings, string_size)) return 0;
		// Loaded dynamic pointer entries are absolute. Bound work even without DT_HASH.
		QWORD count = 1048576;
		QWORD hash = get_dyn_address(process, base, DT_HASH);
		if (hash) { DWORD chains = 0; if (!add_ok(hash, 4) || !read_value(process, hash + 4, chains) || chains > count) return 0; count = chains; }
		else if (strings > symbols) { QWORD until_strings = (strings - symbols) / sizeof(S); if (until_strings < count) count = until_strings; }
		if (!add_ok(symbols, count * sizeof(S))) return 0;
		for (QWORD i = 0; i < count; ++i)
		{
			S symbol{};
			if (!read_value(process, symbols + i * sizeof(S), symbol)) return 0;
			if (!symbol.st_name || symbol.st_name >= string_size || symbol.st_shndx == SHN_UNDEF) continue;
			char buffer[120]{}; bool terminated = false;
			QWORD available = string_size - symbol.st_name;
			for (QWORD n = 0; n < sizeof(buffer) && n < available; ++n) {
				if (!read_value(process, strings + symbol.st_name + n, buffer[n])) return 0;
				if (!buffer[n]) { terminated = true; break; }
			}
			if (!terminated || strcmpi_imp(buffer, name)) continue;
			if (!symbol.st_value) return 0;
			if (symbol.st_shndx == SHN_ABS) return symbol.st_value;
			return add_ok(base, symbol.st_value) ? base + symbol.st_value : 0;
		}
		return 0;
	}
	inline QWORD get_module_export(vm_handle process, QWORD base, PCSTR export_name)
	{
		BYTE kind = 0;
		if (!export_name || !*export_name || !elf_class(process, base, kind)) return 0;
		return kind == ELFCLASS32 ? export_value<Elf32_Sym>(process, base, export_name) : export_value<Elf64_Sym>(process, base, export_name);
	}
	template<class E, class P> inline PVOID dump_image(vm_handle process, QWORD base, VM_MODULE_TYPE mode)
	{
		E header{};
		if (!read_value(process, base, header) || header.e_ehsize != sizeof(E) ||
		    header.e_phentsize != sizeof(P) || !header.e_phnum || header.e_phnum > 1024 ||
		    header.e_phoff > max_image_size || !add_ok(base, header.e_phoff) ||
		    !add_ok(base + header.e_phoff, QWORD(header.e_phnum) * sizeof(P))) return 0;
		QWORD size = 0;
		for (WORD i = 0; i < header.e_phnum; ++i) {
			P segment{};
			if (!read_value(process, base + header.e_phoff + QWORD(i) * sizeof(P), segment)) return 0;
			if (segment.p_type != PT_LOAD) continue;
			if (segment.p_vaddr > max_image_size || segment.p_memsz > max_image_size - segment.p_vaddr ||
			    segment.p_filesz > segment.p_memsz) return 0;
			QWORD end = QWORD(segment.p_vaddr) + segment.p_memsz; if (end > size) size = end;
		}
		if (!size || !add_ok(base, size)) return 0;
		BYTE* allocation = (BYTE*)malloc(size + 16);
		if (!allocation) return 0;
		BYTE* data = allocation + 16; memset(data, 0, size);
		for (WORD i = 0; i < header.e_phnum; ++i) {
			P segment{};
			if (!read_value(process, base + header.e_phoff + QWORD(i) * sizeof(P), segment)) { free(allocation); return 0; }
			if (segment.p_type != PT_LOAD) continue;
			if (mode == VM_MODULE_TYPE::CodeSectionsOnly && !(segment.p_flags & PF_X)) continue;
			if (mode == VM_MODULE_TYPE::ReadOnly && (!(segment.p_flags & PF_R) || (segment.p_flags & (PF_W | PF_X)))) continue;
			// Revalidate metadata on the second read; the source may have changed.
			if (segment.p_vaddr > size || segment.p_memsz > size - segment.p_vaddr ||
			    (segment.p_memsz && !vm::read(process, base + segment.p_vaddr, data + segment.p_vaddr, segment.p_memsz)))
			{ free(allocation); return 0; }
		}
		memcpy(allocation, &base, 8); memcpy(allocation + 8, &size, 8);
		return data;
	}
	inline PVOID dump_module(vm_handle process, QWORD base, VM_MODULE_TYPE module_type)
	{
		BYTE kind = 0; if (!elf_class(process, base, kind) || module_type == VM_MODULE_TYPE::Raw) return 0;
		return kind == ELFCLASS32 ? dump_image<Elf32_Ehdr, Elf32_Phdr>(process, base, module_type) :
			dump_image<Elf64_Ehdr, Elf64_Phdr>(process, base, module_type);
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

	inline QWORD scan_pattern(PVOID dumped_module, PCSTR pattern, PCSTR mask, QWORD length)
	{
		if (!dumped_module || !pattern || !mask || !length)
			return 0;
		QWORD base    = *(QWORD*)((QWORD)dumped_module - 16);
		QWORD size    = *(QWORD*)((QWORD)dumped_module - 8);
		if (length > size || strlen_imp(mask) != length)
			return 0;
		QWORD address = utils::FindPatternEx((QWORD)dumped_module, size, (BYTE*)pattern, (char*)mask);
		if (address)
		{
			return (address - (QWORD)dumped_module) + base;
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

