/**
 * @file dds_ftw_gpio.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief AXI GPIO exposing the DDS phase accumulator's frequency tuning
 *        word, both platforms (axi_gpio_dds_ftw @ 0x4122_0000). 24-bit,
 *        f_out = FTW * f_clk / 2^24 (f_clk = FCLK_CLK0: 50 MHz on
 *        pluto_sky, 40 MHz on rk7020f). See
 *        https://github.com/FernandesKA/iq_forge_hdl/blob/main/docs/regmap.md
 * @version 0.1
 * @date 2026-09-12
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>

#include "mmio_register.h"

namespace drivers {

    class dds_ftw_gpio {
        public:
            explicit dds_ftw_gpio(std::uintptr_t mmio_base);

            bool set_ftw(std::uint32_t ftw) const;
            bool get_ftw(std::uint32_t &ftw) const;

            const hal::mmio_register &ctrl_register() const { return m_reg; }

        private:
            hal::mmio_register m_reg;

            static constexpr std::uint32_t kFtwMask = 0x00FFFFFFu;
    };

}
