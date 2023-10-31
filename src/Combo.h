#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "Controller.h"

/**
 * \brief Represents an individual combo
 */
class Combo
{
public:
    /**
     * \brief The combo's name
     */
    std::string name;
    
    /**
     * \brief The combo's samples
     */
    std::vector<BUTTONS> samples;
    
    /**
     * \return Whether any sample utilizes the joystick (magnitude > 0)
     */
    bool uses_joystick() const
    {
        return std::any_of(samples.begin(), samples.end(), [] (const BUTTONS sample)
        {
            return sample.X_AXIS != 0 || sample.Y_AXIS != 0;   
        });
    }
};

/**
 * \brief Gets all available combos
 * \param path The path to a file to look for combos in
 */
std::vector<Combo*> find_combos(const char* path);

