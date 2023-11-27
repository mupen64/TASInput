/*
Copyright (C) 2001 Deflection

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/*
	Modified for TAS by Nitsuja
*/


#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>
#include <shellscalingapi.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include "Controller.h"
#include "DI.h"
#include "DefDI.h"
#include "Config.h"
#include "resource.h"
#include "Combo.h"
#include <format>

#include "NewConfig.h"
#include "helpers/win_helpers.h"

#ifdef DEBUG
#define PLUGIN_NAME "TAS Input debug"
#else
#define PLUGIN_NAME "TAS Input"
#endif

#define PI 3.14159265358979f
#define BUFFER_CHUNK 128

int MOUSE_LBUTTONREDEFINITION = VK_LBUTTON;
int MOUSE_RBUTTONREDEFINITION = VK_RBUTTON;

#undef List // look at line 32 for cause

volatile int64_t frame_counter = 0;

HINSTANCE g_hInstance;

GUID Guids[MAX_DEVICES];
DEFCONTROLLER Controller[NUMBER_OF_CONTROLS];
CONTROL* ControlDef[NUMBER_OF_CONTROLS];
HWND emulator_hwnd;
LRESULT CALLBACK StatusDlgProc0(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK StatusDlgProc1(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK StatusDlgProc2(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK StatusDlgProc3(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI StatusDlgThreadProc(LPVOID lpParameter);
bool romIsOpen = false;
HMENU hMenu;

UINT systemDPI;

std::vector<Combo*> combos;
#define ACTIVE_COMBO combos[activeCombo]

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

RECT get_window_rect_client_space(HWND parent, HWND child)
{
    RECT offset_client = {0};
    MapWindowRect(child, parent, &offset_client);

    RECT client = {0};
    GetWindowRect(child, &client);

    return {
        offset_client.left,
        offset_client.top,
        offset_client.left + (client.right - client.left),
        offset_client.top + (client.bottom - client.top)
    };
}

struct Status
{
    Status()
    {
        status_thread = NULL;
        dw_thread_id = 0;
        show_m64_inputs = false;
        x_scale = 1.0f;
        y_scale = 1.0f;
        statusDlg = NULL;
        comboTask = C_IDLE;
    }

    void StartThread(int ControllerNumber)
    {
        HANDLE prev_status_thread = status_thread;
        HWND prev_status_dlg = statusDlg;

        controller_index = ControllerNumber;
        dw_thread_id = controller_index;
        DWORD dw_thread_param = MAKEWORD(controller_index, Extend);

        // CreateDialog() won't work, because the emulator eats our messages when it's paused
        // and can't call IsDialogMessage() because it doesn't know what our dialog is.
        // So, create a new thread and spawn a MODAL (to that thread) dialog,
        // to guarantee it always gets the messages it should.
        status_thread = CreateThread(0, 0, StatusDlgThreadProc, &dw_thread_param, 0, &dw_thread_id);

        if (prev_status_dlg)
            DestroyWindow(prev_status_dlg);
        if (prev_status_thread)
            TerminateThread(prev_status_thread, 0); // last because prevStatusThread might be the currently-running thread
    }

    void StopThread()
    {
        if (statusDlg)
        {
            DestroyWindow(statusDlg);
            statusDlg = NULL;
        }
        if (status_thread)
        {
            FreeCombos(); //apparently this is a good idea
            TerminateThread(status_thread, 0);
            status_thread = NULL;
        }
    }

    void EnsureRunning()
    {
        if (!statusDlg || !status_thread || !initialized)
        {
            if (statusDlg) DestroyWindow(statusDlg), statusDlg = NULL;
            if (status_thread) TerminateThread(status_thread, 0), status_thread = NULL;
            StartThread(controller_index);
        }
    }

    void on_config_changed()
    {
        set_style(statusDlg, GWL_EXSTYLE, WS_EX_TOPMOST, new_config.always_on_top);
        set_style(statusDlg, GWL_EXSTYLE, WS_EX_TOOLWINDOW, !new_config.float_from_parent);
        set_style(statusDlg, GWL_STYLE, DS_SYSMODAL, !new_config.float_from_parent);
        set_style(statusDlg, GWL_STYLE, WS_CAPTION, new_config.titlebar);
        
        save_config();
    }

    HANDLE status_thread;
    DWORD dw_thread_id;
    
    bool is_getting_keys = false;
    int show_m64_inputs;
    BUTTONS last_controller_input = {0};
    BUTTONS current_input = {0};
    // Bitflags for buttons with autofire enabled
    BUTTONS autofire_input_a = {0};
    BUTTONS autofire_input_b = {0};
    bool is_dragging_stick;
    bool initialized;
    float x_scale, y_scale;
    int64_t combo_start_frame;
    HWND statusDlg;
    HWND listbox;
    int controller_index;
    int Extend = 0;
    int comboTask;
    int activeCombo;

    void FreeCombos();

    void set_status(std::string str);
    void SaveCombos();
    void load_combos(const char*);
    int CreateNewCombo(int);
    void StartEdit(int);
    void EndEdit(int, char*);

    LRESULT StatusDlgMethod(UINT msg, WPARAM wParam, LPARAM lParam);
    
    /**
     * \brief Updates the UI
     * \param input The values to be shown in the UI
     */
    void set_visuals(BUTTONS input);

    /**
     * \brief Processes the input with steps such as autofire or combo overrides
     * \param input The input to process
     * \return The processed input
     */
    BUTTONS get_processed_input(BUTTONS input);
    
    void update_joystick_position();
    BUTTONS get_controller_input();
    void GetKeys(BUTTONS* Keys);
    void SetKeys(BUTTONS ControllerInput);
};


Status status[NUMBER_OF_CONTROLS];

int WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, PVOID pvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        g_hInstance = hInstance;
        break;

    case DLL_PROCESS_DETACH:
        FreeDirectInput();
        break;
    }

    // HACK: perform windows left handed mode check 
    // and adjust accordingly
    if (GetSystemMetrics(SM_SWAPBUTTON))
    {
        MOUSE_LBUTTONREDEFINITION = VK_RBUTTON;
        MOUSE_RBUTTONREDEFINITION = VK_LBUTTON;
    }
    
    return TRUE;
}

EXPORT void CALL CloseDLL(void)
{
    //Stop and Close Direct Input
    FreeDirectInput();
}

EXPORT void CALL ControllerCommand(int Control, BYTE* Command)
{
}

EXPORT void CALL DllAbout(HWND hParent)
{
    if (MessageBox(
        hParent,
        PLUGIN_NAME
        "\nFor DirectX 7 or higher\nBased on Def's Direct Input 0.54 by Deflection\nTAS Modifications by Nitsuja\nContinued development by the Mupen64-rr-lua contributors.\nDo you want to visit the repository?",
        "About", MB_ICONINFORMATION | MB_YESNO) == IDYES)
        ShellExecute(0, 0, "https://github.com/mkdasher/mupen64-rr-lua-/tree/dev/tasinput_plugin/src", 0, 0, SW_SHOW);
}

EXPORT void CALL DllConfig(HWND hParent)
{
    if (g_lpDI == NULL)
        InitializeAndCheckDevices(hParent);
    else
        DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_CONFIGDLG), hParent, (DLGPROC)ConfigDlgProc);

    // make config box restart the input status control if it's been closed
    if (romIsOpen)
        for (int i = 0; i < NUMBER_OF_CONTROLS; i++)
            if (Controller[i].bActive)
                status[i].EnsureRunning();
            else
                status[i].StopThread();
}

EXPORT void CALL GetDllInfo(PLUGIN_INFO* PluginInfo)
{
    PluginInfo->Version = 0x0100;
    PluginInfo->Type = PLUGIN_TYPE_CONTROLLER;
    wsprintf(PluginInfo->Name,PLUGIN_NAME);
}

EXPORT void CALL GetKeys(int Control, BUTTONS* Keys)
{
    if (Control >= 0 && Control < NUMBER_OF_CONTROLS && Controller[Control].bActive)
        status[Control].GetKeys(Keys);
    else
        Keys->Value = 0;
}

EXPORT void CALL SetKeys(int Control, BUTTONS ControllerInput)
{
    if (Control >= 0 && Control < NUMBER_OF_CONTROLS && Controller[Control].bActive)
        status[Control].SetKeys(ControllerInput);
}

//check if given combo data uses joystick
bool ParseCombo(BUTTONS* data, int len)
{
    for (int i = 0; i != len; i++)
    {
        if (data[i].X_AXIS || data[i].Y_AXIS) return true;
    }
    return false;
}

LRESULT CALLBACK EditBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR sId, DWORD_PTR dwRefData)
{
    switch (msg)
    {
    case WM_GETDLGCODE:
        {
            char txt[MAX_PATH] = "\0";
            if (wParam == VK_RETURN)
            {
                SendMessage(hwnd, WM_GETTEXT, sizeof(txt), (LPARAM)txt);
                SendMessage(GetParent(GetParent(hwnd)), EDIT_END, 0, (LPARAM)txt);
                DestroyWindow(hwnd);
            }
            else if (wParam == VK_ESCAPE)
            {
                DestroyWindow(hwnd);
            }
            break;
        }
    case WM_KILLFOCUS:
        {
            char txt[MAX_PATH] = {0};
            SendMessage(hwnd, WM_GETTEXT, sizeof(txt), (LPARAM)txt);
            SendMessage(GetParent(GetParent(hwnd)), EDIT_END, 0, (LPARAM)txt);
            DestroyWindow(hwnd);
            break;
        }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, EditBoxProc, sId);
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

BUTTONS Status::get_controller_input()
{
    BUTTONS controller_input = {0};

    if (Controller[controller_index].bActive == TRUE)
    {
        BYTE buffer[256]; //Keyboard Info 
        DIJOYSTATE js; //Joystick Info
        HRESULT hr;
        int M1Speed = 0, M2Speed = 0;
        bool analogKey = false;

        for (BYTE devicecount = 0; devicecount < Controller[controller_index].NDevices; devicecount++)
        {
            BYTE DeviceNum = (BYTE)Controller[controller_index].Devices[devicecount];

            if (DeviceNum >= sizeof(DInputDev) / sizeof(DInputDev[0]))
            {
                continue;
            }

            if (DInputDev[DeviceNum].lpDIDevice == NULL)
            {
                continue;
            }

            LONG count;

            if ((DInputDev[DeviceNum].DIDevInst.dwDevType & DI8DEVTYPE_KEYBOARD) == DI8DEVTYPE_KEYBOARD)
            {
                ZeroMemory(&buffer, sizeof(buffer));
                if FAILED(hr = DInputDev[DeviceNum].lpDIDevice->GetDeviceState(sizeof(buffer),&buffer))
                {
                    hr = DInputDev[DeviceNum].lpDIDevice->Acquire();
                    while (hr == DIERR_INPUTLOST)
                        hr = DInputDev[DeviceNum].lpDIDevice->Acquire();
                    return {0};
                }

                for (count = 0; count < NUMBER_OF_BUTTONS; count++)
                {
                    if (Controller[controller_index].Input[count].Device == DeviceNum)
                    {
                        switch (Controller[controller_index].Input[count].type)
                        {
                        //Record Keyboard Button Info from Device State Buffer
                        case INPUT_TYPE_KEY_BUT:
                            if (BUTTONDOWN(buffer, Controller[controller_index].Input[count].vkey))
                            {
                                switch (count)
                                {
                                case 18:
                                    M1Speed = Controller[controller_index].Input[count].button;
                                    break;

                                case 19:
                                    M2Speed = Controller[controller_index].Input[count].button;
                                    break;

                                case 0:
                                case 1:
                                case 2:
                                case 3:
                                    analogKey = true;
                                /* fall through */
                                default:
                                    controller_input.Value |= Controller[controller_index].Input[count].button;
                                    break;
                                }
                            }
                            break;

                        default:
                            break;
                        }
                    }
                }
            }

            else if ((DInputDev[DeviceNum].DIDevInst.dwDevType & DI8DEVTYPE_JOYSTICK) == DI8DEVTYPE_JOYSTICK)
            {
                if FAILED(hr = DInputDev[DeviceNum].lpDIDevice->Poll())
                {
                    hr = DInputDev[DeviceNum].lpDIDevice->Acquire();
                    while (hr == DIERR_INPUTLOST)
                        hr = DInputDev[DeviceNum].lpDIDevice->Acquire();
                    return {0};
                }
                if FAILED(hr = DInputDev[DeviceNum].lpDIDevice->GetDeviceState( sizeof(DIJOYSTATE), &js ))
                {
                    return {0};
                }

                for (count = 0; count < NUMBER_OF_BUTTONS; count++)
                {
                    if (Controller[controller_index].Input[count].Device == DeviceNum)
                    {
                        BYTE count2;
                        switch (Controller[controller_index].Input[count].type)
                        {
                        //Get Joystick button Info from Device State js stucture
                        case INPUT_TYPE_JOY_BUT:
                            if (BUTTONDOWN(js.rgbButtons, Controller[controller_index].Input[count].vkey))
                            {
                                switch (count)
                                {
                                case 18:
                                    M1Speed = Controller[controller_index].Input[count].button;
                                    break;

                                case 19:
                                    M2Speed = Controller[controller_index].Input[count].button;
                                    break;

                                case 0:
                                case 1:
                                case 2:
                                case 3:
                                    analogKey = true;
                                /* fall through */
                                default:
                                    controller_input.Value |= Controller[controller_index].Input[count].button;
                                    break;
                                }
                            }
                            break;

                        case INPUT_TYPE_JOY_AXIS:
                            switch (Controller[controller_index].Input[count].vkey)
                            {
                            case DIJOFS_YN:
                                if (js.lY < (LONG)-Controller[controller_index].SensMin)
                                    GetNegAxisVal(js.lY, controller_index, count, &controller_input, M1Speed, M2Speed);
                                break;
                            case DIJOFS_YP:
                                if (js.lY > (LONG)Controller[controller_index].SensMin)
                                    GetPosAxisVal(js.lY, controller_index, count, &controller_input, M1Speed, M2Speed);
                                break;
                            case DIJOFS_XN:
                                if (js.lX < (LONG)-Controller[controller_index].SensMin)
                                    GetNegAxisVal(js.lX, controller_index, count, &controller_input, M1Speed, M2Speed);
                                break;
                            case DIJOFS_XP:
                                if (js.lX > (LONG)Controller[controller_index].SensMin)
                                    GetPosAxisVal(js.lX, controller_index, count, &controller_input, M1Speed, M2Speed);
                                break;
                            case DIJOFS_ZN:
                                if (js.lZ < (LONG)-Controller[controller_index].SensMin)
                                    GetNegAxisVal(js.lZ, controller_index, count, &controller_input, M1Speed, M2Speed);
                                break;
                            case DIJOFS_ZP:
                                if (js.lZ > (LONG)Controller[controller_index].SensMin)
                                    GetPosAxisVal(js.lZ, controller_index, count, &controller_input, M1Speed, M2Speed);
                                break;
                            case DIJOFS_RYN:
                                if (js.lRy < (LONG)-Controller[controller_index].SensMin)
                                    GetNegAxisVal(js.lRy, controller_index, count, &controller_input, M1Speed, M2Speed);
                                break;
                            case DIJOFS_RYP:
                                if (js.lRy > (LONG)Controller[controller_index].SensMin)
                                    GetPosAxisVal(js.lRy, controller_index, count, &controller_input, M1Speed, M2Speed);
                                break;
                            case DIJOFS_RXN:
                                if (js.lRx < (LONG)-Controller[controller_index].SensMin)
                                    GetNegAxisVal(js.lRx, controller_index, count, &controller_input, M1Speed, M2Speed);
                                break;
                            case DIJOFS_RXP:
                                if (js.lRx > (LONG)Controller[controller_index].SensMin)
                                    GetPosAxisVal(js.lRx, controller_index, count, &controller_input, M1Speed, M2Speed);
                                break;
                            case DIJOFS_RZN:
                                if (js.lRz < (LONG)-Controller[controller_index].SensMin)
                                    GetNegAxisVal(js.lRz, controller_index, count, &controller_input, M1Speed, M2Speed);
                                break;
                            case DIJOFS_RZP:
                                if (js.lRz > (LONG)Controller[controller_index].SensMin)
                                    GetPosAxisVal(js.lRz, controller_index, count, &controller_input, M1Speed, M2Speed);
                                break;
                            case DIJOFS_SLIDER0N:
                                if (js.rglSlider[0] < (LONG)-Controller[controller_index].SensMin)
                                    GetNegAxisVal(js.rglSlider[0], controller_index, count, &controller_input, M1Speed,
                                                  M2Speed);
                                break;
                            case DIJOFS_SLIDER0P:
                                if (js.rglSlider[0] > (LONG)Controller[controller_index].SensMin)
                                    GetPosAxisVal(js.rglSlider[0], controller_index, count, &controller_input, M1Speed,
                                                  M2Speed);
                                break;
                            case DIJOFS_SLIDER1N:
                                if (js.rglSlider[1] < (LONG)-Controller[controller_index].SensMin)
                                    GetNegAxisVal(js.rglSlider[1], controller_index, count, &controller_input, M1Speed,
                                                  M2Speed);
                                break;
                            case DIJOFS_SLIDER1P:
                                if (js.rglSlider[1] > (LONG)Controller[controller_index].SensMin)
                                    GetPosAxisVal(js.rglSlider[1], controller_index, count, &controller_input, M1Speed,
                                                  M2Speed);
                                break;
                            }
                            break;

                        case INPUT_TYPE_JOY_POV:
                            for (count2 = 0; count2 < NUMBER_OF_CONTROLS; count2++)
                            {
                                if ((js.rgdwPOV[count2] != -1) && (LOWORD(js.rgdwPOV[count2]) != 0xFFFF))
                                {
                                    switch (Controller[controller_index].Input[count].vkey)
                                    {
                                    case DIJOFS_POV0N:
                                    case DIJOFS_POV1N:
                                    case DIJOFS_POV2N:
                                    case DIJOFS_POV3N:
                                        if ((js.rgdwPOV[count2] >= 31500) || (js.rgdwPOV[count2] <= 4500))
                                        {
                                            switch (count)
                                            {
                                            case 18:
                                                M1Speed = Controller[controller_index].Input[count].button;
                                                break;

                                            case 19:
                                                M2Speed = Controller[controller_index].Input[count].button;
                                                break;

                                            case 0:
                                            case 1:
                                            case 2:
                                            case 3:
                                                analogKey = true;
                                            /* fall through */
                                            default:
                                                controller_input.Value |= Controller[controller_index].Input[count].button;
                                                break;
                                            }
                                        }
                                        break;
                                    case DIJOFS_POV0E:
                                    case DIJOFS_POV1E:
                                    case DIJOFS_POV2E:
                                    case DIJOFS_POV3E:
                                        if ((js.rgdwPOV[count2] >= 4500) && (js.rgdwPOV[count2] <= 13500))
                                        {
                                            switch (count2)
                                            {
                                            case 18:
                                                M1Speed = Controller[controller_index].Input[count].button;
                                                break;

                                            case 19:
                                                M2Speed = Controller[controller_index].Input[count].button;
                                                break;

                                            case 0:
                                            case 1:
                                            case 2:
                                            case 3:
                                                analogKey = true;
                                            /* fall through */
                                            default:
                                                controller_input.Value |= Controller[controller_index].Input[count].button;
                                                break;
                                            }
                                        }
                                        break;
                                    case DIJOFS_POV0S:
                                    case DIJOFS_POV1S:
                                    case DIJOFS_POV2S:
                                    case DIJOFS_POV3S:
                                        if ((js.rgdwPOV[count2] >= 13500) && (js.rgdwPOV[count2] <= 22500))
                                        {
                                            switch (count2)
                                            {
                                            case 18:
                                                M1Speed = Controller[controller_index].Input[count].button;
                                                break;

                                            case 19:
                                                M2Speed = Controller[controller_index].Input[count].button;
                                                break;

                                            case 0:
                                            case 1:
                                            case 2:
                                            case 3:
                                                analogKey = true;
                                            /* fall through */
                                            default:
                                                controller_input.Value |= Controller[controller_index].Input[count].button;
                                                break;
                                            }
                                        }
                                        break;
                                    case DIJOFS_POV0W:
                                    case DIJOFS_POV1W:
                                    case DIJOFS_POV2W:
                                    case DIJOFS_POV3W:
                                        if ((js.rgdwPOV[count2] >= 22500) && (js.rgdwPOV[count2] <= 31500))
                                        {
                                            switch (count2)
                                            {
                                            case 18:
                                                M1Speed = Controller[controller_index].Input[count].button;
                                                break;

                                            case 19:
                                                M2Speed = Controller[controller_index].Input[count].button;
                                                break;

                                            case 0:
                                            case 1:
                                            case 2:
                                            case 3:
                                                analogKey = true;
                                            /* fall through */
                                            default:
                                                controller_input.Value |= Controller[controller_index].Input[count].button;
                                                break;
                                            }
                                        }
                                        break;
                                    }
                                }
                            }

                        default:
                            break;
                        }
                    }
                }
            }
        }

        if (M2Speed)
        {
            if (controller_input.Y_AXIS < 0)
                controller_input.Y_AXIS = (char)-M2Speed;
            else if (controller_input.Y_AXIS > 0)
                controller_input.Y_AXIS = (char)M2Speed;

            if (controller_input.X_AXIS < 0)
                controller_input.X_AXIS = (char)-M2Speed;
            else if (controller_input.X_AXIS > 0)
                controller_input.X_AXIS = (char)M2Speed;
        }
        if (M1Speed)
        {
            if (controller_input.Y_AXIS < 0)
                controller_input.Y_AXIS = (char)-M1Speed;
            else if (controller_input.Y_AXIS > 0)
                controller_input.Y_AXIS = (char)M1Speed;

            if (controller_input.X_AXIS < 0)
                controller_input.X_AXIS = (char)-M1Speed;
            else if (controller_input.X_AXIS > 0)
                controller_input.X_AXIS = (char)M1Speed;
        }
        if (analogKey)
        {
            if (controller_input.X_AXIS && controller_input.Y_AXIS)
            {
                const static float mult = 1.0f / sqrtf(2.0f);
                float mult2;
                if (Controller[controller_index].SensMax > 127)
                    mult2 = (float)Controller[controller_index].SensMax * (1.0f / 127.0f);
                else
                    mult2 = 1.0f;

                    controller_input.X_AXIS = (int)(controller_input.X_AXIS * mult * mult2 + (controller_input.X_AXIS >
                        0
                            ? 0.5f
                            : -0.5f));

                controller_input.Y_AXIS = (int)(controller_input.Y_AXIS * mult * mult2 + (controller_input.Y_AXIS >
                        0
                            ? 0.5f
                            : -0.5f));

                int newX = (int)((float)controller_input.X_AXIS * x_scale + (
                    controller_input.X_AXIS > 0 ? 0.5f : -0.5f));
                int newY = (int)((float)controller_input.Y_AXIS * y_scale + (
                    controller_input.Y_AXIS > 0 ? 0.5f : -0.5f));
                if (abs(newX) >= abs(newY) && (newX > 127 || newX < -128))
                {
                    newY = newY * (newY > 0 ? 127 : 128) / abs(newX);
                    newX = (newX > 0) ? 127 : -128;
                }
                else if (abs(newX) <= abs(newY) && (newY > 127 || newY < -128))
                {
                    newX = newX * (newX > 0 ? 127 : 128) / abs(newY);
                    newY = (newY > 0) ? 127 : -128;
                }
                if (!newX && controller_input.X_AXIS) newX = (controller_input.X_AXIS > 0) ? 1 : -1;
                if (!newY && controller_input.Y_AXIS) newY = (controller_input.Y_AXIS > 0) ? 1 : -1;
                controller_input.X_AXIS = newX;
                controller_input.Y_AXIS = newY;
            }
            else
            {
                if (controller_input.X_AXIS)
                {
                    int newX = (int)((float)controller_input.X_AXIS * x_scale + (
                        controller_input.X_AXIS > 0 ? 0.5f : -0.5f));
                    if (!newX && controller_input.X_AXIS) newX = (controller_input.X_AXIS > 0) ? 1 : -1;
                    controller_input.X_AXIS = min(127, max(-128,newX));
                }
                if (controller_input.Y_AXIS)
                {
                    int newY = (int)((float)controller_input.Y_AXIS * y_scale + (
                        controller_input.Y_AXIS > 0 ? 0.5f : -0.5f));
                    if (!newY && controller_input.Y_AXIS) newY = (controller_input.Y_AXIS > 0) ? 1 : -1;
                    controller_input.Y_AXIS = min(127, max(-128,newY));
                }
            }
        }
    }

    return controller_input;
}

void Status::GetKeys(BUTTONS* Keys)
{
    //here ControllerInput holds true physical controller, which gets modified by tasinput / combo
    // if (activeCombo != -1 && (comboTask == C_RUNNING || comboTask == C_LOOP))
    // {
    //     int frame = frameCounter - comboStart;
    //     if (frame > ACTIVE_COMBO->samples.size() - 1)
    //     {
    //         //if frame is out of range
    //         if (comboTask == C_LOOP)
    //         {
    //             comboStart = frameCounter;
    //             frame = 0;
    //         }
    //         else
    //         {
    //             set_status("Idle");
    //             comboTask = C_IDLE;
    //             goto continue_controller;
    //         }
    //     }
    //     char buf[64];
    //     sprintf(buf, "Playing combo (%d/%d)", frame + 1, ACTIVE_COMBO->samples.size());
    //     set_status(buf);
    //     //allows to use joystick with combos
    //     buttonOverride.Value |= ACTIVE_COMBO->samples[frame].Value;
    //     // if joystick used, copy the values too (because simply ORing is not enough)
    //     if (ACTIVE_COMBO->uses_joystick())
    //     {
    //         overrideX = controller_input.X_AXIS = ACTIVE_COMBO->samples[frame].X_AXIS;
    //         overrideY = controller_input.Y_AXIS = ACTIVE_COMBO->samples[frame].Y_AXIS;
    //     }
    // }
    // else if (comboTask == C_PAUSE) comboStart++;

continue_controller:
    
    Keys->Value = get_processed_input(current_input).Value;

    printf("Current Input: A: %d | X: %d | Y: %d\n", current_input.A_BUTTON, current_input.X_AXIS, current_input.Y_AXIS);

    if (comboTask == C_RECORD)
    {
        //extend if full and frame is not 0
        char buf[64];
        sprintf(buf, "Recording combo (%d)", frame_counter - combo_start_frame + 1);
        set_status(buf);
        ACTIVE_COMBO->samples.push_back(*Keys);
    }

    set_visuals(*Keys);
}

void Status::update_joystick_position()
{
    // we sometimes dont receive lmb up notification, so its better to just check here
    if (!(GetAsyncKeyState(MOUSE_LBUTTONREDEFINITION) & 0x8000))
    {
        is_dragging_stick = false;
    }

    if (!is_dragging_stick)
    {
        return;
    }

    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(GetDlgItem(statusDlg, IDC_STICKPIC), &pt);
    RECT pic_rect;
    GetWindowRect(GetDlgItem(statusDlg, IDC_STICKPIC), &pic_rect);
    int x = (pt.x * 256 / (signed)(pic_rect.right - pic_rect.left) - 128 + 1);
    int y = -(pt.y * 256 / (signed)(pic_rect.bottom - pic_rect.top) - 128 + 1);
    
    // clamp joystick inside of control bounds
    if (x > 127 || y > 127 || x < -128 || y < -129)
    {
        int div = max(abs(x), abs(y));
        x = x * (x > 0 ? 127 : 128) / div;
        y = y * (y > 0 ? 127 : 128) / div;
    }
    // snap clicks to zero
    if (x < 7 && x > -7)
        x = 0;
    if (y < 7 && y > -7)
        y = 0;

    current_input.X_AXIS = x;
    current_input.Y_AXIS = y;
    set_visuals(current_input);
}


BUTTONS Status::get_processed_input(BUTTONS input)
{
    // TODO: implement combo overrides
    
    input.Value |= frame_counter % 2 == 0 ? autofire_input_a.Value : autofire_input_b.Value;
    
    return input;
}

void Status::set_visuals(BUTTONS input)
{
    input = get_processed_input(input);
    
    SetDlgItemText(statusDlg, IDC_EDITX, std::to_string(input.X_AXIS).c_str());
    SetDlgItemText(statusDlg, IDC_EDITY, std::to_string(input.Y_AXIS).c_str());
    
    CheckDlgButton(statusDlg, IDC_CHECK_A, input.A_BUTTON);
    CheckDlgButton(statusDlg, IDC_CHECK_B, input.B_BUTTON);
    CheckDlgButton(statusDlg, IDC_CHECK_START, input.START_BUTTON);
    CheckDlgButton(statusDlg, IDC_CHECK_L, input.L_TRIG);
    CheckDlgButton(statusDlg, IDC_CHECK_R, input.R_TRIG);
    CheckDlgButton(statusDlg, IDC_CHECK_Z, input.Z_TRIG);
    CheckDlgButton(statusDlg, IDC_CHECK_CUP, input.U_CBUTTON);
    CheckDlgButton(statusDlg, IDC_CHECK_CLEFT, input.L_CBUTTON);
    CheckDlgButton(statusDlg, IDC_CHECK_CRIGHT, input.R_CBUTTON);
    CheckDlgButton(statusDlg, IDC_CHECK_CDOWN, input.D_CBUTTON);
    CheckDlgButton(statusDlg, IDC_CHECK_DUP, input.U_DPAD);
    CheckDlgButton(statusDlg, IDC_CHECK_DLEFT, input.L_DPAD);
    CheckDlgButton(statusDlg, IDC_CHECK_DRIGHT, input.R_DPAD);
    CheckDlgButton(statusDlg, IDC_CHECK_DDOWN, input.D_DPAD);

    RECT rect = get_window_rect_client_space(statusDlg, GetDlgItem(statusDlg, IDC_STICKPIC));
    InvalidateRect(statusDlg, &rect, TRUE);
}

void Status::SetKeys(BUTTONS ControllerInput)
{
#if 0
    if (show_m64_inputs) //copy m64 data to current input
    {
        buttonOverride.Value = ControllerInput.Value;
        buttonOverride.X_AXIS = buttonOverride.Y_AXIS = 0;
    }
    show_m64_inputs = true;
    bool changed = false;
    if (statusDlg)
    {
        //true if physical controller state is changed (because logical is handled in GetKeys)
        if (buttonDisplayed.Value != ControllerInput.Value)
        {
            set_ui_buttons(ControllerInput);
            buttonDisplayed.Value = ControllerInput.Value;
        }
        if (relativeXOn == 3 && radialRecalc)
        {
            radialDistance = sqrtf(
                (float)((overrideX * overrideX) / (xScale * xScale) + (overrideY * overrideY) / (yScale * yScale)));
            if (radialDistance != 0)
                radialAngle = atan2f((float)-(overrideY / yScale), (float)(overrideX / xScale));
            radialRecalc = false;
        }

        // relative change amount
        int addX = (int)((ControllerInput.X_AXIS * xScale) / 12.0f);
        int addY = (int)((ControllerInput.Y_AXIS * yScale) / 12.0f);

        // calculate fractional (over time) change
        if (relativeXOn && ControllerInput.X_AXIS)
        {
            static float incrX = 0.0f;
            incrX += ((ControllerInput.X_AXIS * xScale) / 12.0f) - addX;
            if (incrX > 1.0f)
                addX++, incrX -= 1.0f;
            else if (incrX < -1.0f)
                addX--, incrX += 1.0f;
        }
        if ((relativeYOn || relativeXOn == 3) && ControllerInput.Y_AXIS)
        {
            static float incrY = 0.0f;
            incrY += ((ControllerInput.Y_AXIS * yScale) / 12.0f) - addY;
            if (incrY > 1.0f)
                addY++, incrY -= 1.0f;
            else if (incrY < -1.0f)
                addY--, incrY += 1.0f;
        }

        if (relativeXOn && overrideAllowed)
        {
            if (ControllerInput.X_AXIS)
            // increment x position by amount relative to attempted new x position
            {
                if (relativeXOn != 3)
                {
                    int nextVal = overrideX + addX;
                    if (nextVal <= 127 && nextVal >= -128)
                    {
                        if (!overrideX || (relativeXOn == 1
                                               ? (((overrideX < 0) == (ControllerInput.X_AXIS < 0)) || !ControllerInput.
                                                   X_AXIS)
                                               : ((overrideX < 0) != (nextVal > 0))))
                            ControllerInput.X_AXIS = nextVal;
                        else
                            ControllerInput.X_AXIS = 0;
                        // lock to 0 once on crossing +/- boundary, or "semi-relative" jump to 0 on direction reversal
                    }
                    else
                    {
                        ControllerInput.X_AXIS = nextVal > 0 ? 127 : -128;
                        if (abs(overrideY + addY) < 129)
                            overrideY = overrideY * 127 / abs(nextVal);
                    }
                }
                else // radial mode, angle change
                {
                    radialAngle += 4 * PI; // keeping it positive
                    float oldAngle = fmodf(radialAngle, 2 * PI);
                    radialAngle = fmodf(
                        radialAngle + (ControllerInput.X_AXIS) / (250.0f + 500.0f / (sqrtf(xScale * yScale) + 0.001f)),
                        2 * PI);

                    // snap where crossing 45 degree boundaries
                    if (radialAngle - oldAngle > PI) radialAngle -= 2 * PI;
                    else if (oldAngle - radialAngle > PI) oldAngle -= 2 * PI;
                    float oldDeg = oldAngle * (180.0f / PI);
                    float newDeg = radialAngle * (180.0f / PI);
                    for (float ang = 0; ang < 360; ang += 45)
                        if (fabsf(oldDeg - ang) > (0.0001f) && (oldDeg < ang) != (newDeg < ang))
                        {
                            radialAngle = ang * (PI / 180.0f);
                            break;
                        }

                    float xAng = xScale * radialDistance * cosf(radialAngle);
                    float yAng = yScale * radialDistance * sinf(radialAngle);
                    int newX = (int)(xAng + (xAng > 0 ? 0.5f : -0.5f));
                    int newY = -(int)(yAng + (yAng > 0 ? 0.5f : -0.5f));
                    if (newX > 127) newX = 127;
                    if (newX < -128) newX = -128;
                    if (newY > 127) newY = 127;
                    if (newY < -128) newY = -128;
                    ControllerInput.X_AXIS = newX;
                    overrideY = newY;
                }
            }
            else
                ControllerInput.X_AXIS = overrideX;
        }
        if ((relativeYOn || (relativeXOn == 3)) && overrideAllowed)
        {
            if ((ControllerInput.Y_AXIS || (!relativeYOn && ControllerInput.Y_AXIS !=
                LastControllerInput.Y_AXIS))) // increment y position by amount relative to attempted new y position
            {
                if (relativeXOn != 3) // not a typo (relativeXOn holds radial mode setting)
                {
                    int nextVal = overrideY + addY;
                    if (nextVal <= 127 && nextVal >= -128)
                    {
                        if (!overrideY || (relativeYOn == 1
                                               ? (((overrideY < 0) == (ControllerInput.Y_AXIS < 0)) || !ControllerInput.
                                                   Y_AXIS)
                                               : ((overrideY < 0) != (nextVal > 0))))
                            ControllerInput.Y_AXIS = nextVal;
                        else
                            ControllerInput.Y_AXIS = 0;
                        // lock to 0 once on crossing +/- boundary, or "semi-relative" jump to 0 on direction reversal
                    }
                    else
                    {
                        if (abs(overrideX + addX) < 129)
                            ControllerInput.X_AXIS = ControllerInput.X_AXIS * 127 / abs(nextVal);
                        ControllerInput.Y_AXIS = nextVal > 0 ? 127 : -128;
                    }
                }
                else // radial mode, length change
                {
                    if (!relativeYOn) // relative rotation + instant distance
                    {
                        if (ControllerInput.Y_AXIS < 0)
                            radialDistance = 0;
                        else if (!Controller[controller_index].SensMin || abs(ControllerInput.Y_AXIS) >= Controller[controller_index].
                            SensMin)
                        {
                            radialDistance = (float)ControllerInput.Y_AXIS;
                            if (radialDistance == 127) radialDistance = 128;
                        }
                        else
                            radialDistance = (float)Controller[controller_index].SensMin;
                        LastControllerInput.Y_AXIS = ControllerInput.Y_AXIS;
                    }
                    else if (relativeYOn == 1 && radialDistance && (radialDistance > 0) != (ControllerInput.Y_AXIS > 0))
                        radialDistance = 0; // "semi-relative" distance, jump to 0 on direction reversal
                    else
                        radialDistance += addY;
                    const static float maxDist = sqrtf(128 * 128 + 128 * 128);
                    if (radialDistance > maxDist) radialDistance = maxDist;
                    float xAng = xScale * radialDistance * cosf(radialAngle);
                    float yAng = yScale * radialDistance * sinf(radialAngle);
                    int newX = (int)(xAng + (xAng > 0 ? 0.5f : -0.5f));
                    int newY = -(int)(yAng + (yAng > 0 ? 0.5f : -0.5f));
                    if (newX > 127) newX = 127;
                    if (newX < -128) newX = -128;
                    if (newY > 127) newY = 127;
                    if (newY < -128) newY = -128;
                    ControllerInput.X_AXIS = newX;
                    ControllerInput.Y_AXIS = newY;
                }
            }
            else
                ControllerInput.Y_AXIS = overrideY;
        }
        if (!overrideOn && LastControllerInput.X_AXIS != ControllerInput.X_AXIS)
        {
            update_joystick_spinner(ControllerInput.X_AXIS, ControllerInput.Y_AXIS);
            changed = true;
        }
        if (!overrideOn && LastControllerInput.Y_AXIS != ControllerInput.Y_AXIS)
        {
            update_joystick_spinner(ControllerInput.X_AXIS, -ControllerInput.Y_AXIS);
            changed = true;
        }
    }
    if (relativeYOn || relativeXOn != 3)
        LastControllerInput = ControllerInput;

    if (changed & !overrideOn)
    {
        overrideX = ControllerInput.X_AXIS;
        overrideY = ControllerInput.Y_AXIS;
        RefreshAnalogPicture();
    }
    overrideOn = false; //reset every frame
#endif
}


void WINAPI GetNegAxisVal(LONG AxisValue, int Control, LONG count, BUTTONS* ControllerInput, int& M1Speed, int& M2Speed)
{
    switch (count)
    {
    case 0:
        if (AxisValue < (LONG)-Controller[Control].SensMax)
            ControllerInput->Y_AXIS = min(127, Controller[Control].SensMax);
        else
            ControllerInput->Y_AXIS = -AxisValue;
        break;
    case 1:
        if (AxisValue < (LONG)-Controller[Control].SensMax)
            ControllerInput->Y_AXIS = -min(128, Controller[Control].SensMax);
        else
            ControllerInput->Y_AXIS = AxisValue;
        break;
    case 2:
        if (AxisValue < (LONG)-Controller[Control].SensMax)
            ControllerInput->X_AXIS = -min(128, Controller[Control].SensMax);
        else
            ControllerInput->X_AXIS = AxisValue;
        break;
    case 3:
        if (AxisValue < (LONG)-Controller[Control].SensMax)
            ControllerInput->X_AXIS = min(127, Controller[Control].SensMax);
        else
            ControllerInput->X_AXIS = -AxisValue;
        break;

    case 18:
        M1Speed = Controller[Control].Input[count].button;
        break;
    case 19:
        M2Speed = Controller[Control].Input[count].button;
        break;

    default:
        ControllerInput->Value |= Controller[Control].Input[count].button;
        break;
    }
}

void WINAPI GetPosAxisVal(LONG AxisValue, int Control, LONG count, BUTTONS* ControllerInput, int& M1Speed, int& M2Speed)
{
    switch (count)
    {
    case 0:
        if (AxisValue > (LONG)Controller[Control].SensMax)
            ControllerInput->Y_AXIS = min(127, Controller[Control].SensMax);
        else
            ControllerInput->Y_AXIS = AxisValue;
        break;
    case 1:
        if (AxisValue > (LONG)Controller[Control].SensMax)
            ControllerInput->Y_AXIS = -min(128, Controller[Control].SensMax);
        else
            ControllerInput->Y_AXIS = -AxisValue;
        break;
    case 2:
        if (AxisValue > (LONG)Controller[Control].SensMax)
            ControllerInput->X_AXIS = -min(128, Controller[Control].SensMax);
        else
            ControllerInput->X_AXIS = -AxisValue;
        break;
    case 3:
        if (AxisValue > (LONG)Controller[Control].SensMax)
            ControllerInput->X_AXIS = min(127, Controller[Control].SensMax);
        else
            ControllerInput->X_AXIS = AxisValue;
        break;

    case 18:
        M1Speed = Controller[Control].Input[count].button;
        break;
    case 19:
        M2Speed = Controller[Control].Input[count].button;
        break;

    default:
        ControllerInput->Value |= Controller[Control].Input[count].button;
        break;
    }
}


EXPORT void CALL InitiateControllers(HWND hMainWindow, CONTROL Controls[4])
{
    HKEY hKey;
    DWORD dwSize, dwType;
    emulator_hwnd = hMainWindow;
    for (BYTE i = 0; i < NUMBER_OF_CONTROLS; i++)
    {
        ControlDef[i] = &Controls[i];
        ControlDef[i]->Present = FALSE;
        ControlDef[i]->RawData = FALSE;
        ControlDef[i]->Plugin = PLUGIN_NONE;

        Controller[i].NDevices = 0;
        Controller[i].bActive = i == 0 ? TRUE : FALSE;
        Controller[i].SensMax = 128;
        Controller[i].SensMin = 32;
        Controller[i].Input[18].button = 42;
        Controller[i].Input[19].button = 20;
        wsprintf(Controller[i].szName, "Controller %d", i + 1);
    }

    if (g_lpDI == NULL)
    {
        InitializeAndCheckDevices(hMainWindow);
    }

    dwType = REG_BINARY;
    dwSize = sizeof(DEFCONTROLLER);

    if (RegCreateKeyEx(HKEY_CURRENT_USER, SUBKEY, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, 0) != ERROR_SUCCESS)
    {
        MessageBox(NULL, "Could not create Registry Key", "Error", MB_ICONERROR | MB_OK);
    }
    else
    {
        for (BYTE NController = 0; NController < NUMBER_OF_CONTROLS; NController++)
        {
            if (RegQueryValueEx(hKey, Controller[NController].szName, 0, &dwType, (LPBYTE)&Controller[NController],
                                &dwSize) == ERROR_SUCCESS)
            {
                if (Controller[NController].bActive)
                    ControlDef[NController]->Present = TRUE;
                else
                    ControlDef[NController]->Present = FALSE;

                if (Controller[NController].bMemPak)
                    ControlDef[NController]->Plugin = PLUGIN_MEMPAK;
                else
                    ControlDef[NController]->Plugin = PLUGIN_NONE;

                if (dwSize != sizeof(DEFCONTROLLER))
                {
                    dwType = REG_BINARY;
                    dwSize = sizeof(DEFCONTROLLER);
                    ZeroMemory(&Controller[NController], sizeof(DEFCONTROLLER));

                    Controller[NController].NDevices = 0;
                    Controller[NController].bActive = NController == 0 ? TRUE : FALSE;
                    ControlDef[NController]->Present = FALSE;
                    ControlDef[NController]->Plugin = PLUGIN_NONE;
                    Controller[NController].SensMax = 128;
                    Controller[NController].SensMin = 32;
                    Controller[NController].Input[18].button = 42;
                    Controller[NController].Input[19].button = 20;
                    wsprintf(Controller[NController].szName, "Controller %d", NController + 1);

                    RegDeleteValue(hKey, Controller[NController].szName);
                    RegSetValueEx(hKey, Controller[NController].szName, 0, dwType, (LPBYTE)&Controller[NController],
                                  dwSize);
                }
            }
            else
            {
                dwType = REG_BINARY;
                dwSize = sizeof(DEFCONTROLLER);
                RegDeleteValue(hKey, Controller[NController].szName);
                RegSetValueEx(hKey, Controller[NController].szName, 0, dwType, (LPBYTE)&Controller[NController],
                              dwSize);
            }
        }
    }
    RegCloseKey(hKey);
}

void WINAPI InitializeAndCheckDevices(HWND hMainWindow)
{
    HKEY hKey;
    BYTE i;
    DWORD dwSize, dwType;

    //Initialize Direct Input function
    if (FAILED(InitDirectInput(hMainWindow)))
    {
        MessageBox(NULL, "DirectInput Initialization Failed!", "Error",MB_ICONERROR | MB_OK);
        FreeDirectInput();
    }
    else
    {
        dwType = REG_BINARY;
        dwSize = sizeof(Guids);
        //Check Guids for Device Changes
        RegCreateKeyEx(HKEY_CURRENT_USER, SUBKEY, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, 0);
        if (RegQueryValueEx(hKey, TEXT("Guids"), 0, &dwType, (LPBYTE)Guids, &dwSize) != ERROR_SUCCESS)
        {
            for (i = 0; i < MAX_DEVICES; i++)
            {
                if (DInputDev[i].lpDIDevice == NULL)
                    ZeroMemory(&Guids[i], sizeof(GUID));
                else
                    memcpy(&Guids[i], &DInputDev[i].DIDevInst.guidInstance, sizeof(GUID));
            }
            dwType = REG_BINARY;
            RegSetValueEx(hKey, TEXT("Guids"), 0, dwType, (LPBYTE)Guids, dwSize);
        }
        else
        {
            if (CheckForDeviceChange(hKey))
            {
                for (i = 0; i < MAX_DEVICES; i++)
                {
                    if (DInputDev[i].lpDIDevice == NULL)
                        ZeroMemory(&Guids[i], sizeof(GUID));
                    else
                        memcpy(&Guids[i], &DInputDev[i].DIDevInst.guidInstance, sizeof(GUID));
                }
                dwType = REG_BINARY;
                RegSetValueEx(hKey, TEXT("Guids"), 0, dwType, (LPBYTE)Guids, dwSize);
            }
        }
        RegCloseKey(hKey);
    }
}

BOOL WINAPI CheckForDeviceChange(HKEY hKey)
{
    BOOL DeviceChanged;
    DWORD dwSize, dwType;

    dwType = REG_BINARY;
    dwSize = sizeof(DEFCONTROLLER);

    DeviceChanged = FALSE;

    for (BYTE DeviceNumCheck = 0; DeviceNumCheck < MAX_DEVICES; DeviceNumCheck++)
    {
        if (memcmp(&Guids[DeviceNumCheck], &DInputDev[DeviceNumCheck].DIDevInst.guidInstance, sizeof(GUID)) != 0)
        {
            DeviceChanged = TRUE;
            for (BYTE NController = 0; NController < NUMBER_OF_CONTROLS; NController++)
            {
                RegQueryValueEx(hKey, Controller[NController].szName, 0, &dwType, (LPBYTE)&Controller[NController],
                                &dwSize);
                for (BYTE DeviceNum = 0; DeviceNum < Controller[NController].NDevices; DeviceNum++)
                {
                    if (Controller[NController].Devices[DeviceNum] == DeviceNumCheck)
                    {
                        Controller[NController].NDevices = 0;
                        Controller[NController].bActive = FALSE;
                        RegSetValueEx(hKey, Controller[NController].szName, 0, dwType, (LPBYTE)&Controller[NController],
                                      dwSize);
                    }
                }
            }
        }
    }

    return DeviceChanged;
}

EXPORT void CALL ReadController(int Control, BYTE* Command)
{
    // XXX: Increment frame counter here because the plugin specification provides no means of finding out when a frame goes by.
    //      Mupen64 calls ReadController(-1) every input frame, but other emulators might not do that.
    //      (The frame counter is used only for autofire and combo progression.)
    if (Control == -1)
        frame_counter++;
}

EXPORT void CALL RomClosed(void)
{
    romIsOpen = false;
    for (int i = 0; i < NUMBER_OF_CONTROLS; i++)
        status[i].StopThread();
}

EXPORT void CALL DllTest(HWND hParent)
{
}

EXPORT void CALL RomOpen(void)
{
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
        for (BYTE Control = 0; Control < NUMBER_OF_CONTROLS; Control++)
        {
            RegQueryValueEx(hKey, Controller[Control].szName, 0, &dwType, (LPBYTE)&Controller[Control], &dwSize);
            if (Controller[Control].bActive)
                ControlDef[Control]->Present = TRUE;
            else
                ControlDef[Control]->Present = FALSE;
        }
    }
    RegCloseKey(hKey);

    for (int i = 0; i < NUMBER_OF_CONTROLS; i++)
        if (Controller[i].bActive)
            status[i].StartThread(i);
        else
            status[i].controller_index = i;
}

EXPORT void CALL WM_KeyDown(WPARAM wParam, LPARAM lParam)
{
}

EXPORT void CALL WM_KeyUp(WPARAM wParam, LPARAM lParam)
{
}

void Status::FreeCombos()
{
    for (auto combo : combos)
    {
        delete combo;
    }
    combos.clear();
}

//sets information about current task
void Status::set_status(std::string str)
{
    HWND hTask = GetDlgItem(statusDlg, IDC_STATUS);
    SendMessage(hTask, WM_SETTEXT, 0, (LPARAM)str.c_str());
}

//Creates new combo, if id!=1 duplicates existing (todo)
int Status::CreateNewCombo(int id)
{
    auto combo = new Combo();
    combos.push_back(combo);
    return ListBox_InsertString(listbox, -1, combo->name.c_str());
}

//shows edit box
void Status::StartEdit(int id)
{
    RECT edit; //, box;
    char txt[MAX_PATH];
    ListBox_GetItemRect(listbox, id, &edit);
    //If editBox isn't child of listbox (which would solve padding issues, but doesn't quite work)
    //GetWindowRect(listbox, &box);
    //POINT pt = { box.left, box.top };
    //ScreenToClient(statusDlg, &pt);
    //edit.top += pt.y;
    //edit.left += pt.x;
    //edit.bottom += pt.y;
    //edit.right += pt.x;
    HWND editBox = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP, edit.left, edit.top,
                                  edit.right - edit.left, edit.bottom - edit.top + 4, listbox, 0, g_hInstance, 0);
    ListBox_SetCurSel(listbox, -1); //just to be safe, it sometimes can be seen in background

    //FW_BOLD
    //HFONT font = CreateFont(12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE, "MS Shell Dlg 2");
    SendMessage(editBox,WM_SETFONT, (WPARAM)SendMessage(listbox, WM_GETFONT, 0, 0), 0);
    SetWindowSubclass(editBox, EditBoxProc, 0, 0); //to add more functionality
    ListBox_GetText(listbox, activeCombo, txt);
    SendMessage(editBox, WM_SETTEXT, 0, (LPARAM)txt);
    PostMessage(statusDlg, WM_NEXTDLGCTL, (WPARAM)editBox, TRUE);
}

void Status::EndEdit(int id, char* name)
{
    if (name != NULL)
    {
        ListBox_DeleteString(listbox, id);

        if (name[0] == NULL)
        {
            combos.erase(combos.begin() + id);
        }
        else
        {
            ListBox_InsertString(listbox, id, name);
        }
    }
    set_status("Idle");
}

//saves combos to combos.cmb
void Status::SaveCombos()
{
    save_combos("combos.cmb", combos);
}

//load combos to listBox
void Status::load_combos(const char* path)
{
    combos = find_combos("combos.cmb");

    for (auto combo : combos)
    {
        ListBox_InsertString(listbox, -1, combo->name.c_str());
    }
}

DWORD WINAPI StatusDlgThreadProc(LPVOID lpParameter)
{
    int Control = LOBYTE(*(int*)lpParameter);
    auto extend = HIBYTE(*(int*)lpParameter);
    int DialogID = extend ? IDD_STATUS_COMBOS : IDD_STATUS_NORMAL;

    switch (Control)
    {
    case 0: DialogBox(g_hInstance, MAKEINTRESOURCE(DialogID), NULL, (DLGPROC)StatusDlgProc0);
        break;
    case 1: DialogBox(g_hInstance, MAKEINTRESOURCE(DialogID), NULL, (DLGPROC)StatusDlgProc1);
        break;
    case 2: DialogBox(g_hInstance, MAKEINTRESOURCE(DialogID), NULL, (DLGPROC)StatusDlgProc2);
        break;
    case 3: DialogBox(g_hInstance, MAKEINTRESOURCE(DialogID), NULL, (DLGPROC)StatusDlgProc3);
        break;
    default: DialogBox(g_hInstance, MAKEINTRESOURCE(DialogID), NULL, (DLGPROC)StatusDlgProc0);
    }
    return 0;
}

static bool IsMouseOverControl(HWND hDlg, int dialogItemID)
{
    POINT pt;
    RECT rect;

    GetCursorPos(&pt);
    if (GetWindowRect(GetDlgItem(hDlg, dialogItemID), &rect)) //failed to get the dimensions
        return (pt.x <= rect.right && pt.x >= rect.left && pt.y <= rect.bottom && pt.y >= rect.top);
    return FALSE;
}


bool ShowContextMenu(HWND hwnd, HWND hitwnd, int x, int y)
{
    if (hitwnd != hwnd || IsMouseOverControl(hwnd, IDC_STICKPIC) || (GetKeyState(MOUSE_LBUTTONREDEFINITION) & 0x8000))
        return TRUE;

    // HACK: disable topmost so menu doesnt appear under tasinput
    hMenu = CreatePopupMenu();
    AppendMenu(hMenu, new_config.always_on_top ? MF_CHECKED : 0, offsetof(t_config, always_on_top),
               "Always on top");
    AppendMenu(hMenu, new_config.float_from_parent ? MF_CHECKED : 0, offsetof(t_config, float_from_parent),
               "Float from parent");
    AppendMenu(hMenu, new_config.titlebar ? MF_CHECKED : 0, offsetof(t_config, titlebar),
           "Titlebar");

    int offset = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, x, y, hwnd, 0);

    if (offset != 0)
    {
        // offset is the offset into menu config struct of the field which was selected by user, we need to convert it from byte offset to int-width offset 
        auto arr = reinterpret_cast<int32_t*>(&new_config);
        arr[offset / sizeof(int32_t)] ^= true;
    }

    for (auto status_dlg : status)
    {
        if (status_dlg.initialized && status_dlg.statusDlg)
        {
            status_dlg.on_config_changed();
        }
    }

    DestroyMenu(hMenu);
    return TRUE;
}

#define MAKE_DLG_PROC(i) \
LRESULT CALLBACK StatusDlgProc##i (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) \
{ \
	status[i].statusDlg = hDlg; \
	return status[i].StatusDlgMethod(msg, wParam, lParam); \
}
MAKE_DLG_PROC(0)
MAKE_DLG_PROC(1)
MAKE_DLG_PROC(2)
MAKE_DLG_PROC(3)


LRESULT Status::StatusDlgMethod(UINT msg, WPARAM wParam, LPARAM lParam)
{
    static bool last_lmb_down = false;
    static bool last_rmb_down = false;
    bool lmb_just_up = !(GetAsyncKeyState(MOUSE_LBUTTONREDEFINITION) & 0x8000) && last_lmb_down;
    bool rmb_just_down = GetAsyncKeyState(MOUSE_RBUTTONREDEFINITION) & 0x8000 && !last_rmb_down;

    if (initialized || msg == WM_INITDIALOG /*|| msg == WM_DESTROY || msg == WM_NCDESTROY*/)
        switch (msg)
        {
        case WM_CONTEXTMENU:
            ShowContextMenu(statusDlg, (HWND)wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            break;
        case WM_INITDIALOG:
            {
                x_scale = 1.0f;
                y_scale = 1.0f;
                is_dragging_stick = false;

                SetWindowText(statusDlg, std::format("TASInput - Controller {}", controller_index + 1).c_str());

                // set ranges
                SendDlgItemMessage(statusDlg, IDC_SLIDERX, TBM_SETRANGE, TRUE, (LPARAM)MAKELONG(10, 2010));
                SendDlgItemMessage(statusDlg, IDC_SLIDERX, TBM_SETPOS, TRUE, 1000);
                SendDlgItemMessage(statusDlg, IDC_SLIDERY, TBM_SETRANGE, TRUE, (LPARAM)MAKELONG(10, 2010));
                SendDlgItemMessage(statusDlg, IDC_SLIDERY, TBM_SETPOS, TRUE, 1000);

                // begin accepting other messages
                initialized = true;

                //Load combos
                //I realised there's HasPanel() too, but doesn't make much difference
                listbox = GetDlgItem(statusDlg, IDC_MACROLIST);
                if (listbox)
                {
                    FreeCombos();
                    load_combos("combos.cmb");
                }

                // windows likes to scale stick control in particular, so we force it to a specific size
                SetWindowPos(GetDlgItem(statusDlg, IDC_STICKPIC), nullptr, 0, 0, 131, 131, SWP_NOMOVE);

                SetTimer(statusDlg, IDT_TIMER_STATUS_0 + controller_index, 1, nullptr);
                load_config();
                on_config_changed();
            }
            break;
        case SC_MINIMIZE:
            DestroyMenu(hMenu); // nuke context menu when minimized...
            break;
        case WM_NCDESTROY:
        case WM_DESTROY:
            {
                initialized = false;
                KillTimer(statusDlg, IDT_TIMER_STATUS_0 + controller_index);
                statusDlg = NULL;
            }
            break;
        case WM_SETCURSOR:
            {
                if (lmb_just_up)
                {
                    // activate mupen window to allow it to get key inputs
                    SetForegroundWindow(emulator_hwnd);
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
                    AUTOFIRE(IDC_CHECK_A, A_BUTTON);
                    AUTOFIRE(IDC_CHECK_B, B_BUTTON);
                    AUTOFIRE(IDC_CHECK_START, START_BUTTON);
                    AUTOFIRE(IDC_CHECK_L, L_TRIG);
                    AUTOFIRE(IDC_CHECK_R, R_TRIG);
                    AUTOFIRE(IDC_CHECK_Z, Z_TRIG);
                    AUTOFIRE(IDC_CHECK_CUP, U_CBUTTON);
                    AUTOFIRE(IDC_CHECK_CLEFT, L_CBUTTON);
                    AUTOFIRE(IDC_CHECK_CRIGHT, R_CBUTTON);
                    AUTOFIRE(IDC_CHECK_CDOWN, D_CBUTTON);
                    AUTOFIRE(IDC_CHECK_DUP, U_DPAD);
                    AUTOFIRE(IDC_CHECK_DLEFT, L_DPAD);
                    AUTOFIRE(IDC_CHECK_DRIGHT, R_DPAD);
                    AUTOFIRE(IDC_CHECK_DDOWN, D_DPAD);
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
            BUTTONS controller_input = get_controller_input();

            if (controller_input.Value != last_controller_input.Value)
            {
                // Input changed, override everything with current

#define BTN(field)\
                if (controller_input.field && !last_controller_input.field)\
                {\
                    current_input.field = 1;\
                }\
                if (!controller_input.field && last_controller_input.field)\
                {\
                    current_input.field = 0;\
                }\

#define JOY(field)\
                if (controller_input.field != last_controller_input.field)\
                {\
                    current_input.field = controller_input.field;\
                }\
                
                BTN(R_DPAD)
                BTN(L_DPAD)
                BTN(D_DPAD)
                BTN(U_DPAD)
                BTN(START_BUTTON)
                BTN(Z_TRIG)
                BTN(B_BUTTON)
                BTN(A_BUTTON)
                BTN(R_CBUTTON)
                BTN(L_CBUTTON)
                BTN(D_CBUTTON)
                BTN(U_CBUTTON)
                BTN(R_TRIG)
                BTN(L_TRIG)
                JOY(X_AXIS)
                JOY(Y_AXIS)
                
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
                joystick_rect.left += 2;
                joystick_rect.top += 2;
                joystick_rect.right -= 4;
                joystick_rect.bottom -= 4;

                GetClientRect(statusDlg, &window_rect);
                POINT joystick_rect_size = {
                    joystick_rect.right - joystick_rect.left, joystick_rect.bottom - joystick_rect.top
                };


                // set up double buffering
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(statusDlg, &ps);
                HDC compat_dc = CreateCompatibleDC(hdc);
                HBITMAP bmp = CreateCompatibleBitmap(hdc, joystick_rect_size.x, joystick_rect_size.y);
                SelectObject(compat_dc, bmp);

                static HPEN outline_pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
                static HPEN line_pen = CreatePen(PS_SOLID, 3, RGB(0, 0, 255));
                static HPEN tip_pen = CreatePen(PS_SOLID, 7, RGB(255, 0, 0));

                int mid_x = joystick_rect_size.x / 2;
                int mid_y = joystick_rect_size.y / 2;
                int stick_x = (current_input.X_AXIS + 128) * joystick_rect_size.x / 256;
                int stick_y = (-current_input.Y_AXIS + 128) * joystick_rect_size.y / 256;

                // clear background with color which makes background (hopefully)
                // cool idea: maybe use user accent color for joystick tip?
                RECT normalized = {0, 0, joystick_rect_size.x, joystick_rect_size.y};
                FillRect(compat_dc, &normalized, GetSysColorBrush(COLOR_BTNFACE));

                // draw the back layer: ellipse and alignment lines
                SelectObject(compat_dc, outline_pen);
                Ellipse(compat_dc, 0, 0, joystick_rect_size.x, joystick_rect_size.y);
                MoveToEx(compat_dc, 0, mid_y, NULL);
                LineTo(compat_dc, joystick_rect_size.x, mid_y);
                MoveToEx(compat_dc, mid_x, 0, NULL);
                LineTo(compat_dc, mid_x, joystick_rect_size.y);

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
                BitBlt(hdc, joystick_rect.left, joystick_rect.top, joystick_rect_size.x, joystick_rect_size.y,
                       compat_dc, 0, 0, SRCCOPY);

                EndPaint(statusDlg, &ps);
                DeleteDC(compat_dc);
                DeleteObject(bmp);
            }

            break;
        case WM_NOTIFY:
            {
                switch (LOWORD(wParam))
                {
                case IDC_SLIDERX:
                    {
                        int pos = SendDlgItemMessage(statusDlg, IDC_SLIDERX, TBM_GETPOS, 0, 0);
                        x_scale = pos / 1000.0f;
                    }
                    break;

                case IDC_SLIDERY:
                    {
                        int pos = SendDlgItemMessage(statusDlg, IDC_SLIDERY, TBM_GETPOS, 0, 0);
                        y_scale = pos / 1000.0f;
                    }
                    break;
                }
            }
            break;
        case WM_LBUTTONDOWN:
            printf("ld\n");
            if (IsMouseOverControl(statusDlg,IDC_STICKPIC))
            {
                is_dragging_stick = true;
                SetForegroundWindow(emulator_hwnd);
            }
            break;
        case WM_MOUSEMOVE:
            update_joystick_position();
            break;
        case EDIT_END:
            EndEdit(activeCombo, (char*)lParam);
            break;
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_EDITX:
                {
                    BUTTONS last_input = current_input;
                    char str[32] = {0};
                    GetDlgItemText(statusDlg, IDC_EDITX, str, std::size(str));
                    current_input.X_AXIS = std::atoi(str);

                    // We don't want an infinite loop, since set_visuals will send IDC_EDITX again
                    if (current_input.X_AXIS != last_input.X_AXIS)
                    {
                        set_visuals(current_input);
                    }
                }
                break;

            case IDC_EDITY:
                {
                    BUTTONS last_input = current_input;
                    char str[32] = {0};
                    GetDlgItemText(statusDlg, IDC_EDITY, str, std::size(str));
                    current_input.Y_AXIS = std::atoi(str);

                    // We don't want an infinite loop, since set_visuals will send IDC_EDITX again
                    if (current_input.Y_AXIS != last_input.Y_AXIS)
                    {
                        set_visuals(current_input);
                    }
                }
                break;
            //on checkbox click set buttonOverride and buttonDisplayed field and reset autofire
            case IDC_CHECK_A:
                TOGGLE(A_BUTTON);
                break;
            case IDC_CHECK_B:
                TOGGLE(B_BUTTON);
                break;
            case IDC_CHECK_START:
                TOGGLE(START_BUTTON);
                break;
            case IDC_CHECK_Z:
                TOGGLE(Z_TRIG);
                break;
            case IDC_CHECK_L:
                TOGGLE(L_TRIG);
                break;
            case IDC_CHECK_R:
                TOGGLE(R_TRIG);
                break;
            case IDC_CHECK_CLEFT:
                TOGGLE(L_CBUTTON);
                break;
            case IDC_CHECK_CUP:
                TOGGLE(U_CBUTTON);
                break;
            case IDC_CHECK_CRIGHT:
                TOGGLE(R_CBUTTON);
                break;
            case IDC_CHECK_CDOWN:
                TOGGLE(D_CBUTTON);
                break;
            case IDC_CHECK_DLEFT:
                TOGGLE(L_DPAD);
                break;
            case IDC_CHECK_DUP:
                TOGGLE(U_DPAD);
                break;
            case IDC_CHECK_DRIGHT:
                TOGGLE(R_DPAD);
                break;
            case IDC_CHECK_DDOWN:
                TOGGLE(D_DPAD);
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
                current_input.X_AXIS += increment;
                set_visuals(current_input);
            }
            break;
        case IDC_Y_DOWN:
        case IDC_Y_UP:
            {
                int increment = get_joystick_increment(LOWORD(wParam) == IDC_Y_DOWN);
                current_input.Y_AXIS += increment;
                set_visuals(current_input);
            }
            break;
            case IDC_MOREBUTTON4:
            case IDC_MOREBUTTON5:
                {
                    switch (LOWORD(wParam)) // Extend
                    {
                    case IDC_MOREBUTTON4: Extend = 1;
                        break;
                    case IDC_MOREBUTTON5: Extend = 0;
                        break;
                    }

                    comboTask = C_IDLE;
                    RECT rect;
                    GetWindowRect(statusDlg, &rect);

                    // Extend the dialog by replacing it with a new one created from a different resource.
                    // Resizing wouldn't work, because any resizing causes visible damage to the dialog's background
                    // due to some messages not getting through to repair it
                    StartThread(controller_index);
                }
                break;
            case IDC_PLAY:
                activeCombo = ListBox_GetCurSel(GetDlgItem(statusDlg, IDC_MACROLIST));
                if (activeCombo == -1)
                {
                    set_status("Select a combo first");
                    break;
                }
                set_status("Playing combo");
                CheckDlgButton(statusDlg, IDC_LOOP, 0);
                if (comboTask != C_PAUSE) combo_start_frame = frame_counter;
                comboTask = C_RUNNING;
                break;
            case IDC_STOP:
                set_status("Idle");
                CheckDlgButton(statusDlg, IDC_LOOP, 0);
                comboTask = C_IDLE;
                combo_start_frame = 0; //should avoid unnecessary bugs
                break;
            case IDC_PAUSE:
                if (comboTask == C_RUNNING || comboTask == C_LOOP)
                {
                    set_status("Paused");
                    CheckDlgButton(statusDlg, IDC_LOOP, 0);
                    comboTask = C_PAUSE;
                }
                break;
            case IDC_LOOP:
                if (comboTask == C_LOOP)
                {
                    set_status("Stopped looping");
                    comboTask = C_RUNNING;
                    break;
                }
                activeCombo = ListBox_GetCurSel(GetDlgItem(statusDlg, IDC_MACROLIST));
                if (activeCombo == -1)
                {
                    set_status("Select a combo first");
                    break;
                }
                if (comboTask != C_PAUSE) combo_start_frame = frame_counter;
                comboTask = C_LOOP;
                set_status("Looping combo");
                break;
            case IDC_RECORD:
                if (comboTask == C_RECORD)
                {
                    set_status("Recording stopped");
                    comboTask = C_IDLE;
                    //free(aCombo.data); //is it better to copy to new, aligned buffer or leave it?
                }
                else
                {
                    if (comboTask == C_IDLE || activeCombo == -1)
                    {
                        set_status("Recording new combo");
                        activeCombo = CreateNewCombo(-1);
                        ListBox_SetCurSel(listbox, activeCombo);
                        combo_start_frame = frame_counter;
                    }
                    else
                    {
                        set_status("Overwriting combo");
                        // FIXME: is this right?
                        ACTIVE_COMBO->samples = {};
                    }
                    comboTask = C_RECORD;
                }
                break;
            case IDC_EDIT:
                activeCombo = ListBox_GetCurSel(GetDlgItem(statusDlg, IDC_MACROLIST));
                if (activeCombo == -1)
                {
                    set_status("Select a combo first");
                    break;
                }
                set_status("Editing...");
                StartEdit(activeCombo);
                break;
            case IDC_CLEAR:
                comboTask = C_IDLE;
                activeCombo = -1;
                set_status("Clearing...");
                CheckDlgButton(statusDlg, IDC_LOOP, 0);
                FreeCombos();
                ListBox_ResetContent(listbox);
                set_status("Cleared all combos");
                break;
            case IDC_IMPORT:
                {
                    set_status("Importing...");
                    OPENFILENAME data = {0};
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
                set_status("Saving...");
                SaveCombos();
                set_status("Saved to combos.cmb");
                break;
            default:
                break;
            }
            break;

        default:
            break;
        }
    return FALSE; //Using DefWindowProc is prohibited but worked anyway
}

