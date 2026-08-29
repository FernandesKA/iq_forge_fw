/**
 * @file ad9361_ctrl_gpio.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief AXI GPIO exposing AD9361's RESETB/ENABLE/TXNRX pins on pluto_sky
 *        (axi_gpio_ad9361_ctrl @ 0x4120_0000). PL side only, no automatic
 *        init anywhere else - see
 *        https://github.com/FernandesKA/iq_forge_hdl/blob/main/docx/regmap.md
 * @version 0.1
 * @date 2026-08-27
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>

#include "mmio_register.h"

namespace drivers {

    class ad9361_ctrl_gpio {
        public:
            explicit ad9361_ctrl_gpio(std::uintptr_t mmio_base);

            // hold reset -> release resetb -> resetb=1,enable=0,txnrx=1
            // (FDD steady state, per regmap.md's bring-up sequence).
            bool bring_up() const;

            const hal::mmio_register &ctrl_register() const { return m_reg; }

        private:
            hal::mmio_register m_reg;

            static constexpr std::uint32_t kHoldReset    = 0x0;
            static constexpr std::uint32_t kReleaseReset  = 0x1;
            static constexpr std::uint32_t kSteadyState   = 0x5;
    };

}
