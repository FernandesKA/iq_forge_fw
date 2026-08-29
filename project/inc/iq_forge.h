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
#include "spi_device.h"

#include "dt_overlay.hpp"
#include "fpga_manager.hpp"

namespace project {

    class iq_forge {
        public:
            explicit iq_forge(const hal::spi_config &ad9361_spi_config = {},
                               std::optional<std::uintptr_t> ad9361_ctrl_gpio_base = std::nullopt);

            std::optional<std::uint8_t> read_ad9361_vendor_id() const;
            const std::string &ad9361_spi_error() const;

            fpga::LoadResult load_fpga_bitstream(const std::filesystem::path &bitstream, std::uint32_t flags = fpga::FpgaFlagNone);

            bool apply_fpga_overlay(const std::string &name, const std::filesystem::path &dtbo_path, bool replace = false);

            // Raw fpga_manager state (e.g. "operating") - use to skip a redundant reload.
            std::string fpga_state() const;

            // Raw overlay status (e.g. "applied") - use to skip a redundant re-apply.
            std::string overlay_status(const std::string &name) const;

            // Releases the AD9361 out of hardware reset via axi_gpio_ad9361_ctrl (see
            // https://github.com/FernandesKA/iq_forge_hdl/blob/main/docx/regmap.md).
            // No-op if constructed without ad9361_ctrl_gpio_base (e.g. rk7020f, which
            // ties those pins to fixed constants in the PL and has no such GPIO).
            // Must run before any AD9361 SPI access - the chip stays in reset
            // (and SPI reads back a floating bus) until this write happens.
            bool bring_up_ad9361() const;
            const std::string &ad9361_ctrl_gpio_error() const;

            bool init_ad9361_transceiver();
            std::int32_t ad9361_transceiver_error_code() const;

            bool ad9361_transceiver_ready() const;

            bool set_ad9361_tx_lo_frequency(std::uint64_t hz);
            bool set_ad9361_rx_gain_control_mode(drivers::rx_gain_mode mode);
            bool enable_ad9361_tx();

        private:
            hal::spi_device m_ad9361_spi;
            drivers::ad9361 m_ad9361;
            drivers::ad9361_transceiver m_ad9361_transceiver;
            fpga::FpgaManager m_fpga_manager;
            fpga::DtOverlay m_dt_overlay;

            // Only the address is stored - the mmio mapping itself is made on
            // demand in bring_up_ad9361(), since the AXI GPIO it points at
            // isn't backed by anything until after the FPGA bitstream loads,
            // which happens after this class is constructed.
            std::optional<std::uintptr_t> m_ad9361_ctrl_gpio_base;

            mutable std::string m_ad9361_ctrl_gpio_last_error;
    };

}
