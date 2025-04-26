/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

static void set_checkbox_state(const HWND hwnd, const int32_t id, int32_t is_checked)
{
    SendMessage(GetDlgItem(hwnd, id), BM_SETCHECK, is_checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

static int32_t get_checkbox_state(const HWND hwnd, const int32_t id)
{
    return SendDlgItemMessage(hwnd, id, BM_GETCHECK, 0, 0) == BST_CHECKED
    ? TRUE
    : FALSE;
}

static int32_t get_primary_monitor_refresh_rate()
{
    DISPLAY_DEVICE dd;
    dd.cb = sizeof(dd);
    EnumDisplayDevices(NULL, 0, &dd, 0);

    DEVMODE dm;
    dm.dmSize = sizeof(dm);
    EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm);

    return dm.dmDisplayFrequency;
}

static void read_combo_box_value(const HWND hwnd, const int resource_id, char* ret)
{
    int index = SendDlgItemMessage(hwnd, resource_id, CB_GETCURSEL, 0, 0);
    SendDlgItemMessage(hwnd, resource_id, CB_GETLBTEXT, index, (LPARAM)ret);
}

/**
 * \brief Accurately sleeps for the specified amount of time
 * \param seconds The seconds to sleep for
 * \remarks https://blat-blatnik.github.io/computerBear/making-accurate-sleep-function/
 */
static void accurate_sleep(double seconds)
{
    using namespace std;
    using namespace std::chrono;

    static double estimate = 5e-3;
    static double mean = 5e-3;
    static double m2 = 0;
    static int64_t count = 1;

    while (seconds > estimate)
    {
        auto start = high_resolution_clock::now();
        this_thread::sleep_for(milliseconds(1));
        auto end = high_resolution_clock::now();

        double observed = (end - start).count() / 1e9;
        seconds -= observed;

        ++count;
        double delta = observed - mean;
        mean += delta / count;
        m2 += delta * (observed - mean);
        double stddev = sqrt(m2 / (count - 1));
        estimate = mean + stddev;
    }

    // spin lock
    auto start = high_resolution_clock::now();
    while ((high_resolution_clock::now() - start).count() / 1e9 < seconds)
        ;
}

static void set_style(HWND hwnd, int domain, int style, bool value)
{
    auto base = GetWindowLongA(hwnd, domain);

    if (value)
    {
        SetWindowLongA(hwnd, domain, base | style);
    }
    else
    {
        SetWindowLongA(hwnd, domain, base & ~style);
    }
}

static RECT get_window_rect_client_space(HWND parent, HWND child)
{
    RECT offset_client = {0};
    MapWindowRect(child, parent, &offset_client);

    RECT client = {0};
    GetWindowRect(child, &client);

    return {
    offset_client.left,
    offset_client.top,
    offset_client.left + (client.right - client.left),
    offset_client.top + (client.bottom - client.top)};
}
