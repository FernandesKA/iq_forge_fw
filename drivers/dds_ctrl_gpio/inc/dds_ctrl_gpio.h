/**
 * @file dds_ctrl_gpio.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief AXI GPIO exposing the DDS TX chain's enable and reset bits, both
 *        platforms (axi_gpio_dds_ctrl @ 0x4121_0000). PL side only -- see
 *        https://github.com/FernandesKA/iq_forge_hdl/blob/main/docs/regmap.md
 * @version 0.2
 * @date 2026-09-12
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
            bool is_enabled() const;

            // Pulses dds_rst: asserts it (holding the phase accumulator,
            // LUT and LVDS core in reset) then immediately releases it,
            // leaving dds_en untouched. Level-sensitive/active-high in HW,
            // but software only ever needs the momentary-restart behavior.
            bool reset() const;

            const hal::mmio_register &ctrl_register() const { return m_reg; }

        private:
            hal::mmio_register m_reg;

            static constexpr std::uint32_t kEnBit  = 1u << 0;
            static constexpr std::uint32_t kRstBit = 1u << 1;
    };

}
