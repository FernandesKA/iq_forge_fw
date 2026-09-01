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
                       std::optional<std::uintptr_t> dds_ctrl_gpio_base)
        : m_ad9361_spi(ad9361_spi_config), m_ad9361(m_ad9361_spi),
          m_ad9361_transceiver(ad9361_spi_config, ad9361_ctrl_gpio_base),
          m_ad9361_ctrl_gpio_base(ad9361_ctrl_gpio_base), m_dds_ctrl_gpio_base(dds_ctrl_gpio_base) {
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

    bool iq_forge::set_ad9361_rx_gain_control_mode(drivers::rx_gain_mode mode) {
        return m_ad9361_transceiver.set_rx_gain_control_mode(mode);
    }

    bool iq_forge::enable_ad9361_tx() {
        return m_ad9361_transceiver.enable_tx();
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

    const std::string &iq_forge::dds_ctrl_gpio_error() const {
        return m_dds_ctrl_gpio_last_error;
    }

}
