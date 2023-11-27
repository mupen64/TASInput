#pragma once
#include <cstdint>

typedef struct s_config
{
    int32_t version = 0;
    int32_t always_on_top = false;
    int32_t float_from_parent = true;
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

