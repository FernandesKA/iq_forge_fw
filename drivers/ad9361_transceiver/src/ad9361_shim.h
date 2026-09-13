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

/* AD9361's own self-test facilities (REG_BIST_CONFIG) - independent of
 * ad9361_dig_tune/ad9361_hdl_loopback (stubbed on this port, need an
 * FPGA-side BIST/DMA core we don't have). Useful to isolate whether a bad
 * signal is coming from our digital LVDS data or from the analog chain:
 *
 * - bist_tone(mode=INJ_TX, ...) makes AD9361 generate a tone INSIDE the
 *   chip (BIST_CTRL_POINT(0), injected ahead of the TX FIR/DAC), entirely
 *   bypassing whatever we send over the LVDS port. Clean RF from this =
 *   analog TX chain + LO + calibration are fine, problem is digital
 *   interface content/timing. Still bad = problem is upstream of the
 *   digital interface (PLL/mixer/filters/LO leakage cal).
 * - bist_prbs(mode=INJ_TX) does the same but with a PRBS pattern instead
 *   of a tone - useful as a wideband noise-floor/linearity check.
 *
 * mode: 0=disable, 1=inject on TX, 2=inject on RX (ad9361_bist_mode enum
 * in ad9361.h - passed as plain int here since ad9361_api.h doesn't
 * expose that type; the enum has no explicit underlying type, so it's
 * `int`-sized like int32_t on this target). mask selects which of
 * TX1_I/TX1_Q/TX2_I/TX2_Q get the injected signal (bit0=TX1_I,
 * bit1=TX1_Q, bit2=TX2_I, bit3=TX2_Q) - for TX1 I+Q use mask=0x3. */
int32_t ad9361_transceiver_shim_bist_tone(void *phy, int32_t mode, uint32_t freq_hz, uint32_t level_db,
                                           uint32_t mask);

int32_t ad9361_transceiver_shim_bist_prbs(void *phy, int32_t mode);

/* AD9361's internal digital TX->RX loopback (REG_OBSERVE_CONFIG,
 * DATA_PORT_LOOP_TEST_ENABLE): mode 0=off, 1=loop TX digital data
 * straight back to the RX digital port INSIDE the chip (no DAC/mixer/
 * ADC involved), 2=loop RX->TX via the FPGA (needs ad9361_hdl_loopback,
 * stubbed on this port - don't use). Mode 1 is what lets
 * ad9361_rx_lvds_wrapper on the FPGA see exactly what the chip received
 * from ad9361_tx_lvds, to verify data integrity end to end. */
int32_t ad9361_transceiver_shim_bist_loopback(void *phy, int32_t mode);

#ifdef __cplusplus
}
#endif
