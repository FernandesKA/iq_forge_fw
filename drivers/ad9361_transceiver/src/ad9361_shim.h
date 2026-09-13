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

int32_t ad9361_transceiver_shim_get_tx_lo_freq(void *phy, uint64_t *lo_freq_hz);

int32_t ad9361_transceiver_shim_set_tx_attenuation(void *phy, uint8_t ch, uint32_t attenuation_mdb);

int32_t ad9361_transceiver_shim_get_tx_attenuation(void *phy, uint8_t ch, uint32_t *attenuation_mdb);

int32_t ad9361_transceiver_shim_set_rx_gain_control_mode(void *phy, uint8_t ch, uint8_t gc_mode);

int32_t ad9361_transceiver_shim_enable_tx(void *phy);

int32_t ad9361_transceiver_shim_disable_tx(void *phy);

int32_t ad9361_transceiver_shim_get_ensm_state(void *phy, uint8_t *state);

/* Manual TX digital-interface delay tuning - see ad9361_set_tx_clock_data_delay
 * in ad9361.c for why this is needed (dig_tune is stubbed on this port). */
int32_t ad9361_transceiver_shim_set_tx_clock_data_delay(void *phy, uint8_t fb_clk_delay, uint8_t tx_data_delay);

int32_t ad9361_transceiver_shim_get_tx_clock_data_delay(void *phy, uint8_t *fb_clk_delay, uint8_t *tx_data_delay);

/* Forces a TX quadrature/LO-leakage recalibration (TX_QUAD_CAL) at whatever
 * TX LO frequency is currently set. ad9361_set_tx_lo_freq() only retunes the
 * synthesizer - it does NOT recalibrate, so the quad/LOL correction stays
 * whatever it was at ad9361_init() time (calibrated for the init default
 * frequency) until this is called. */
int32_t ad9361_transceiver_shim_calibrate_tx_quad(void *phy);

/* Reconfigures AD9361's whole RX/TX digital-interface clock chain (BBPLL,
 * ADC/DAC, HB filters, TX_SAMPL_FREQ/RX_SAMPL_FREQ) for the given sample
 * rate. Needed because ad9361_default_init_param's tx_path_clock_frequencies
 * hardcodes a 30.72 MSPS ADI reference-design default that has nothing to
 * do with this board - dds_tx_chain actually delivers samples at
 * dds_clk_hz/2 (i_ce only fires every other i_clk cycle, see
 * dds_tx_chain.sv/ad9361_tx_lvds.sv). Without this, AD9361 samples the LVDS
 * port at the wrong rate and the TX data comes out incoherent (confirmed on
 * hardware: no detectable DDS tone anywhere in the spectrum, at any
 * TX_CLOCK_DATA_DELAY setting, regardless of calibration). */
int32_t ad9361_transceiver_shim_set_tx_sampling_freq(void *phy, uint32_t sampling_freq_hz);

int32_t ad9361_transceiver_shim_get_tx_sampling_freq(void *phy, uint32_t *sampling_freq_hz);

/* Raw REG_LVDS_INVERT_CTRL1/2 access - see ad9361_set_lvds_invert in
 * ad9361.c for why (the default masks are an unvalidated copy from some
 * other reference design's board). */
int32_t ad9361_transceiver_shim_set_lvds_invert(void *phy, uint8_t ctrl1, uint8_t ctrl2);

int32_t ad9361_transceiver_shim_get_lvds_invert(void *phy, uint8_t *ctrl1, uint8_t *ctrl2);

#ifdef __cplusplus
}
#endif
