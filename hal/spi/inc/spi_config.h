/**
 * @file spi_config.h
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace hal {

struct spi_config {
  std::string device = "/dev/spix-0";
  std::uint8_t mode = 1;
  std::uint8_t bits_per_word = 8;
  std::uint32_t speed_hz = 1000000;
};

std::optional<spi_config> load_spi_config(const std::string &path);

} // namespace hal
