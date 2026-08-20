/**
 * @file ad9361.h
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

#include "spi_device.h"

namespace drivers {

class ad9361 {
public:
  explicit ad9361(const hal::spi_device &spi);

  std::uint8_t read_register(std::uint16_t address) const;
  void write_register(std::uint16_t address, std::uint8_t value) const;

  std::uint8_t read_vendor_id() const;

private:
  const hal::spi_device &m_spi;

  static constexpr std::uint16_t REG_PRODUCT_ID = 0x0037;
  static constexpr std::uint8_t PRODUCT_ID_MASK = 0x1F;
};

} // namespace drivers
