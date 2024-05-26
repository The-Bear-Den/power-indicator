#include "power.h"

struct power_handle
{
    uint8_t reserved;
} handle;

struct power_handle *power_init(struct power_config *config)
{
    (void)config;

    return &handle;
}

double power_get_price(struct power_handle *handle)
{
    (void)handle;

    return 0.0;
}
