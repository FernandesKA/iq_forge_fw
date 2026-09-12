/**
 * @file dds_ftw_gpio.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-09-12
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "dds_ftw_gpio.h"

namespace drivers {

    dds_ftw_gpio::dds_ftw_gpio(std::uintptr_t mmio_base) : m_reg(mmio_base) {}

    bool dds_ftw_gpio::set_ftw(std::uint32_t ftw) const {
        m_reg.write(ftw & kFtwMask);
        return m_reg.ok();
    }

    bool dds_ftw_gpio::get_ftw(std::uint32_t &ftw) const {
        std::uint32_t v = m_reg.read();
        if (!m_reg.ok()) {
            return false;
        }
        ftw = v & kFtwMask;
        return true;
    }

}
