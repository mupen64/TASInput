/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include "DirectInputHelper.h"

GUID Guids[MAX_DEVICES];
DEFCONTROLLER Controller[NUMBER_OF_CONTROLS];
CONTROL* ControlDef[NUMBER_OF_CONTROLS];

LPDIRECTINPUT8 g_lpDI = NULL;
DIINPUTDEVICE DInputDev[MAX_DEVICES];
BYTE nCurrentDevices;

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


BOOL WINAPI InitDirectInput(HWND hMainWindow)
{
    HRESULT hr;

    FreeDirectInput();

    // Create the DirectInput object.
    nCurrentDevices = 0;
    if FAILED (hr = DirectInput8Create(GetModuleHandle(0), DIRECTINPUT_VERSION, IID_IDirectInput8, (LPVOID*)&g_lpDI, NULL))
    {
        if (hr == DIERR_OLDDIRECTINPUTVERSION)
            MessageBox(NULL, "Old version of DirectX detected. Use DirectX 7 or higher!", "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    else
    {
        g_lpDI->EnumDevices(DI8DEVCLASS_KEYBOARD, DIEnumDevicesCallback,
                            (LPVOID)hMainWindow, DIEDFL_ATTACHEDONLY);
        g_lpDI->EnumDevices(DI8DEVCLASS_GAMECTRL, DIEnumDevicesCallback,
                            (LPVOID)hMainWindow, DIEDFL_ATTACHEDONLY);
        if (nCurrentDevices == 0)
            return FALSE;
    }
    return TRUE;
}

BOOL CALLBACK DIEnumDevicesCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef)
{
    HRESULT hr;
    auto hMainWindow = (HWND)pvRef;
    BOOL bOK = TRUE;

    if (nCurrentDevices >= MAX_DEVICES)
        return DIENUM_STOP;

    if ((lpddi->dwDevType & DI8DEVTYPE_KEYBOARD) == DI8DEVTYPE_KEYBOARD)
    {
        memcpy(&DInputDev[nCurrentDevices].DIDevInst, lpddi, sizeof(DIDEVICEINSTANCE));
        if (!FAILED(hr = g_lpDI->CreateDevice(lpddi->guidInstance,
                                              &DInputDev[nCurrentDevices].lpDIDevice, 0)))
        {
            if FAILED (hr = DInputDev[nCurrentDevices].lpDIDevice->SetDataFormat(&c_dfDIKeyboard))
                bOK = FALSE;
            if FAILED (hr = DInputDev[nCurrentDevices].lpDIDevice->SetCooperativeLevel(hMainWindow,
                                                                                       DISCL_BACKGROUND | DISCL_NONEXCLUSIVE))
                bOK = FALSE;
        }
    }
    else if ((lpddi->dwDevType & DI8DEVTYPE_JOYSTICK) == DI8DEVTYPE_JOYSTICK)
    {
        memcpy(&DInputDev[nCurrentDevices].DIDevInst, lpddi, sizeof(DIDEVICEINSTANCE));
        if (!FAILED(hr = g_lpDI->CreateDevice(lpddi->guidInstance,
                                              &DInputDev[nCurrentDevices].lpDIDevice, 0)))
        {
            if FAILED (hr = DInputDev[nCurrentDevices].lpDIDevice->SetDataFormat(&c_dfDIJoystick))
                bOK = FALSE;
            if FAILED (hr = DInputDev[nCurrentDevices].lpDIDevice->SetCooperativeLevel(hMainWindow,
                                                                                       DISCL_BACKGROUND | DISCL_NONEXCLUSIVE))
                bOK = FALSE;
            if FAILED (hr = DInputDev[nCurrentDevices].lpDIDevice->EnumObjects(EnumAxesCallback,
                                                                               (LPVOID)hMainWindow, DIDFT_AXIS))
                bOK = FALSE;
        }
    }
    else
    {
        return DIENUM_CONTINUE;
    }

    if (DInputDev[nCurrentDevices].lpDIDevice == NULL)
    {
        MessageBox(0, std::format("Fatal device error, please report issue on github. Type {}, name: {} / {}", lpddi->dwDevType, lpddi->tszInstanceName, lpddi->tszProductName).c_str(), "error", MB_ICONERROR);
        return DIENUM_CONTINUE;
    }

    if (bOK == TRUE)
    {
        DInputDev[nCurrentDevices].lpDIDevice->Acquire();
        nCurrentDevices++;
    }
    else
    {
        DInputDev[nCurrentDevices].lpDIDevice->Release();
        DInputDev[nCurrentDevices].lpDIDevice = NULL;
    }

    return DIENUM_CONTINUE;
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
    if FAILED (DInputDev[nCurrentDevices].lpDIDevice->SetProperty(DIPROP_RANGE, &diprg.diph))
        return DIENUM_STOP;

    return TRUE;
}

void WINAPI FreeDirectInput()
{
    if (g_lpDI)
    {
        for (int i = 0; i < MAX_DEVICES; i++)
        {
            if (DInputDev[i].lpDIDevice != NULL)
            {
                DInputDev[i].lpDIDevice->Unacquire();
                DInputDev[i].lpDIDevice->Release();
                DInputDev[i].lpDIDevice = NULL;
            }
        }
        g_lpDI->Release();
        g_lpDI = NULL;
    }
}


BUTTONS get_controller_input(DEFCONTROLLER* controllers, size_t index, float x_scale, float y_scale)
{
    auto controller = controllers[index];

    if (!controller.bActive)
        return {0};

    BUTTONS controller_input = {0};
    BYTE buffer[256]; // Keyboard Info
    DIJOYSTATE js; // Joystick Info
    HRESULT hr;
    int M1Speed = 0, M2Speed = 0;
    bool analogKey = false;

    for (BYTE devicecount = 0; devicecount < controller.NDevices; devicecount++)
    {
        BYTE DeviceNum = (BYTE)controller.Devices[devicecount];

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
            if FAILED (hr = DInputDev[DeviceNum].lpDIDevice->GetDeviceState(sizeof(buffer), &buffer))
            {
                hr = DInputDev[DeviceNum].lpDIDevice->Acquire();
                while (hr == DIERR_INPUTLOST)
                    hr = DInputDev[DeviceNum].lpDIDevice->Acquire();
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
                                controller_input.Value |= controller.Input[count].button;
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
            if FAILED (hr = DInputDev[DeviceNum].lpDIDevice->Poll())
            {
                hr = DInputDev[DeviceNum].lpDIDevice->Acquire();
                while (hr == DIERR_INPUTLOST)
                    hr = DInputDev[DeviceNum].lpDIDevice->Acquire();
                return {0};
            }
            if FAILED (hr = DInputDev[DeviceNum].lpDIDevice->GetDeviceState(sizeof(DIJOYSTATE), &js))
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
                                controller_input.Value |= controller.Input[count].button;
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
                                GetNegAxisVal(js.rglSlider[0], index, count, &controller_input, M1Speed,
                                              M2Speed);
                            break;
                        case DIJOFS_SLIDER0P:
                            if (js.rglSlider[0] > (LONG)controller.SensMin)
                                GetPosAxisVal(js.rglSlider[0], index, count, &controller_input, M1Speed,
                                              M2Speed);
                            break;
                        case DIJOFS_SLIDER1N:
                            if (js.rglSlider[1] < (LONG)-controller.SensMin)
                                GetNegAxisVal(js.rglSlider[1], index, count, &controller_input, M1Speed,
                                              M2Speed);
                            break;
                        case DIJOFS_SLIDER1P:
                            if (js.rglSlider[1] > (LONG)controller.SensMin)
                                GetPosAxisVal(js.rglSlider[1], index, count, &controller_input, M1Speed,
                                              M2Speed);
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
                                            controller_input.Value |= controller.Input[count].button;
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
                                            controller_input.Value |= controller.Input[count].button;
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
                                            controller_input.Value |= controller.Input[count].button;
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
                                            controller_input.Value |= controller.Input[count].button;
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
            if (controller.SensMax > 127)
                mult2 = (float)controller.SensMax * (1.0f / 127.0f);
            else
                mult2 = 1.0f;

            controller_input.X_AXIS = (int)(controller_input.X_AXIS * mult * mult2 + (controller_input.X_AXIS > 0 ? 0.5f : -0.5f));

            controller_input.Y_AXIS = (int)(controller_input.Y_AXIS * mult * mult2 + (controller_input.Y_AXIS > 0 ? 0.5f : -0.5f));

            int newX = (int)((float)controller_input.X_AXIS * x_scale + (controller_input.X_AXIS > 0 ? 0.5f : -0.5f));
            int newY = (int)((float)controller_input.Y_AXIS * y_scale + (controller_input.Y_AXIS > 0 ? 0.5f : -0.5f));
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
            if (!newX && controller_input.X_AXIS)
                newX = (controller_input.X_AXIS > 0) ? 1 : -1;
            if (!newY && controller_input.Y_AXIS)
                newY = (controller_input.Y_AXIS > 0) ? 1 : -1;
            controller_input.X_AXIS = newX;
            controller_input.Y_AXIS = newY;
        }
        else
        {
            if (controller_input.X_AXIS)
            {
                int newX = (int)((float)controller_input.X_AXIS * x_scale + (controller_input.X_AXIS > 0 ? 0.5f : -0.5f));
                if (!newX && controller_input.X_AXIS)
                    newX = (controller_input.X_AXIS > 0) ? 1 : -1;
                controller_input.X_AXIS = min(127, max(-128, newX));
            }
            if (controller_input.Y_AXIS)
            {
                int newY = (int)((float)controller_input.Y_AXIS * y_scale + (controller_input.Y_AXIS > 0 ? 0.5f : -0.5f));
                if (!newY && controller_input.Y_AXIS)
                    newY = (controller_input.Y_AXIS > 0) ? 1 : -1;
                controller_input.Y_AXIS = min(127, max(-128, newY));
            }
        }
    }

    return controller_input;
}
