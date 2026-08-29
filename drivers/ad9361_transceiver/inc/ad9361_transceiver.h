/**
 * @file ad9361_transceiver.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>
#include <optional>

#include "spi_device.h"

struct ad9361_rf_phy;

namespace drivers {

    enum class rx_channel : std::uint8_t {
        rx1 = 0,
        rx2 = 1,
    };

    enum class rx_gain_mode : std::uint8_t {
        manual = 0,
        fast_attack_agc = 1,
        slow_attack_agc = 2,
        hybrid_agc = 3,
    };

    class ad9361_transceiver {
        public:
            explicit ad9361_transceiver(const hal::spi_config &spi_config,
                                         std::optional<std::uintptr_t> ctrl_gpio_base = std::nullopt);
            ~ad9361_transceiver();

            ad9361_transceiver(const ad9361_transceiver &) = delete;
            ad9361_transceiver &operator=(const ad9361_transceiver &) = delete;

            bool init();

            bool set_tx_lo_frequency(std::uint64_t hz);
            bool set_rx_gain_control_mode(rx_channel ch, rx_gain_mode mode);
            bool set_rx_gain_control_mode(rx_gain_mode mode);
            bool enable_tx();

            bool is_initialized() const noexcept;

            std::int32_t error_code() const noexcept;

        private:
            hal::spi_config m_spi_config;
            std::optional<std::uintptr_t> m_ctrl_gpio_base;
            struct ad9361_rf_phy *m_phy;
            std::int32_t m_error_code;
    };

}
