/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include <JoystickControl.h>
#include <NewConfig.h>

using Mode = JoystickControl::t_context::t_internal::Mode;

static void update_joystick_position(HWND hwnd, JoystickControl::t_context* ctx)
{
    if (ctx->internal.mode == Mode::None)
    {
        return;
    }

    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(hwnd, &pt);

    RECT pic_rect;
    GetWindowRect(hwnd, &pic_rect);

    ctx->x = (pt.x * UINT8_MAX / (signed)(pic_rect.right - pic_rect.left) - INT8_MAX + 1);
    ctx->y = -(pt.y * UINT8_MAX / (signed)(pic_rect.bottom - pic_rect.top) - INT8_MAX + 1);

    if (ctx->internal.mode == Mode::Relative)
    {
        ctx->x -= ctx->internal.cursor_diff.x;
        ctx->y -= ctx->internal.cursor_diff.y;
    }

    if (ctx->x > INT8_MAX || ctx->y > INT8_MAX || ctx->x < INT8_MIN || ctx->y < INT8_MIN)
    {
        int div = max(abs(ctx->x), abs(ctx->y));
        ctx->x = ctx->x * INT8_MAX / div;
        ctx->y = ctx->y * INT8_MAX / div;
    }

    if (abs(ctx->x) <= 8)
        ctx->x = 0;
    if (abs(ctx->y) <= 8)
        ctx->y = 0;

    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE);
    SendMessage(GetParent(hwnd), JoystickControl::WM_JOYSTICK_POSITION_CHANGED, 0, 0);
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    const auto ctx = reinterpret_cast<JoystickControl::t_context*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg)
    {
    case WM_MOUSEWHEEL:
        {
            const auto delta = GET_WHEEL_DELTA_WPARAM(wparam);
            const auto increment = delta < 0 ? -1 : 1;

            if (GetKeyState(VK_CONTROL) & 0x8000)
            {
                ctx->y += increment;
            }
            else if (GetKeyState(VK_SHIFT) & 0x8000)
            {
                const auto angle = std::atan2(ctx->y, ctx->x);
                const auto mag = std::ceil(std::sqrt(std::pow(ctx->x, 2) + std::pow(ctx->y, 2)));
                const auto new_ang = angle + (increment * (M_PI / 180.0f));
                ctx->x = (int)std::round(mag * std::cos(new_ang));
                ctx->y = (int)std::round(mag * std::sin(new_ang));
            }
            else
            {
                ctx->x += increment;
            }

            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE);
            SendMessage(GetParent(hwnd), JoystickControl::WM_JOYSTICK_POSITION_CHANGED, 0, 0);
        }
        break;
    case WM_MBUTTONDOWN:
        {
            POINT cursor;
            GetCursorPos(&cursor);
            ScreenToClient(hwnd, &cursor);

            RECT pic_rect;
            GetWindowRect(hwnd, &pic_rect);

            int x = (cursor.x * 256 / (signed)(pic_rect.right - pic_rect.left) - 128 + 1);
            int y = -(cursor.y * 256 / (signed)(pic_rect.bottom - pic_rect.top) - 128 + 1);

            ctx->internal.cursor_diff = POINT{
            x - ctx->x,
            y - ctx->y,
            };

            ctx->internal.mode = Mode::Relative;
            SendMessage(GetParent(hwnd), JoystickControl::WM_JOYSTICK_DRAG_BEGIN, 0, 0);
            SetCapture(hwnd);
            break;
        }
    case WM_RBUTTONDOWN:
        ctx->internal.mode = ctx->internal.mode == Mode::None ? Mode::Sticky : Mode::None;
        SendMessage(GetParent(hwnd), JoystickControl::WM_JOYSTICK_DRAG_BEGIN, 0, 0);
        SetCapture(hwnd);
        break;
    case WM_LBUTTONDOWN:
        ctx->internal.mode = Mode::Absolute;
        SendMessage(GetParent(hwnd), JoystickControl::WM_JOYSTICK_DRAG_BEGIN, 0, 0);
        SetCapture(hwnd);
        break;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
        if (ctx->internal.mode == Mode::Sticky)
        {
            break;
        }
        ctx->internal.mode = Mode::None;
        ReleaseCapture();
        break;
    case WM_MOUSEMOVE:
        update_joystick_position(hwnd, ctx);
        break;
    case WM_PAINT:
        {
            RECT joystick_rect{};
            GetClientRect(hwnd, &joystick_rect);

            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HDC compat_dc = CreateCompatibleDC(hdc);
            int scale = new_config.hifi_joystick ? 4 : 1;
            POINT bmp_size = {joystick_rect.right * scale, joystick_rect.bottom * scale};
            HBITMAP bmp = CreateCompatibleBitmap(hdc, bmp_size.x, bmp_size.y);
            SelectObject(compat_dc, bmp);

            HPEN outline_pen = CreatePen(PS_SOLID, 1 * scale, RGB(0, 0, 0));
            HPEN line_pen = CreatePen(PS_SOLID, 3 * scale, RGB(0, 0, 255));
            HPEN tip_pen = CreatePen(PS_SOLID, 7 * scale, RGB(255, 0, 0));

            int mid_x = bmp_size.x / 2;
            int mid_y = bmp_size.y / 2;
            int stick_x = (ctx->x + 128) * bmp_size.x / 256;
            int stick_y = (-ctx->y + 128) * bmp_size.y / 256;

            RECT normalized = {0, 0, bmp_size.x, bmp_size.y};
            FillRect(compat_dc, &normalized, GetSysColorBrush(COLOR_BTNFACE));

            SelectObject(compat_dc, outline_pen);
            Ellipse(compat_dc, 0, 0, bmp_size.x, bmp_size.y);
            MoveToEx(compat_dc, 0, mid_y, NULL);
            LineTo(compat_dc, bmp_size.x, mid_y);
            MoveToEx(compat_dc, mid_x, 0, NULL);
            LineTo(compat_dc, mid_x, bmp_size.y);

            SelectObject(compat_dc, line_pen);
            MoveToEx(compat_dc, mid_x, mid_y, nullptr);
            LineTo(compat_dc, stick_x, stick_y);

            SelectObject(compat_dc, tip_pen);
            MoveToEx(compat_dc, stick_x, stick_y, NULL);
            LineTo(compat_dc, stick_x, stick_y);

            SelectObject(compat_dc, nullptr);

            SetStretchBltMode(hdc, HALFTONE);
            SetStretchBltMode(compat_dc, HALFTONE);
            StretchBlt(hdc, joystick_rect.left, joystick_rect.top, joystick_rect.right, joystick_rect.bottom, compat_dc, 0, 0, bmp_size.x, bmp_size.y, SRCCOPY);
            EndPaint(hwnd, &ps);
            DeleteDC(compat_dc);
            DeleteObject(bmp);
            DeleteObject(outline_pen);
            DeleteObject(line_pen);
            DeleteObject(tip_pen);
        }

        break;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

void JoystickControl::register_class(HINSTANCE hinst)
{
    WNDCLASS wndclass = {0};
    wndclass.style = CS_GLOBALCLASS | CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndclass.lpfnWndProc = (WNDPROC)wndproc;
    wndclass.hInstance = hinst;
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.lpszClassName = CLASS_NAME;
    RegisterClass(&wndclass);
}
