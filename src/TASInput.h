/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _DEFDI_H_INCLUDED__
#define _DEFDI_H_INCLUDED__

#define SUBKEY "Software\\N64 Emulation\\DLL\\TASDI"

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

//----

void WINAPI InitializeAndCheckDevices(HWND hMainWindow);
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
