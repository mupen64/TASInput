/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define DIRECTINPUT_VERSION 0x0800
#define _USE_MATH_DEFINES

#include <filesystem>
#include <string>
#include <format>
#include <algorithm>
#include <memory>
#include <functional>
#include <vector>
#include <memory>
#include <tchar.h>
#include <span>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <cstdio>
#include <map>
#include <cassert>
#include <cmath>
#include <cfloat>
#include <stack>
#include <numeric>
#include <Windows.h>
#include <dinput.h>
#include <shlobj.h>
#include <commctrl.h>
#include <windowsx.h>
#include <shellscalingapi.h>
#include "core_plugin.h"
#include "resource.h"