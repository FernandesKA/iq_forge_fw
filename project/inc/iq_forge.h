/**
 * @file iq_forge.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "ad9361.h"
#include "ad9361_ctrl_gpio.h"
#include "ad9361_transceiver.h"
#include "dds_ctrl_gpio.h"
#include "dds_ftw_gpio.h"
#include "spi_device.h"

#include "dt_overlay.hpp"
#include "fpga_manager.hpp"

namespace project {

    class iq_forge {
        public:
            explicit iq_forge(const hal::spi_config &ad9361_spi_config = {},
                               std::optional<std::uintptr_t> ad9361_ctrl_gpio_base = std::nullopt,
                               std::optional<std::uintptr_t> dds_ctrl_gpio_base = std::nullopt,
                               std::optional<std::uintptr_t> dds_ftw_gpio_base = std::nullopt,
                               std::optional<double> dds_clk_hz = std::nullopt);

            std::optional<std::uint8_t> read_ad9361_vendor_id() const;
            const std::string &ad9361_spi_error() const;

            fpga::LoadResult load_fpga_bitstream(const std::filesystem::path &bitstream, std::uint32_t flags = fpga::FpgaFlagNone);

            bool apply_fpga_overlay(const std::string &name, const std::filesystem::path &dtbo_path, bool replace = false);

            std::string fpga_state() const;

            std::string overlay_status(const std::string &name) const;

            bool bring_up_ad9361() const;
            const std::string &ad9361_ctrl_gpio_error() const;

            bool init_ad9361_transceiver();
            std::int32_t ad9361_transceiver_error_code() const;

            bool ad9361_transceiver_ready() const;

            bool set_ad9361_tx_lo_frequency(std::uint64_t hz);
            bool get_ad9361_tx_lo_frequency(std::uint64_t &hz);

            bool set_ad9361_tx_attenuation(std::uint32_t attenuation_mdb);
            bool get_ad9361_tx_attenuation(std::uint32_t &attenuation_mdb);

            bool set_ad9361_rx_gain_control_mode(drivers::rx_gain_mode mode);

            bool enable_ad9361_tx();
            bool disable_ad9361_tx();

            bool get_ad9361_ensm_state(drivers::ensm_state &state);

            bool set_ad9361_tx_clock_data_delay(std::uint8_t fb_clk_delay, std::uint8_t tx_data_delay);
            bool get_ad9361_tx_clock_data_delay(std::uint8_t &fb_clk_delay, std::uint8_t &tx_data_delay);

            bool calibrate_ad9361_tx_quadrature();

            bool set_ad9361_lvds_invert(std::uint8_t ctrl1, std::uint8_t ctrl2);
            bool get_ad9361_lvds_invert(std::uint8_t &ctrl1, std::uint8_t &ctrl2);

            bool set_ad9361_bist_tone(drivers::bist_mode mode, std::uint32_t freq_hz, std::uint32_t level_db,
                                       std::uint32_t mask);
            bool set_ad9361_bist_prbs(drivers::bist_mode mode);
            bool set_ad9361_bist_loopback(std::int32_t mode);

            bool set_dds_enabled(bool enabled) const;

            std::optional<bool> dds_enabled() const;

            bool reset_dds() const;

            const std::string &dds_ctrl_gpio_error() const;

            bool set_dds_ftw(std::uint32_t ftw) const;
            std::optional<std::uint32_t> get_dds_ftw() const;

            bool set_dds_frequency_hz(double hz) const;
            std::optional<double> get_dds_frequency_hz() const;

            const std::string &dds_ftw_gpio_error() const;

        private:
            hal::spi_device m_ad9361_spi;
            drivers::ad9361 m_ad9361;
            drivers::ad9361_transceiver m_ad9361_transceiver;
            fpga::FpgaManager m_fpga_manager;
            fpga::DtOverlay m_dt_overlay;

            std::optional<std::uintptr_t> m_ad9361_ctrl_gpio_base;
            std::optional<std::uintptr_t> m_dds_ctrl_gpio_base;
            std::optional<std::uintptr_t> m_dds_ftw_gpio_base;
            std::optional<double> m_dds_clk_hz;

            mutable std::string m_ad9361_ctrl_gpio_last_error;
            mutable std::string m_dds_ctrl_gpio_last_error;
            mutable std::string m_dds_ftw_gpio_last_error;
    };

}
