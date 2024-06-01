#pragma once

#include <float.h>
#include <stdint.h>

/** Opaque handle to store parameters for a given power instance */
struct power_handle;

/** Configuration structure to intialise power */
struct power_config
{
    /** Currently unused field */
    uint8_t reserved;
};

enum power_price_descriptor
{
    POWER_PRICE_NEGATIVE,
    POWER_PRICE_EXTREME_LOW,
    POWER_PRICE_VERY_LOW,
    POWER_PRICE_LOW,
    POWER_PRICE_NEUTRAL,
    POWER_PRICE_HIGH,
    POWER_PRICE_SPIKE,
    POWER_PRICE_NUM,
};

/**
 * Function to initialise the power handle.
 *
 * @note It is assumed that an internet connection has been established before
 * any of these functions are called.
 *
 * @param config Configuration stucture
 *
 * @return reference to an @ref power_handle on success, else @c NULL.
 */


void start_fetch_pricing_task(void);

struct power_handle *power_init(struct power_config *config);

/**
 * Function to retrieve the current electricity price.
 *
 * @param reference to an initialised @ref power_handle
 *
 * @return Current electricity price in dollars on success, @c DBL_MAX on error.
 */
double power_get_price(struct power_handle *handle);

enum power_price_descriptor power_get_price_descriptor(struct power_handle *handle, const char *descriptor);
