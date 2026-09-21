/**
 * @file dds_ctrl_gpio.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief AXI GPIO exposing the DDS TX chain's control bits (enable, reset,
 *        sine/LFM mode select, LFM continious flag, LFM restart), both
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

    // What the DDS TX chain generates while enabled: a fixed tone at the FTW
    // register (sine), or the LFM (chirp) sweep configured via dds_lfm_gpio.
    enum class dds_mode : std::uint8_t {
        sine = 0,
        lfm = 1,
    };

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

            bool set_mode(dds_mode mode) const;
            bool get_mode(dds_mode &mode) const;

            // dds_lfm_continious: free-running ramp that ignores the stop
            // value and wraps at 2^24 (1), or one-shot ramp that saturates at
            // stop (0). See rtl/dds/lfm_ftw_generator.sv.
            bool set_lfm_continious(bool continious) const;
            bool get_lfm_continious(bool &continious) const;

            // Restarts the LFM sweep from its start value: dds_lfm_load is
            // edge-triggered in HW (rising edge = restart), so this is just
            // 0 -> 1 -> 0 on the bit. No effect in sine mode.
            bool restart_lfm() const;

            const hal::mmio_register &ctrl_register() const { return m_reg; }

        private:
            bool set_bit(std::uint32_t mask, bool value) const;
            bool get_bit(std::uint32_t mask, bool &value) const;

            hal::mmio_register m_reg;

            static constexpr std::uint32_t kEnBit            = 1u << 0;
            static constexpr std::uint32_t kRstBit           = 1u << 1;
            static constexpr std::uint32_t kModeBit          = 1u << 2;
            static constexpr std::uint32_t kLfmContiniousBit = 1u << 3;
            static constexpr std::uint32_t kLfmLoadBit       = 1u << 4;
    };

}
