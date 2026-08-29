/**
 * @file spi_device.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "spi_config.h"

namespace hal {

    class spi_device {
        public:
            explicit spi_device(const spi_config &config);
            ~spi_device();

            spi_device(const spi_device &) = delete;
            spi_device &operator=(const spi_device &) = delete;

            bool transfer(const std::uint8_t *tx, std::uint8_t *rx, std::size_t len) const;

            bool ok() const;
            const std::string &last_error() const;

        private:
            bool ensure_open() const;

            std::string m_path;
            mutable int m_fd;
            std::uint8_t m_mode;
            std::uint8_t m_bits_per_word;
            std::uint32_t m_speed_hz;
            mutable std::string m_last_error;
    };

}
