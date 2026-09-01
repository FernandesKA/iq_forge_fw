/**
 * @file dds_ctrl_gpio.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief AXI GPIO exposing the DDS TX chain's enable bit on pluto_sky
 *        (axi_gpio_dds_ctrl @ 0x4121_0000). PL side only -- see
 *        https://github.com/FernandesKA/iq_forge_hdl/blob/main/docs/regmap.md
 * @version 0.1
 * @date 2026-09-01
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>

#include "mmio_register.h"

namespace drivers {

    class dds_ctrl_gpio {
        public:
            explicit dds_ctrl_gpio(std::uintptr_t mmio_base);

            bool set_enabled(bool enabled) const;

            const hal::mmio_register &ctrl_register() const { return m_reg; }

        private:
            hal::mmio_register m_reg;

            static constexpr std::uint32_t kDisabled = 0x0;
            static constexpr std::uint32_t kEnabled  = 0x1;
    };

}
