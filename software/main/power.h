#pragma once

#include <float.h>
#include <stdint.h>

/** Opaque handle to store parameters for a given power instance */
struct power_handle;

/** Configuration structure to intialise power */
struct power_config {
  /** Currently unused field */
  uint8_t reserved;
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
struct power_handle *power_init(struct power_config *config);

/**
 * Function to retrieve the current electricity price.
 *
 * @param reference to an initialised @ref power_handle
 *
 * @return Current electricity price in dollars on success, @c DBL_MAX on error.
 */
double power_get_price(struct power_handle *handle);
