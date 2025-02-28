/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#define MAX_DEVICES 5
#define NUMBER_OF_CONTROLS 4
#define NUMBER_OF_BUTTONS 25

typedef struct
{
    BYTE Device;
    BYTE type;
    BYTE vkey;
    DWORD button;
} INPUT_CTRL;

typedef struct
{
    TCHAR szName[16];
    BYTE NDevices;
    DWORD Devices[MAX_DEVICES];
    BOOL bActive;
    BOOL bMemPak;
    BYTE SensMax;
    BYTE SensMin;
    INPUT_CTRL Input[NUMBER_OF_BUTTONS];
} DEFCONTROLLER;

using DIINPUTDEVICE = struct
{
    LPDIRECTINPUTDEVICE8 lpDIDevice;
    DIDEVICEINSTANCE DIDevInst;
};

// Used for Input structure type.
#define INPUT_TYPE_NOT_USED 0
#define INPUT_TYPE_KEY_BUT 1
#define INPUT_TYPE_JOY_BUT 2
#define INPUT_TYPE_JOY_AXIS 3
#define INPUT_TYPE_JOY_POV 4

#define DIJOFS_YN 0
#define DIJOFS_YP 1
#define DIJOFS_XN 2
#define DIJOFS_XP 3
#define DIJOFS_ZN 4
#define DIJOFS_ZP 5
#define DIJOFS_RYN 6
#define DIJOFS_RYP 7
#define DIJOFS_RXN 8
#define DIJOFS_RXP 9
#define DIJOFS_RZN 10
#define DIJOFS_RZP 11
#define DIJOFS_SLIDER0N 12
#define DIJOFS_SLIDER0P 13
#define DIJOFS_SLIDER1N 14
#define DIJOFS_SLIDER1P 15
#define DIJOFS_POV0N 16
#define DIJOFS_POV0E 17
#define DIJOFS_POV0S 18
#define DIJOFS_POV0W 19
#define DIJOFS_POV1N 20
#define DIJOFS_POV1E 21
#define DIJOFS_POV1S 22
#define DIJOFS_POV1W 23
#define DIJOFS_POV2N 24
#define DIJOFS_POV2E 25
#define DIJOFS_POV2S 26
#define DIJOFS_POV2W 27
#define DIJOFS_POV3N 28
#define DIJOFS_POV3E 29
#define DIJOFS_POV3S 30
#define DIJOFS_POV3W 31

#define BUTTONDOWN(name, key) (name[key] & 0x80)

extern BYTE g_device_count;
extern LPDIRECTINPUT8 g_di;
extern GUID g_guids[MAX_DEVICES];
extern DEFCONTROLLER g_controllers[NUMBER_OF_CONTROLS];
extern core_controller* g_controllers_default[NUMBER_OF_CONTROLS];
extern DIINPUTDEVICE g_di_devices[MAX_DEVICES];

void dih_free();

BOOL dih_init(HWND hMainWindow);

/**
 * \brief Gets the current input for a specific controller index
 * \param controllers An array of controllers
 * \param index The index into the array of the controls
 * \param x_scale The multiplier for the X joystick axis
 * \param y_scale The multiplier for the Y joystick axis
 * \return The held controller input
 */
core_buttons dih_get_input(DEFCONTROLLER* controllers, size_t index, float x_scale, float y_scale);

BOOL dih_check_for_device_change(HKEY hKey);
void dih_initialize_and_check_devices(HWND hMainWindow);
