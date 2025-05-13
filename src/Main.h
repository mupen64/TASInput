/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

extern HINSTANCE g_inst;
extern core_plugin_extended_funcs* g_ef;
extern int MOUSE_LBUTTONREDEFINITION;
extern int MOUSE_RBUTTONREDEFINITION;

#define PLUGIN_VERSION L"2.0.0-rc1"

#ifdef _M_X64
#define PLUGIN_ARCH L"-x64"
#else
#define PLUGIN_ARCH L"-x86"
#endif

#ifdef _DEBUG
#define PLUGIN_TARGET L"-debug"
#else
#define PLUGIN_TARGET L"-release"
#endif

#define PLUGIN_NAME L"TASInput " PLUGIN_VERSION PLUGIN_ARCH PLUGIN_TARGET
