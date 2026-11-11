#include <string>
#include <vector>
#include <random>
#include "../../cs2/shared/cs2game.h"
#include "../../csgo/shared/csgogame.h"
#include "../../apex/shared/apexgame.h"

#include <SDL3/SDL.h>
#pragma comment(lib, "SDL3.lib")
#include <tlhelp32.h>

static SDL_Surface* sdl_surface;

namespace
{
    struct KeyOption { const wchar_t* label; int key; };
    constexpr KeyOption keys[] = {
        {L"Mouse 1 (Left)", VK_LBUTTON},
        {L"Mouse 3 (Middle)", VK_MBUTTON}, {L"Mouse 4", VK_XBUTTON1}, {L"Mouse 5", VK_XBUTTON2},
        {L"Alt", VK_MENU}, {L"Ctrl", VK_CONTROL}, {L"Shift", VK_SHIFT},
        {L"F6", VK_F6}, {L"F7", VK_F7}, {L"F9", VK_F9}, {L"F10", VK_F10},
        {L"C", 'C'}, {L"V", 'V'}, {L"X", 'X'}, {L"Z", 'Z'}
    };
    constexpr const wchar_t* setting_names[] = {L"Aim", L"Trigger", L"Overlay"};
    int bindings[] = {VK_XBUTTON2, VK_XBUTTON1, VK_F7};
    wchar_t settings_path[MAX_PATH]{};
    int region_weights[] = {40, 50, 10};
    constexpr const wchar_t* region_names[] = {L"Head", L"Body", L"Arms / Legs"};
    constexpr const wchar_t* weight_names[] = {L"Head", L"Body", L"Limbs"};
    bool overlay_visible = true;
    bool overlay_was_down = false;
    bool game_focused = false;
    constexpr COLORREF panel_bg = RGB(232, 237, 245);
    constexpr COLORREF panel_text = RGB(25, 38, 56);
    constexpr COLORREF panel_muted = RGB(72, 89, 112);
    HFONT panel_heading_font = nullptr;

    struct GameInstance
    {
        DWORD pid;
        std::wstring path;
        bool operator==(const GameInstance& other) const
        {
            return pid == other.pid && path == other.path;
        }
    };
    std::vector<GameInstance> instances;
    std::wstring preferred_path;
    wchar_t profiles_path[MAX_PATH]{};
    HWND game_combo = nullptr;
    HWND game_status = nullptr;
    DWORD selected_pid = 0;
    ULONGLONG next_attach = 0;
    ULONGLONG next_refresh = 0;

    std::wstring process_path(HANDLE process)
    {
        wchar_t path[MAX_PATH]{};
        DWORD length = _countof(path);
        return QueryFullProcessImageNameW(process, 0, path, &length) ?
            std::wstring(path, length) : std::wstring{};
    }

    void refresh_instances()
    {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return;
        std::vector<GameInstance> current;
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                if (_wcsicmp(entry.szExeFile, L"cs2.exe")) continue;
                HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
                if (!process) continue;
                auto path = process_path(process);
                CloseHandle(process);
                if (!path.empty()) current.push_back({entry.th32ProcessID, std::move(path)});
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
        if (current == instances) return;
        instances = std::move(current);
        if (!game_combo) return;
        SendMessageW(game_combo, CB_RESETCONTENT, 0, 0);
        SendMessageW(game_combo, CB_ADDSTRING, 0, (LPARAM)L"Automatic");
        int index = 0;
        for (size_t i = 0; i < instances.size(); ++i)
        {
            const auto& game = instances[i];
            const std::wstring label = L"PID " + std::to_wstring(game.pid) + L"  " + game.path;
            SendMessageW(game_combo, CB_ADDSTRING, 0, (LPARAM)label.c_str());
            if (game.pid == selected_pid) index = static_cast<int>(i + 1);
        }
        if (!index) selected_pid = 0;
        SendMessageW(game_combo, CB_SETCURSEL, index, 0);
    }

    void reconnect_game()
    {
        cs2::features::reset();
        if (cs2::game_handle) vm::close(cs2::game_handle);
        cs2::game_handle = nullptr;
        next_attach = 0;
        SetWindowTextW(game_status, L"Checking game build...");
    }

    bool read_profile_number(const wchar_t* section, const wchar_t* name, DWORD& value)
    {
        wchar_t text[32]{};
        if (!profiles_path[0] || !GetPrivateProfileStringW(section, name, L"", text, _countof(text), profiles_path))
            return false;
        wchar_t* end = nullptr;
        const unsigned long long number = wcstoull(text, &end, 0);
        if (end == text || *end || !number || number > MAXDWORD) return false;
        value = static_cast<DWORD>(number);
        return true;
    }

    bool module_contains(vm_handle process, QWORD module, DWORD rva, DWORD length)
    {
        IMAGE_DOS_HEADER dos{};
        IMAGE_NT_HEADERS nt{};
        if (!module || !vm::read(process, module, &dos, sizeof(dos)) ||
            dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 || dos.e_lfanew > 0x10000 ||
            !vm::read(process, module + dos.e_lfanew, &nt, sizeof(nt)) ||
            nt.Signature != IMAGE_NT_SIGNATURE) return false;
        const DWORD size = nt.OptionalHeader.SizeOfImage;
        return rva < size && length <= size - rva;
    }

    void initialize_profile_file()
    {
        const DWORD length = GetModuleFileNameW(nullptr, profiles_path, _countof(profiles_path));
        wchar_t* separator = length && length < _countof(profiles_path) ? wcsrchr(profiles_path, 92) : nullptr;
        if (!separator || wcscpy_s(separator + 1, _countof(profiles_path) - (separator + 1 - profiles_path), L"um-profiles.ini"))
        {
            profiles_path[0] = 0;
            return;
        }
        HANDLE file = CreateFileW(profiles_path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return;
        constexpr char defaults[] =
            "[Detection]\r\nEngineBuildRva=0x6BA028\r\n\r\n"
            "[Build.14196]\r\nViewMatrixRva=0x2570250\r\nViewAnglesRva=0x2581098\r\n"
            "SensitivityRva=0x2567C48\r\nSensitivityValueOffset=0x58\r\nAttributeManagerOffset=0x1290\r\n";
        DWORD written = 0;
        WriteFile(file, defaults, sizeof(defaults) - 1, &written, nullptr);
        CloseHandle(file);
    }

    void paint_panel(HWND hwnd)
    {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT client{};
        GetClientRect(hwnd, &client);
        HBRUSH background = CreateSolidBrush(panel_bg);
        FillRect(dc, &client, background);
        DeleteObject(background);

        RECT header{0, 0, client.right, 73};
        HBRUSH navy = CreateSolidBrush(RGB(19, 34, 55));
        FillRect(dc, &header, navy);
        DeleteObject(navy);
        RECT accent{0, 0, client.right, 4};
        HBRUSH blue = CreateSolidBrush(RGB(63, 133, 237));
        FillRect(dc, &accent, blue);
        DeleteObject(blue);

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(249, 251, 255));
        HGDIOBJ old_font = SelectObject(dc, panel_heading_font ?
            (HGDIOBJ)panel_heading_font : GetStockObject(DEFAULT_GUI_FONT));
        RECT title{22, 13, client.right - 20, 41};
        DrawTextW(dc, L"CS2 Controls", -1, &title, DT_SINGLELINE | DT_LEFT);
        SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        SetTextColor(dc, RGB(183, 202, 229));
        RECT subtitle{22, 44, client.right - 20, 65};
        DrawTextW(dc, L"Key bindings and aim regions", -1, &subtitle, DT_SINGLELINE | DT_LEFT);
        SelectObject(dc, old_font);

        HGDIOBJ old_brush = SelectObject(dc, GetStockObject(WHITE_BRUSH));
        HPEN border = CreatePen(PS_SOLID, 1, RGB(203, 213, 228));
        HGDIOBJ old_pen = SelectObject(dc, border);
        RoundRect(dc, 12, 86, client.right - 12, 235, 14, 14);
        RoundRect(dc, 12, 246, client.right - 12, 410, 14, 14);
        RoundRect(dc, 12, 421, client.right - 12, 549, 14, 14);
        SelectObject(dc, old_pen);
        SelectObject(dc, old_brush);
        DeleteObject(border);
        EndPaint(hwnd, &paint);
    }

    bool key_down(int key)
    {
        return (GetAsyncKeyState(key) & 0x8000) != 0;
    }

    LRESULT CALLBACK panel_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        if (msg == WM_PAINT)
        {
            paint_panel(hwnd);
            return 0;
        }
        if (msg == WM_CTLCOLORSTATIC)
        {
            const HWND control = (HWND)lparam;
            const int id = GetDlgCtrlID(control);
            const bool footer = id == 212;
            SetBkMode((HDC)wparam, footer ? TRANSPARENT : OPAQUE);
            SetBkColor((HDC)wparam, RGB(255, 255, 255));
            SetTextColor((HDC)wparam, id == 211 || footer ? panel_muted : panel_text);
            return (LRESULT)GetStockObject(footer ? NULL_BRUSH : WHITE_BRUSH);
        }
        if (msg == WM_CLOSE || (msg == WM_HOTKEY && wparam == 1))
        {
            if (IsWindowVisible(hwnd))
                ShowWindow(hwnd, SW_HIDE);
            else
            {
                ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
            }
            return 0;
        }
        if (msg == WM_COMMAND && LOWORD(wparam) == 221 && HIWORD(wparam) == BN_CLICKED)
        {
            refresh_instances();
            reconnect_game();
            return 0;
        }
        if (msg == WM_COMMAND && LOWORD(wparam) == 220 && HIWORD(wparam) == CBN_SELCHANGE)
        {
            const LRESULT index = SendMessageW(game_combo, CB_GETCURSEL, 0, 0);
            if (index < 0 || static_cast<size_t>(index) > instances.size()) return 0;
            selected_pid = index ? instances[index - 1].pid : 0;
            preferred_path = index ? instances[index - 1].path : L"";
            if (settings_path[0])
                WritePrivateProfileStringW(L"Compatibility", L"PreferredPath", preferred_path.c_str(), settings_path);
            reconnect_game();
            return 0;
        }
        if (msg == WM_COMMAND && LOWORD(wparam) == 210 && HIWORD(wparam) == BN_CLICKED)
        {
            int values[3]{};
            for (int row = 0; row < 3; ++row)
            {
                BOOL valid = FALSE;
                values[row] = GetDlgItemInt(hwnd, 200 + row, &valid, FALSE);
                if (!valid || values[row] > 100)
                {
                    MessageBoxW(hwnd, L"Enter percentages from 0 to 100.", L"Aim regions", MB_OK);
                    return 0;
                }
            }
            if (values[0] + values[1] + values[2] != 100)
            {
                MessageBoxW(hwnd, L"The percentages must add up to 100.", L"Aim regions", MB_OK);
                return 0;
            }
            for (int row = 0; row < 3; ++row)
            {
                region_weights[row] = values[row];
                wchar_t text[16]{};
                swprintf_s(text, L"%d", values[row]);
                if (settings_path[0])
                    WritePrivateProfileStringW(L"AimRegions", weight_names[row], text, settings_path);
            }
            SetDlgItemTextW(hwnd, 211, L"Saved. Applied to the next target.");
            return 0;
        }
        if (msg == WM_COMMAND && HIWORD(wparam) == CBN_SELCHANGE)
        {
            const int row = LOWORD(wparam) - 100;
            const LRESULT index = SendMessageW((HWND)lparam, CB_GETCURSEL, 0, 0);
            if (row >= 0 && row < 3 && index >= 0 && index < _countof(keys))
            {
                bindings[row] = keys[index].key;
                wchar_t value[16]{};
                swprintf_s(value, L"%d", bindings[row]);
                if (settings_path[0])
                    WritePrivateProfileStringW(L"Controls", setting_names[row], value, settings_path);
            }
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    HWND create_panel()
    {
        const DWORD length = GetModuleFileNameW(nullptr, settings_path, MAX_PATH);
        wchar_t* separator = length && length < MAX_PATH ? wcsrchr(settings_path, 92) : nullptr;
        if (!separator || wcscpy_s(separator + 1, MAX_PATH - (separator + 1 - settings_path), L"um-settings.ini"))
            settings_path[0] = 0;

        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSW wc{};
        wc.lpfnWndProc = panel_proc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"EcControls";
        if (!RegisterClassW(&wc)) return nullptr;

        HWND panel = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, wc.lpszClassName,
            L"CS2 Controls", WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
            426, 625, nullptr, nullptr, instance, nullptr);
        if (!panel) return nullptr;
        panel_heading_font = CreateFontW(-23, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE,
            FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HWND key_heading = CreateWindowW(L"STATIC", L"KEY BINDINGS", WS_CHILD | WS_VISIBLE,
            26, 100, 350, 20, panel, nullptr, instance, nullptr);
        SendMessageW(key_heading, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        const wchar_t* labels[] = {L"Aim (Hold)", L"Trigger (Hold)", L"Wall (Toggle)"};
        for (int row = 0; row < 3; ++row)
        {
            const int y = 124 + row * 34;
            HWND label = CreateWindowW(L"STATIC", labels[row], WS_CHILD | WS_VISIBLE,
                26, y + 4, 155, 24, panel, nullptr, instance, nullptr);
            HWND combo = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE |
                WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 208, y, 166, 230,
                panel, (HMENU)(INT_PTR)(100 + row), instance, nullptr);
            SendMessageW(label, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
            SendMessageW(combo, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
            const int saved = settings_path[0] ?
                GetPrivateProfileIntW(L"Controls", setting_names[row], bindings[row], settings_path) : bindings[row];
            int selected = 0;
            for (int i = 0; i < _countof(keys); ++i)
                if (keys[i].key == bindings[row]) selected = i;
            for (int i = 0; i < _countof(keys); ++i)
            {
                SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)keys[i].label);
                if (keys[i].key == saved) selected = i;
            }
            bindings[row] = keys[selected].key;
            SendMessageW(combo, CB_SETCURSEL, selected, 0);
        }
        int total = 0;
        for (int row = 0; row < 3; ++row)
        {
            const int saved = settings_path[0] ?
                GetPrivateProfileIntW(L"AimRegions", weight_names[row], region_weights[row], settings_path) : region_weights[row];
            region_weights[row] = saved >= 0 && saved <= 100 ? saved : 0;
            total += region_weights[row];
        }
        if (total != 100) { region_weights[0] = 40; region_weights[1] = 50; region_weights[2] = 10; }
        auto add_control = [&](const wchar_t* type, const wchar_t* text, DWORD style,
            int x, int y, int width, int height, int id) {
            HWND control = CreateWindowW(type, text, WS_CHILD | WS_VISIBLE | style,
                x, y, width, height, panel, (HMENU)(INT_PTR)id, instance, nullptr);
            SendMessageW(control, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
            return control;
        };
        add_control(L"STATIC", L"AIM REGIONS", 0, 26, 260, 350, 24, 0);
        for (int row = 0; row < 3; ++row)
        {
            const int x = 26 + row * 120;
            add_control(L"STATIC", region_names[row], 0, x, 288, 110, 20, 0);
            HWND edit = add_control(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_NUMBER,
                x, 310, 78, 25, 200 + row);
            SendMessageW(edit, EM_SETLIMITTEXT, 3, 0);
            SetDlgItemInt(panel, 200 + row, region_weights[row], FALSE);
            add_control(L"STATIC", L"%", 0, x + 83, 314, 20, 20, 0);
        }
        add_control(L"BUTTON", L"Apply regions", WS_TABSTOP | BS_PUSHBUTTON, 26, 351, 122, 29, 210);
        add_control(L"STATIC", L"Total must equal 100%.", 0, 161, 358, 218, 23, 211);
        initialize_profile_file();
        wchar_t path[MAX_PATH]{};
        if (settings_path[0])
            GetPrivateProfileStringW(L"Compatibility", L"PreferredPath", L"", path, _countof(path), settings_path);
        preferred_path = path;
        add_control(L"STATIC", L"GAME COMPATIBILITY", 0, 26, 434, 350, 22, 0);
        game_combo = add_control(L"COMBOBOX", nullptr,
            WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 26, 460, 258, 240, 220);
        SendMessageW(game_combo, CB_ADDSTRING, 0, (LPARAM)L"Automatic");
        SendMessageW(game_combo, CB_SETCURSEL, 0, 0);
        SendMessageW(game_combo, CB_SETDROPPEDWIDTH, 640, 0);
        add_control(L"BUTTON", L"Reload", WS_TABSTOP | BS_PUSHBUTTON, 297, 459, 80, 27, 221);
        game_status = add_control(L"STATIC", L"Waiting for CS2...", 0, 26, 499, 353, 36, 222);
        refresh_instances();
        HWND hint = CreateWindowW(L"STATIC", L"F8 opens or closes this panel.",
            WS_CHILD | WS_VISIBLE, 26, 560, 350, 23, panel, (HMENU)(INT_PTR)212, instance, nullptr);
        SendMessageW(hint, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        if (!RegisterHotKey(panel, 1, MOD_NOREPEAT, VK_F8))
            printf("F8 unavailable. Controls panel opened at startup.\n");
        ShowWindow(panel, SW_SHOW);
        return panel;
    }

    void update_controls()
    {
        const ULONGLONG now = GetTickCount64();
        if (now >= next_refresh)
        {
            next_refresh = now + 2000;
            refresh_instances();
        }
        DWORD foreground_pid = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foreground_pid);
        game_focused = cs2::game_handle && foreground_pid &&
            GetProcessId(cs2::game_handle) == foreground_pid;
        const bool pressed = key_down(bindings[2]);
        if (game_focused && pressed && !overlay_was_down) overlay_visible = !overlay_visible;
        overlay_was_down = pressed;
    }
}

namespace client
{
    vm_handle open_cs2_instance()
    {
        const ULONGLONG now = GetTickCount64();
        if (now < next_attach) return nullptr;
        next_attach = now + 2000;
        if (instances.empty())
        {
            SetWindowTextW(game_status, L"Waiting for CS2...");
            return nullptr;
        }

        const GameInstance* chosen = nullptr;
        for (const auto& instance : instances)
        {
            if (selected_pid && instance.pid == selected_pid)
            {
                chosen = &instance;
                break;
            }
            if (!selected_pid && !preferred_path.empty() &&
                !_wcsicmp(preferred_path.c_str(), instance.path.c_str()))
            {
                if (chosen) { chosen = nullptr; break; }
                chosen = &instance;
            }
        }
        if (!chosen && instances.size() == 1) chosen = &instances.front();
        if (!chosen)
        {
            SetWindowTextW(game_status, L"Multiple CS2 instances. Select a process.");
            return nullptr;
        }

        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, chosen->pid);
        if (!process)
        {
            SetWindowTextW(game_status, L"Could not open the selected process.");
            return nullptr;
        }
        if (_wcsicmp(process_path(process).c_str(), chosen->path.c_str()))
        {
            CloseHandle(process);
            return nullptr;
        }
        return process;
    }

    bool load_cs2_profile(vm_handle process, QWORD engine, QWORD client_module, GameProfile& profile)
    {
        DWORD build_rva = 0x6BA028;
        read_profile_number(L"Detection", L"EngineBuildRva", build_rva);
        DWORD build = 0;
        if (!module_contains(process, engine, build_rva, sizeof(build)) ||
            !vm::read(process, engine + build_rva, &build, sizeof(build)) ||
            build < 1000 || build > 999999)
        {
            SetWindowTextW(game_status, L"Build could not be detected. Check EngineBuildRva.");
            return false;
        }

        wchar_t section[32]{};
        swprintf_s(section, L"Build.%u", build);
        const wchar_t* names[] = {L"ViewMatrixRva", L"ViewAnglesRva", L"SensitivityRva",
            L"SensitivityValueOffset", L"AttributeManagerOffset"};
        DWORD* values[] = {&profile.matrix_rva, &profile.angles_rva, &profile.sensitivity_rva,
            &profile.sensitivity_value_offset, &profile.attribute_manager_offset};
        wchar_t first_entry[4]{};
        const bool has_section = profiles_path[0] &&
            GetPrivateProfileStringW(section, nullptr, L"", first_entry, _countof(first_entry), profiles_path);
        bool valid = true;
        if (!has_section && build == 14196)
            profile = {0x2570250, 0x2581098, 0x2567C48, 0x58, 0x1290};
        else
            for (int i = 0; i < _countof(names); ++i)
                valid = read_profile_number(section, names[i], *values[i]) && valid;

        valid = valid && module_contains(process, client_module, profile.matrix_rva, 64) &&
            module_contains(process, client_module, profile.angles_rva, 8) &&
            module_contains(process, client_module, profile.sensitivity_rva, 8) &&
            profile.sensitivity_value_offset < 0x1000 && profile.attribute_manager_offset < 0x10000;
        wchar_t status[96]{};
        swprintf_s(status, valid ? L"PID %u  |  Build %u  |  Profile loaded" :
            L"PID %u  |  Build %u  |  Missing or invalid profile", GetProcessId(process), build);
        SetWindowTextW(game_status, status);
        return valid;
    }

	int choose_aim_bone()
    {
        static std::mt19937 generator(std::random_device{}());
        const int region = std::discrete_distribution<int>(
            region_weights, region_weights + 3)(generator);
        if (region == 0) return 6;
        if (region == 1) return 4;
        constexpr int limbs[] = {8, 13, 23, 26};
        return limbs[std::uniform_int_distribution<int>(0, 3)(generator)];
    }

    bool aim_pressed() { return game_focused && key_down(bindings[0]); }
    bool trigger_pressed() { return game_focused && key_down(bindings[1]); }
    bool overlay_enabled() { return overlay_visible; }

    void mouse_move(int x, int y)
	{
		mouse_event(MOUSEEVENTF_MOVE, (DWORD)x, (DWORD)y, 0, 0);
	}

	void mouse1_down(void)
	{
		mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
	}

	void mouse1_up(void)
	{
		mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
	}

	void DrawRect(void *hwnd, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b)
	{
		UNREFERENCED_PARAMETER(hwnd);
		SDL_Rect rect{ x,y,w,h };
		constexpr static int thickness = 2;

		SDL_FillSurfaceRect(sdl_surface, &rect, SDL_MapRGBA(sdl_surface->format, r, g, b, 255));

		rect.x += thickness;
		rect.y += thickness;
		rect.w -= thickness * 2;
		rect.h -= thickness * 2;

		SDL_FillSurfaceRect(sdl_surface, &rect, 0x0);
	}

	void DrawFillRect(void *hwnd, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b)
	{
		UNREFERENCED_PARAMETER(hwnd);	
		SDL_Rect rect{};
		rect.x = (int)x;
		rect.y = (int)y;
		rect.w = (int)w;
		rect.h = (int)h;
		SDL_FillSurfaceRect(sdl_surface, &rect, SDL_MapRGBA(sdl_surface->format, r, g, b, 255));
	}
}

int main(void)
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		return 0;
	}

	SDL_Window *window = SDL_CreateWindow("EC", 640, 480, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS);
	if (window == NULL)
	{
		return 0;
	}

	SDL_DisplayID id = SDL_GetDisplayForWindow(window);
	const SDL_DisplayMode *disp = SDL_GetCurrentDisplayMode(id);
	if (!disp) { printf("Display mode failed: %s\n", SDL_GetError()); SDL_DestroyWindow(window); SDL_Quit(); return 1; }
    SDL_SetWindowSize(window, disp->w, disp->h);
	SDL_SetWindowPosition(window, 0, 0);

	SDL_SetWindowAlwaysOnTop(window, SDL_TRUE);
	SDL_SetWindowOpacity(window, 0.90f);
	sdl_surface = SDL_GetWindowSurface(window);
    if (!sdl_surface) { printf("Window surface failed: %s\n", SDL_GetError()); SDL_DestroyWindow(window); SDL_Quit(); return 1; }

	HWND panel = create_panel();
    BOOL quit = 0;
	while (!quit)
	{
		SDL_Event e;
		while ( SDL_PollEvent( &e ) )
		{
			if (e.type == SDL_EVENT_QUIT)
			{
				quit = 1;
				break;
			}
		}

		update_controls();
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
		else
		{
			BOOL is_running = 0;
			if (!is_running) is_running = cs2::running();
			if (!is_running) is_running = csgo::running();
			if (!is_running) is_running = apex::running();
		}

		if (SDL_UpdateWindowSurface(window) < 0) { printf("Window update failed: %s\n", SDL_GetError()); break; }
		SDL_FillSurfaceRect(sdl_surface, NULL, 0x000000);
	}
	cs2::features::reset();
    if (panel)
    {
        UnregisterHotKey(panel, 1);
        DestroyWindow(panel);
        if (panel_heading_font) DeleteObject(panel_heading_font);
    }
    SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}

