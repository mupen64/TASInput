/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include <DirectInputHelper.h>
#include <NewConfig.h>

GUID g_guids[MAX_DEVICES];
DEFCONTROLLER g_controllers[NUMBER_OF_CONTROLS];
core_controller* g_controllers_default[NUMBER_OF_CONTROLS];

LPDIRECTINPUT8 g_di = NULL;
DIINPUTDEVICE g_di_devices[MAX_DEVICES];
BYTE g_device_count;

BOOL dih_check_for_device_change(HKEY hKey)
{
    BOOL DeviceChanged;
    DWORD dwSize, dwType;

    dwType = REG_BINARY;
    dwSize = sizeof(DEFCONTROLLER);

    DeviceChanged = FALSE;

    for (BYTE DeviceNumCheck = 0; DeviceNumCheck < MAX_DEVICES; DeviceNumCheck++)
    {
        if (memcmp(&g_guids[DeviceNumCheck], &g_di_devices[DeviceNumCheck].DIDevInst.guidInstance, sizeof(GUID)) != 0)
        {
            DeviceChanged = TRUE;
            for (BYTE NController = 0; NController < NUMBER_OF_CONTROLS; NController++)
            {
                RegQueryValueEx(hKey, g_controllers[NController].szName, 0, &dwType, (LPBYTE)&g_controllers[NController], &dwSize);
                for (BYTE DeviceNum = 0; DeviceNum < g_controllers[NController].NDevices; DeviceNum++)
                {
                    if (g_controllers[NController].Devices[DeviceNum] == DeviceNumCheck)
                    {
                        g_controllers[NController].NDevices = 0;
                        g_controllers[NController].bActive = FALSE;
                        RegSetValueEx(hKey, g_controllers[NController].szName, 0, dwType, (LPBYTE)&g_controllers[NController], dwSize);
                    }
                }
            }
        }
    }

    return DeviceChanged;
}


void WINAPI GetNegAxisVal(LONG AxisValue, int Control, LONG count, core_buttons* ControllerInput, int& M1Speed, int& M2Speed)
{
    switch (count)
    {
    case 0:
        if (AxisValue < (LONG)-g_controllers[Control].SensMax)
            ControllerInput->y = min(127, g_controllers[Control].SensMax);
        else
            ControllerInput->y = -AxisValue;
        break;
    case 1:
        if (AxisValue < (LONG)-g_controllers[Control].SensMax)
            ControllerInput->y = -min(128, g_controllers[Control].SensMax);
        else
            ControllerInput->y = AxisValue;
        break;
    case 2:
        if (AxisValue < (LONG)-g_controllers[Control].SensMax)
            ControllerInput->x = -min(128, g_controllers[Control].SensMax);
        else
            ControllerInput->x = AxisValue;
        break;
    case 3:
        if (AxisValue < (LONG)-g_controllers[Control].SensMax)
            ControllerInput->x = min(127, g_controllers[Control].SensMax);
        else
            ControllerInput->x = -AxisValue;
        break;

    case 18:
        M1Speed = g_controllers[Control].Input[count].button;
        break;
    case 19:
        M2Speed = g_controllers[Control].Input[count].button;
        break;

    default:
        ControllerInput->value |= g_controllers[Control].Input[count].button;
        break;
    }
}

void WINAPI GetPosAxisVal(LONG AxisValue, int Control, LONG count, core_buttons* ControllerInput, int& M1Speed, int& M2Speed)
{
    switch (count)
    {
    case 0:
        if (AxisValue > (LONG)g_controllers[Control].SensMax)
            ControllerInput->y = min(127, g_controllers[Control].SensMax);
        else
            ControllerInput->y = AxisValue;
        break;
    case 1:
        if (AxisValue > (LONG)g_controllers[Control].SensMax)
            ControllerInput->y = -min(128, g_controllers[Control].SensMax);
        else
            ControllerInput->y = -AxisValue;
        break;
    case 2:
        if (AxisValue > (LONG)g_controllers[Control].SensMax)
            ControllerInput->x = -min(128, g_controllers[Control].SensMax);
        else
            ControllerInput->x = -AxisValue;
        break;
    case 3:
        if (AxisValue > (LONG)g_controllers[Control].SensMax)
            ControllerInput->x = min(127, g_controllers[Control].SensMax);
        else
            ControllerInput->x = AxisValue;
        break;

    case 18:
        M1Speed = g_controllers[Control].Input[count].button;
        break;
    case 19:
        M2Speed = g_controllers[Control].Input[count].button;
        break;

    default:
        ControllerInput->value |= g_controllers[Control].Input[count].button;
        break;
    }
}

BOOL CALLBACK EnumAxesCallback(const DIDEVICEOBJECTINSTANCE* pdidoi,
                               LPVOID pContext)
{
    auto hDlg = (HWND)pContext;

    DIPROPRANGE diprg;
    diprg.diph.dwSize = sizeof(DIPROPRANGE);
    diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    diprg.diph.dwHow = DIPH_BYID;
    diprg.diph.dwObj = pdidoi->dwType; // Specify the enumerated axis
    diprg.lMin = -127;
    diprg.lMax = +128;

    // Set the range for the axis
    if FAILED (g_di_devices[g_device_count].lpDIDevice->SetProperty(DIPROP_RANGE, &diprg.diph))
        return DIENUM_STOP;

    return TRUE;
}

BOOL CALLBACK DIEnumDevicesCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef)
{
    HRESULT hr;
    auto hMainWindow = (HWND)pvRef;
    BOOL bOK = TRUE;

    if (g_device_count >= MAX_DEVICES)
        return DIENUM_STOP;

    if ((lpddi->dwDevType & DI8DEVTYPE_KEYBOARD) == DI8DEVTYPE_KEYBOARD)
    {
        memcpy(&g_di_devices[g_device_count].DIDevInst, lpddi, sizeof(DIDEVICEINSTANCE));
        if (!FAILED(hr = g_di->CreateDevice(lpddi->guidInstance,
                                            &g_di_devices[g_device_count].lpDIDevice,
                                            0)))
        {
            if FAILED (hr = g_di_devices[g_device_count].lpDIDevice->SetDataFormat(&c_dfDIKeyboard))
                bOK = FALSE;
            if FAILED (hr = g_di_devices[g_device_count].lpDIDevice->SetCooperativeLevel(hMainWindow,
                                                                                         DISCL_BACKGROUND | DISCL_NONEXCLUSIVE))
                bOK = FALSE;
        }
    }
    else if ((lpddi->dwDevType & DI8DEVTYPE_JOYSTICK) == DI8DEVTYPE_JOYSTICK)
    {
        memcpy(&g_di_devices[g_device_count].DIDevInst, lpddi, sizeof(DIDEVICEINSTANCE));
        if (!FAILED(hr = g_di->CreateDevice(lpddi->guidInstance,
                                            &g_di_devices[g_device_count].lpDIDevice,
                                            0)))
        {
            if FAILED (hr = g_di_devices[g_device_count].lpDIDevice->SetDataFormat(&c_dfDIJoystick))
                bOK = FALSE;
            if FAILED (hr = g_di_devices[g_device_count].lpDIDevice->SetCooperativeLevel(hMainWindow,
                                                                                         DISCL_BACKGROUND | DISCL_NONEXCLUSIVE))
                bOK = FALSE;
            if FAILED (hr = g_di_devices[g_device_count].lpDIDevice->EnumObjects(EnumAxesCallback,
                                                                                 (LPVOID)hMainWindow,
                                                                                 DIDFT_AXIS))
                bOK = FALSE;
        }
    }
    else
    {
        return DIENUM_CONTINUE;
    }

    if (g_di_devices[g_device_count].lpDIDevice == NULL)
    {
        MessageBox(0, std::format("Fatal device error, please report issue on github. Type {}, name: {} / {}", lpddi->dwDevType, lpddi->tszInstanceName, lpddi->tszProductName).c_str(), "error", MB_ICONERROR);
        return DIENUM_CONTINUE;
    }

    if (bOK == TRUE)
    {
        g_di_devices[g_device_count].lpDIDevice->Acquire();
        g_device_count++;
    }
    else
    {
        g_di_devices[g_device_count].lpDIDevice->Release();
        g_di_devices[g_device_count].lpDIDevice = NULL;
    }

    return DIENUM_CONTINUE;
}

BOOL dih_init(HWND hMainWindow)
{
    HRESULT hr;

    dih_free();

    // Create the DirectInput object.
    g_device_count = 0;
    if FAILED (hr = DirectInput8Create(GetModuleHandle(0), DIRECTINPUT_VERSION, IID_IDirectInput8, (LPVOID*)&g_di, NULL))
    {
        if (hr == DIERR_OLDDIRECTINPUTVERSION)
            MessageBox(NULL, "Old version of DirectX detected. Use DirectX 7 or higher!", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    else
    {
        g_di->EnumDevices(DI8DEVCLASS_KEYBOARD, DIEnumDevicesCallback, (LPVOID)hMainWindow, DIEDFL_ATTACHEDONLY);
        g_di->EnumDevices(DI8DEVCLASS_GAMECTRL, DIEnumDevicesCallback, (LPVOID)hMainWindow, DIEDFL_ATTACHEDONLY);
        if (g_device_count == 0)
            return FALSE;
    }
    return TRUE;
}

void dih_free()
{
    if (g_di)
    {
        for (int i = 0; i < MAX_DEVICES; i++)
        {
            if (g_di_devices[i].lpDIDevice != NULL)
            {
                g_di_devices[i].lpDIDevice->Unacquire();
                g_di_devices[i].lpDIDevice->Release();
                g_di_devices[i].lpDIDevice = NULL;
            }
        }
        g_di->Release();
        g_di = NULL;
    }
}


core_buttons dih_get_input(DEFCONTROLLER* controllers, size_t index, float x_scale, float y_scale)
{
    auto controller = controllers[index];

    if (!controller.bActive)
        return {0};

    core_buttons controller_input = {0};
    BYTE buffer[256]; // Keyboard Info
    DIJOYSTATE js; // Joystick Info
    HRESULT hr;
    int M1Speed = 0, M2Speed = 0;
    bool analogKey = false;

    for (BYTE devicecount = 0; devicecount < controller.NDevices; devicecount++)
    {
        BYTE DeviceNum = (BYTE)controller.Devices[devicecount];

        if (DeviceNum >= sizeof(g_di_devices) / sizeof(g_di_devices[0]))
        {
            continue;
        }

        if (g_di_devices[DeviceNum].lpDIDevice == NULL)
        {
            continue;
        }

        LONG count;

        if ((g_di_devices[DeviceNum].DIDevInst.dwDevType & DI8DEVTYPE_KEYBOARD) == DI8DEVTYPE_KEYBOARD)
        {
            ZeroMemory(&buffer, sizeof(buffer));
            if FAILED (hr = g_di_devices[DeviceNum].lpDIDevice->GetDeviceState(sizeof(buffer), &buffer))
            {
                hr = g_di_devices[DeviceNum].lpDIDevice->Acquire();
                while (hr == DIERR_INPUTLOST)
                    hr = g_di_devices[DeviceNum].lpDIDevice->Acquire();
                return {0};
            }

            for (count = 0; count < NUMBER_OF_BUTTONS; count++)
            {
                if (controller.Input[count].Device == DeviceNum)
                {
                    switch (controller.Input[count].type)
                    {
                    // Record Keyboard Button Info from Device State Buffer
                    case INPUT_TYPE_KEY_BUT:
                        if (BUTTONDOWN(buffer, controller.Input[count].vkey))
                        {
                            switch (count)
                            {
                            case 18:
                                M1Speed = controller.Input[count].button;
                                break;

                            case 19:
                                M2Speed = controller.Input[count].button;
                                break;

                            case 0:
                            case 1:
                            case 2:
                            case 3:
                                analogKey = true;
                            /* fall through */
                            default:
                                controller_input.value |= controller.Input[count].button;
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

        else if ((g_di_devices[DeviceNum].DIDevInst.dwDevType & DI8DEVTYPE_JOYSTICK) == DI8DEVTYPE_JOYSTICK)
        {
            if FAILED (hr = g_di_devices[DeviceNum].lpDIDevice->Poll())
            {
                hr = g_di_devices[DeviceNum].lpDIDevice->Acquire();
                while (hr == DIERR_INPUTLOST)
                    hr = g_di_devices[DeviceNum].lpDIDevice->Acquire();
                return {0};
            }
            if FAILED (hr = g_di_devices[DeviceNum].lpDIDevice->GetDeviceState(sizeof(DIJOYSTATE), &js))
            {
                return {0};
            }

            for (count = 0; count < NUMBER_OF_BUTTONS; count++)
            {
                if (controller.Input[count].Device == DeviceNum)
                {
                    BYTE count2;
                    switch (controller.Input[count].type)
                    {
                    // Get Joystick button Info from Device State js stucture
                    case INPUT_TYPE_JOY_BUT:
                        if (BUTTONDOWN(js.rgbButtons, controller.Input[count].vkey))
                        {
                            switch (count)
                            {
                            case 18:
                                M1Speed = controller.Input[count].button;
                                break;

                            case 19:
                                M2Speed = controller.Input[count].button;
                                break;

                            case 0:
                            case 1:
                            case 2:
                            case 3:
                                analogKey = true;
                            /* fall through */
                            default:
                                controller_input.value |= controller.Input[count].button;
                                break;
                            }
                        }
                        break;

                    case INPUT_TYPE_JOY_AXIS:
                        switch (controller.Input[count].vkey)
                        {
                        case DIJOFS_YN:
                            if (js.lY < (LONG)-controller.SensMin)
                                GetNegAxisVal(js.lY, index, count, &controller_input, M1Speed, M2Speed);
                            break;
                        case DIJOFS_YP:
                            if (js.lY > (LONG)controller.SensMin)
                                GetPosAxisVal(js.lY, index, count, &controller_input, M1Speed, M2Speed);
                            break;
                        case DIJOFS_XN:
                            if (js.lX < (LONG)-controller.SensMin)
                                GetNegAxisVal(js.lX, index, count, &controller_input, M1Speed, M2Speed);
                            break;
                        case DIJOFS_XP:
                            if (js.lX > (LONG)controller.SensMin)
                                GetPosAxisVal(js.lX, index, count, &controller_input, M1Speed, M2Speed);
                            break;
                        case DIJOFS_ZN:
                            if (js.lZ < (LONG)-controller.SensMin)
                                GetNegAxisVal(js.lZ, index, count, &controller_input, M1Speed, M2Speed);
                            break;
                        case DIJOFS_ZP:
                            if (js.lZ > (LONG)controller.SensMin)
                                GetPosAxisVal(js.lZ, index, count, &controller_input, M1Speed, M2Speed);
                            break;
                        case DIJOFS_RYN:
                            if (js.lRy < (LONG)-controller.SensMin)
                                GetNegAxisVal(js.lRy, index, count, &controller_input, M1Speed, M2Speed);
                            break;
                        case DIJOFS_RYP:
                            if (js.lRy > (LONG)controller.SensMin)
                                GetPosAxisVal(js.lRy, index, count, &controller_input, M1Speed, M2Speed);
                            break;
                        case DIJOFS_RXN:
                            if (js.lRx < (LONG)-controller.SensMin)
                                GetNegAxisVal(js.lRx, index, count, &controller_input, M1Speed, M2Speed);
                            break;
                        case DIJOFS_RXP:
                            if (js.lRx > (LONG)controller.SensMin)
                                GetPosAxisVal(js.lRx, index, count, &controller_input, M1Speed, M2Speed);
                            break;
                        case DIJOFS_RZN:
                            if (js.lRz < (LONG)-controller.SensMin)
                                GetNegAxisVal(js.lRz, index, count, &controller_input, M1Speed, M2Speed);
                            break;
                        case DIJOFS_RZP:
                            if (js.lRz > (LONG)controller.SensMin)
                                GetPosAxisVal(js.lRz, index, count, &controller_input, M1Speed, M2Speed);
                            break;
                        case DIJOFS_SLIDER0N:
                            if (js.rglSlider[0] < (LONG)-controller.SensMin)
                                GetNegAxisVal(js.rglSlider[0], index, count, &controller_input, M1Speed, M2Speed);
                            break;
                        case DIJOFS_SLIDER0P:
                            if (js.rglSlider[0] > (LONG)controller.SensMin)
                                GetPosAxisVal(js.rglSlider[0], index, count, &controller_input, M1Speed, M2Speed);
                            break;
                        case DIJOFS_SLIDER1N:
                            if (js.rglSlider[1] < (LONG)-controller.SensMin)
                                GetNegAxisVal(js.rglSlider[1], index, count, &controller_input, M1Speed, M2Speed);
                            break;
                        case DIJOFS_SLIDER1P:
                            if (js.rglSlider[1] > (LONG)controller.SensMin)
                                GetPosAxisVal(js.rglSlider[1], index, count, &controller_input, M1Speed, M2Speed);
                            break;
                        }
                        break;

                    case INPUT_TYPE_JOY_POV:
                        for (count2 = 0; count2 < NUMBER_OF_CONTROLS; count2++)
                        {
                            if ((js.rgdwPOV[count2] != -1) && (LOWORD(js.rgdwPOV[count2]) != 0xFFFF))
                            {
                                switch (controller.Input[count].vkey)
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
                                            M1Speed = controller.Input[count].button;
                                            break;

                                        case 19:
                                            M2Speed = controller.Input[count].button;
                                            break;

                                        case 0:
                                        case 1:
                                        case 2:
                                        case 3:
                                            analogKey = true;
                                        /* fall through */
                                        default:
                                            controller_input.value |= controller.Input[count].button;
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
                                            M1Speed = controller.Input[count].button;
                                            break;

                                        case 19:
                                            M2Speed = controller.Input[count].button;
                                            break;

                                        case 0:
                                        case 1:
                                        case 2:
                                        case 3:
                                            analogKey = true;
                                        /* fall through */
                                        default:
                                            controller_input.value |= controller.Input[count].button;
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
                                            M1Speed = controller.Input[count].button;
                                            break;

                                        case 19:
                                            M2Speed = controller.Input[count].button;
                                            break;

                                        case 0:
                                        case 1:
                                        case 2:
                                        case 3:
                                            analogKey = true;
                                        /* fall through */
                                        default:
                                            controller_input.value |= controller.Input[count].button;
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
                                            M1Speed = controller.Input[count].button;
                                            break;

                                        case 19:
                                            M2Speed = controller.Input[count].button;
                                            break;

                                        case 0:
                                        case 1:
                                        case 2:
                                        case 3:
                                            analogKey = true;
                                        /* fall through */
                                        default:
                                            controller_input.value |= controller.Input[count].button;
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
        if (controller_input.y < 0)
            controller_input.y = (char)-M2Speed;
        else if (controller_input.y > 0)
            controller_input.y = (char)M2Speed;

        if (controller_input.x < 0)
            controller_input.x = (char)-M2Speed;
        else if (controller_input.x > 0)
            controller_input.x = (char)M2Speed;
    }
    if (M1Speed)
    {
        if (controller_input.y < 0)
            controller_input.y = (char)-M1Speed;
        else if (controller_input.y > 0)
            controller_input.y = (char)M1Speed;

        if (controller_input.x < 0)
            controller_input.x = (char)-M1Speed;
        else if (controller_input.x > 0)
            controller_input.x = (char)M1Speed;
    }
    if (analogKey)
    {
        if (controller_input.x && controller_input.y)
        {
            const static float mult = 1.0f / sqrtf(2.0f);
            float mult2;
            if (controller.SensMax > 127)
                mult2 = (float)controller.SensMax * (1.0f / 127.0f);
            else
                mult2 = 1.0f;

            controller_input.x = (int)(controller_input.x * mult * mult2 + (controller_input.x > 0 ? 0.5f : -0.5f));

            controller_input.y = (int)(controller_input.y * mult * mult2 + (controller_input.y > 0 ? 0.5f : -0.5f));

            int newX = (int)((float)controller_input.x * x_scale + (controller_input.x > 0 ? 0.5f : -0.5f));
            int newY = (int)((float)controller_input.y * y_scale + (controller_input.y > 0 ? 0.5f : -0.5f));
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
            if (!newX && controller_input.x)
                newX = (controller_input.x > 0) ? 1 : -1;
            if (!newY && controller_input.y)
                newY = (controller_input.y > 0) ? 1 : -1;
            controller_input.x = newX;
            controller_input.y = newY;
        }
        else
        {
            if (controller_input.x)
            {
                int newX = (int)((float)controller_input.x * x_scale + (controller_input.x > 0 ? 0.5f : -0.5f));
                if (!newX && controller_input.x)
                    newX = (controller_input.x > 0) ? 1 : -1;
                controller_input.x = min(127, max(-128, newX));
            }
            if (controller_input.y)
            {
                int newY = (int)((float)controller_input.y * y_scale + (controller_input.y > 0 ? 0.5f : -0.5f));
                if (!newY && controller_input.y)
                    newY = (controller_input.y > 0) ? 1 : -1;
                controller_input.y = min(127, max(-128, newY));
            }
        }
    }

    return controller_input;
}

void dih_initialize_and_check_devices(HWND hMainWindow)
{
    if (g_di != NULL)
    {
        printf("InitializeAndCheckDevices early return because g_lpDI != NULL\n");
        return;
    }

    HKEY hKey;
    BYTE i;
    DWORD dwSize, dwType;

    // Initialize Direct Input function
    if (FAILED(dih_init(hMainWindow)))
    {
        MessageBox(NULL, "DirectInput Initialization Failed!", "Error", MB_ICONERROR | MB_OK);
        dih_free();
    }
    else
    {
        dwType = REG_BINARY;
        dwSize = sizeof(g_guids);
        // Check Guids for Device Changes
        RegCreateKeyEx(HKEY_CURRENT_USER, SUBKEY, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, 0);
        if (RegQueryValueEx(hKey, TEXT("Guids"), 0, &dwType, (LPBYTE)g_guids, &dwSize) != ERROR_SUCCESS)
        {
            for (i = 0; i < MAX_DEVICES; i++)
            {
                if (g_di_devices[i].lpDIDevice == NULL)
                    ZeroMemory(&g_guids[i], sizeof(GUID));
                else
                    memcpy(&g_guids[i], &g_di_devices[i].DIDevInst.guidInstance, sizeof(GUID));
            }
            dwType = REG_BINARY;
            RegSetValueEx(hKey, TEXT("Guids"), 0, dwType, (LPBYTE)g_guids, dwSize);
        }
        else
        {
            if (dih_check_for_device_change(hKey))
            {
                for (i = 0; i < MAX_DEVICES; i++)
                {
                    if (g_di_devices[i].lpDIDevice == NULL)
                        ZeroMemory(&g_guids[i], sizeof(GUID));
                    else
                        memcpy(&g_guids[i], &g_di_devices[i].DIDevInst.guidInstance, sizeof(GUID));
                }
                dwType = REG_BINARY;
                RegSetValueEx(hKey, TEXT("Guids"), 0, dwType, (LPBYTE)g_guids, dwSize);
            }
        }
        RegCloseKey(hKey);
    }
}
