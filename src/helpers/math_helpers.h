/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

/**
 * \brief Remaps a value from a range to another
 * \param value The value to remap
 * \param from1 The first range's lower bounds
 * \param to1 The first range's upper bounds
 * \param from2 The second range's lower bounds
 * \param to2 The second range's upper bounds
 * \return The remapped value
 */
static float remap(const float value, const float from1, const float to1, const float from2, const float to2)
{
    return (value - from1) / (to1 - from1) * (to2 - from2) + from2;
}
