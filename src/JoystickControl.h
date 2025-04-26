/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

/**
 * \brief A module responsible for implementing a joystick control.
 */
namespace JoystickControl
{
    const auto CLASS_NAME = "JOYSTICK";
    constexpr auto WM_JOYSTICK_POSITION_CHANGED = WM_USER + 10;
    constexpr auto WM_JOYSTICK_DRAG_BEGIN = WM_USER + 11;

    struct t_context {
        struct t_internal {
            enum class Mode {
                None,
                Absolute,
                Sticky,
                Relative
            };
            Mode mode = Mode::None;
            POINT cursor_diff{};
        };

        int x{};
        int y{};
        t_internal internal{};
    };

    void register_class(HINSTANCE hinst);
} // namespace JoystickControl
