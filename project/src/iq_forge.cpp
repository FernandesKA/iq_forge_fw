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
        return m_ad9361_transceiver.init();
    }

    std::int32_t iq_forge::ad9361_transceiver_error_code() const {
        return m_ad9361_transceiver.error_code();
    }

    bool iq_forge::ad9361_transceiver_ready() const {
        return m_ad9361_transceiver.is_initialized();
    }

    bool iq_forge::set_ad9361_tx_lo_frequency(std::uint64_t hz) {
        return m_ad9361_transceiver.set_tx_lo_frequency(hz);
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
        std::uint32_t ftw = static_cast<std::uint32_t>(hz * kAccScale / *m_dds_clk_hz + 0.5);
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
        return *ftw * *m_dds_clk_hz / kAccScale;
    }

    const std::string &iq_forge::dds_ftw_gpio_error() const {
        return m_dds_ftw_gpio_last_error;
    }

}
