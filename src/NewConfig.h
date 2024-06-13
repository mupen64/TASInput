#pragma once
#include <cstdint>

typedef struct s_config
{
    int32_t version = 4;
    int32_t always_on_top = false;
    int32_t float_from_parent = true;
    int32_t titlebar = true;
    int32_t client_drag = false;
    int32_t hifi_joystick = false;
    int32_t dialog_expanded[4] = { 0, 0, 0, 0};
    int32_t controller_active[4] = {1, 0, 0, 0};
    int32_t loop_combo = false;
    // Increments joystick position by the value of the magnitude slider when moving via keyboard or gamepad 
    int32_t relative_mode = false;
    float x_scale[4] = { 1, 1, 1, 1};
    float y_scale[4] = { 1, 1, 1, 1};
} t_config;

extern t_config new_config;

/**
 * \brief Saves the current config to a file
 */
void save_config();

/**
 * \brief Loads the config from a file
 */
void load_config();

