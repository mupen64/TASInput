/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include <JoystickControl.h>

#define WITH_VALID_CTX()            \
    if (!IsWindow(hwnd))            \
    {                               \
        return FALSE;               \
    }                               \
    const auto ctx = get_ctx(hwnd); \
    if (!ctx)                       \
    {                               \
        return FALSE;               \
    }

struct t_context {
    enum class Mode {
        None,
        Absolute,
        Sticky,
        Relative
    };

    int x{};
    int y{};
    Mode mode = Mode::None;
    POINT cursor_diff{};
    HDC front_dc{};
    HDC back_dc{};
    HBITMAP back_bmp{};
    Gdiplus::Graphics* g{};
    Gdiplus::Brush* bg_brush{};
    Gdiplus::Brush* tip_brush{};
    Gdiplus::Pen* outline_pen{};
    Gdiplus::Pen* line_pen{};
};

using Mode = t_context::Mode;

static void update_joystick_position(HWND hwnd, t_context* ctx)
{
    if (ctx->mode == Mode::None)
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

    if (ctx->mode == Mode::Relative)
    {
        ctx->x -= ctx->cursor_diff.x;
        ctx->y -= ctx->cursor_diff.y;
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

static void destroy_dcs(const HWND hwnd, t_context* ctx)
{
    if (!ctx->front_dc)
    {
        return;
    }

    SelectObject(ctx->back_dc, nullptr);
    DeleteObject(ctx->back_bmp);
    DeleteDC(ctx->back_dc);

    ReleaseDC(hwnd, ctx->front_dc);
    ctx->front_dc = nullptr;

    delete ctx->g;
    delete ctx->bg_brush;
    delete ctx->tip_brush;
    delete ctx->outline_pen;
    delete ctx->line_pen;
}

static void create_dcs(const HWND hwnd, t_context* ctx)
{
    if (ctx->front_dc)
    {
        return;
    }

    RECT rc{};
    GetClientRect(hwnd, &rc);

    ctx->front_dc = GetDC(hwnd);
    ctx->back_dc = CreateCompatibleDC(ctx->front_dc);
    ctx->back_bmp = CreateCompatibleBitmap(ctx->front_dc, rc.right, rc.bottom);
    SelectObject(ctx->back_dc, ctx->back_bmp);

    ctx->g = new Gdiplus::Graphics(ctx->back_dc);
    ctx->g->SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

    ctx->bg_brush = new Gdiplus::SolidBrush(Gdiplus::Color::White);
    ctx->tip_brush = new Gdiplus::SolidBrush(Gdiplus::Color(255, 255, 0, 0));
    ctx->outline_pen = new Gdiplus::Pen(Gdiplus::Color(255, 0, 0, 0), 1);
    ctx->line_pen = new Gdiplus::Pen(Gdiplus::Color(255, 0, 0, 255), 3);
}

static auto get_ctx(const HWND hwnd)
{
    return reinterpret_cast<t_context*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    const auto ctx = get_ctx(hwnd);

    switch (msg)
    {
    case WM_NCCREATE:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG) new t_context());
        create_dcs(hwnd, get_ctx(hwnd));
        break;
    case WM_NCDESTROY:
        destroy_dcs(hwnd, ctx);
        delete ctx;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
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

            ctx->cursor_diff = POINT{
            x - ctx->x,
            y - ctx->y,
            };

            ctx->mode = Mode::Relative;
            SendMessage(GetParent(hwnd), JoystickControl::WM_JOYSTICK_DRAG_BEGIN, 0, 0);
            SetCapture(hwnd);
            break;
        }
    case WM_RBUTTONDOWN:
        ctx->mode = ctx->mode == Mode::None ? Mode::Sticky : Mode::None;
        SendMessage(GetParent(hwnd), JoystickControl::WM_JOYSTICK_DRAG_BEGIN, 0, 0);
        SetCapture(hwnd);
        break;
    case WM_LBUTTONDOWN:
        ctx->mode = Mode::Absolute;
        SendMessage(GetParent(hwnd), JoystickControl::WM_JOYSTICK_DRAG_BEGIN, 0, 0);
        SetCapture(hwnd);
        break;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
        if (ctx->mode == Mode::Sticky)
        {
            break;
        }
        ctx->mode = Mode::None;
        ReleaseCapture();
        break;
    case WM_MOUSEMOVE:
        update_joystick_position(hwnd, ctx);
        break;
    case WM_PAINT:
        {
            RECT rc{};
            GetClientRect(hwnd, &rc);

            rc.right -= 1;
            rc.bottom -= 1;
            int mid_x = rc.right / 2;
            int mid_y = rc.bottom / 2;
            int stick_x = (ctx->x + 128) * rc.right / 256;
            int stick_y = (-ctx->y + 128) * rc.bottom / 256;

            const COLORREF bg_color = GetSysColor(COLOR_BTNFACE);
            Gdiplus::Color bg_gdip_color{};
            bg_gdip_color.SetFromCOLORREF(bg_color);
            ctx->g->Clear(bg_gdip_color);

            ctx->g->FillEllipse(ctx->bg_brush, 0, 0, rc.right, rc.bottom);
            ctx->g->DrawEllipse(ctx->outline_pen, 0, 0, rc.right, rc.bottom);
            ctx->g->DrawLine(ctx->outline_pen, mid_x, 0, mid_x, rc.bottom);
            ctx->g->DrawLine(ctx->outline_pen, 0, mid_y, rc.right, mid_y);
            ctx->g->DrawLine(ctx->line_pen, mid_x, mid_y, stick_x, stick_y);
            ctx->g->FillEllipse(ctx->tip_brush, stick_x - 4, stick_y - 4, 8, 8);

            rc.right += 1;
            rc.bottom += 1;
            BitBlt(ctx->front_dc, 0, 0, rc.right, rc.bottom, ctx->back_dc, 0, 0, SRCCOPY);

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
    WNDCLASS wndclass{};
    wndclass.style = CS_GLOBALCLASS | CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndclass.lpfnWndProc = (WNDPROC)wndproc;
    wndclass.hInstance = hinst;
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.lpszClassName = CLASS_NAME;
    RegisterClass(&wndclass);
}

BOOL JoystickControl::get_position(HWND hwnd, int* x, int* y)
{
    WITH_VALID_CTX()

    if (x)
    {
        *x = ctx->x;
    }
    if (y)
    {
        *y = ctx->y;
    }

    return TRUE;
}

BOOL JoystickControl::set_position(HWND hwnd, int x, int y)
{
    WITH_VALID_CTX()

    ctx->x = x;
    ctx->y = y;
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE);
    SendMessage(GetParent(hwnd), WM_JOYSTICK_POSITION_CHANGED, 1, 0);

    return TRUE;
}
