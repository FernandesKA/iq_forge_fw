/**
 * @file dds_ctrl_gpio.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-09-01
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "dds_ctrl_gpio.h"

namespace drivers {

    dds_ctrl_gpio::dds_ctrl_gpio(std::uintptr_t mmio_base) : m_reg(mmio_base) {}

    bool dds_ctrl_gpio::set_enabled(bool enabled) const {
        std::uint32_t v = m_reg.read();
        if (!m_reg.ok()) {
            return false;
        }
        v = enabled ? (v | kEnBit) : (v & ~kEnBit);
        m_reg.write(v);
        return m_reg.ok();
    }

    bool dds_ctrl_gpio::is_enabled() const {
        std::uint32_t v = m_reg.read();
        return (v & kEnBit) != 0;
    }

    bool dds_ctrl_gpio::reset() const {
        std::uint32_t v = m_reg.read();
        if (!m_reg.ok()) {
            return false;
        }
        m_reg.write(v | kRstBit);
        if (!m_reg.ok()) {
            return false;
        }
        m_reg.write(v & ~kRstBit);
        return m_reg.ok();
    }

}
