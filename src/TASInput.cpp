/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include <Combo.h>
#include <ConfigDialog.h>
#include <DirectInputHelper.h>
#include <JoystickControl.h>
#include <Main.h>
#include <MiscHelpers.h>
#include <NewConfig.h>
#include <TASInput.h>

#define EXPORT __declspec(dllexport)
#define CALL _cdecl

#define WM_EDIT_END (WM_USER + 3)
#define WM_UPDATE_VISUALS (WM_USER + 4)

enum class ComboTask {
    Idle,
    Play,
    Record
};

struct Status {
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
    POINT window_position{};

    /**
     * \brief The current internal input state before any processing
     */
    core_buttons current_input{};

    /**
     * \brief The internal input state at the previous GetKeys call before any processing
     */
    core_buttons last_controller_input{};

    /**
     * \brief Ignores the next joystick increment, used for relative mode tracking
     */
    bool ignore_next_down[2]{};

    /**
     * \brief Ignores the next joystick decrement, used for relative mode tracking
     */
    bool ignore_next_up[2]{};

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
     * \brief Handle of the edit box used for renaming combos
     */
    HWND combo_edit_box = nullptr;

    std::vector<t_combo> combos{};

    bool last_lmb_down{};
    bool last_rmb_down{};
    core_buttons autofire_input_a{};
    core_buttons autofire_input_b{};
    bool ready;
    HWND hwnd{};
    HWND combos_hwnd{};
    HWND joy_hwnd;
    HWND combo_listbox;
    int controller_index;
    ComboTask combo_task = ComboTask::Idle;

    void set_status(const std::wstring& str);

    bool show_context_menu(int x, int y);

    /**
     * \brief Gets whether a combo is currently active.
     */
    bool combo_active();
    /**
     * \brief Saves the combo list to a file
     */
    void save_combos();

    /**
     * \brief Loads the combo list from a file
     */
    void load_combos(const std::filesystem::path& path);

    void start_edit(int);

    void end_edit(int, char*);

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

    void on_config_changed();

    void get_input(core_buttons* keys);
};

static ULONG gdi_plus_token{};
static std::atomic<int64_t> frame_counter{};
static std::atomic<bool> new_frame{};
static std::atomic<bool> rom_open{};
static HWND emulator_hwnd{};
static HMENU hmenu{};
static HFONT icon_font{};
static Status status[NUMBER_OF_CONTROLS]{};

static int MOUSE_LBUTTONREDEFINITION = VK_LBUTTON;
static int MOUSE_RBUTTONREDEFINITION = VK_RBUTTON;

EXPORT void CALL CloseDLL()
{
    if (gdi_plus_token)
    {
        Gdiplus::GdiplusShutdown(gdi_plus_token);
        gdi_plus_token = 0;
    }

    dih_free();
}

EXPORT void CALL DllAbout(void* hParent)
{
    const auto msg = PLUGIN_NAME L"\n"
                                 L"Part of the Mupen64 project family."
                                 L"\n\n"
                                 L"https://github.com/mupen64/TASInput";

    MessageBox((HWND)hParent, msg, L"About", MB_ICONINFORMATION | MB_OK);
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
    strncpy_s(info->name, wstring_to_string(PLUGIN_NAME).c_str(), std::size(info->name));
}

EXPORT void CALL GetKeys(int Control, core_buttons* Keys)
{
    if (new_frame)
    {
        ++frame_counter;
        new_frame = false;
    }

    if (Control >= 0 && Control < NUMBER_OF_CONTROLS && g_controllers[Control].bActive)
        status[Control].get_input(Keys);
    else
        Keys->value = 0;

    // DirectInputHelper is messing up the axes so we gotta flip these lol
    const auto tmp = Keys->x;
    Keys->x = Keys->y;
    Keys->y = tmp;
}

EXPORT void CALL SetKeys(int Control, core_buttons ControllerInput)
{
    status[Control].set_visuals(ControllerInput, false);
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


void Status::get_input(core_buttons* keys)
{
    keys->value = get_processed_input(current_input).value;

    if (combo_task == ComboTask::Play && !combo_paused)
    {
        if (combo_frame >= combos[active_combo_index].samples.size() - 1)
        {
            if (new_config.loop_combo)
            {
                combo_frame = 0;
            }
            else
            {
                set_status(L"Finished combo");
                combo_task = ComboTask::Idle;
                // Reset input on last frame, or it sticks which feels weird
                // We also need to reprocess the inputs since source data change
                current_input = {0};
                keys->value = get_processed_input(current_input).value;
                goto end;
            }
        }

        set_status(std::format(L"Playing... ({} / {})", combo_frame + 1, combos[active_combo_index].samples.size() - 1));
        combo_frame++;
    }

end:
    if (combo_task == ComboTask::Record)
    {
        // We process this last, because we need the processed inputs
        combos[active_combo_index].samples.push_back(*keys);
        set_status(std::format(L"Recording... ({})", combos[active_combo_index].samples.size()));
    }

    if (new_config.async_visual_updates)
    {
        PostMessage(hwnd, WM_UPDATE_VISUALS, 0, keys->value);
    }
    else
    {
        SendMessage(hwnd, WM_UPDATE_VISUALS, 0, keys->value);
    }
}

core_buttons Status::get_processed_input(core_buttons input)
{
    input.value |= frame_counter % 2 == 0 ? autofire_input_a.value : autofire_input_b.value;

    if (combo_task == ComboTask::Play && !combo_paused)
    {
        auto combo_input = combos[active_combo_index].samples[combo_frame];
        if (!combos[active_combo_index].uses_joystick())
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
    if (GetFocus() == GetDlgItem(hwnd, IDC_EDITX) || GetFocus() == GetDlgItem(hwnd, IDC_EDITY) || (combo_edit_box != nullptr && GetFocus() == combo_edit_box))
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
    if (GetFocus() != GetDlgItem(hwnd, IDC_EDITX))
    {
        SetDlgItemText(hwnd, IDC_EDITX, std::to_wstring(input.x).c_str());
    }

    if (GetFocus() != GetDlgItem(hwnd, IDC_EDITY))
    {
        SetDlgItemText(hwnd, IDC_EDITY, std::to_wstring(input.y).c_str());
    }

    CheckDlgButton(hwnd, IDC_CHECK_A, input.a);
    CheckDlgButton(hwnd, IDC_CHECK_B, input.b);
    CheckDlgButton(hwnd, IDC_CHECK_START, input.start);
    CheckDlgButton(hwnd, IDC_CHECK_L, input.l);
    CheckDlgButton(hwnd, IDC_CHECK_R, input.r);
    CheckDlgButton(hwnd, IDC_CHECK_Z, input.z);
    CheckDlgButton(hwnd, IDC_CHECK_CUP, input.cu);
    CheckDlgButton(hwnd, IDC_CHECK_CLEFT, input.cl);
    CheckDlgButton(hwnd, IDC_CHECK_CRIGHT, input.cr);
    CheckDlgButton(hwnd, IDC_CHECK_CDOWN, input.cd);
    CheckDlgButton(hwnd, IDC_CHECK_DUP, input.du);
    CheckDlgButton(hwnd, IDC_CHECK_DLEFT, input.dl);
    CheckDlgButton(hwnd, IDC_CHECK_DRIGHT, input.dr);
    CheckDlgButton(hwnd, IDC_CHECK_DDOWN, input.dd);

    JoystickControl::set_position(joy_hwnd, input.x, input.y);
}

static int get_joystick_increment(const bool up)
{
    int increment = up ? 1 : -1;

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

INT_PTR CALLBACK combos_dlgproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    auto ctx = reinterpret_cast<Status*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg)
    {
    case WM_INITDIALOG:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, lparam);
        ctx = reinterpret_cast<Status*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        ctx->combo_listbox = GetDlgItem(hwnd, IDC_MACROLIST);
        ctx->load_combos("combos.cmb");
        break;
    case WM_EDIT_END:
        ctx->end_edit(ctx->renaming_combo_index, (char*)lparam);
        ctx->combo_edit_box = nullptr;
        break;
    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case IDC_PLAY:
            ctx->active_combo_index = ListBox_GetCurSel(ctx->combo_listbox);
            if (ctx->active_combo_index == -1)
            {
                ctx->set_status(L"No combo selected");
                break;
            }
            ctx->set_status(L"Playing combo");
            ctx->combo_frame = 0;
            ctx->combo_task = ComboTask::Play;
            break;
        case IDC_STOP:
            ctx->set_status(L"Idle");
            ctx->combo_task = ComboTask::Idle;
            break;
        case IDC_PAUSE:
            ctx->combo_paused ^= true;
            break;
        case IDC_LOOP:
            new_config.loop_combo ^= true;
            save_config();
            break;
        case IDC_RECORD:
            if (ctx->combo_task == ComboTask::Record)
            {
                ctx->set_status(L"Recording stopped");
                ctx->combo_task = ComboTask::Idle;
                break;
            }

            ctx->set_status(L"Recording new combo...");
            ctx->combos.push_back({.name = "Unnamed Combo"});
            ctx->active_combo_index = ListBox_InsertString(ctx->combo_listbox, -1, string_to_wstring(ctx->combos.back().name).c_str());
            ListBox_SetCurSel(ctx->combo_listbox, ctx->active_combo_index);
            ctx->combo_task = ComboTask::Record;
            break;
        case IDC_EDIT:
            ctx->renaming_combo_index = ListBox_GetCurSel(ctx->combo_listbox);
            if (ctx->renaming_combo_index == -1)
            {
                ctx->set_status(L"No combo selected");
                break;
            }
            ctx->start_edit(ctx->renaming_combo_index);
            break;
        case IDC_CLEAR:
            ctx->combo_task = ComboTask::Idle;
            ctx->active_combo_index = -1;
            ctx->combos.clear();
            ListBox_ResetContent(ctx->combo_listbox);
            break;
        case IDC_IMPORT:
            {
                wchar_t file[MAX_PATH]{};

                ctx->set_status(L"Importing...");
                OPENFILENAME data{};
                data.lStructSize = sizeof(data);
                data.lpstrFilter = L"Combo file (*.cmb)\0*.cmb\0\0";
                data.nFilterIndex = 1;
                data.nMaxFile = MAX_PATH;
                data.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                data.lpstrFile = file;
                if (GetOpenFileName(&data))
                {
                    ctx->load_combos(file);
                }
                ctx->set_status(L"Imported combo data");
                break;
            }
        case IDC_SAVE:
            ctx->save_combos();
            ctx->set_status(L"Saved to combos.cmb");
            break;
        default:
            break;
        }
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_SETCURSOR:
        SendMessage(ctx->hwnd, msg, wparam, lparam);
        break;
    default:
        break;
    }
    return FALSE;
}

INT_PTR CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    auto ctx = reinterpret_cast<Status*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    bool lmb_down = GetAsyncKeyState(MOUSE_LBUTTONREDEFINITION) & 0x8000;
    bool rmb_down = GetAsyncKeyState(MOUSE_RBUTTONREDEFINITION) & 0x8000;

    if (!lmb_down && ctx)
    {
        ctx->is_dragging_window = false;
    }

    switch (msg)
    {
    case WM_INITDIALOG:
        {
            SetWindowLongPtr(hwnd, GWLP_USERDATA, lparam);
            ctx = reinterpret_cast<Status*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
            ctx->hwnd = hwnd;

            GetClientRect(ctx->hwnd, &ctx->initial_client_rect);
            GetWindowRect(ctx->hwnd, &ctx->initial_window_rect);

            SetWindowPos(ctx->hwnd, nullptr, ctx->window_position.x, ctx->window_position.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);

            SetWindowText(ctx->hwnd, std::format(L"TASInput - Controller {}", ctx->controller_index + 1).c_str());

            SendDlgItemMessage(ctx->hwnd, IDC_SLIDERX, TBM_SETRANGE, TRUE, MAKELONG(10, 2010));
            SendDlgItemMessage(ctx->hwnd, IDC_SLIDERX, TBM_SETPOS, TRUE, remap(new_config.x_scale[ctx->controller_index], 0, 1, 10, 2010));
            SendDlgItemMessage(ctx->hwnd, IDC_SLIDERY, TBM_SETRANGE, TRUE, MAKELONG(10, 2010));
            SendDlgItemMessage(ctx->hwnd, IDC_SLIDERY, TBM_SETPOS, TRUE, remap(new_config.y_scale[ctx->controller_index], 0, 1, 10, 2010));

            SendMessage(GetDlgItem(ctx->hwnd, IDC_X_DOWN), WM_SETFONT, (WPARAM)icon_font, TRUE);
            SendMessage(GetDlgItem(ctx->hwnd, IDC_X_UP), WM_SETFONT, (WPARAM)icon_font, TRUE);
            SendMessage(GetDlgItem(ctx->hwnd, IDC_Y_DOWN), WM_SETFONT, (WPARAM)icon_font, TRUE);
            SendMessage(GetDlgItem(ctx->hwnd, IDC_Y_UP), WM_SETFONT, (WPARAM)icon_font, TRUE);

            SetDlgItemText(ctx->hwnd, IDC_X_DOWN, L"3");
            SetDlgItemText(ctx->hwnd, IDC_X_UP, L"4");
            SetDlgItemText(ctx->hwnd, IDC_Y_DOWN, L"6");
            SetDlgItemText(ctx->hwnd, IDC_Y_UP, L"5");

            const auto scale = GetDpiForWindow(hwnd) / 96.0;

            ctx->joy_hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, JoystickControl::CLASS_NAME, L"", WS_CHILD | WS_VISIBLE, 8, 4, 131 * scale, 131 * scale, ctx->hwnd, nullptr, g_inst, nullptr);

            // It can take a bit until we receive the first GetKeys, so let's just show some basic default state in the meanwhile
            ctx->set_visuals(ctx->current_input);

            SetTimer(ctx->hwnd, IDT_TIMER_STATUS_0 + ctx->controller_index, 1, nullptr);
            ctx->on_config_changed();

            ctx->ready = true;
        }
        break;
    case WM_SHOWWINDOW:
        if (!wparam)
        {
            save_config();
        }
        break;
    case SC_MINIMIZE:
        DestroyMenu(hmenu);
        break;
    case WM_DESTROY:
        {
            ctx->ready = false;
            DestroyWindow(ctx->joy_hwnd);
            KillTimer(ctx->hwnd, IDT_TIMER_STATUS_0 + ctx->controller_index);
            ctx->hwnd = nullptr;
        }
        break;
    case JoystickControl::WM_JOYSTICK_POSITION_CHANGED:
        {
            int x{}, y{};
            JoystickControl::get_position(ctx->joy_hwnd, &x, &y);

            ctx->current_input.x = x;
            ctx->current_input.y = y;

            if (!wparam)
            {
                ctx->set_visuals(ctx->current_input);
            }
            break;
        }
    case JoystickControl::WM_JOYSTICK_DRAG_BEGIN:
        ctx->activate_emulator_window();
        break;
    case WM_CONTEXTMENU:
        if ((HWND)wparam == ctx->hwnd)
        {
            ctx->show_context_menu(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        }
        break;
    case WM_LBUTTONDOWN:
        {
            if (!new_config.client_drag || is_mouse_over_control(ctx->joy_hwnd))
            {
                break;
            }

            POINT cursor_position{};
            GetCursorPos(&cursor_position);

            RECT rect{};
            GetWindowRect(ctx->hwnd, &rect);

            ctx->is_dragging_window = true;
            ctx->dragging_window_cursor_diff = {
            cursor_position.x - rect.left,
            cursor_position.y - rect.top,
            };

            break;
        }
    case WM_SETCURSOR:
        {
            const bool lmb_just_up = !lmb_down && ctx->last_lmb_down;
            const bool rmb_just_up = !rmb_down && ctx->last_rmb_down;
            const bool rmb_just_down = rmb_down && !ctx->last_rmb_down;

            if (ctx->is_dragging_window)
            {
                POINT cursor_position = {0};
                GetCursorPos(&cursor_position);
                SetWindowPos(ctx->hwnd, nullptr, cursor_position.x - ctx->dragging_window_cursor_diff.x, cursor_position.y - ctx->dragging_window_cursor_diff.y, 0, 0, SWP_NOSIZE | SWP_NOREDRAW);
            }

            if (lmb_just_up || rmb_just_up)
            {
                // activate mupen window to allow it to get key inputs
                ctx->activate_emulator_window();
            }

            if (rmb_just_down && is_mouse_over_control(ctx->hwnd, IDC_SLIDERX))
            {
                const auto max = SendDlgItemMessage(ctx->hwnd, IDC_SLIDERX, TBM_GETRANGEMAX, 0, 0);
                SendDlgItemMessage(ctx->hwnd, IDC_SLIDERX, TBM_SETPOS, TRUE, max);
            }

            if (rmb_just_down && is_mouse_over_control(ctx->hwnd, IDC_SLIDERY))
            {
                const auto max = SendDlgItemMessage(ctx->hwnd, IDC_SLIDERY, TBM_GETRANGEMAX, 0, 0);
                SendDlgItemMessage(ctx->hwnd, IDC_SLIDERY, TBM_SETPOS, TRUE, max);
            }

            if (rmb_just_down)
            {

#define AUTOFIRE(id, field)                                                    \
    {                                                                          \
        if (is_mouse_over_control(ctx->hwnd, id))                              \
        {                                                                      \
            if (ctx->autofire_input_a.field || ctx->autofire_input_b.field)    \
            {                                                                  \
                ctx->autofire_input_a.field = ctx->autofire_input_b.field = 0; \
            }                                                                  \
            else                                                               \
            {                                                                  \
                if (frame_counter % 2 == 0)                                    \
                    ctx->autofire_input_a.field ^= 1;                          \
                else                                                           \
                    ctx->autofire_input_b.field ^= 1;                          \
            }                                                                  \
        }                                                                      \
    }
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
#undef AUTOFIRE
                ctx->set_visuals(ctx->current_input);
            }

            ctx->last_lmb_down = GetAsyncKeyState(MOUSE_LBUTTONREDEFINITION) & 0x8000;
            ctx->last_rmb_down = GetAsyncKeyState(MOUSE_RBUTTONREDEFINITION) & 0x8000;
        }
        break;
    case WM_TIMER:
        // Looks like there  isn't an event mechanism in DirectInput, so we just poll and diff the inputs to emulate events
        core_buttons controller_input = dih_get_input(g_controllers, ctx->controller_index, new_config.x_scale[ctx->controller_index], new_config.y_scale[ctx->controller_index]);

        if (controller_input.value != ctx->last_controller_input.value)
        {
            // Input changed, override everything with current

#define BTN(field)                                                   \
    if (controller_input.field && !ctx->last_controller_input.field) \
    {                                                                \
        ctx->current_input.field = 1;                                \
    }                                                                \
    if (!controller_input.field && ctx->last_controller_input.field) \
    {                                                                \
        ctx->current_input.field = 0;                                \
    }
#define JOY(field, i)                                                           \
    if (controller_input.field != ctx->last_controller_input.field)             \
    {                                                                           \
        if (new_config.relative_mode)                                           \
        {                                                                       \
            if (controller_input.field > ctx->last_controller_input.field)      \
            {                                                                   \
                if (ctx->ignore_next_down[i])                                   \
                {                                                               \
                    ctx->ignore_next_down[i] = false;                           \
                }                                                               \
                else                                                            \
                {                                                               \
                    ctx->current_input.field = ctx->current_input.field + 5;    \
                    ctx->ignore_next_up[i] = true;                              \
                }                                                               \
            }                                                                   \
            else if (controller_input.field < ctx->last_controller_input.field) \
            {                                                                   \
                if (ctx->ignore_next_up[i])                                     \
                {                                                               \
                    ctx->ignore_next_up[i] = false;                             \
                }                                                               \
                else                                                            \
                {                                                               \
                    ctx->current_input.field = ctx->current_input.field - 5;    \
                    ctx->ignore_next_down[i] = true;                            \
                }                                                               \
            }                                                                   \
        }                                                                       \
        else                                                                    \
        {                                                                       \
            ctx->current_input.field = controller_input.field;                  \
        }                                                                       \
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

            ctx->set_visuals(ctx->current_input);
        }
        ctx->last_controller_input = controller_input;

        break;
    case WM_NOTIFY:
        {
            switch (LOWORD(wparam))
            {
            case IDC_SLIDERX:
            case IDC_SLIDERY:
                {
                    const auto id = LOWORD(wparam);
                    const auto min = SendDlgItemMessage(ctx->hwnd, id, TBM_GETRANGEMIN, 0, 0);
                    const auto max = SendDlgItemMessage(ctx->hwnd, id, TBM_GETRANGEMAX, 0, 0);
                    const int pos = SendDlgItemMessage(ctx->hwnd, id, TBM_GETPOS, 0, 0);
                    const auto scale = remap(pos, min, max, 0, 1);
                    if (id == IDC_SLIDERX)
                    {
                        new_config.x_scale[ctx->controller_index] = scale;
                    }
                    else
                    {
                        new_config.y_scale[ctx->controller_index] = scale;
                    }
                }
                break;
            default:
                break;
            }
        }
        break;
    case WM_UPDATE_VISUALS:
        ctx->set_visuals(static_cast<core_buttons>(lparam), false);
        break;
    case WM_SIZE:
    case WM_MOVE:
        {
            RECT window_rect{};
            GetWindowRect(ctx->hwnd, &window_rect);
            ctx->window_position = {
            window_rect.left,
            window_rect.top,
            };
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case IDC_EDITX:
            {
                if (HIWORD(wparam) != EN_CHANGE)
                {
                    break;
                }
                core_buttons last_input = ctx->current_input;
                wchar_t str[8]{};
                GetDlgItemText(ctx->hwnd, LOWORD(wparam), str, std::size(str));
                try
                {
                    ctx->current_input.x = std::stol(str);
                }
                catch (...)
                {
                }

                // We don't want an infinite loop, since set_visuals will send IDC_EDITX again
                if (ctx->current_input.x != last_input.x)
                {
                    ctx->set_visuals(ctx->current_input);
                }
            }
            break;

        case IDC_EDITY:
            {
                if (HIWORD(wparam) != EN_CHANGE)
                {
                    break;
                }
                core_buttons last_input = ctx->current_input;
                wchar_t str[8]{};
                GetDlgItemText(ctx->hwnd, LOWORD(wparam), str, std::size(str));
                try
                {
                    ctx->current_input.y = std::stol(str);
                }
                catch (...)
                {
                }

                // We don't want an infinite loop, since set_visuals will send IDC_EDITX again
                if (ctx->current_input.y != last_input.y)
                {
                    ctx->set_visuals(ctx->current_input);
                }
            }
            break;
        case IDC_CLEARINPUT:
            ctx->current_input = {0};
            ctx->autofire_input_a = {0};
            ctx->autofire_input_b = {0};
            ctx->set_visuals(ctx->current_input);
            break;
        case IDC_X_DOWN:
        case IDC_X_UP:
            {
                int increment = get_joystick_increment(LOWORD(wparam) == IDC_X_UP);
                ctx->current_input.x += increment;
                ctx->set_visuals(ctx->current_input);
            }
            break;
        case IDC_Y_DOWN:
        case IDC_Y_UP:
            {
                int increment = get_joystick_increment(LOWORD(wparam) == IDC_Y_UP);
                ctx->current_input.y += increment;
                ctx->set_visuals(ctx->current_input);
            }
            break;
        case IDC_EXPAND:
            new_config.dialog_expanded[ctx->controller_index] ^= true;
            save_config();
            ctx->on_config_changed();
            break;
#define TOGGLE(field)                                                                     \
    {                                                                                     \
        ctx->current_input.field = IsDlgButtonChecked(ctx->hwnd, LOWORD(wparam)) ? 1 : 0; \
        ctx->autofire_input_a.field = ctx->autofire_input_b.field = 0;                    \
    }
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
#undef TOGGLE
        default:
            break;
        }
        break;

    default:
        break;
    }

    return FALSE;
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
        wsprintf(g_controllers[i].szName, L"Controller %d", i + 1);
    }

    dih_initialize_and_check_devices((HWND)hMainWindow);

    dwType = REG_BINARY;
    dwSize = sizeof(DEFCONTROLLER);

    if (RegCreateKeyEx(HKEY_CURRENT_USER, SUBKEY, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, 0) != ERROR_SUCCESS)
    {
        MessageBox(NULL, L"Could not create Registry Key", L"Error", MB_ICONERROR | MB_OK);
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
                    wsprintf(g_controllers[i].szName, L"Controller %d", i + 1);

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
    rom_open = false;

    for (auto& st : status)
    {
        ShowWindow(st.hwnd, SW_HIDE);
    }
}

static void show_activated_windows()
{
    size_t i = 0;
    for (const auto& st : status)
    {
        ShowWindow(st.hwnd, new_config.controller_active[i] ? SW_SHOW : SW_HIDE);
        i++;
    }
}

static void create_dialog_for_status(Status* status, size_t i)
{
    status[i].hwnd = CreateDialogParam(g_inst, MAKEINTRESOURCE(IDD_MAIN), nullptr, wndproc, (LPARAM)status);
}

static void ui_thread()
{
    Gdiplus::GdiplusStartupInput startup_input;
    GdiplusStartup(&gdi_plus_token, &startup_input, NULL);

    icon_font = CreateFont(
    -20,
    0,
    0,
    0,
    FW_NORMAL,
    FALSE,
    FALSE,
    FALSE,
    SYMBOL_CHARSET,
    OUT_DEFAULT_PRECIS,
    CLIP_DEFAULT_PRECIS,
    DEFAULT_QUALITY,
    DEFAULT_PITCH,
    TEXT("Marlett"));

    // HACK: perform windows left handed mode check
    // and adjust accordingly
    if (GetSystemMetrics(SM_SWAPBUTTON))
    {
        MOUSE_LBUTTONREDEFINITION = VK_RBUTTON;
        MOUSE_RBUTTONREDEFINITION = VK_LBUTTON;
    }

    JoystickControl::register_class(g_inst);

    for (size_t i = 0; i < std::size(status); ++i)
    {
        status[i].controller_index = i;
        create_dialog_for_status(&status[i], i);
    }

    show_activated_windows();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        for (size_t i = 0; i < std::size(status); ++i)
        {
            if (!status[i].hwnd)
            {
                create_dialog_for_status(&status[i], i);
            }
        }

        bool handled = false;
        for (auto& st : status)
        {
            if (IsDialogMessage(st.hwnd, &msg))
            {
                handled = true;
                break;
            }
        }

        if (!handled)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    save_config();
}

EXPORT void CALL RomOpen()
{
    HKEY h_key;
    DWORD dw_type = REG_BINARY;
    DWORD dw_size = sizeof(DEFCONTROLLER);
    if (RegOpenKeyEx(HKEY_CURRENT_USER, SUBKEY, 0, KEY_READ, &h_key) == ERROR_SUCCESS)
    {
        for (size_t i = 0; i < NUMBER_OF_CONTROLS; i++)
        {
            g_controllers_default[i]->Present = new_config.controller_active[i];
            RegQueryValueEx(h_key, g_controllers[i].szName, 0, &dw_type, (LPBYTE)&g_controllers[i], &dw_size);
        }
    }
    RegCloseKey(h_key);

    load_config();

    static bool first_time = true;

    if (first_time)
    {
        std::thread(ui_thread).detach();

        first_time = false;
    }
    else
    {
        show_activated_windows();
    }

    rom_open = true;
}

bool Status::combo_active()
{
    return active_combo_index != -1;
}

void Status::set_status(const std::wstring& str)
{
    if (combos_hwnd)
    {
        Static_SetText(GetDlgItem(combos_hwnd, IDC_STATUS), str.c_str());
    }
}

void Status::start_edit(int id)
{
    RECT item_rect;
    ListBox_GetItemRect(combo_listbox, id, &item_rect);
    combo_edit_box = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, item_rect.left, item_rect.top, item_rect.right - item_rect.left, item_rect.bottom - item_rect.top + 4, combo_listbox, 0, g_inst, 0);
    // Clear selection to prevent it from repainting randomly and fighting with our textbox
    ListBox_SetCurSel(combo_listbox, -1);
    SendMessage(combo_edit_box, WM_SETFONT, (WPARAM)SendMessage(combo_listbox, WM_GETFONT, 0, 0), 0);
    SetWindowSubclass(combo_edit_box, EditBoxProc, 0, 0);

    char txt[MAX_PATH]{};
    ListBox_GetText(combo_listbox, id, txt);
    SendMessage(combo_edit_box, WM_SETTEXT, 0, (LPARAM)txt);
    PostMessage(hwnd, WM_NEXTDLGCTL, (WPARAM)combo_edit_box, TRUE);
}

void Status::end_edit(int id, char* name)
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
            combos[id].name = name;
            ListBox_InsertString(combo_listbox, id, name);
        }
    }
    set_status(L"Idle");
}

void Status::save_combos()
{
    const auto path = _T("combos.cmb");

    g_ef->log_trace(std::format(L"Saving combos to {}...", path).c_str());

    FILE* f{};
    if (_tfopen_s(&f, path, _T("wb")))
    {
        return;
    }

    const auto serialized = t_combo::serialize_combos(combos);

    (void)fwrite(serialized.data(), sizeof(uint8_t), serialized.size(), f);

    (void)fclose(f);
}

void Status::load_combos(const std::filesystem::path& path)
{
    g_ef->log_trace(std::format(L"Loading combos from {}...", path.c_str()).c_str());

    auto buf = read_file_buffer(path);
    if (buf.empty())
    {
        g_ef->log_error(L"read_file_buffer failed");
        return;
    }

    combos = t_combo::deserialize_combos(buf);

    ListBox_ResetContent(combo_listbox);
    for (const auto& combo : combos)
    {
        ListBox_InsertString(combo_listbox, -1, string_to_wstring(combo.name).c_str());
    }
}


bool Status::show_context_menu(int x, int y)
{
    if (is_mouse_over_control(joy_hwnd) || (GetKeyState(MOUSE_LBUTTONREDEFINITION) & 0x8000))
        return TRUE;

    // HACK: disable topmost so menu doesnt appear under tasinput
    hmenu = CreatePopupMenu();
#define ADD_ITEM(hmenu, x, y) AppendMenu(hmenu, new_config.x ? MF_CHECKED : 0, offsetof(t_config, x), y)
    ADD_ITEM(hmenu, relative_mode, L"Relative");
    ADD_ITEM(hmenu, always_on_top, L"Always on top");
    ADD_ITEM(hmenu, float_from_parent, L"Float from parent");
    ADD_ITEM(hmenu, titlebar, L"Titlebar");
    ADD_ITEM(hmenu, client_drag, L"Client drag");
    ADD_ITEM(hmenu, async_visual_updates, L"Async Visual Updates");

    int offset = TrackPopupMenuEx(hmenu, TPM_RETURNCMD | TPM_NONOTIFY, x, y, hwnd, 0);

    if (offset != 0)
    {
        // offset is the offset into menu config struct of the field which was selected by user, we need to convert it from byte offset to int-width offset
        auto arr = reinterpret_cast<int32_t*>(&new_config);
        arr[offset / sizeof(int32_t)] ^= true;
    }

    for (auto status_dlg : status)
    {
        if (status_dlg.ready && status_dlg.hwnd)
        {
            status_dlg.on_config_changed();
            status_dlg.activate_emulator_window();
        }
    }

    DestroyMenu(hmenu);
    return TRUE;
}

void Status::on_config_changed()
{
    if (new_config.always_on_top)
    {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    else
    {
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    set_style(hwnd, GWL_EXSTYLE, WS_EX_TOOLWINDOW, !new_config.float_from_parent);
    set_style(hwnd, GWL_STYLE, DS_SYSMODAL, !new_config.float_from_parent);
    set_style(hwnd, GWL_STYLE, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, new_config.titlebar);

    // HACK: Fix window size when switching between titlebar and no titlebar
    RECT rect = new_config.titlebar ? initial_window_rect : initial_client_rect;
    if (!new_config.titlebar)
    {
        rect.right += 8;
        rect.bottom += 5;
    }
    SetWindowPos(hwnd, nullptr, 0, 0, rect.right, rect.bottom, SWP_NOMOVE);

    const bool expanded = new_config.dialog_expanded[controller_index];

    if (!expanded)
    {
        DestroyWindow(combos_hwnd);
        combos_hwnd = nullptr;
    }

    if (expanded)
    {
        combos_hwnd = CreateDialogParam(g_inst, MAKEINTRESOURCE(IDD_COMBOS), hwnd, combos_dlgproc, (LPARAM)this);
        CheckDlgButton(combos_hwnd, IDC_LOOP, new_config.loop_combo);

        RECT expanded_rect = rect;
        RECT combos_dlg_rect{};
        GetClientRect(combos_hwnd, &combos_dlg_rect);
        expanded_rect.bottom += combos_dlg_rect.bottom;

        SetWindowPos(hwnd, nullptr, 0, 0, expanded_rect.right, expanded_rect.bottom, SWP_NOMOVE);
        SetWindowPos(combos_hwnd, nullptr, 0, initial_client_rect.bottom, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
    }

    SetDlgItemText(hwnd, IDC_EXPAND, expanded ? L"Less" : L"More");

    save_config();
}

void TASInput::on_detach()
{
    dih_free();

    if (icon_font)
    {
        DeleteFont(icon_font);
        icon_font = {};
    }
}
