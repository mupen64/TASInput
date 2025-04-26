/*
 * Copyright (c) 2025, TASInput maintainers, contributors, and original authors (nitsuja, Deflection).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

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


/**
 * \brief Reads a file into a buffer
 * \param path The file's path
 * \return The file's contents, or an empty vector if the operation failed
 */
static std::vector<uint8_t> read_file_buffer(const std::filesystem::path& path)
{
    FILE* f = fopen(path.string().c_str(), "rb");

    if (!f)
    {
        return {};
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<uint8_t> b;
    b.resize(len);

    fread(b.data(), sizeof(uint8_t), len, f);

    fclose(f);
    return b;
}

/**
 * \brief Reads source data into the destination, advancing the source pointer by <c>len</c>
 * \param src A pointer to the source data
 * \param dest The destination buffer
 * \param len The destination buffer's length
 */
static void memread(uint8_t** src, void* dest, unsigned int len)
{
    memcpy(dest, *src, len);
    *src += len;
}

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
