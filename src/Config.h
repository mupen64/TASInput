/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _CONFIG_H_INCLUDED__
#define _CONFIG_H_INCLUDED__

#define SUBKEY "Software\\N64 Emulation\\DLL\\TASDI"

#define IDT_TIMER1 1
#define IDT_TIMER2 2

LRESULT CALLBACK ConfigDlgProc(HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam);
void WINAPI Start_Timer(HWND hDlg);
void WINAPI Initialize_Controller_Display(HWND hDlg, BYTE NController);
void WINAPI Dis_En_AbleApply(HWND hParent, BOOL bActive);
void WINAPI Dis_En_AbleControls(HWND hParent, BOOL bActive);
BOOL WINAPI GetAControl(HWND hDlg, DWORD ControlValue, BYTE NController, BYTE NControl);
BOOL WINAPI GetAControlValue(HWND hDlg, DWORD ControlValue, BYTE NController, BYTE NControl);
void WINAPI GetAControlName(BYTE NController, BYTE NControl, TCHAR ControlName[32]);
void WINAPI GetKeyName(BYTE Value, TCHAR KeyControlName[16]);

LRESULT CALLBACK MacroDlgProc(HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam);
void WINAPI InitializeMacroChecks(HWND hDlg, BYTE NController, BYTE NControl);
void WINAPI SetMacroButtonValue(HWND hDlg, BYTE NController, BYTE NControl);

#endif
