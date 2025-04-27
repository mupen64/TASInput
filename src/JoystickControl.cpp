/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include "Main.h"
#include <JoystickControl.h>

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

static void destroy_dcs(const HWND hwnd, JoystickControl::t_context* ctx)
{
    if (!ctx->internal.front_dc)
    {
        return;
    }

    ReleaseDC(hwnd, ctx->internal.front_dc);
    ctx->internal.front_dc = nullptr;

    SelectObject(ctx->internal.back_dc, nullptr);
    DeleteObject(ctx->internal.back_bmp);
    DeleteDC(ctx->internal.back_dc);

    DeleteObject(ctx->internal.outline_pen);
    DeleteObject(ctx->internal.line_pen);
    DeleteObject(ctx->internal.tip_pen);
}

static void create_dcs(const HWND hwnd, JoystickControl::t_context* ctx)
{
    if (ctx->internal.front_dc)
    {
        return;
    }

    RECT rc{};
    GetClientRect(hwnd, &rc);
    g_ef->log_info(std::format(L"create_dcs with size {}x{}", rc.right, rc.bottom).c_str());

    ctx->internal.front_dc = GetDC(hwnd);
    ctx->internal.back_dc = CreateCompatibleDC(ctx->internal.front_dc);
    ctx->internal.back_bmp = CreateCompatibleBitmap(ctx->internal.front_dc, rc.right, rc.bottom);
    SelectObject(ctx->internal.back_dc, ctx->internal.back_bmp);
    
    SetStretchBltMode(ctx->internal.front_dc, HALFTONE);
    SetStretchBltMode(ctx->internal.back_dc, HALFTONE);

    ctx->internal.outline_pen = CreatePen(PS_SOLID, 1 * ctx->scale, RGB(0, 0, 0));
    ctx->internal.line_pen = CreatePen(PS_SOLID, 3 * ctx->scale, RGB(0, 0, 255));
    ctx->internal.tip_pen = CreatePen(PS_SOLID, 7 * ctx->scale, RGB(255, 0, 0));

}

static JoystickControl::t_context* get_ctx(HWND hwnd)
{
    return reinterpret_cast<JoystickControl::t_context*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    const auto ctx = get_ctx(hwnd);

    switch (msg)
    {
    case WM_NCCREATE:
        {
            const auto csp = reinterpret_cast<CREATESTRUCT*>(lparam);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)csp->lpCreateParams);
            create_dcs(hwnd, get_ctx(hwnd));
            break;
        }
    case WM_NCDESTROY:
        destroy_dcs(hwnd, ctx);
        break;
    case WM_SIZE:
        destroy_dcs(hwnd, ctx);
        create_dcs(hwnd, ctx);
        break;
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
            RECT rc{};
            GetClientRect(hwnd, &rc);
        
            int mid_x = rc.right / 2;
            int mid_y = rc.bottom / 2;
            int stick_x = (ctx->x + 128) * rc.right / 256;
            int stick_y = (-ctx->y + 128) * rc.bottom / 256;

            FillRect(ctx->internal.back_dc, &rc, GetSysColorBrush(COLOR_BTNFACE));

            SelectObject(ctx->internal.back_dc, ctx->internal.outline_pen);
            Ellipse(ctx->internal.back_dc, 0, 0, rc.right, rc.bottom);
            MoveToEx(ctx->internal.back_dc, 0, mid_y, NULL);
            LineTo(ctx->internal.back_dc, rc.right, mid_y);
            MoveToEx(ctx->internal.back_dc, mid_x, 0, NULL);
            LineTo(ctx->internal.back_dc, mid_x, rc.bottom);

            SelectObject(ctx->internal.back_dc, ctx->internal.line_pen);
            MoveToEx(ctx->internal.back_dc, mid_x, mid_y, nullptr);
            LineTo(ctx->internal.back_dc, stick_x, stick_y);

            SelectObject(ctx->internal.back_dc, ctx->internal.tip_pen);
            MoveToEx(ctx->internal.back_dc, stick_x, stick_y, NULL);
            LineTo(ctx->internal.back_dc, stick_x, stick_y);

            StretchBlt(ctx->internal.front_dc, rc.left, rc.top, rc.right, rc.bottom, ctx->internal.back_dc, 0, 0, rc.right, rc.bottom, SRCCOPY);
        
            ValidateRect(hwnd, nullptr);
            return 0;
        }
    default:
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
