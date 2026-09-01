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
        m_reg.write(enabled ? kEnabled : kDisabled);
        return m_reg.ok();
    }

}
