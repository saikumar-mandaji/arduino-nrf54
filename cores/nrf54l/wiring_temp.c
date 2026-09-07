#include "wiring_temp.h"
#include <errno.h>
#include <limits.h>
#include <nrfx_temp.h>

static bool s_temp_began = false;

bool temp_begin(void)
{
    if (s_temp_began)
    {
        return true;
    }

    nrfx_temp_config_t config = NRFX_TEMP_DEFAULT_CONFIG;
    /* NULL handler == blocking mode, per nrfx_temp_init()'s own doc. */
    int err = nrfx_temp_init(&config, NULL);
    if (err != 0 && err != -EALREADY)
    {
        return false;
    }

    s_temp_began = true;
    return true;
}

int32_t temp_read_centi_celsius(void)
{
    if (!temp_begin())
    {
        return INT32_MIN;
    }

    int err = nrfx_temp_measure();
    if (err != 0)
    {
        return INT32_MIN;
    }

    int32_t raw = nrfx_temp_result_get();
    return nrfx_temp_calculate(raw);
}
