﻿/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

namespace Combos
{
    /**
     * \brief Represents an individual combo
     */
    class Combo {
    public:
        /**
         * \brief The combo's name
         */
        std::string name = "Unnamed Combo";

        /**
         * \brief The combo's samples
         */
        std::vector<core_buttons> samples;

        /**
         * \return Whether any sample utilizes the joystick (magnitude > 0)
         */
        bool uses_joystick() const
        {
            return std::any_of(samples.begin(), samples.end(), [](const core_buttons sample) {
                return sample.x != 0 || sample.y != 0;
            });
        }
    };

    /**
     * \brief Gets all available combos
     * \param path The path to a file to look for combos in
     */
    std::vector<Combo*> find(const std::filesystem::path& path);

    /**
     * \brief Writes combos to a file
     * \param path The path to the combo file
     * \param combos A list of combos
     * \return Whether the operation succeeded
     */
    bool save(const char* path, std::vector<Combo*> combos);
}; // namespace Combos
