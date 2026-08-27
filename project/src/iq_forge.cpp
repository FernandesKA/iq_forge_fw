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

    iq_forge::iq_forge(const hal::spi_config &ad9361_spi_config, std::optional<std::uintptr_t> ad9361_ctrl_gpio_base)
        : m_ad9361_spi(ad9361_spi_config), m_ad9361(m_ad9361_spi), m_ad9361_ctrl_gpio_base(ad9361_ctrl_gpio_base) {
    }

    std::uint8_t iq_forge::read_ad9361_vendor_id() const {
        return m_ad9361.read_vendor_id();
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

    void iq_forge::bring_up_ad9361() const {
        if (!m_ad9361_ctrl_gpio_base) {
            return;
        }
        drivers::ad9361_ctrl_gpio(*m_ad9361_ctrl_gpio_base).bring_up();
    }

}
