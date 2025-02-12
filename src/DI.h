/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _DI_H_INCLUDED__
#define _DI_H_INCLUDED__

#define MAX_DEVICES 5
#include "DefDI.h"

extern BYTE nCurrentDevices;

extern LPDIRECTINPUT8 g_lpDI;

extern GUID Guids[MAX_DEVICES];

using DIINPUTDEVICE = struct
{
    LPDIRECTINPUTDEVICE8 lpDIDevice;
    DIDEVICEINSTANCE DIDevInst;
};

extern DIINPUTDEVICE DInputDev[MAX_DEVICES];

void WINAPI FreeDirectInput();
BOOL WINAPI InitDirectInput(HWND hMainWindow);
BOOL CALLBACK DIEnumDevicesCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef);
BOOL CALLBACK EnumAxesCallback(const DIDEVICEOBJECTINSTANCE* pdidoi, LPVOID pContext);


/**
 * \brief Gets the current input for a specific controller index
 * \param controllers An array of controllers
 * \param index The index into the array of the controls
 * \param x_scale The multiplier for the X joystick axis
 * \param y_scale The multiplier for the Y joystick axis
 * \return The held controller input
 */
BUTTONS get_controller_input(DEFCONTROLLER* controllers, size_t index, float x_scale, float y_scale);

#endif
