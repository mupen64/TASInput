/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include <Main.h>
#include <DirectInputHelper.h>

#define EXPORT __declspec(dllexport)
#define CALL _cdecl

HINSTANCE g_inst;
core_plugin_extended_funcs* g_ef{};
int MOUSE_LBUTTONREDEFINITION = VK_LBUTTON;
int MOUSE_RBUTTONREDEFINITION = VK_RBUTTON;

// ReSharper disable once CppInconsistentNaming
int WINAPI DllMain(const HINSTANCE h_instance, const DWORD fdw_reason, PVOID)
{
    switch (fdw_reason)
    {
    case DLL_PROCESS_ATTACH:
        g_inst = h_instance;
        break;

    case DLL_PROCESS_DETACH:
        dih_free();
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

EXPORT void CALL ReceiveExtendedFuncs(core_plugin_extended_funcs* funcs)
{
    g_ef = funcs;
}
