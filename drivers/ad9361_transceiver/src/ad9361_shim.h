/**
 * @file ad9361_shim.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t ad9361_transceiver_shim_init(void **out_phy,
                                      const char *spi_device_path,
                                      uint32_t spi_speed_hz,
                                      uint8_t spi_mode,
                                      int32_t ctrl_gpio_port,
                                      int has_ctrl_gpio);

void ad9361_transceiver_shim_remove(void *phy);

#ifdef __cplusplus
}
#endif
