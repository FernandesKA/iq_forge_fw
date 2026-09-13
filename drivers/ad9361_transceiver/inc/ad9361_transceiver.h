/**
 * @file ad9361_transceiver.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>
#include <optional>

#include "spi_device.h"

struct ad9361_rf_phy;

namespace drivers {

    enum class rx_channel : std::uint8_t {
        rx1 = 0,
        rx2 = 1,
    };

    enum class rx_gain_mode : std::uint8_t {
        manual = 0,
        fast_attack_agc = 1,
        slow_attack_agc = 2,
        hybrid_agc = 3,
    };

    enum class tx_channel : std::uint8_t {
        tx1 = 0,
        tx2 = 1,
    };

    // Mirrors REG_ENSM_MODE (ad9361.h) - what the chip's state machine is
    // actually doing right now, independent of what was last commanded.
    enum class ensm_state : std::uint8_t {
        sleep_wait = 0x0,
        alert = 0x5,
        tx = 0x6,
        tx_flush = 0x7,
        rx = 0x8,
        rx_flush = 0x9,
        fdd = 0xA,
        fdd_flush = 0xB,
        sleep = 0x80,
        invalid = 0xFF,
    };

    // Mirrors enum ad9361_bist_mode (ad9361.h).
    enum class bist_mode : std::uint8_t {
        disable = 0,
        inject_tx = 1,
        inject_rx = 2,
    };

    class ad9361_transceiver {
        public:
            explicit ad9361_transceiver(const hal::spi_config &spi_config,
                                         std::optional<std::uintptr_t> ctrl_gpio_base = std::nullopt);
            ~ad9361_transceiver();

            ad9361_transceiver(const ad9361_transceiver &) = delete;
            ad9361_transceiver &operator=(const ad9361_transceiver &) = delete;

            bool init();

            bool set_tx_lo_frequency(std::uint64_t hz);
            bool get_tx_lo_frequency(std::uint64_t &hz);

            bool set_tx_attenuation(tx_channel ch, std::uint32_t attenuation_mdb);
            bool set_tx_attenuation(std::uint32_t attenuation_mdb);
            bool get_tx_attenuation(tx_channel ch, std::uint32_t &attenuation_mdb);

            bool set_rx_gain_control_mode(rx_channel ch, rx_gain_mode mode);
            bool set_rx_gain_control_mode(rx_gain_mode mode);

            bool enable_tx();
            bool disable_tx();
            bool get_ensm_state(ensm_state &state);

            // Manual TX digital-interface delay tuning (REG_TX_CLOCK_DATA_DELAY).
            // ad9361_dig_tune()/ad9361_hdl_loopback() are stubbed out on this
            // bare-metal port (no AXI-ADC/DMA BIST loopback core to run them
            // through), so the TX LVDS sampling point is never auto-calibrated -
            // these are for manually sweeping fb_clk_delay/tx_data_delay
            // (0-15 each) until the FPGA-side TX_D/TX_FRAME timing is sampled
            // correctly by the chip.
            bool set_tx_clock_data_delay(std::uint8_t fb_clk_delay, std::uint8_t tx_data_delay);
            bool get_tx_clock_data_delay(std::uint8_t &fb_clk_delay, std::uint8_t &tx_data_delay);

            // Forces a TX quadrature/LO-leakage recalibration (TX_QUAD_CAL)
            // at the currently-set TX LO. set_tx_lo_frequency() calls this
            // automatically after retuning - ad9361_set_tx_lo_freq() only
            // moves the synthesizer, it does NOT recalibrate, so without
            // this the quad/LOL correction stays calibrated for whatever
            // frequency was active at init() and LO leakage dominates the
            // spectrum at any other frequency (~40 dB above a DDS tone,
            // confirmed on hardware). Exposed standalone too, for
            // recalibrating without a frequency change (e.g. after an
            // attenuation change).
            bool calibrate_tx_quadrature();

            // Reconfigures the whole RX/TX digital clock chain (BBPLL, ADC/
            // DAC, HB filters, TX_SAMPL_FREQ/RX_SAMPL_FREQ) for hz. The
            // default init params hardcode a 30.72 MSPS ADI reference-design
            // rate that has nothing to do with dds_tx_chain's actual output
            // rate (dds_clk_hz/2) - without this call AD9361 samples the
            // LVDS port at the wrong rate and TX data comes out incoherent.
            bool set_tx_sample_rate(std::uint32_t hz);
            bool get_tx_sample_rate(std::uint32_t &hz);

            // Raw REG_LVDS_INVERT_CTRL1/2 (TX_FRAME/TX_D[5:0] and RX-side/
            // clock invert bits). The default masks are an unvalidated
            // copy from some other reference design's board - for
            // sweeping bit-by-bit against a known-good signal.
            bool set_lvds_invert(std::uint8_t ctrl1, std::uint8_t ctrl2);
            bool get_lvds_invert(std::uint8_t &ctrl1, std::uint8_t &ctrl2);

            // AD9361's own internal self-test (REG_BIST_CONFIG) - a tone or
            // PRBS pattern generated INSIDE the chip (ahead of the TX FIR/
            // DAC), entirely bypassing whatever comes in over our LVDS
            // port. Useful to split the problem in half: clean RF from
            // this while our own DDS tone is bad means the analog TX
            // chain/LO/calibration are fine and the fault is in the
            // digital interface content/timing; still bad here too means
            // the fault is upstream (PLL/mixer/filters/LO leakage cal).
            // mask selects TX1_I/TX1_Q/TX2_I/TX2_Q (bit0..3) - use 0x3 for
            // TX1 I+Q. Remember to call with mode=disable afterwards to
            // hand the DAC back to the real digital interface.
            bool set_bist_tone(bist_mode mode, std::uint32_t freq_hz, std::uint32_t level_db, std::uint32_t mask);
            bool set_bist_prbs(bist_mode mode);

            // AD9361's internal TX->RX digital loopback (mode=1) - loops
            // whatever it received on the TX digital port straight back
            // out the RX digital port, inside the chip, no DAC/mixer/ADC
            // involved. Pairs with an FPGA-side RX capture
            // (ad9361_rx_lvds_wrapper) to verify data sent out
            // ad9361_tx_lvds actually arrives at the chip correctly.
            bool set_bist_loopback(std::int32_t mode);

            bool is_initialized() const noexcept;

            std::int32_t error_code() const noexcept;

        private:
            hal::spi_config m_spi_config;
            std::optional<std::uintptr_t> m_ctrl_gpio_base;
            struct ad9361_rf_phy *m_phy;
            std::int32_t m_error_code;
    };

}
