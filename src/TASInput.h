/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _DEFDI_H_INCLUDED__
#define _DEFDI_H_INCLUDED__

#define NUMBER_OF_CONTROLS 4
#define NUMBER_OF_BUTTONS 25

#define SUBKEY "Software\\N64 Emulation\\DLL\\TASDI"

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

#define IDT_TIMER_STATUS_0 5000
#define IDT_TIMER_STATUS_1 5001
#define IDT_TIMER_STATUS_2 5002
#define IDT_TIMER_STATUS_3 5003

// combo tasks
#define C_IDLE 0
#define C_PLAY 1
#define C_RECORD 4

// custom messages
#define EDIT_END 10001

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

extern DEFCONTROLLER Controller[NUMBER_OF_CONTROLS];

extern HINSTANCE g_hInstance;


//----

void WINAPI GetNegAxisVal(LONG AxisValue, int Control, LONG count, BUTTONS* ControllerInput, int& M1Speed,
                          int& M2Speed);
void WINAPI GetPosAxisVal(LONG AxisValue, int Control, LONG count, BUTTONS* ControllerInput, int& M1Speed,
                          int& M2Speed);
void WINAPI InitializeAndCheckDevices(HWND hMainWindow);
BOOL WINAPI CheckForDeviceChange(HKEY hKey);
LRESULT CALLBACK StatusDlgProc(HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam);
VOID CALLBACK StatusDlgProcTimer(UINT idEvent, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2);

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

#endif
