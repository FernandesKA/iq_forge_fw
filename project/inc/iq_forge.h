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
#include <string>

#include "ad9361.h"
#include "spi_device.h"

#include "dt_overlay.hpp"
#include "fpga_manager.hpp"

namespace project {

    class iq_forge {
        public:
            explicit iq_forge(const hal::spi_config &ad9361_spi_config = {});

            std::uint8_t read_ad9361_vendor_id() const;

            fpga::LoadResult load_fpga_bitstream(const std::filesystem::path &bitstream, std::uint32_t flags = fpga::FpgaFlagNone);

            bool apply_fpga_overlay(const std::string &name, const std::filesystem::path &dtbo_path, bool replace = false);

        private:
            hal::spi_device m_ad9361_spi;
            drivers::ad9361 m_ad9361;
            fpga::FpgaManager m_fpga_manager;
            fpga::DtOverlay m_dt_overlay;
    };

}
