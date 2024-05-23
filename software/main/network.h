#pragma once

#include <stdbool.h>

/**
 * Function to initialise networking.
 *
 * @note This assumes that the NVS has been initialised already.
 *
 * @return @c true if succesful, else @c false
 */
bool network_init(void);
