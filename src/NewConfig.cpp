/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include <NewConfig.h>
#include <MiscHelpers.h>
#include <Main.h>

#define CONFIG_PATH "tasinput.cfg"

constexpr t_config default_config;
t_config new_config;


void save_config()
{
    g_ef->log_trace(L"Saving config...");
    FILE* f = fopen(CONFIG_PATH, "wb");
    if (!f)
    {
        g_ef->log_error(L"Couldn't save config");
        return;
    }
    fwrite(&new_config, sizeof(t_config), 1, f);
    fclose(f);
}

void load_config()
{
    g_ef->log_trace(L"Loading config...");

    auto buffer = read_file_buffer(CONFIG_PATH);
    t_config loaded_config;

    if (buffer.empty() || buffer.size() != sizeof(t_config))
    {
        g_ef->log_trace(L"No config found, using default");
        loaded_config = default_config;
    }
    else
    {
        uint8_t* ptr = buffer.data();
        memread(&ptr, &loaded_config, sizeof(t_config));
    }

    if (loaded_config.version < default_config.version)
    {
        g_ef->log_trace(L"Outdated config version, using default");
        loaded_config = default_config;
    }

    new_config = loaded_config;
}
