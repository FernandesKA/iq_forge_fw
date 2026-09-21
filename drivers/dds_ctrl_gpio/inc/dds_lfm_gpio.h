/**
 * @file dds_lfm_gpio.h
 * @brief Three AXI GPIOs holding the DDS LFM (chirp) sweep parameters, all
 *        24-bit FTW values in the same units as dds_ftw_gpio (see
 *        rtl/dds/lfm_ftw_generator.sv):
 *
 *          axi_gpio_lfm_start  @ base + 0x00000  first FTW of the sweep
 *          axi_gpio_lfm_stop   @ base + 0x10000  FTW the one-shot ramp saturates at
 *          axi_gpio_lfm_incr   @ base + 0x20000  FTW added per PL clock cycle
 *
 *        Both platforms, base 0x4123_0000. The three registers sit in
 *        separate 64 KiB AXI windows (one GPIO IP each), hence the stride.
 *        See https://github.com/FernandesKA/iq_forge_hdl/blob/main/docs/regmap.md
 */

#pragma once

#include <cstdint>
#include <string>

#include "mmio_register.h"

namespace drivers {

    class dds_lfm_gpio {
        public:
            explicit dds_lfm_gpio(std::uintptr_t mmio_base);

            bool set_start(std::uint32_t ftw) const;
            bool get_start(std::uint32_t &ftw) const;

            bool set_stop(std::uint32_t ftw) const;
            bool get_stop(std::uint32_t &ftw) const;

            bool set_incr(std::uint32_t ftw_per_clk) const;
            bool get_incr(std::uint32_t &ftw_per_clk) const;

            // Register that failed (or would have) most recently -- for error
            // text. Only meaningful right after a call returned false.
            const std::string &last_error() const { return m_last_error; }

            static constexpr std::uintptr_t kRegisterStride = 0x10000;

        private:
            bool write(const hal::mmio_register &reg, std::uint32_t ftw) const;
            bool read(const hal::mmio_register &reg, std::uint32_t &ftw) const;

            hal::mmio_register m_start;
            hal::mmio_register m_stop;
            hal::mmio_register m_incr;
            mutable std::string m_last_error;

            static constexpr std::uint32_t kFtwMask = 0x00FFFFFFu;
    };

}
