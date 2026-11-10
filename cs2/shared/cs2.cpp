#include "cs2.h"

namespace cs2
{
	vm_handle game_handle = 0;

	namespace sdl
	{
		static QWORD sdl_window;
		static QWORD sdl_mouse;
	}

	namespace interfaces
	{
		static QWORD resource;
		static QWORD entity;
		static QWORD cvar;
		static QWORD input;
		static QWORD player;
	}

	namespace direct
	{
		static QWORD local_player;
		static QWORD view_angles;
		static QWORD view_matrix;
		static DWORD button_state;
	}

	namespace convars
	{
		static QWORD sensitivity;
		static QWORD mp_teammates_are_enemies;
		static QWORD cl_crosshairalpha;
	}

	namespace netvars
	{
		static int m_flFOVSensitivityAdjust = 0;
		static int m_pGameSceneNode = 0;
		static int m_iHealth = 0;
		static int m_lifeState = 0;
		static int m_iTeamNum = 0;
		static int m_vecViewOffset = 0;
		static int m_vecOrigin = 0;
		static int m_bDormant = 0;
		static int m_hPawn = 0;
		static int m_modelState = 0;
		static int m_aimPunchCache = 0;
		static int m_iShotsFired = 0;
		static int m_vOldOrigin = 0;
		static int m_pClippingWeapon = 0;
		static int v_angle = 0;

		static int m_bIsDefusing = 0;
		static int m_bPawnHasDefuser = 0;
		static int m_hActiveWeapon = 0;
		static int m_pWeaponServices = 0;
	}

	template<class T> static T read_dump(PVOID dump, QWORD address)
	{
		T value{}; QWORD size = 0;
		memcpy(&size, (BYTE*)dump - 8, 8);
		QWORD start = (QWORD)dump;
		if (address < start || address - start > size || sizeof(T) > size - (address - start)) return value;
		memcpy(&value, (const void*)address, sizeof(T));
		return value;
	}
	static bool dump_name_equals(PVOID dump, PCSTR name, PCSTR expected)
	{
		QWORD size = 0; memcpy(&size, (BYTE*)dump - 8, 8);
		QWORD start = (QWORD)dump, address = (QWORD)name;
		if (address < start || address - start >= size) return false;
		QWORD remaining = size - (address - start);
		for (QWORD i = 0; i < remaining; ++i) {
			if (to_lower_imp((BYTE)name[i]) != to_lower_imp((BYTE)expected[i])) return false;
			if (!name[i]) return true;
		}
		return false;
	}

	static void reset_session(void)
	{
		if (game_handle)
			vm::close(game_handle);
		game_handle = 0;
		sdl::sdl_window = 0;
		sdl::sdl_mouse = 0;
		interfaces::resource = 0;
		interfaces::entity = 0;
		interfaces::cvar = 0;
		interfaces::input = 0;
		interfaces::player = 0;
		direct::local_player = 0;
		direct::view_angles = 0;
		direct::view_matrix = 0;
		direct::button_state = 0;
		convars::sensitivity = 0;
		convars::mp_teammates_are_enemies = 0;
		convars::cl_crosshairalpha = 0;
		netvars::m_flFOVSensitivityAdjust = 0;
		netvars::m_pGameSceneNode = 0;
		netvars::m_iHealth = 0;
		netvars::m_lifeState = 0;
		netvars::m_iTeamNum = 0;
		netvars::m_vecViewOffset = 0;
		netvars::m_vecOrigin = 0;
		netvars::m_bDormant = 0;
		netvars::m_hPawn = 0;
		netvars::m_modelState = 0;
		netvars::m_aimPunchCache = 0;
		netvars::m_iShotsFired = 0;
		netvars::m_vOldOrigin = 0;
		netvars::m_pClippingWeapon = 0;
		netvars::v_angle = 0;
		netvars::m_bIsDefusing = 0;
		netvars::m_bPawnHasDefuser = 0;
		netvars::m_hActiveWeapon = 0;
		netvars::m_pWeaponServices = 0;
	}

	static BOOL initialize(void);
	static QWORD get_interface(QWORD base, PCSTR name);
	QWORD get_interface_function(QWORD interface_address, DWORD index);
}

BOOL cs2::running(void)
{
	return cs2::initialize();
}

/*
optional code:
interfaces::player = get_interface(vm::get_module(game_handle, "client.dll"), "Source2ClientPrediction0");
interfaces::player = vm::get_relative_address(game_handle, get_interface_function(interfaces::player, 120) + 0x16, 3, 7);
interfaces::player = vm::read_i64(game_handle, interfaces::player);
*/


inline const char *get_process_name() { return vm::get_target_os() == VmOs::Windows ? "cs2.exe" : "cs2"; }
inline const char *get_client_name() { return vm::get_target_os() == VmOs::Windows ? "client.dll" : "libclient.so"; }
inline const char *get_engine_name() { return vm::get_target_os() == VmOs::Windows ? "engine2.dll" : "libengine2.so"; }
inline const char *get_sdl3_name() { return vm::get_target_os() == VmOs::Windows ? "SDL3.dll" : "libSDL3.so.0"; }
inline const char *get_tier0_name() { return vm::get_target_os() == VmOs::Windows ? "tier0.dll" : "libtier0.so"; }
inline const char *get_inputsystem_name() { return vm::get_target_os() == VmOs::Windows ? "inputsystem.dll" : "libinputsystem.so"; }
inline int get_entity_off() { return vm::get_target_os() == VmOs::Windows ? 0x58 : 0x50; }
inline int get_button_off() { return vm::get_target_os() == VmOs::Windows ? 0x13 : 0x14; }
inline int get_viewangles_off() { return vm::get_target_os() == VmOs::Windows ? 0x6140 : 0x4528; }

#ifdef DEBUG

#define JZ(val,field) \
if ((val) == 0)  \
{ \
LOG(__FILE__ ": line %d: initialization failed: %s\n", __LINE__, #val); \
goto field; \
} \

#else

#define JZ(val,field) \
	if ((val) == 0) goto field;

#endif

static BOOL cs2::initialize(void)
{
	if (game_handle)
	{
		if (vm::running(game_handle))
		{
			return 1;
		}
		reset_session();
	}
	reset_session();

	// Verificar se o processo "cs2.exe" está em execução
	if (!vm::process_exists(get_process_name()))
	{
		LOG("CS2 process not found - process check failed\n");
		return 0;
	}

	game_handle = vm::open_process_ex(get_process_name(), get_client_name());
	if (!game_handle)
	{
		LOG("CS2 detected, but opening the process or locating client.dll/libclient.so failed\n");
		return 0;
	}

	QWORD client_dll, sdl, inputsystem;
	JZ(client_dll = vm::get_module(game_handle, get_client_name()), E1);
	JZ(sdl = vm::get_module(game_handle, get_sdl3_name()), E1);
	JZ(inputsystem = vm::get_module(game_handle, get_inputsystem_name()), E1);
	interfaces::resource = get_interface(vm::get_module(game_handle, get_engine_name()), "GameResourceServiceClientV0");
	if (interfaces::resource == 0)
	{
		LOG(__FILE__ ": line %d\n", __LINE__);
	E1:
		reset_session();
		return 0;
	}
	
	if (vm::get_target_os() == VmOs::Linux)
	{
		JZ(sdl::sdl_window   = vm::get_module_export(game_handle, sdl, "SDL_GetKeyboardFocus"), E1);
		sdl::sdl_window      = vm::get_relative_address(game_handle, sdl::sdl_window, 2, 6);
		JZ(sdl::sdl_window   = vm::read_i64(game_handle, sdl::sdl_window), E1);
		sdl::sdl_window      = vm::get_relative_address(game_handle, sdl::sdl_window, 3, 7);
	}
	else
	{
		JZ(sdl::sdl_window   = vm::get_module_export(game_handle, sdl, "SDL_GetKeyboardFocus"), E1);
		sdl::sdl_window      = vm::get_relative_address(game_handle, sdl::sdl_window, 3, 7);
		JZ(sdl::sdl_window   = vm::read_i64(game_handle, sdl::sdl_window), E1);
		sdl::sdl_window      = vm::get_relative_address(game_handle, sdl::sdl_window, 3, 7);

		JZ(sdl::sdl_mouse     = vm::get_module_export(game_handle, sdl, "SDL_GetMouseState"), E1);
		sdl::sdl_mouse        = vm::get_relative_address(game_handle, sdl::sdl_mouse, 3, 7);
		JZ(sdl::sdl_mouse     = vm::read_i64(game_handle, sdl::sdl_mouse), E1);
		sdl::sdl_mouse        = vm::get_relative_address(game_handle, sdl::sdl_mouse + 0x10, 1, 5);
		sdl::sdl_mouse        = vm::get_relative_address(game_handle, sdl::sdl_mouse + 0x00, 3, 7);
	}

	JZ(interfaces::entity   = vm::read_i64(game_handle, interfaces::resource + get_entity_off()), E1);
	interfaces::player      = interfaces::entity + 0x10;
	JZ(interfaces::cvar     = get_interface(vm::get_module(game_handle, get_tier0_name()), "VEngineCvar0"), E1);
	JZ(interfaces::input    = get_interface(inputsystem, "InputSystemVersion0"), E1);
	{
		QWORD function = get_interface_function(interfaces::input,
			vm::get_target_os() == VmOs::Windows ? 18 : 19);
		if (!function || function > ~(QWORD)0 - get_button_off() ||
		    !vm::read(game_handle, function + get_button_off(),
		              &direct::button_state, sizeof(direct::button_state)) ||
		    !direct::button_state || interfaces::input > ~(QWORD)0 - direct::button_state)
		{
			LOG("CS2 input initialization failed: function lookup, displacement read, or address validation.\n");
			goto E1;
		}
	}

	if (vm::get_target_os() == VmOs::Linux)
	{
		JZ(direct::local_player = vm::scan_pattern_direct(game_handle, client_dll, "\x48\x83\x3D\x00\x00\x00\x00\x00\x0F\x95\xC0\xC3", "xxx????xxxxx", 12), E1);
		direct::local_player    = vm::get_relative_address(game_handle, direct::local_player, 3, 8);
	}
	else
	{
		/*
		JZ(direct::local_player = get_interface(client_dll, "Source2ClientPrediction0"), E1);
		JZ(direct::local_player = get_interface_function(direct::local_player, 180), E1);
		direct::local_player    = vm::get_relative_address(game_handle, direct::local_player + 0xF0, 3, 7);
		*/
		JZ(direct::local_player = vm::scan_pattern_direct(game_handle, client_dll, "\x48\x83\x3D\x00\x00\x00\x00\x00\x0F\x95\xC0\xC3", "xxx????xxxxx", 12), E1);
		direct::local_player    = vm::get_relative_address(game_handle, direct::local_player, 3, 8);
	}

	JZ(direct::view_angles  = get_interface(client_dll, "Source2Client0"), E1);
	direct::view_angles     = vm::get_relative_address(game_handle, get_interface_function(direct::view_angles, 16), 3, 7);
	JZ(direct::view_angles  = vm::read_i64(game_handle, direct::view_angles), E1);
	direct::view_angles     += get_viewangles_off();


	if (vm::get_target_os() == VmOs::Linux)
	{
		//
		// viewmatrix: 48 8d 05 ? ? ? ? 4c 8d 05 ? ? ? ? 48 8d 0d
		//
		JZ(direct::view_matrix = vm::scan_pattern_direct(game_handle, client_dll, "\x48\x8D\x05\x00\x00\x00\x00\x4C\x8D\x05\x00\x00\x00\x00\x48\x8D\x0D", "xxx????xxx????xxx", 17), E1);
		direct::view_matrix    = vm::get_relative_address(game_handle, direct::view_matrix + 0x7, 3, 7);
	}
	else
	{
		//
		// viewmatrix: 48 63 c2 48 8d 0d ? ? ? ? 48 c1
		// Fallback pattern: 48 8d 0d ? ? ? ? 48 8b
		//
		direct::view_matrix = vm::scan_pattern_direct(game_handle, client_dll, "\x48\x63\xc2\x48\x8d\x0d\x00\x00\x00\x00\x48\xc1", "xxxxxx????xx", 12);
		
		// Se o primeiro padrão falhar, tenta o padrão alternativo
		if (direct::view_matrix == 0)
		{
			JZ(direct::view_matrix = vm::scan_pattern_direct(game_handle, client_dll, "\x48\x8D\x0D\x00\x00\x00\x00\x48\x8B", "xxx????xx", 9), E1);
			direct::view_matrix = vm::get_relative_address(game_handle, direct::view_matrix, 3, 7);
		}
		else
		{
			direct::view_matrix = vm::get_relative_address(game_handle, direct::view_matrix + 0x03, 3, 7);
		}
	}

	if (!direct::view_matrix)
	{
		LOG("CS2 view-matrix discovery failed after the available lookup strategies. A zero result does not distinguish dump/read failure from no pattern match.\n");
		goto E1;
	}

	JZ(convars::sensitivity              = engine::get_convar("sensitivity"), E1);
	JZ(convars::mp_teammates_are_enemies = engine::get_convar("mp_teammates_are_enemies"), E1);
	JZ(convars::cl_crosshairalpha        = engine::get_convar("cl_crosshairalpha"), E1);

	//
	// to-do schemas
	//
	LOG("interfaces::cvar       %p\n", (void *)interfaces::cvar);
	LOG("interfaces::input      %p\n", (void *)interfaces::input);
	LOG("interfaces::resource   %p\n", (void *)interfaces::resource);
	LOG("interfaces::entity     %p\n", (void *)interfaces::entity);
	LOG("interfaces::player     %p\n", (void *)interfaces::player);
	LOG("direct::local_player   %p\n", (void *)direct::local_player);
	LOG("direct::view_angles    %p\n", (void *)direct::view_angles);
	LOG("direct::view_matrix    %p\n", (void *)direct::view_matrix);


	if (vm::get_target_os() == VmOs::Linux)
	{
		PVOID dump_client = vm::dump_module(game_handle, client_dll, VM_MODULE_TYPE::Full);
		if (dump_client == 0)
		{
			goto E1;
		}

		QWORD base = *(QWORD*)((QWORD)dump_client - 16);
		QWORD size = *(QWORD*)((QWORD)dump_client - 8);
		for (QWORD i = size >= 32 ? size - 32 : 0; i--;)
		{
			QWORD entry    = ((QWORD)dump_client + i);
		
			BOOL network_enable = 0;
			{
				QWORD ptr_to_name = read_dump<QWORD>(dump_client, entry);
				if (ptr_to_name >= base && ptr_to_name <= (base + size))
				{
					ptr_to_name = read_dump<QWORD>(dump_client, (ptr_to_name - base) + (QWORD)dump_client);
					if (ptr_to_name >= base && ptr_to_name <= (base + size))
					{
						if (dump_name_equals(dump_client, (const char*)(ptr_to_name - base) + (QWORD)dump_client, "MNetworkEnable"))
						{
							network_enable = 1;
						}
					}
				}
			}

			QWORD name_ptr;
			if (network_enable == 0)
			{
				name_ptr = read_dump<QWORD>(dump_client, entry);
				if (name_ptr < base)
				{
					continue;
				}
				if (name_ptr > (base + size))
				{
					continue;
				}
			}
			else
			{
				name_ptr = read_dump<QWORD>(dump_client, entry + 0x08);
				if (name_ptr < base)
				{
					continue;
				}
				if (name_ptr > (base + size))
				{
					continue;
				}
			}

			PCSTR netvar_name = (PCSTR)((name_ptr - base) + (QWORD)dump_client);

			if (!netvars::m_iHealth && dump_name_equals(dump_client, netvar_name, "m_iHealth") && network_enable)
			{
				LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, entry + 0x08 + 0x10));
				netvars::m_iHealth = read_dump<int>(dump_client, entry + 0x08 + 0x10);
			}
			else if (!netvars::m_iTeamNum && dump_name_equals(dump_client, netvar_name, "m_iTeamNum") && network_enable)
			{
				LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, entry + 0x08 + 0x10));
				netvars::m_iTeamNum = read_dump<int>(dump_client, entry + 0x08 + 0x10);
			}
			else if (!netvars::m_hPawn && dump_name_equals(dump_client, netvar_name, "m_hPawn") && network_enable)
			{
				LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, entry + 0x08 + 0x10));
				netvars::m_hPawn = read_dump<int>(dump_client, entry + 0x08 + 0x10);
			}
			else if (!netvars::m_lifeState && dump_name_equals(dump_client, netvar_name, "m_lifeState") && network_enable)
			{
				LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, entry + 0x08 + 0x10));
				netvars::m_lifeState = read_dump<int>(dump_client, entry + 0x08 + 0x10);
			}
			else if (!netvars::m_vecViewOffset && dump_name_equals(dump_client, netvar_name, "m_vecViewOffset") && network_enable)
			{
				LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, entry + 0x08 + 0x10));
				netvars::m_vecViewOffset = read_dump<int>(dump_client, entry + 0x08 + 0x10);
			}
			else if (!netvars::m_aimPunchCache && dump_name_equals(dump_client, netvar_name, "m_aimPunchCache") && network_enable)
			{
				LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, entry + 0x08 + 0x10));
				netvars::m_aimPunchCache = read_dump<int>(dump_client, entry + 0x08 + 0x10);
			}
			else if (!netvars::m_iShotsFired && dump_name_equals(dump_client, netvar_name, "m_iShotsFired") && network_enable)
			{
				LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, entry + 0x08 + 0x10));
				netvars::m_iShotsFired = read_dump<int>(dump_client, entry + 0x08 + 0x10);
			}
			else if (!netvars::m_flFOVSensitivityAdjust && dump_name_equals(dump_client, netvar_name, "m_flFOVSensitivityAdjust"))
			{
				LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, entry + 0x08));
				netvars::m_flFOVSensitivityAdjust = read_dump<int>(dump_client, entry + 0x08);
			}
			else if (!netvars::m_pGameSceneNode && dump_name_equals(dump_client, netvar_name, "m_pGameSceneNode"))
			{
				LOG("%s, %x\n", netvar_name, read_dump<WORD>(dump_client, entry + 0x10));
				netvars::m_pGameSceneNode = read_dump<WORD>(dump_client, entry + 0x10);
			}
			else if (!netvars::m_bDormant && dump_name_equals(dump_client, netvar_name, "m_bDormant"))
			{
				LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, entry + 0x08));
				netvars::m_bDormant = read_dump<int>(dump_client, entry + 0x08);
			}
			else if (!netvars::m_modelState && dump_name_equals(dump_client, netvar_name, "m_modelState"))
			{
				LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, entry + 0x08));
				netvars::m_modelState = read_dump<int>(dump_client, entry + 0x08);
			}
			else if (!netvars::m_vecOrigin && dump_name_equals(dump_client, netvar_name, "m_vecOrigin") && network_enable)
			{
				LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, entry + 0x08 + 0x10));
				netvars::m_vecOrigin = read_dump<int>(dump_client, entry + 0x08 + 0x10);
				if (netvars::m_vecOrigin > 0x200) netvars::m_vecOrigin = 0;
			}
			else if (!netvars::m_vOldOrigin && dump_name_equals(dump_client, netvar_name, "m_vOldOrigin"))
			{
				LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, entry + 0x08));
				netvars::m_vOldOrigin = read_dump<int>(dump_client, entry + 0x08);
			}
			else if (!netvars::m_pClippingWeapon && dump_name_equals(dump_client, netvar_name, "m_pClippingWeapon"))
			{
				LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, entry + 0x10));
				netvars::m_pClippingWeapon = read_dump<int>(dump_client, entry + 0x10);
			}
			else if (!netvars::v_angle && dump_name_equals(dump_client, netvar_name, "v_angle"))
			{
				LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, entry + 0x08));
				netvars::v_angle = read_dump<int>(dump_client, entry + 0x08);
			}
		}
		vm::free_module(dump_client);
	}
	else
	{
		PVOID dump_client = vm::dump_module(game_handle, client_dll, VM_MODULE_TYPE::Full);
		if (dump_client == 0)
		{
			goto E1;
		}
	
		QWORD dos_header = (QWORD)dump_client;
		QWORD nt_header = (QWORD)read_dump<DWORD>(dump_client, dos_header + 0x03C) + dos_header;
		WORD  machine = read_dump<WORD>(dump_client, nt_header + 0x4);

		QWORD section_header = machine == 0x8664 ?
			nt_header + 0x0108 :
			nt_header + 0x00F8;

		for (WORD i = 0; i < read_dump<WORD>(dump_client, nt_header + 0x06); i++) {
			QWORD section = section_header + ((QWORD)i * 40);
			PCSTR name = (PCSTR)section;

			if (!dump_name_equals(dump_client, name, ".data"))
			{
				continue;
			}
			
			DWORD section_size = read_dump<DWORD>(dump_client, section + 0x08);
			DWORD section_addr = read_dump<DWORD>(dump_client, section + 0x0C);

			QWORD image_size = *(QWORD*)(dos_header - 8);
			if (section_addr > image_size || section_size > image_size - section_addr) { vm::free_module(dump_client); goto E1; }
			for (QWORD j = section_addr; j + 32 <= QWORD(section_addr) + section_size; j += sizeof(QWORD))
			{	
				BOOL network_enable = 0;
				{
					QWORD ptr_to_name = read_dump<QWORD>(dump_client, dos_header + j);
					if (ptr_to_name >= client_dll && ptr_to_name <= (client_dll + *(QWORD*)(dos_header - 8)))
					{
						ptr_to_name = read_dump<QWORD>(dump_client, (ptr_to_name - client_dll) + dos_header);
						if (ptr_to_name >= client_dll && ptr_to_name <= (client_dll + *(QWORD*)(dos_header - 8)))
						{
							if (dump_name_equals(dump_client, (const char*)(ptr_to_name - client_dll) + dos_header, "MNetworkEnable"))
							{
								network_enable = 1;
							}
						}
					}
				}

				QWORD ptr_to_name;
				if (network_enable == 0)
				{
					ptr_to_name = read_dump<QWORD>(dump_client, dos_header + j + 0x00);
					if (ptr_to_name < client_dll)
					{
						continue;
					}	
					if (ptr_to_name > (client_dll + *(QWORD*)(dos_header - 8)))
					{
						continue;
					}
				}
				else
				{
					ptr_to_name = read_dump<QWORD>(dump_client, dos_header + j + 0x08);
					if (ptr_to_name < client_dll)
					{
						continue;
					}	
					if (ptr_to_name > (client_dll + *(QWORD*)(dos_header - 8)))
					{
						continue;
					}
				}

				PCSTR netvar_name = (PCSTR)((ptr_to_name - client_dll) + dos_header);

				if (!netvars::m_iHealth && dump_name_equals(dump_client, netvar_name, "m_iHealth") && network_enable)
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10));
					netvars::m_iHealth = read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10);
				}
				else if (!netvars::m_iTeamNum && dump_name_equals(dump_client, netvar_name, "m_iTeamNum") && network_enable)
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10));
					netvars::m_iTeamNum = read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10);
				}
				else if (!netvars::m_hPawn && dump_name_equals(dump_client, netvar_name, "m_hPawn") && network_enable)
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10));
					netvars::m_hPawn = read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10);
				}
				else if (!netvars::m_lifeState && dump_name_equals(dump_client, netvar_name, "m_lifeState") && network_enable)
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10));
					netvars::m_lifeState = read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10);
				}
				else if (!netvars::m_vecViewOffset && dump_name_equals(dump_client, netvar_name, "m_vecViewOffset") && network_enable)
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10));
					netvars::m_vecViewOffset = read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10);
				}
				else if (!netvars::m_aimPunchCache && dump_name_equals(dump_client, netvar_name, "m_aimPunchCache") && network_enable)
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10));
					netvars::m_aimPunchCache = read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10);
				}
				else if (!netvars::m_iShotsFired && dump_name_equals(dump_client, netvar_name, "m_iShotsFired") && network_enable)
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10));
					netvars::m_iShotsFired = read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10);
				}
				else if (!netvars::m_flFOVSensitivityAdjust && dump_name_equals(dump_client, netvar_name, "m_flFOVSensitivityAdjust"))
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x08));
					netvars::m_flFOVSensitivityAdjust = read_dump<int>(dump_client, dos_header + j + 0x08);
				}
				else if (!netvars::m_pGameSceneNode && dump_name_equals(dump_client, netvar_name, "m_pGameSceneNode"))
				{
					LOG("%s, %x\n", netvar_name, read_dump<WORD>(dump_client, dos_header + j + 0x10));
					netvars::m_pGameSceneNode = read_dump<WORD>(dump_client, dos_header + j + 0x10);
				}
				else if (!netvars::m_bDormant && dump_name_equals(dump_client, netvar_name, "m_bDormant"))
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x10));
					netvars::m_bDormant = read_dump<int>(dump_client, dos_header + j + 0x10);
				}
				else if (!netvars::m_modelState && dump_name_equals(dump_client, netvar_name, "m_modelState"))
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x10));
					netvars::m_modelState = read_dump<int>(dump_client, dos_header + j + 0x10);
				}
				else if (!netvars::m_vecOrigin && dump_name_equals(dump_client, netvar_name, "m_vecOrigin") && network_enable)
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10));
					netvars::m_vecOrigin = read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10);

					if (netvars::m_vecOrigin > 0x200) netvars::m_vecOrigin = 0;
				}
				else if (!netvars::m_vOldOrigin && dump_name_equals(dump_client, netvar_name, "m_vOldOrigin"))
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x10));
					netvars::m_vOldOrigin = read_dump<int>(dump_client, dos_header + j + 0x10);
				}
				else if (!netvars::m_pClippingWeapon && dump_name_equals(dump_client, netvar_name, "m_pClippingWeapon"))
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x10));
					netvars::m_pClippingWeapon = read_dump<int>(dump_client, dos_header + j + 0x10);
				}
				else if (!netvars::v_angle && dump_name_equals(dump_client, netvar_name, "v_angle"))
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x10));
					netvars::v_angle = read_dump<int>(dump_client, dos_header + j + 0x10);
				}
				else if (!netvars::m_bIsDefusing && network_enable && dump_name_equals(dump_client, netvar_name, "m_bIsDefusing"))
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10));
					netvars::m_bIsDefusing = read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10);
				}
				else if (!netvars::m_bPawnHasDefuser && network_enable && dump_name_equals(dump_client, netvar_name, "m_bPawnHasDefuser"))
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10));
					netvars::m_bPawnHasDefuser = read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10);
				}
				else if (!netvars::m_hActiveWeapon && dump_name_equals(dump_client, netvar_name, "m_hActiveWeapon"))
				{
					LOG("%s, %x\n", netvar_name, read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10));
					netvars::m_hActiveWeapon = read_dump<int>(dump_client, dos_header + j + 0x08 + 0x10);
				}
				else if (
					(netvars::m_pWeaponServices < 0x1000 || netvars::m_pWeaponServices > 0x2000) &&
					dump_name_equals(dump_client, netvar_name, "m_pWeaponServices"))
				{
					netvars::m_pWeaponServices = read_dump<int>(dump_client, dos_header + j + 0x10);
				}
			}
		}
		vm::free_module(dump_client);
	}

	JZ(netvars::m_flFOVSensitivityAdjust, E1);
	JZ(netvars::m_pGameSceneNode, E1);
	JZ(netvars::m_iHealth, E1);
	JZ(netvars::m_lifeState, E1);
	JZ(netvars::m_iTeamNum, E1);
	JZ(netvars::m_vecViewOffset, E1);
	JZ(netvars::m_vecOrigin, E1);
	JZ(netvars::m_bDormant, E1);
	JZ(netvars::m_hPawn, E1);
	JZ(netvars::m_modelState, E1);
	JZ(netvars::m_aimPunchCache, E1);
	JZ(netvars::m_iShotsFired, E1);
	JZ(netvars::m_vOldOrigin, E1);
	JZ(netvars::m_pClippingWeapon, E1);
	JZ(netvars::v_angle, E1);
	if (vm::get_target_os() == VmOs::Windows) { JZ(netvars::m_bIsDefusing, E1); }
	if (vm::get_target_os() == VmOs::Windows) { JZ(netvars::m_bPawnHasDefuser, E1); }
	if (vm::get_target_os() == VmOs::Windows) { JZ(netvars::m_hActiveWeapon, E1); }
	if (vm::get_target_os() == VmOs::Windows) { JZ(netvars::m_pWeaponServices, E1); }



	LOG("CS2 initialization complete\n");
	return 1;
}

QWORD cs2::sdl::get_window(void)
{
	return vm::read_i64(game_handle, sdl::sdl_window);
}

BOOL cs2::sdl::get_window_info(QWORD window, WINDOW_INFO *info)
{
	typedef struct 
	{
		int x,y,w,h;
	} wvec4 ;

	wvec4 buffer{};
	if (!vm::read(game_handle, window + 0x18, &buffer, sizeof(buffer)))
		return 0;

	info->x = (float)buffer.x;
	info->y = (float)buffer.y;
	info->w = (float)buffer.w;
	info->h = (float)buffer.h;

	return 1;
}

QWORD cs2::sdl::get_window_data(QWORD window)
{
	return vm::read_i64(game_handle, window + 0x168);
}

QWORD cs2::sdl::get_hwnd(QWORD window_data)
{
	return vm::read_i64(game_handle, window_data + 0x08);
}

QWORD cs2::sdl::get_hdc(QWORD window_data)
{
	return vm::read_i64(game_handle, window_data + 0x10);
}

QWORD cs2::sdl::get_mouse(void)
{
	/*
	    0x60 float x;
	    0x64 float y;
	    0x68 float xdelta;
	    0x6C float ydelta;
	    0x70 float last_x, last_y;
	    0x78 SDL_bool has_position;
	    0x79 SDL_bool relative_mode;
	    0x7A SDL_bool relative_mode_warp;
	    0x7B SDL_bool relative_mode_warp_motion;
	    0x7C SDL_bool enable_normal_speed_scale;
	*/
	return sdl::sdl_mouse;
}

BOOL cs2::sdl::get_mouse_button(QWORD mouse)
{
	if (!game_handle || !mouse || mouse > ~(QWORD)0 - 0xD8)
		return 0;
	DWORD count = 0, buttonstate = 0;
	const DWORD max_sources = 256; // Defensive work budget.
	QWORD sources = mouse + 0xD8;
	if (!vm::read(game_handle, mouse + 0xD0, &count, sizeof(count)) ||
	    count > max_sources || sources > ~(QWORD)0 - QWORD(count) * 8)
		return 0;
	for (DWORD i = 0; i < count; ++i)
	{
		QWORD source = 0;
		DWORD id = 0, state = 0;
		if (!vm::read(game_handle, sources + QWORD(i) * 8, &source, sizeof(source)) ||
		    !source || source > ~(QWORD)0 - 4 ||
		    !vm::read(game_handle, source, &id, sizeof(id)) ||
		    !vm::read(game_handle, source + 4, &state, sizeof(state)))
			return 0;
		if (id != (DWORD)-1)
			buttonstate |= state;
	}
	return buttonstate;
}

QWORD cs2::engine::get_convar(const char *name)
{
	if (!game_handle || !interfaces::cvar || !name) {
		LOG("convar lookup: missing session, interface, or name\n"); return 0;
	}
	QWORD name_length = 0;
	while (name_length < 120 && name[name_length]) ++name_length;
	if (!name_length || name_length >= 120 || interfaces::cvar > ~(QWORD)0 - 160) {
		LOG("convar lookup: invalid name length or interface address\n"); return 0;
	}
	QWORD objs = 0; DWORD count = 0;
	const DWORD max_convars = 65536;
	if (!vm::read(game_handle, interfaces::cvar + 64, &objs, sizeof(objs))) {
		LOG("convar [%s]: table pointer read failed\n", name); return 0;
	}
	if (!vm::read(game_handle, interfaces::cvar + 160, &count, sizeof(count))) {
		LOG("convar [%s]: count read failed\n", name); return 0;
	}
	if (!objs || count > max_convars || objs > ~(QWORD)0 - QWORD(count) * 16) {
		LOG("convar [%s]: invalid table metadata (count=%u, table_present=%d)\n", name, (unsigned)count, objs != 0); return 0;
	}
	DWORD empty = 0;
	for (DWORD i = 0; i < count; ++i) {
		QWORD entry = 0, source = 0;
		if (!vm::read(game_handle, objs + QWORD(i) * 16, &entry, sizeof(entry))) {
			LOG("convar [%s]: entry read failed at index %u\n", name, (unsigned)i); return 0;
		}
		if (!entry) { ++empty; continue; }
		if (!vm::read(game_handle, entry, &source, sizeof(source))) {
			LOG("convar [%s]: name pointer read failed at index %u\n", name, (unsigned)i); return 0;
		}
		if (!source) { ++empty; continue; }
		if (source > ~(QWORD)0 - name_length) {
			LOG("convar [%s]: name address overflow at index %u\n", name, (unsigned)i); return 0;
		}
		// Stop at the first mismatch or terminator; do not read past short names.
		bool matches = true;
		for (QWORD n = 0; n <= name_length; ++n) {
			BYTE ch = 0;
			if (!vm::read(game_handle, source + n, &ch, sizeof(ch))) {
				LOG("convar [%s]: name read failed at index %u, byte %u\n", name, (unsigned)i, (unsigned)n); return 0;
			}
			if (to_lower_imp(ch) != to_lower_imp((BYTE)name[n])) { matches = false; break; }
			if (!ch) break;
		}
		if (matches) return entry;
	}
	LOG("convar [%s]: not found (count=%u, empty=%u); table layout compatibility is not verified\n", name, (unsigned)count, (unsigned)empty);
	return 0;
}

DWORD cs2::engine::get_current_ms(void)
{
	QWORD function = get_interface_function(interfaces::input, 16);
	DWORD offset = 0, value = 0;
	if (!function || function > ~(QWORD)0 - 2 ||
	    !vm::read(game_handle, function + 2, &offset, sizeof(offset)) ||
	    !offset || interfaces::input > ~(QWORD)0 - offset ||
	    !vm::read(game_handle, interfaces::input + offset, &value, sizeof(value)))
		return 0;
	return value;
}

vec2 cs2::engine::get_viewangles(void)
{
	vec2 va{};
	if (!vm::read(game_handle, direct::view_angles, &va, sizeof(va)))
	{
		va = {};
	}
	return va;
}

view_matrix_t cs2::engine::get_viewmatrix(void)
{
	view_matrix_t matrix{};

	vm::read(game_handle, direct::view_matrix, &matrix, sizeof(matrix));

	return matrix;
}

QWORD cs2::entity::get_local_player_controller(void)
{
	return vm::read_i64(game_handle, direct::local_player);
}

QWORD cs2::entity::get_client_entity(int index)
{
	if (index < 0 || index > 0x7FFF || !interfaces::entity) return 0;
	QWORD v2 = vm::read_i64(game_handle, interfaces::entity + QWORD(8) * (index >> 9) + 16);
	if (v2 == 0)
		return 0;

	return vm::read_i64(game_handle, (QWORD)(120 * (index & 0x1FF) + v2));
}

BOOL cs2::entity::is_player(QWORD controller)
{
	/*
	if (vm::get_target_os() == VmOs::Linux)
	{
		return 1;
	}

	QWORD vfunc = get_interface_function(controller, 144);
	if (vfunc == 0)
		return 0;

	DWORD value = vm::read_i32(game_handle, vfunc);
	//
	// mov al, 0x1
	// ret
	// cc
	//
	return value == 0xCCC301B0;
	*/

	//
	// deprecated function, just compiler fix
	//
	return controller != 0;
}

QWORD cs2::entity::get_player(QWORD controller)
{
	//
	// 5DC offset might be changing in time
	// 8B 91 ? 05 00 00 83
	//
	DWORD v1 = vm::read_i32(game_handle, controller + netvars::m_hPawn);
	if (v1 == (DWORD)(-1))
	{
		return 0;
	}

	QWORD v2 = vm::read_i64(game_handle, interfaces::player + 8 * ((QWORD)(v1 & 0x7FFF) >> 9));
	if (v2 == 0)
	{
		return 0;
	}
	return vm::read_i64(game_handle, v2 + 120 * (v1 & 0x1FF));
}

BOOL cs2::entity::has_defuser(QWORD entity)
{
	if (!entity || !netvars::m_bPawnHasDefuser) return 0;
	return (BOOL)vm::read_i8(game_handle, entity + netvars::m_bPawnHasDefuser);
}

int cs2::get_crosshairalpha(void)
{
	return vm::read_i32(game_handle, convars::cl_crosshairalpha + 0x40);
}

float cs2::mouse::get_sensitivity(void)
{
	return vm::read_float(game_handle, convars::sensitivity + 0x40);
}

BOOL cs2::gamemode::is_ffa(void)
{
	return vm::read_i32(game_handle, convars::mp_teammates_are_enemies + 0x40) == 1;
}

BOOL cs2::input::is_button_down(DWORD button)
{
	DWORD v = vm::read_i32(game_handle, (QWORD)(interfaces::input + (((QWORD(button) >> 5) * 4) + direct::button_state)));
	return (v >> (button & 31)) & 1;
}

void cs2::input::move(int x, int y)
{
	// remove compiler warning
	x = 0;
	y = 0;
}

DWORD cs2::player::get_health(QWORD player)
{
	return vm::read_i32(game_handle, player + netvars::m_iHealth);
}

DWORD cs2::player::get_team_num(QWORD player)
{
	return vm::read_i32(game_handle, player + netvars::m_iTeamNum);
}

int cs2::player::get_life_state(QWORD player)
{
	return vm::read_i32(game_handle, player + netvars::m_lifeState);
}

vec3 cs2::player::get_origin(QWORD player)
{
	vec3 value{};
	if (!vm::read(game_handle, player + netvars::m_vOldOrigin, &value, sizeof(value)))
	{
		value = {};
	}
	return value;
}

float cs2::player::get_vec_view(QWORD player)
{
	return vm::read_float(game_handle, player + netvars::m_vecViewOffset + 8);
}

vec3 cs2::player::get_eye_position(QWORD player)
{
	vec3 origin = get_origin(player);
	origin.z += get_vec_view(player);
	return origin;
}

DWORD cs2::player::get_shots_fired(QWORD player)
{
	return vm::read_i32(game_handle, player + netvars::m_iShotsFired);
}

float cs2::player::get_fov_multipler(QWORD player)
{
	return vm::read_float(game_handle, player + netvars::m_flFOVSensitivityAdjust);
}

vec2 cs2::player::get_vec_punch(QWORD player)
{
	vec2 data{};

	QWORD aim_punch_cache[2]{};
	if (!vm::read(game_handle, player + netvars::m_aimPunchCache, &aim_punch_cache, sizeof(aim_punch_cache)))
	{
		return data;
	}

	DWORD aimpunch_size = ((DWORD*)&aim_punch_cache[0])[0];
	if (aimpunch_size < 1 || aimpunch_size > 4096 || !aim_punch_cache[1] ||
	    aim_punch_cache[1] > ~(QWORD)0 - QWORD(aimpunch_size) * 12)
	{
		return data;
	}

	if (aimpunch_size == 129)
		aimpunch_size = 130;

	if (!vm::read(game_handle, aim_punch_cache[1] + (QWORD(aimpunch_size-1) * 12), &data, sizeof(data)))
	{
		data = {};
	}

	return data;
}

vec2 cs2::player::get_viewangle(QWORD player)
{
	vec2 value{};
	if (!vm::read(game_handle, player + netvars::v_angle, &value, sizeof(value)))
	{
		value = {};
	}
	return value;
}

cs2::WEAPON_CLASS cs2::player::get_weapon_class(QWORD player)
{
	QWORD weapon = vm::read_i64(game_handle, player + netvars::m_pClippingWeapon);

	if (weapon == 0)
	{
		return cs2::WEAPON_CLASS::Invalid;
	}

	//
	// C_EconEntity::m_AttributeManager + C_AttributeContainer::m_Item + C_EconItemView::m_iItemDefinitionIndex
	//
	WORD weapon_index = vm::read_i16(game_handle, weapon + 0x1140 + 0x50 + 0x1BA);

	/* knife */
	{
		WORD data[] = {
			42, // {"knife"},
			59,  //{"knife_t"},
		};
		for (int i = 0; i < sizeof(data) / sizeof(*data); i++)
		{
			if (data[i] == weapon_index)
			{
				return cs2::WEAPON_CLASS::Knife;
			}
		}
	}

	/* grenade */
	{
		WORD data[] = {
			44, // {"hegrenade"},
			43, // {"flashbang"},
			45, // {"smokegrenade"},
			47, // {"decoy"},
			46, // {"molotov"},
			48, // {"incgrenade"},
			49, // {"c4"},
		};
		for (int i = 0; i < sizeof(data) / sizeof(*data); i++)
		{
			if (data[i] == weapon_index)
			{
				return cs2::WEAPON_CLASS::Grenade;
			}
		}
	}

	/* pistol */
	{
		WORD data[] = {
			40, // {"ssg08"}, scout and deagle, in my opinion definitely belongs to same category
			32, // {"hkp2000"},
			61, // {"usp-s"},
			1, // {"deagle"},
			36, // {"p250"},
			2, // {"elite"},
			3, // {"fiveseven"},
			4, // {"glock"},
			30, // {"tec9"},
		};
		for (int i = 0; i < sizeof(data) / sizeof(*data); i++)
		{
			if (data[i] == weapon_index)
			{
				return cs2::WEAPON_CLASS::Pistol;
			}
		}
	}

	/* sniper */
	{
		WORD data[] = {
			9, // {"awp"},
			38, // {"scar20"},
			11, // {"g3sg1"},
		};
		for (int i = 0; i < sizeof(data) / sizeof(*data); i++)
		{
			if (data[i] == weapon_index)
			{
				return cs2::WEAPON_CLASS::Sniper;
			}
		}
	}

	return cs2::WEAPON_CLASS::Rifle;
}

QWORD cs2::player::get_node(QWORD player)
{
	return vm::read_i64(game_handle, player + netvars::m_pGameSceneNode);
}

QWORD cs2::player::get_weapon_address(QWORD player)
{
	if (!player || !netvars::m_pWeaponServices || !netvars::m_hActiveWeapon) return 0;
	QWORD weapon_services = vm::read_i64(game_handle, player + netvars::m_pWeaponServices);
	if (!weapon_services)
		return 0;

	int index = vm::read_i32(game_handle, weapon_services + netvars::m_hActiveWeapon) & 0xFFF;
	return cs2::entity::get_client_entity(index);
}

BOOL cs2::player::is_defusing(QWORD player)
{
	if (!player || !netvars::m_bIsDefusing) return 0;
	return (BOOL)vm::read_i8(game_handle, player + netvars::m_bIsDefusing);
}

BOOL cs2::player::is_valid(QWORD player, QWORD node)
{
	if (!player || !node)
	{
		return 0;
	}

	DWORD player_health = get_health(player);
	if (player_health < 1)
	{
		return 0;
	}

	if (player_health > 150)
	{
		return 0;
	}

	int player_lifestate = get_life_state(player);
	if (player_lifestate != 256)
	{
		return 0;
	}

	return node::get_dormant(node) == 0;
}

BOOLEAN cs2::node::get_dormant(QWORD node)
{
	BYTE value = 1;
	if (!node || !netvars::m_bDormant || node > ~(QWORD)0 - netvars::m_bDormant ||
	    !vm::read(game_handle, node + netvars::m_bDormant, &value, sizeof(value))) return 1;
	return value;
}

vec3 cs2::node::get_origin(QWORD node)
{
	vec3 val{};
	if (!vm::read(game_handle, node + netvars::m_vecOrigin, &val, sizeof(val)))
	{
		val = {};
	}
	return val;
}

BOOL cs2::node::get_bone_position(QWORD node, int index, vec3 *data)
{
	// get skeleton 
	// QWORD fun = get_interface_function(node, 8);
	//
	// get skeleton:
	// mov rax, rcx
	// ret
	//
	if (!node || !data || index < 0 || index > 4095) return 0;
	QWORD skeleton    = node;
	QWORD model_state = skeleton + netvars::m_modelState;
	QWORD bone_data   = vm::read_i64(game_handle, model_state + 0x80);

	if (bone_data == 0)
		return 0;

	return vm::read(game_handle, bone_data + (QWORD(index) * 32), data, sizeof(vec3));
}

static QWORD cs2::get_interface(QWORD base, PCSTR name)
{
	if (!base || !name || !game_handle)
	{
		return 0;
	}

	QWORD export_address = vm::get_module_export(game_handle, base, "CreateInterface");
	if (export_address == 0)
	{
		return 0;
	}

	if (vm::get_target_os() == VmOs::Linux)
	{
		export_address = vm::get_relative_address(game_handle, export_address, 1, 5);
		export_address += 0x10;
	}
	
	QWORD registry_address = vm::get_relative_address(game_handle, export_address, 3, 7);
	if (!registry_address) {
		LOG("interface [%s]: invalid registry displacement or read failure\n", name); return 0;
	}
	QWORD interface_entry = vm::read_i64(game_handle, registry_address);
	if (interface_entry == 0)
	{
		return 0;
	}

	QWORD name_length = 0;
	while (name_length < 120 && name[name_length])
		++name_length;
	if (!name_length || name_length >= 120)
		return 0;

	for (DWORD visited = 0; visited < 4096; ++visited)
	{
		char interface_name[120]{};
		QWORD source = 0;
		if (interface_entry > ~(QWORD)0 - 16 ||
		    !vm::read(game_handle, interface_entry + 8, &source, sizeof(source)) || !source ||
		    !vm::read(game_handle, source, interface_name, name_length))
			return 0;

		interface_name[name_length] = 0;
		
		if (!strcmpi_imp(interface_name, name))
		{
			//
			// lea    rax, [rip+0xXXXXXX]
			// ret
			//
			QWORD vfunc = vm::read_i64(game_handle, interface_entry);
			if (!vfunc || vfunc > ~(QWORD)0 - 7)
				return 0;


			//
			// emulate vfunc call
			//
			QWORD addr = vm::get_relative_address(game_handle, vfunc, 3, 7);
			if (!addr) LOG("interface [%s]: invalid factory displacement or read failure\n", name);
			return addr;
		}

		interface_entry = vm::read_i64(game_handle, interface_entry + 16);
		if (interface_entry == 0)
			break;
	}
	return 0;
}

QWORD cs2::get_interface_function(QWORD interface_address, DWORD index)
{
	if (!game_handle || !interface_address)
		return 0;
	QWORD table = 0, function = 0;
	if (!vm::read(game_handle, interface_address, &table, sizeof(table)) || !table ||
	    table > ~(QWORD)0 - QWORD(index) * 8)
		return 0;
	if (!vm::read(game_handle, table + QWORD(index) * 8, &function, sizeof(function)))
		return 0;
	return function;
}

