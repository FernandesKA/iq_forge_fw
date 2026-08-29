/**
 * @file hal_spi.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "no_os_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hal_spi_extra {
    const char *device_path;
};

extern const struct no_os_spi_platform_ops hal_spi_ops;

#ifdef __cplusplus
}
#endif
