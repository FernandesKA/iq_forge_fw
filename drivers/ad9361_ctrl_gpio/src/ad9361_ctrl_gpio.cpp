/**
 * @file ad9361_ctrl_gpio.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-27
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "ad9361_ctrl_gpio.h"

#include <chrono>
#include <thread>

namespace drivers {

    ad9361_ctrl_gpio::ad9361_ctrl_gpio(std::uintptr_t mmio_base) : m_reg(mmio_base) {}

    bool ad9361_ctrl_gpio::bring_up() const {
        m_reg.write(kHoldReset);
        if (!m_reg.ok()) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        m_reg.write(kReleaseReset);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        m_reg.write(kSteadyState);
        return true;
    }

}
