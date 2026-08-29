/**
 * @file ad9361_transceiver.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "ad9361_transceiver.h"

#include "ad9361_shim.h"

namespace drivers {

    ad9361_transceiver::ad9361_transceiver(const hal::spi_config &spi_config,
                                            std::optional<std::uintptr_t> ctrl_gpio_base)
        : m_spi_config(spi_config), m_ctrl_gpio_base(ctrl_gpio_base), m_phy(nullptr), m_error_code(0) {
    }

    ad9361_transceiver::~ad9361_transceiver() {
        if (m_phy) {
            ad9361_transceiver_shim_remove(m_phy);
        }
    }

    bool ad9361_transceiver::is_initialized() const noexcept {
        return m_phy != nullptr;
    }

    std::int32_t ad9361_transceiver::error_code() const noexcept {
        return m_error_code;
    }

    bool ad9361_transceiver::init() {
        if (m_phy) {
            return true;
        }

        void *phy = nullptr;
        std::int32_t ret = ad9361_transceiver_shim_init(
            &phy, m_spi_config.device.c_str(), m_spi_config.speed_hz, m_spi_config.mode,
            m_ctrl_gpio_base ? static_cast<std::int32_t>(*m_ctrl_gpio_base) : 0, m_ctrl_gpio_base.has_value() ? 1 : 0);

        if (ret < 0) {
            m_error_code = ret;
            return false;
        }

        m_phy = static_cast<struct ad9361_rf_phy *>(phy);
        return true;
    }

} // namespace drivers
