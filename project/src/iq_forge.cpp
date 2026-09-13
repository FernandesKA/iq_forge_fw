/**
 * @file iq_forge.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "iq_forge.h"

#include <cstdio>

namespace project {

    iq_forge::iq_forge(const hal::spi_config &ad9361_spi_config, std::optional<std::uintptr_t> ad9361_ctrl_gpio_base,
                       std::optional<std::uintptr_t> dds_ctrl_gpio_base,
                       std::optional<std::uintptr_t> dds_ftw_gpio_base, std::optional<double> dds_clk_hz)
        : m_ad9361_spi(ad9361_spi_config), m_ad9361(m_ad9361_spi),
          m_ad9361_transceiver(ad9361_spi_config, ad9361_ctrl_gpio_base),
          m_ad9361_ctrl_gpio_base(ad9361_ctrl_gpio_base), m_dds_ctrl_gpio_base(dds_ctrl_gpio_base),
          m_dds_ftw_gpio_base(dds_ftw_gpio_base), m_dds_clk_hz(dds_clk_hz) {
    }

    std::optional<std::uint8_t> iq_forge::read_ad9361_vendor_id() const {
        std::uint8_t id = m_ad9361.read_vendor_id();
        if (!m_ad9361_spi.last_error().empty()) {
            return std::nullopt;
        }
        return id;
    }

    const std::string &iq_forge::ad9361_spi_error() const {
        return m_ad9361_spi.last_error();
    }

    fpga::LoadResult iq_forge::load_fpga_bitstream(const std::filesystem::path &bitstream, std::uint32_t flags) {
        return m_fpga_manager.load(bitstream, flags);
    }

    bool iq_forge::apply_fpga_overlay(const std::string &name, const std::filesystem::path &dtbo_path, bool replace) {
        return m_dt_overlay.apply(name, dtbo_path, replace);
    }

    std::string iq_forge::fpga_state() const {
        return m_fpga_manager.state();
    }

    std::string iq_forge::overlay_status(const std::string &name) const {
        return m_dt_overlay.status(name);
    }

    bool iq_forge::bring_up_ad9361() const {
        if (!m_ad9361_ctrl_gpio_base) {
            m_ad9361_ctrl_gpio_last_error.clear();
            return true;
        }

        drivers::ad9361_ctrl_gpio ctrl(*m_ad9361_ctrl_gpio_base);
        bool ok = ctrl.bring_up();
        m_ad9361_ctrl_gpio_last_error = ok ? std::string() : ctrl.ctrl_register().last_error();
        return ok;
    }

    const std::string &iq_forge::ad9361_ctrl_gpio_error() const {
        return m_ad9361_ctrl_gpio_last_error;
    }

    bool iq_forge::init_ad9361_transceiver() {
        if (!m_ad9361_transceiver.init()) {
            return false;
        }

        // ad9361_default_init_param hardcodes TX_SAMPL_FREQ=30.72 MSPS (an
        // ADI reference-design default) - dds_tx_chain actually delivers
        // samples at dds_clk_hz/2 (see ad9361_transceiver::set_tx_sample_rate).
        // Best-effort: no dds_clk_hz means no DDS on this board (or an
        // fw_config without it), nothing to correct.
        if (m_dds_clk_hz) {
            std::uint32_t rate = static_cast<std::uint32_t>(*m_dds_clk_hz / 2.0);
            if (!m_ad9361_transceiver.set_tx_sample_rate(rate)) {
                std::fprintf(stderr, "warning: set_tx_sample_rate(%u) failed (%d)\n", rate,
                            m_ad9361_transceiver.error_code());
            } else {
                std::uint32_t readback = 0;
                if (m_ad9361_transceiver.get_tx_sample_rate(readback)) {
                    std::fprintf(stderr, "debug: tx sample rate requested=%u readback=%u\n", rate, readback);
                }
            }
        }

        return true;
    }

    std::int32_t iq_forge::ad9361_transceiver_error_code() const {
        return m_ad9361_transceiver.error_code();
    }

    bool iq_forge::ad9361_transceiver_ready() const {
        return m_ad9361_transceiver.is_initialized();
    }

    bool iq_forge::set_ad9361_tx_lo_frequency(std::uint64_t hz) {
        // set_tx_lo_frequency() retunes the synth and then runs TX_QUAD_CAL
        // (see ad9361_transceiver::set_tx_lo_frequency). That calibration
        // needs a quiet TX baseband to measure against - if the DDS is
        // still driving a tone through it (its FPGA-side enable bit
        // survives across ad9361_init(), unlike anything on the AD9361
        // itself), the calibration comes out ~30 dB worse (confirmed on
        // hardware: residual LO leakage dropped from ~156 dB to ~125 dB
        // just from muting the DDS first). Mute it for the retune+cal, then
        // put it back the way it was.
        std::optional<bool> was_enabled = dds_enabled();
        if (was_enabled && *was_enabled) {
            set_dds_enabled(false);
        }

        bool ok = m_ad9361_transceiver.set_tx_lo_frequency(hz);

        if (was_enabled && *was_enabled) {
            set_dds_enabled(true);
        }

        return ok;
    }

    bool iq_forge::get_ad9361_tx_lo_frequency(std::uint64_t &hz) {
        return m_ad9361_transceiver.get_tx_lo_frequency(hz);
    }

    bool iq_forge::set_ad9361_tx_attenuation(std::uint32_t attenuation_mdb) {
        return m_ad9361_transceiver.set_tx_attenuation(attenuation_mdb);
    }

    bool iq_forge::get_ad9361_tx_attenuation(std::uint32_t &attenuation_mdb) {
        return m_ad9361_transceiver.get_tx_attenuation(drivers::tx_channel::tx1, attenuation_mdb);
    }

    bool iq_forge::set_ad9361_rx_gain_control_mode(drivers::rx_gain_mode mode) {
        return m_ad9361_transceiver.set_rx_gain_control_mode(mode);
    }

    bool iq_forge::enable_ad9361_tx() {
        return m_ad9361_transceiver.enable_tx();
    }

    bool iq_forge::disable_ad9361_tx() {
        return m_ad9361_transceiver.disable_tx();
    }

    bool iq_forge::get_ad9361_ensm_state(drivers::ensm_state &state) {
        return m_ad9361_transceiver.get_ensm_state(state);
    }

    bool iq_forge::set_ad9361_tx_clock_data_delay(std::uint8_t fb_clk_delay, std::uint8_t tx_data_delay) {
        return m_ad9361_transceiver.set_tx_clock_data_delay(fb_clk_delay, tx_data_delay);
    }

    bool iq_forge::get_ad9361_tx_clock_data_delay(std::uint8_t &fb_clk_delay, std::uint8_t &tx_data_delay) {
        return m_ad9361_transceiver.get_tx_clock_data_delay(fb_clk_delay, tx_data_delay);
    }

    bool iq_forge::calibrate_ad9361_tx_quadrature() {
        return m_ad9361_transceiver.calibrate_tx_quadrature();
    }

    bool iq_forge::set_ad9361_lvds_invert(std::uint8_t ctrl1, std::uint8_t ctrl2) {
        return m_ad9361_transceiver.set_lvds_invert(ctrl1, ctrl2);
    }

    bool iq_forge::get_ad9361_lvds_invert(std::uint8_t &ctrl1, std::uint8_t &ctrl2) {
        return m_ad9361_transceiver.get_lvds_invert(ctrl1, ctrl2);
    }

    bool iq_forge::set_ad9361_bist_tone(drivers::bist_mode mode, std::uint32_t freq_hz, std::uint32_t level_db,
                                        std::uint32_t mask) {
        return m_ad9361_transceiver.set_bist_tone(mode, freq_hz, level_db, mask);
    }

    bool iq_forge::set_ad9361_bist_prbs(drivers::bist_mode mode) {
        return m_ad9361_transceiver.set_bist_prbs(mode);
    }

    bool iq_forge::set_ad9361_bist_loopback(std::int32_t mode) {
        return m_ad9361_transceiver.set_bist_loopback(mode);
    }

    bool iq_forge::set_dds_enabled(bool enabled) const {
        if (!m_dds_ctrl_gpio_base) {
            m_dds_ctrl_gpio_last_error.clear();
            return true;
        }

        drivers::dds_ctrl_gpio ctrl(*m_dds_ctrl_gpio_base);
        bool ok = ctrl.set_enabled(enabled);
        m_dds_ctrl_gpio_last_error = ok ? std::string() : ctrl.ctrl_register().last_error();
        return ok;
    }

    std::optional<bool> iq_forge::dds_enabled() const {
        if (!m_dds_ctrl_gpio_base) {
            m_dds_ctrl_gpio_last_error.clear();
            return std::nullopt;
        }

        drivers::dds_ctrl_gpio ctrl(*m_dds_ctrl_gpio_base);
        bool enabled = ctrl.is_enabled();
        bool ok = ctrl.ctrl_register().ok();
        m_dds_ctrl_gpio_last_error = ok ? std::string() : ctrl.ctrl_register().last_error();
        if (!ok) {
            return std::nullopt;
        }
        return enabled;
    }

    bool iq_forge::reset_dds() const {
        if (!m_dds_ctrl_gpio_base) {
            m_dds_ctrl_gpio_last_error.clear();
            return true;
        }

        drivers::dds_ctrl_gpio ctrl(*m_dds_ctrl_gpio_base);
        bool ok = ctrl.reset();
        m_dds_ctrl_gpio_last_error = ok ? std::string() : ctrl.ctrl_register().last_error();
        return ok;
    }

    const std::string &iq_forge::dds_ctrl_gpio_error() const {
        return m_dds_ctrl_gpio_last_error;
    }

    bool iq_forge::set_dds_ftw(std::uint32_t ftw) const {
        if (!m_dds_ftw_gpio_base) {
            m_dds_ftw_gpio_last_error = "no DDS FTW GPIO base (DDS_FTW_GPIO_BASE not set)";
            return false;
        }

        drivers::dds_ftw_gpio ctrl(*m_dds_ftw_gpio_base);
        bool ok = ctrl.set_ftw(ftw);
        m_dds_ftw_gpio_last_error = ok ? std::string() : ctrl.ctrl_register().last_error();
        return ok;
    }

    std::optional<std::uint32_t> iq_forge::get_dds_ftw() const {
        if (!m_dds_ftw_gpio_base) {
            m_dds_ftw_gpio_last_error = "no DDS FTW GPIO base (DDS_FTW_GPIO_BASE not set)";
            return std::nullopt;
        }

        drivers::dds_ftw_gpio ctrl(*m_dds_ftw_gpio_base);
        std::uint32_t ftw = 0;
        bool ok = ctrl.get_ftw(ftw);
        m_dds_ftw_gpio_last_error = ok ? std::string() : ctrl.ctrl_register().last_error();
        if (!ok) {
            return std::nullopt;
        }
        return ftw;
    }

    bool iq_forge::set_dds_frequency_hz(double hz) const {
        if (!m_dds_clk_hz) {
            m_dds_ftw_gpio_last_error = "no DDS clock rate (DDS_CLK_HZ not set) - can't convert Hz to FTW";
            return false;
        }
        if (hz < 0.0) {
            m_dds_ftw_gpio_last_error = "frequency must not be negative";
            return false;
        }

        constexpr double kAccScale = 16777216.0; // 2^24
        // The phase accumulator only advances on i_ce = i_en & lvds_phase_sel,
        // and lvds_phase_sel toggles every i_clk cycle (ad9361_tx_lvds.sv) -
        // so it actually accumulates at dds_clk_hz/2, not dds_clk_hz. See
        // regmap.md's f_out formula.
        double sample_rate_hz = *m_dds_clk_hz / 2.0;
        std::uint32_t ftw = static_cast<std::uint32_t>(hz * kAccScale / sample_rate_hz + 0.5);
        return set_dds_ftw(ftw);
    }

    std::optional<double> iq_forge::get_dds_frequency_hz() const {
        if (!m_dds_clk_hz) {
            m_dds_ftw_gpio_last_error = "no DDS clock rate (DDS_CLK_HZ not set) - can't convert FTW to Hz";
            return std::nullopt;
        }

        auto ftw = get_dds_ftw();
        if (!ftw) {
            return std::nullopt;
        }

        constexpr double kAccScale = 16777216.0; // 2^24
        double sample_rate_hz = *m_dds_clk_hz / 2.0;
        return *ftw * sample_rate_hz / kAccScale;
    }

    const std::string &iq_forge::dds_ftw_gpio_error() const {
        return m_dds_ftw_gpio_last_error;
    }

}
