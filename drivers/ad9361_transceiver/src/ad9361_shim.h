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

int32_t ad9361_transceiver_shim_set_tx_lo_freq(void *phy, uint64_t lo_freq_hz);

int32_t ad9361_transceiver_shim_set_rx_gain_control_mode(void *phy, uint8_t ch, uint8_t gc_mode);

int32_t ad9361_transceiver_shim_enable_tx(void *phy);

#ifdef __cplusplus
}
#endif
