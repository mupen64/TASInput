/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "stdafx.h"
#include "GamepadManager.h"
#include <Main.h>

struct gamepad_manager_context {
    SDL_Gamepad* gamepad{};
};

static gamepad_manager_context g_ctx;

void GamepadManager::init()
{
    RT_ASSERT(SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK), L"Failed to initialize SDL gamepad subsystem");
}
void GamepadManager::poll_events()
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
        case SDL_EVENT_GAMEPAD_ADDED:
            {
                SDL_Gamepad* pad = SDL_OpenGamepad(e.gdevice.which);
                const auto name = SDL_GetGamepadName(pad);
                MessageBoxA(NULL, std::format("Gamepad {} connected", name).c_str(), "Info", MB_OK);
            }
            break;

        case SDL_EVENT_GAMEPAD_REMOVED:
            printf("Gamepad removed: %d\n", e.gdevice.which);
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            printf("Button %d pressed\n", e.gbutton.button);
            break;
        default:
            break;
        }
    }
}

core_buttons GamepadManager::get_input()
{
    return {};
}
