/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include <Combo.h>
#include <ConfigDialog.h>
#include <DirectInputHelper.h>
#include <Main.h>
#include <MiscHelpers.h>
#include <NewConfig.h>
#include <TASInput.h>

#define EXPORT __declspec(dllexport)
#define CALL _cdecl

#define PLUGIN_VERSION "1.2.0"

#ifdef _M_X64
#define PLUGIN_ARCH "-x64"
#else
#define PLUGIN_ARCH "-x86"
#endif

#ifdef _DEBUG
#define PLUGIN_TARGET "-debug"
#else
#define PLUGIN_TARGET "-release"
#endif

#define PLUGIN_NAME "TASInput " PLUGIN_VERSION PLUGIN_ARCH PLUGIN_TARGET

#define C_IDLE 0
#define C_PLAY 1
#define C_RECORD 4

#define WM_EDIT_END 10001
#define WM_UPDATE_VISUALS 10002

#define AUTOFIRE(id, field)                                          \
    {                                                                \
        if (IsMouseOverControl(statusDlg, id))                       \
        {                                                            \
            if (autofire_input_a.field || autofire_input_b.field)    \
            {                                                        \
                autofire_input_a.field = autofire_input_b.field = 0; \
            }                                                        \
            else                                                     \
            {                                                        \
                if (frame_counter % 2 == 0)                          \
                    autofire_input_a.field ^= 1;                     \
                else                                                 \
                    autofire_input_b.field ^= 1;                     \
            }                                                        \
        }                                                            \
    }
#define TOGGLE(field)                                                                \
    {                                                                                \
        current_input.field = IsDlgButtonChecked(statusDlg, LOWORD(wParam)) ? 1 : 0; \
        autofire_input_a.field = autofire_input_b.field = 0;                         \
    }


volatile int64_t frame_counter = 0;
volatile bool new_frame = false;

HWND emulator_hwnd;
LRESULT CALLBACK StatusDlgProc0(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK StatusDlgProc1(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK StatusDlgProc2(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK StatusDlgProc3(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
bool romIsOpen = false;
HMENU hMenu;

UINT systemDPI;

std::vector<Combos::Combo*> combos{};

struct Status {
    enum class JoystickMode {
        none,
        abs,
        sticky,
        rel
    };

    Status()
    {
        show_m64_inputs = false;
        statusDlg = NULL;
    }

    /**
     * \brief Starts the dialog
     * \param controller_index The controller index the dialog is responsible for
     */
    void start(int32_t controller_index)
    {
        load_config();
        this->controller_index = controller_index;

        int dialog_id = new_config.dialog_expanded[controller_index] ? IDD_STATUS_COMBOS : IDD_STATUS_NORMAL;

        switch (controller_index)
        {
        case 0:
            DialogBox(g_inst, MAKEINTRESOURCE(dialog_id), NULL, (DLGPROC)StatusDlgProc0);
            break;
        case 1:
            DialogBox(g_inst, MAKEINTRESOURCE(dialog_id), NULL, (DLGPROC)StatusDlgProc1);
            break;
        case 2:
            DialogBox(g_inst, MAKEINTRESOURCE(dialog_id), NULL, (DLGPROC)StatusDlgProc2);
            break;
        case 3:
            DialogBox(g_inst, MAKEINTRESOURCE(dialog_id), NULL, (DLGPROC)StatusDlgProc3);
            break;
        default:
            assert(false);
        }
    }

    /**
     * \brief Stops the dialog
     */
    void stop()
    {
        EndDialog(statusDlg, 0);
    }

    void on_config_changed()
    {
        if (new_config.always_on_top)
        {
            SetWindowPos(statusDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }
        else
        {
            SetWindowPos(statusDlg, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }
        set_style(statusDlg, GWL_EXSTYLE, WS_EX_TOOLWINDOW, !new_config.float_from_parent);
        set_style(statusDlg, GWL_STYLE, DS_SYSMODAL, !new_config.float_from_parent);
        set_style(statusDlg, GWL_STYLE, WS_CAPTION, new_config.titlebar);

        // If we remove the titlebar, window contents will get clipped due to the window size not expanding, so we need to account for that
        RECT rect = new_config.titlebar ? initial_window_rect : initial_client_rect;
        SetWindowPos(statusDlg, nullptr, 0, 0, rect.right, rect.bottom, SWP_NOMOVE);
        save_config();

        CheckDlgButton(statusDlg, IDC_LOOP, new_config.loop_combo);
    }

    /**
     * \brief The instance's UI thread
     */
    HANDLE thread = nullptr;

    /**
     * \brief The initial client rectangle before any style changes are applied
     */
    RECT initial_client_rect;

    /**
     * \brief The initial window rectangle before any style changes are applied
     */
    RECT initial_window_rect;

    /**
     * \brief Whether the window is currently being dragged
     */
    bool is_dragging_window;

    /**
     * \brief The position of the cursor relative to the window origin at the drag operation's start
     */
    POINT dragging_window_cursor_diff;

    /**
     * \brief The window's position. Used for restoring the position after dialog changes and its position is reset by window manager
     */
    POINT window_position = {0};

    /**
     * \brief The current internal input state before any processing
     */
    core_buttons current_input = {0};

    /**
     * \brief The internal input state at the previous GetKeys call before any processing
     */
    core_buttons last_controller_input = {0};

    /**
     * \brief Ignores the next joystick increment, used for relative mode tracking
     */
    bool ignore_next_down[2] = {0};

    /**
     * \brief Ignores the next joystick decrement, used for relative mode tracking
     */
    bool ignore_next_up[2] = {0};

    /**
     * \brief The index of the currently active combo into the combos array, or -1 if none is active
     */
    int32_t active_combo_index = -1;

    /**
     * \brief The index of the currently renamed combo into the combos array, or -1 if none is being renamed
     */
    int32_t renaming_combo_index = -1;

    /**
     * \brief The frame count relative to the current combo's start
     */
    int64_t combo_frame = 0;

    /**
     * \brief Whether the currently playing combo is paused
     */
    bool combo_paused = false;

    /**
     * \brief The current joystick move mode
     */
    JoystickMode joystick_mode = JoystickMode::none;

    /**
     * \brief The difference between the mouse and joystick's mapped position at the last middle mouse button down interaction
     */
    POINT joystick_mouse_diff = {0};

    /**
     * \brief Handle of the edit box used for renaming combos
     */
    HWND combo_edit_box = nullptr;

    bool combo_active()
    {
        return active_combo_index != -1;
    }

    /**
     * \brief Clears the combo list
     */
    void clear_combos();

    /**
     * \brief Saves the combo list to a file
     */
    void save_combos();

    /**
     * \brief Loads the combo list from a file
     */
    void load_combos(const char*);

    /**
     * \brief Creates a new combo and inserts it into the combos list
     * \return The combo's index in the combos list
     */
    int create_new_combo();

    void StartEdit(int);
    void EndEdit(int, char*);

    bool is_getting_keys = false;
    int show_m64_inputs;
    // Bitflags for buttons with autofire enabled
    core_buttons autofire_input_a = {0};
    core_buttons autofire_input_b = {0};
    bool ready;
    HWND statusDlg;
    HWND combo_listbox;
    int controller_index;
    int comboTask = C_IDLE;


    void set_status(std::string str);


    LRESULT StatusDlgMethod(UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * \brief Updates the UI
     * \param input The values to be shown in the UI
     * \param needs_processing Whether the UI values need per-frame processing.
     */
    void set_visuals(core_buttons input, bool needs_processing = true);

    /**
     * \brief Processes the input with steps such as autofire or combo overrides
     * \param input The input to process
     * \return The processed input
     */
    core_buttons get_processed_input(core_buttons input);

    /**
     * \brief Activates the mupen window, releasing focus capture from the current window
     */
    void activate_emulator_window();

    void update_joystick_position();
    void GetKeys(core_buttons* Keys);
    void SetKeys(core_buttons ControllerInput);
};

Status status[NUMBER_OF_CONTROLS];

/**
 * \brief Starts all dialogs with active controllers, ending any existing ones
 */
void start_dialogs()
{
    for (auto& val : status)
    {
        if (val.statusDlg)
        {
            EndDialog(val.statusDlg, 0);
        }
    }

    for (int i = 0; i < NUMBER_OF_CONTROLS; i++)
    {
        if (g_controllers[i].bActive)
        {
            std::thread([i] {
                status[i].start(i);
            })
            .detach();
        }
    }
}

EXPORT void CALL CloseDLL(void)
{
    // Stop and Close Direct Input
    dih_free();
}

EXPORT void CALL ControllerCommand(int Control, BYTE* Command)
{
}

EXPORT void CALL DllAbout(void* hParent)
{
    if (MessageBox(
        (HWND)hParent,
        PLUGIN_NAME
        "\nFor DirectX 7 or higher\nBased on Def's Direct Input 0.54 by Deflection\nTAS Modifications by Nitsuja\nContinued development by the Mupen64-rr-lua contributors.\nDo you want to visit the repository?",
        "About",
        MB_ICONINFORMATION | MB_YESNO) == IDYES)
        ShellExecute(0, 0, "https://github.com/Mupen64-Rewrite/TASInput", 0, 0, SW_SHOW);
}

EXPORT void CALL DllConfig(void* hParent)
{
    dih_initialize_and_check_devices((HWND)hParent);
    cfgdiag_show((HWND)hParent);

    // TODO: Do we have to restart the dialogs here like in old version?
}

EXPORT void CALL GetDllInfo(core_plugin_info* info)
{
    info->ver = 0x0100;
    info->type = plugin_input;
    wsprintf(info->name, PLUGIN_NAME);
}

EXPORT void CALL GetKeys(int Control, core_buttons* Keys)
{
    if (new_frame)
    {
        ++frame_counter;
        new_frame = false;
    }

    if (Control >= 0 && Control < NUMBER_OF_CONTROLS && g_controllers[Control].bActive)
        status[Control].GetKeys(Keys);
    else
        Keys->value = 0;
}

EXPORT void CALL SetKeys(int Control, core_buttons ControllerInput)
{
    if (Control >= 0 && Control < NUMBER_OF_CONTROLS && g_controllers[Control].bActive)
        status[Control].SetKeys(ControllerInput);
}

LRESULT CALLBACK EditBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR sId, DWORD_PTR dwRefData)
{
    switch (msg)
    {
    case WM_GETDLGCODE:
        {
            if (wParam == VK_RETURN)
            {
                goto apply;
            }
            if (wParam == VK_ESCAPE)
            {
                DestroyWindow(hwnd);
            }
            break;
        }
    case WM_KILLFOCUS:
        {
            goto apply;
        }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, EditBoxProc, sId);
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);

apply:

    char txt[MAX_PATH] = "\0";
    SendMessage(hwnd, WM_GETTEXT, sizeof(txt), (LPARAM)txt);
    SendMessage(GetParent(GetParent(hwnd)), WM_EDIT_END, 0, (LPARAM)txt);
    DestroyWindow(hwnd);

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}


void Status::GetKeys(core_buttons* Keys)
{
    Keys->value = get_processed_input(current_input).value;

    if (comboTask == C_PLAY && !combo_paused)
    {
        if (combo_frame >= combos[active_combo_index]->samples.size() - 1)
        {
            if (new_config.loop_combo)
            {
                combo_frame = 0;
            }
            else
            {
                set_status("Finished combo");
                comboTask = C_IDLE;
                // Reset input on last frame, or it sticks which feels weird
                // We also need to reprocess the inputs since source data change
                current_input = {0};
                Keys->value = get_processed_input(current_input).value;
                goto end;
            }
        }

        set_status(std::format("Playing... ({} / {})", combo_frame + 1, combos[active_combo_index]->samples.size() - 1));
        combo_frame++;
    }

end:
    if (comboTask == C_RECORD)
    {
        // We process this last, because we need the processed inputs
        combos[active_combo_index]->samples.push_back(*Keys);
        set_status(std::format("Recording... ({})", combos[active_combo_index]->samples.size()));
    }

    if (new_config.async_visual_updates)
    {
        PostMessage(statusDlg, WM_UPDATE_VISUALS, 0, Keys->value);
    }
    else
    {
        SendMessage(statusDlg, WM_UPDATE_VISUALS, 0, Keys->value);
    }
}

void Status::update_joystick_position()
{
    // We don't receive "X_UP" messages when releasing buttons outside of the window, so we need to check here
    if (joystick_mode == JoystickMode::abs && !(GetAsyncKeyState(MOUSE_LBUTTONREDEFINITION) & 0x8000))
    {
        joystick_mode = JoystickMode::none;
    }

    if (joystick_mode == JoystickMode::rel && !(GetAsyncKeyState(VK_MBUTTON) & 0x8000))
    {
        joystick_mode = JoystickMode::none;
    }

    if (joystick_mode == JoystickMode::none)
    {
        return;
    }

    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(GetDlgItem(statusDlg, IDC_STICKPIC), &pt);

    RECT pic_rect;
    GetWindowRect(GetDlgItem(statusDlg, IDC_STICKPIC), &pic_rect);
    int x = (pt.x * UINT8_MAX / (signed)(pic_rect.right - pic_rect.left) - INT8_MAX + 1);
    int y = -(pt.y * UINT8_MAX / (signed)(pic_rect.bottom - pic_rect.top) - INT8_MAX + 1);

    if (joystick_mode == JoystickMode::rel)
    {
        x -= joystick_mouse_diff.x;
        y -= joystick_mouse_diff.y;
    }

    // Clamp the value to legal bounds
    if (x > INT8_MAX || y > INT8_MAX || x < INT8_MIN || y < INT8_MIN)
    {
        int div = max(abs(x), abs(y));
        x = x * INT8_MAX / div;
        y = y * INT8_MAX / div;
    }

    // snap clicks to zero
    if (abs(x) <= 8)
        x = 0;
    if (abs(y) <= 8)
        y = 0;

    current_input.x = x;
    current_input.y = y;
    set_visuals(current_input);
}

core_buttons Status::get_processed_input(core_buttons input)
{
    input.value |= frame_counter % 2 == 0 ? autofire_input_a.value : autofire_input_b.value;

    if (comboTask == C_PLAY && !combo_paused)
    {
        auto combo_input = combos[active_combo_index]->samples[combo_frame];
        if (!combos[active_combo_index]->uses_joystick())
        {
            // We want to use our joystick inputs
            combo_input.x = input.x;
            combo_input.y = input.y;
        }
        input = combo_input;
    }

    return input;
}

void Status::activate_emulator_window()
{
    if (GetFocus() == GetDlgItem(statusDlg, IDC_EDITX) || GetFocus() == GetDlgItem(statusDlg, IDC_EDITY) || (combo_edit_box != nullptr && GetFocus() == combo_edit_box))
    {
        return;
    }
    SetForegroundWindow(emulator_hwnd);
}

void Status::set_visuals(core_buttons input, bool needs_processing)
{
    if (needs_processing)
    {
        input = get_processed_input(input);
    }

    // We don't want to mess with the user's selection
    if (GetFocus() != GetDlgItem(statusDlg, IDC_EDITX))
    {
        SetDlgItemText(statusDlg, IDC_EDITX, std::to_string(input.x).c_str());
    }

    if (GetFocus() != GetDlgItem(statusDlg, IDC_EDITY))
    {
        SetDlgItemText(statusDlg, IDC_EDITY, std::to_string(input.y).c_str());
    }

    CheckDlgButton(statusDlg, IDC_CHECK_A, input.a);
    CheckDlgButton(statusDlg, IDC_CHECK_B, input.b);
    CheckDlgButton(statusDlg, IDC_CHECK_START, input.start);
    CheckDlgButton(statusDlg, IDC_CHECK_L, input.l);
    CheckDlgButton(statusDlg, IDC_CHECK_R, input.r);
    CheckDlgButton(statusDlg, IDC_CHECK_Z, input.z);
    CheckDlgButton(statusDlg, IDC_CHECK_CUP, input.cu);
    CheckDlgButton(statusDlg, IDC_CHECK_CLEFT, input.cl);
    CheckDlgButton(statusDlg, IDC_CHECK_CRIGHT, input.cr);
    CheckDlgButton(statusDlg, IDC_CHECK_CDOWN, input.cd);
    CheckDlgButton(statusDlg, IDC_CHECK_DUP, input.du);
    CheckDlgButton(statusDlg, IDC_CHECK_DLEFT, input.dl);
    CheckDlgButton(statusDlg, IDC_CHECK_DRIGHT, input.dr);
    CheckDlgButton(statusDlg, IDC_CHECK_DDOWN, input.dd);

    RECT rect = get_window_rect_client_space(statusDlg, GetDlgItem(statusDlg, IDC_STICKPIC));
    InvalidateRect(statusDlg, &rect, FALSE);
}

void Status::SetKeys(core_buttons ControllerInput)
{
    set_visuals(ControllerInput);
}

EXPORT void CALL InitiateControllers(void* hMainWindow, core_controller Controls[4])
{
    HKEY hKey;
    DWORD dwSize, dwType;
    emulator_hwnd = (HWND)hMainWindow;
    for (BYTE i = 0; i < NUMBER_OF_CONTROLS; i++)
    {
        g_controllers_default[i] = &Controls[i];
        g_controllers_default[i]->Present = FALSE;
        g_controllers_default[i]->RawData = FALSE;
        g_controllers_default[i]->Plugin = ce_none;

        g_controllers[i].NDevices = 0;
        g_controllers[i].bActive = i == 0 ? TRUE : FALSE;
        g_controllers[i].SensMax = 128;
        g_controllers[i].SensMin = 32;
        g_controllers[i].Input[18].button = 42;
        g_controllers[i].Input[19].button = 20;
        wsprintf(g_controllers[i].szName, "Controller %d", i + 1);
    }

    dih_initialize_and_check_devices((HWND)hMainWindow);

    dwType = REG_BINARY;
    dwSize = sizeof(DEFCONTROLLER);

    if (RegCreateKeyEx(HKEY_CURRENT_USER, SUBKEY, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, 0) != ERROR_SUCCESS)
    {
        MessageBox(NULL, "Could not create Registry Key", "Error", MB_ICONERROR | MB_OK);
    }
    else
    {
        for (size_t i = 0; i < NUMBER_OF_CONTROLS; i++)
        {
            g_controllers_default[i]->Present = new_config.controller_active[i];

            if (RegQueryValueEx(hKey, g_controllers[i].szName, 0, &dwType, (LPBYTE)&g_controllers[i], &dwSize) == ERROR_SUCCESS)
            {
                if (g_controllers[i].bMemPak)
                    g_controllers_default[i]->Plugin = ce_mempak;
                else
                    g_controllers_default[i]->Plugin = ce_none;

                if (dwSize != sizeof(DEFCONTROLLER))
                {
                    dwType = REG_BINARY;
                    dwSize = sizeof(DEFCONTROLLER);
                    ZeroMemory(&g_controllers[i], sizeof(DEFCONTROLLER));

                    g_controllers[i].NDevices = 0;
                    g_controllers[i].bActive = i == 0 ? TRUE : FALSE;
                    g_controllers_default[i]->Plugin = ce_none;
                    g_controllers[i].SensMax = 128;
                    g_controllers[i].SensMin = 32;
                    g_controllers[i].Input[18].button = 42;
                    g_controllers[i].Input[19].button = 20;
                    wsprintf(g_controllers[i].szName, "Controller %d", i + 1);

                    RegDeleteValue(hKey, g_controllers[i].szName);
                    RegSetValueEx(hKey, g_controllers[i].szName, 0, dwType, (LPBYTE)&g_controllers[i], dwSize);
                }
            }
            else
            {
                dwType = REG_BINARY;
                dwSize = sizeof(DEFCONTROLLER);
                RegDeleteValue(hKey, g_controllers[i].szName);
                RegSetValueEx(hKey, g_controllers[i].szName, 0, dwType, (LPBYTE)&g_controllers[i], dwSize);
            }
        }
    }
    RegCloseKey(hKey);
}

EXPORT void CALL ReadController(int Control, BYTE* Command)
{
    if (Control == -1)
    {
        new_frame = true;
    }
}

EXPORT void CALL RomClosed(void)
{
    romIsOpen = false;
    for (auto& val : status)
    {
        val.stop();
    }
}

EXPORT void CALL DllTest(HWND hParent)
{
}

EXPORT void CALL RomOpen(void)
{
    // Show a warning when no controllers are active
    size_t active_controllers = 0;
    for (size_t i = 0; i < NUMBER_OF_CONTROLS; i++)
    {
        if (new_config.controller_active[i])
        {
            active_controllers++;
        }
    }

    if (active_controllers == 0)
    {
        MessageBox(emulator_hwnd, "No controllers are active. Please enable at least one controller in the plugin settings, or emulation will not work correctly.", "Warning", MB_ICONWARNING | MB_OK);
    }

    RomClosed();
    romIsOpen = true;

    HKEY hKey;
    DWORD dwSize, dwType, dwDWSize, dwDWType;

    dwType = REG_BINARY;
    dwSize = sizeof(DEFCONTROLLER);
    dwDWType = REG_DWORD;
    dwDWSize = sizeof(DWORD);

    if (RegOpenKeyEx(HKEY_CURRENT_USER, SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        for (size_t i = 0; i < NUMBER_OF_CONTROLS; i++)
        {
            g_controllers_default[i]->Present = new_config.controller_active[i];
            RegQueryValueEx(hKey, g_controllers[i].szName, 0, &dwType, (LPBYTE)&g_controllers[i], &dwSize);
        }
    }
    RegCloseKey(hKey);

    start_dialogs();
}

void Status::clear_combos()
{
    for (auto combo : combos)
    {
        delete combo;
    }
    combos.clear();
}

int Status::create_new_combo()
{
    auto combo = new Combos::Combo();
    combos.push_back(combo);
    return ListBox_InsertString(combo_listbox, -1, combo->name.c_str());
}

void Status::set_status(std::string str)
{
    HWND hTask = GetDlgItem(statusDlg, IDC_STATUS);
    SendMessage(hTask, WM_SETTEXT, 0, (LPARAM)str.c_str());
}

// shows edit box
void Status::StartEdit(int id)
{
    RECT item_rect;
    ListBox_GetItemRect(combo_listbox, id, &item_rect);
    combo_edit_box = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP, item_rect.left, item_rect.top, item_rect.right - item_rect.left, item_rect.bottom - item_rect.top + 4, combo_listbox, 0, g_inst, 0);
    // Clear selection to prevent it from repainting randomly and fighting with our textbox
    ListBox_SetCurSel(combo_listbox, -1);
    SendMessage(combo_edit_box, WM_SETFONT, (WPARAM)SendMessage(combo_listbox, WM_GETFONT, 0, 0), 0);
    SetWindowSubclass(combo_edit_box, EditBoxProc, 0, 0);
    char txt[MAX_PATH];
    ListBox_GetText(combo_listbox, id, txt);
    SendMessage(combo_edit_box, WM_SETTEXT, 0, (LPARAM)txt);
    PostMessage(statusDlg, WM_NEXTDLGCTL, (WPARAM)combo_edit_box, TRUE);
}

void Status::EndEdit(int id, char* name)
{
    if (name != NULL)
    {
        ListBox_DeleteString(combo_listbox, id);

        if (name[0] == NULL)
        {
            combos.erase(combos.begin() + id);
        }
        else
        {
            combos[id]->name = name;
            ListBox_InsertString(combo_listbox, id, name);
        }
    }
    set_status("Idle");
}

void Status::save_combos()
{
    Combos::save("combos.cmb", combos);
}

// load combos to listBox
void Status::load_combos(const char* path)
{
    combos = Combos::find(path);

    ListBox_ResetContent(combo_listbox);
    for (auto combo : combos)
    {
        ListBox_InsertString(combo_listbox, -1, combo->name.c_str());
    }
}


static bool IsMouseOverControl(HWND hDlg, int dialogItemID)
{
    POINT pt;
    RECT rect;

    GetCursorPos(&pt);
    if (GetWindowRect(GetDlgItem(hDlg, dialogItemID), &rect)) // failed to get the dimensions
        return (pt.x <= rect.right && pt.x >= rect.left && pt.y <= rect.bottom && pt.y >= rect.top);
    return FALSE;
}


bool ShowContextMenu(HWND hwnd, HWND hitwnd, int x, int y)
{
    if (hitwnd != hwnd || IsMouseOverControl(hwnd, IDC_STICKPIC) || (GetKeyState(MOUSE_LBUTTONREDEFINITION) & 0x8000))
        return TRUE;

    // HACK: disable topmost so menu doesnt appear under tasinput
    hMenu = CreatePopupMenu();
#define ADD_ITEM(x, y) AppendMenu(hMenu, new_config.x ? MF_CHECKED : 0, offsetof(t_config, x), y)
    ADD_ITEM(relative_mode, "Relative");
    ADD_ITEM(always_on_top, "Always on top");
    ADD_ITEM(float_from_parent, "Float from parent");
    ADD_ITEM(titlebar, "Titlebar");
    ADD_ITEM(client_drag, "Client drag");
    ADD_ITEM(hifi_joystick, "High-quality joystick");
    ADD_ITEM(async_visual_updates, "Async Visual Updates");

    int offset = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, x, y, hwnd, 0);

    if (offset != 0)
    {
        // offset is the offset into menu config struct of the field which was selected by user, we need to convert it from byte offset to int-width offset
        auto arr = reinterpret_cast<int32_t*>(&new_config);
        arr[offset / sizeof(int32_t)] ^= true;
    }

    for (auto status_dlg : status)
    {
        if (status_dlg.ready && status_dlg.statusDlg)
        {
            status_dlg.on_config_changed();
            status_dlg.activate_emulator_window();
        }
    }

    DestroyMenu(hMenu);
    return TRUE;
}

#define MAKE_DLG_PROC(i)                                                                 \
    LRESULT CALLBACK StatusDlgProc##i(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) \
    {                                                                                    \
        status[i].statusDlg = hDlg;                                                      \
        return status[i].StatusDlgMethod(msg, wParam, lParam);                           \
    }
MAKE_DLG_PROC(0)
MAKE_DLG_PROC(1)
MAKE_DLG_PROC(2)
MAKE_DLG_PROC(3)

int get_joystick_increment(bool is_up)
{
    int increment = is_up ? -1 : 1;

    if (GetKeyState(VK_CONTROL) & 0x8000)
    {
        increment *= 2;
    }

    if (GetKeyState(VK_MENU) & 0x8000)
    {
        increment *= 4;
    }

    return increment;
}

LRESULT Status::StatusDlgMethod(UINT msg, WPARAM wParam, LPARAM lParam)
{
    static bool last_lmb_down = false;
    static bool last_rmb_down = false;
    bool lmb_down = GetAsyncKeyState(MOUSE_LBUTTONREDEFINITION) & 0x8000;
    bool rmb_down = GetAsyncKeyState(MOUSE_RBUTTONREDEFINITION) & 0x8000;
    bool lmb_just_up = !lmb_down && last_lmb_down;
    bool rmb_just_up = !rmb_down && last_rmb_down;
    bool rmb_just_down = rmb_down && !last_rmb_down;

    if (!lmb_down)
    {
        is_dragging_window = false;
    }

    switch (msg)
    {
    case WM_CONTEXTMENU:
        ShowContextMenu(statusDlg, (HWND)wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;
    case WM_INITDIALOG:
        {
            GetClientRect(statusDlg, &initial_client_rect);
            GetWindowRect(statusDlg, &initial_window_rect);

            SetWindowPos(statusDlg, nullptr, window_position.x, window_position.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);

            SetWindowText(statusDlg, std::format("TASInput - Controller {}", controller_index + 1).c_str());

            // set ranges
            SendDlgItemMessage(statusDlg, IDC_SLIDERX, TBM_SETRANGE, TRUE, MAKELONG(10, 2010));
            SendDlgItemMessage(statusDlg, IDC_SLIDERX, TBM_SETPOS, TRUE, remap(new_config.x_scale[controller_index], 0, 1, 10, 2010));
            SendDlgItemMessage(statusDlg, IDC_SLIDERY, TBM_SETRANGE, TRUE, MAKELONG(10, 2010));
            SendDlgItemMessage(statusDlg, IDC_SLIDERY, TBM_SETPOS, TRUE, remap(new_config.y_scale[controller_index], 0, 1, 10, 2010));

            if (new_config.dialog_expanded[controller_index])
            {
                SetDlgItemText(statusDlg, IDC_EXPAND, "Less");
            }

            combo_listbox = GetDlgItem(statusDlg, IDC_MACROLIST);
            if (combo_listbox)
            {
                clear_combos();
                load_combos("combos.cmb");
            }

            // windows likes to scale stick control in particular, so we force it to a specific size
            SetWindowPos(GetDlgItem(statusDlg, IDC_STICKPIC), nullptr, 0, 0, 131, 131, SWP_NOMOVE);

            // It can take a bit until we receive the first GetKeys, so let's just show some basic default state in the meanwhile
            set_visuals(current_input);

            SetTimer(statusDlg, IDT_TIMER_STATUS_0 + controller_index, 1, nullptr);
            on_config_changed();

            ready = true;
        }
        break;
    case SC_MINIMIZE:
        DestroyMenu(hMenu); // nuke context menu when minimized...
        break;
    case WM_NCDESTROY:
    case WM_DESTROY:
        {
            ready = false;
            KillTimer(statusDlg, IDT_TIMER_STATUS_0 + controller_index);
            save_config();
            statusDlg = NULL;
        }
        break;
    case WM_SETCURSOR:
        {
            if (is_dragging_window)
            {
                POINT cursor_position = {0};
                GetCursorPos(&cursor_position);
                SetWindowPos(statusDlg, nullptr, cursor_position.x - dragging_window_cursor_diff.x, cursor_position.y - dragging_window_cursor_diff.y, 0, 0, SWP_NOSIZE | SWP_NOREDRAW);
            }

            if (lmb_just_up || rmb_just_up)
            {
                // activate mupen window to allow it to get key inputs
                activate_emulator_window();
            }

            if (rmb_just_down && IsMouseOverControl(statusDlg, IDC_SLIDERX))
            {
                SendDlgItemMessage(statusDlg, IDC_SLIDERX, TBM_SETPOS, TRUE, (LPARAM)(LONG)(1000));
            }

            if (rmb_just_down && IsMouseOverControl(statusDlg, IDC_SLIDERY))
            {
                SendDlgItemMessage(statusDlg, IDC_SLIDERY, TBM_SETPOS, TRUE, (LPARAM)(LONG)(1000));
            }

            if (rmb_just_down)
            {
                AUTOFIRE(IDC_CHECK_A, a);
                AUTOFIRE(IDC_CHECK_B, b);
                AUTOFIRE(IDC_CHECK_START, start);
                AUTOFIRE(IDC_CHECK_L, l);
                AUTOFIRE(IDC_CHECK_R, r);
                AUTOFIRE(IDC_CHECK_Z, z);
                AUTOFIRE(IDC_CHECK_CUP, cu);
                AUTOFIRE(IDC_CHECK_CLEFT, cl);
                AUTOFIRE(IDC_CHECK_CRIGHT, cr);
                AUTOFIRE(IDC_CHECK_CDOWN, cd);
                AUTOFIRE(IDC_CHECK_DUP, du);
                AUTOFIRE(IDC_CHECK_DLEFT, dl);
                AUTOFIRE(IDC_CHECK_DRIGHT, dr);
                AUTOFIRE(IDC_CHECK_DDOWN, dd);
                set_visuals(current_input);
            }

            last_lmb_down = GetAsyncKeyState(MOUSE_LBUTTONREDEFINITION) & 0x8000;
            last_rmb_down = GetAsyncKeyState(MOUSE_RBUTTONREDEFINITION) & 0x8000;
        }
        break;
    case WM_TIMER:
        if (is_getting_keys)
        {
            break;
        }

        // We can't capture mouse correctly in a dialog,
        // so we have to rely on WM_TIMER for updating the joystick position when the cursor is outside of client bounds
        update_joystick_position();

        // Looks like there  isn't an event mechanism in DirectInput, so we just poll and diff the inputs to emulate events
        core_buttons controller_input = dih_get_input(g_controllers, controller_index, new_config.x_scale[controller_index], new_config.y_scale[controller_index]);

        if (controller_input.value != last_controller_input.value)
        {
            // Input changed, override everything with current

#define BTN(field)                                              \
    if (controller_input.field && !last_controller_input.field) \
    {                                                           \
        current_input.field = 1;                                \
    }                                                           \
    if (!controller_input.field && last_controller_input.field) \
    {                                                           \
        current_input.field = 0;                                \
    }
#define JOY(field, i)                                                      \
    if (controller_input.field != last_controller_input.field)             \
    {                                                                      \
        if (new_config.relative_mode)                                      \
        {                                                                  \
            if (controller_input.field > last_controller_input.field)      \
            {                                                              \
                if (ignore_next_down[i])                                   \
                {                                                          \
                    ignore_next_down[i] = false;                           \
                }                                                          \
                else                                                       \
                {                                                          \
                    current_input.field = current_input.field + 5;         \
                    ignore_next_up[i] = true;                              \
                }                                                          \
            }                                                              \
            else if (controller_input.field < last_controller_input.field) \
            {                                                              \
                if (ignore_next_up[i])                                     \
                {                                                          \
                    ignore_next_up[i] = false;                             \
                }                                                          \
                else                                                       \
                {                                                          \
                    current_input.field = current_input.field - 5;         \
                    ignore_next_down[i] = true;                            \
                }                                                          \
            }                                                              \
        }                                                                  \
        else                                                               \
        {                                                                  \
            current_input.field = controller_input.field;                  \
        }                                                                  \
    }
            BTN(dr)
            BTN(dl)
            BTN(dd)
            BTN(du)
            BTN(start)
            BTN(z)
            BTN(b)
            BTN(a)
            BTN(cr)
            BTN(cl)
            BTN(cd)
            BTN(cu)
            BTN(r)
            BTN(l)
            JOY(x, 0)
            JOY(y, 1)

            set_visuals(current_input);
        }
        last_controller_input = controller_input;

        break;
    case WM_PAINT:
        {
            // get dimensions of target control in client(!!!) coordinates
            RECT window_rect;
            RECT joystick_rect = get_window_rect_client_space(statusDlg, GetDlgItem(statusDlg, IDC_STICKPIC));

            // HACK: we compensate the static edge size
            joystick_rect.left += 1;
            joystick_rect.top += 1;
            joystick_rect.right -= 2;
            joystick_rect.bottom -= 2;

            GetClientRect(statusDlg, &window_rect);
            POINT joystick_rect_size = {
            joystick_rect.right - joystick_rect.left,
            joystick_rect.bottom - joystick_rect.top};


            // set up double buffering
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(statusDlg, &ps);
            HDC compat_dc = CreateCompatibleDC(hdc);
            int scale = new_config.hifi_joystick ? 4 : 1;
            POINT bmp_size = {joystick_rect_size.x * scale, joystick_rect_size.y * scale};
            HBITMAP bmp = CreateCompatibleBitmap(hdc, bmp_size.x, bmp_size.y);
            SelectObject(compat_dc, bmp);

            HPEN outline_pen = CreatePen(PS_SOLID, 1 * scale, RGB(0, 0, 0));
            HPEN line_pen = CreatePen(PS_SOLID, 3 * scale, RGB(0, 0, 255));
            HPEN tip_pen = CreatePen(PS_SOLID, 7 * scale, RGB(255, 0, 0));

            int mid_x = bmp_size.x / 2;
            int mid_y = bmp_size.y / 2;
            int stick_x = (current_input.x + 128) * bmp_size.x / 256;
            int stick_y = (-current_input.y + 128) * bmp_size.y / 256;

            // clear background with color which makes background (hopefully)
            // cool idea: maybe use user accent color for joystick tip?
            RECT normalized = {0, 0, bmp_size.x, bmp_size.y};
            FillRect(compat_dc, &normalized, GetSysColorBrush(COLOR_BTNFACE));

            // draw the back layer: ellipse and alignment lines
            SelectObject(compat_dc, outline_pen);
            Ellipse(compat_dc, 0, 0, bmp_size.x, bmp_size.y);
            MoveToEx(compat_dc, 0, mid_y, NULL);
            LineTo(compat_dc, bmp_size.x, mid_y);
            MoveToEx(compat_dc, mid_x, 0, NULL);
            LineTo(compat_dc, mid_x, bmp_size.y);

            // now joystick line
            SelectObject(compat_dc, line_pen);
            MoveToEx(compat_dc, mid_x, mid_y, nullptr);
            LineTo(compat_dc, stick_x, stick_y);

            // and finally the joystick tip
            SelectObject(compat_dc, tip_pen);
            MoveToEx(compat_dc, stick_x, stick_y, NULL);
            LineTo(compat_dc, stick_x, stick_y);

            // release pen from dc or it will be leaked
            SelectObject(compat_dc, nullptr);

            // now we can blit the new picture in one pass
            SetStretchBltMode(hdc, HALFTONE);
            SetStretchBltMode(compat_dc, HALFTONE);
            StretchBlt(hdc, joystick_rect.left, joystick_rect.top, joystick_rect_size.x, joystick_rect_size.y, compat_dc, 0, 0, bmp_size.x, bmp_size.y, SRCCOPY);
            EndPaint(statusDlg, &ps);
            DeleteDC(compat_dc);
            DeleteObject(bmp);
            DeleteObject(outline_pen);
            DeleteObject(line_pen);
            DeleteObject(tip_pen);
        }

        break;
    case WM_NOTIFY:
        {
            switch (LOWORD(wParam))
            {
            case IDC_SLIDERX:
                {
                    auto min = SendDlgItemMessage(statusDlg, IDC_SLIDERX, TBM_GETRANGEMIN, 0, 0);
                    auto max = SendDlgItemMessage(statusDlg, IDC_SLIDERX, TBM_GETRANGEMAX, 0, 0);
                    int pos = SendDlgItemMessage(statusDlg, IDC_SLIDERX, TBM_GETPOS, 0, 0);
                    new_config.x_scale[controller_index] = remap(pos, min, max, 0, 1);
                }
                break;

            case IDC_SLIDERY:
                {
                    const auto min = (double)SendDlgItemMessage(statusDlg, IDC_SLIDERY, TBM_GETRANGEMIN, 0, 0);
                    const auto max = (double)SendDlgItemMessage(statusDlg, IDC_SLIDERY, TBM_GETRANGEMAX, 0, 0);
                    const auto pos = (double)SendDlgItemMessage(statusDlg, IDC_SLIDERY, TBM_GETPOS, 0, 0);
                    new_config.y_scale[controller_index] = remap(pos, min, max, 0.0, 1.0);
                }
                break;
            }
        }
        break;
    case WM_MOUSEWHEEL:
        {
            if (!IsMouseOverControl(statusDlg, IDC_STICKPIC))
            {
                break;
            }
            auto delta = GET_WHEEL_DELTA_WPARAM(wParam);
            auto increment = delta < 0 ? -1 : 1;

            if (GetKeyState(VK_CONTROL) & 0x8000)
            {
                current_input.y += increment;
            }
            else if (GetKeyState(VK_SHIFT) & 0x8000)
            {
                const auto angle = std::atan2(current_input.y, current_input.x);
                const auto mag = std::ceil(std::sqrt(std::pow(current_input.x, 2) + std::pow(current_input.y, 2)));
                const auto new_ang = angle + (increment * (M_PI / 180.0f));
                current_input.x = (int)std::round(mag * std::cos(new_ang));
                current_input.y = (int)std::round(mag * std::sin(new_ang));
            }
            else
            {
                current_input.x += increment;
            }

            set_visuals(current_input);
        }
        break;
    case WM_MBUTTONDOWN:
        if (IsMouseOverControl(statusDlg, IDC_STICKPIC))
        {
            POINT cursor;
            GetCursorPos(&cursor);
            ScreenToClient(GetDlgItem(statusDlg, IDC_STICKPIC), &cursor);

            RECT pic_rect;
            GetWindowRect(GetDlgItem(statusDlg, IDC_STICKPIC), &pic_rect);
            int x = (cursor.x * 256 / (signed)(pic_rect.right - pic_rect.left) - 128 + 1);
            int y = -(cursor.y * 256 / (signed)(pic_rect.bottom - pic_rect.top) - 128 + 1);

            joystick_mouse_diff = POINT{
            x - current_input.x,
            y - current_input.y,
            };

            joystick_mode = JoystickMode::rel;
            activate_emulator_window();
        }
        break;
    case WM_RBUTTONDOWN:
        if (IsMouseOverControl(statusDlg, IDC_STICKPIC))
        {
            joystick_mode = joystick_mode == JoystickMode::none ? JoystickMode::sticky : JoystickMode::none;
            activate_emulator_window();
        }
        break;
    case WM_LBUTTONDOWN:
        {
            if (IsMouseOverControl(statusDlg, IDC_STICKPIC))
            {
                joystick_mode = JoystickMode::abs;
                activate_emulator_window();
            }

            if (!new_config.client_drag)
            {
                break;
            }

            POINT cursor_position{};
            GetCursorPos(&cursor_position);
            // NOTE: Windows doesn't consider STATIC controls when hittesting, so we need to check for the stick picture manually
            if (WindowFromPoint(cursor_position) != statusDlg || IsMouseOverControl(statusDlg, IDC_STICKPIC))
            {
                break;
            }

            RECT window_rect{};
            GetWindowRect(statusDlg, &window_rect);

            is_dragging_window = true;
            dragging_window_cursor_diff = {
            cursor_position.x - window_rect.left,
            cursor_position.y - window_rect.top,
            };
        }
        break;
    case WM_MOUSEMOVE:
        update_joystick_position();
        break;
    case WM_EDIT_END:
        EndEdit(renaming_combo_index, (char*)lParam);
        combo_edit_box = nullptr;
        break;
    case WM_UPDATE_VISUALS:
        set_visuals(static_cast<core_buttons>(lParam), false);
        break;
    case WM_SIZE:
    case WM_MOVE:
        {
            RECT window_rect{};
            GetWindowRect(statusDlg, &window_rect);
            window_position = {
            window_rect.left,
            window_rect.top,
            };
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_EDITX:
            {
                core_buttons last_input = current_input;
                char str[32] = {0};
                GetDlgItemText(statusDlg, IDC_EDITX, str, std::size(str));
                current_input.x = std::atoi(str);

                // We don't want an infinite loop, since set_visuals will send IDC_EDITX again
                if (current_input.x != last_input.x)
                {
                    set_visuals(current_input);
                }
            }
            break;

        case IDC_EDITY:
            {
                core_buttons last_input = current_input;
                char str[32] = {0};
                GetDlgItemText(statusDlg, IDC_EDITY, str, std::size(str));
                current_input.y = std::atoi(str);

                // We don't want an infinite loop, since set_visuals will send IDC_EDITX again
                if (current_input.y != last_input.y)
                {
                    set_visuals(current_input);
                }
            }
            break;
        // on checkbox click set buttonOverride and buttonDisplayed field and reset autofire
        case IDC_CHECK_A:
            TOGGLE(a)
            break;
        case IDC_CHECK_B:
            TOGGLE(b)
            break;
        case IDC_CHECK_START:
            TOGGLE(start)
            break;
        case IDC_CHECK_Z:
            TOGGLE(z)
            break;
        case IDC_CHECK_L:
            TOGGLE(l)
            break;
        case IDC_CHECK_R:
            TOGGLE(r)
            break;
        case IDC_CHECK_CLEFT:
            TOGGLE(cl)
            break;
        case IDC_CHECK_CUP:
            TOGGLE(cu)
            break;
        case IDC_CHECK_CRIGHT:
            TOGGLE(cr)
            break;
        case IDC_CHECK_CDOWN:
            TOGGLE(cd)
            break;
        case IDC_CHECK_DLEFT:
            TOGGLE(dl)
            break;
        case IDC_CHECK_DUP:
            TOGGLE(du)
            break;
        case IDC_CHECK_DRIGHT:
            TOGGLE(dr)
            break;
        case IDC_CHECK_DDOWN:
            TOGGLE(dd)
            break;
        case IDC_CLEARINPUT:
            current_input = {0};
            autofire_input_a = {0};
            autofire_input_b = {0};
            set_visuals(current_input);
            break;
        case IDC_X_DOWN:
        case IDC_X_UP:
            {
                int increment = get_joystick_increment(LOWORD(wParam) == IDC_X_UP);
                current_input.x += increment;
                set_visuals(current_input);
            }
            break;
        case IDC_Y_DOWN:
        case IDC_Y_UP:
            {
                int increment = get_joystick_increment(LOWORD(wParam) == IDC_Y_DOWN);
                current_input.y += increment;
                set_visuals(current_input);
            }
            break;
        case IDC_EXPAND:
            {
                new_config.dialog_expanded[controller_index] ^= true;
                save_config();
                start_dialogs();
            }
            break;
        case IDC_PLAY:
            active_combo_index = ListBox_GetCurSel(GetDlgItem(statusDlg, IDC_MACROLIST));
            if (active_combo_index == -1)
            {
                set_status("No combo selected");
                break;
            }
            set_status("Playing combo");
            combo_frame = 0;
            comboTask = C_PLAY;
            break;
        case IDC_STOP:
            set_status("Idle");
            comboTask = C_IDLE;
            break;
        case IDC_PAUSE:
            combo_paused ^= true;
            break;
        case IDC_LOOP:
            new_config.loop_combo ^= true;
            save_config();
            break;
        case IDC_RECORD:
            if (comboTask == C_RECORD)
            {
                set_status("Recording stopped");
                comboTask = C_IDLE;
                break;
            }

            set_status("Recording new combo...");
            active_combo_index = create_new_combo();
            ListBox_SetCurSel(combo_listbox, active_combo_index);
            comboTask = C_RECORD;
            break;
        case IDC_EDIT:
            renaming_combo_index = ListBox_GetCurSel(GetDlgItem(statusDlg, IDC_MACROLIST));
            if (renaming_combo_index == -1)
            {
                set_status("No combo selected");
                break;
            }
            StartEdit(renaming_combo_index);
            break;
        case IDC_CLEAR:
            comboTask = C_IDLE;
            active_combo_index = -1;
            clear_combos();
            ListBox_ResetContent(combo_listbox);
            break;
        case IDC_IMPORT:
            {
                set_status("Importing...");
                OPENFILENAME data{};
                char file[MAX_PATH] = "\0";
                data.lStructSize = sizeof(data);
                data.lpstrFilter = "Combo file (*.cmb)\0*.cmb\0\0";
                data.nFilterIndex = 1;
                data.nMaxFile = MAX_PATH;
                data.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                data.lpstrFile = file;
                if (GetOpenFileName(&data))
                {
                    load_combos(file);
                }
                set_status("Imported combo data");
                break;
            }
        case IDC_SAVE:
            save_combos();
            set_status("Saved to combos.cmb");
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }
    return FALSE; // Using DefWindowProc is prohibited but worked anyway
}
