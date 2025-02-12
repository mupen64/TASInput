/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#define SUBKEY "Software\\N64 Emulation\\DLL\\TASDI"

void WINAPI InitializeAndCheckDevices(HWND hMainWindow);
LRESULT CALLBACK StatusDlgProc(HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam);
VOID CALLBACK StatusDlgProcTimer(UINT idEvent, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2);
