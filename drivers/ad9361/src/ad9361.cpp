/**
 * @file ad9361.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "ad9361.h"

namespace drivers {

namespace {
constexpr std::uint16_t SPI_WRITE_FLAG = 0x8000;
constexpr std::uint16_t SPI_ADDR_MASK = 0x03FF;
} // namespace

ad9361::ad9361(const hal::spi_device &spi) : m_spi(spi) {}

std::uint8_t ad9361::read_register(std::uint16_t address) const {
  std::uint16_t control = address & SPI_ADDR_MASK;

  std::uint8_t tx[3] = {static_cast<std::uint8_t>(control >> 8),
                        static_cast<std::uint8_t>(control & 0xFF), 0x00};
  std::uint8_t rx[3] = {0, 0, 0};

  m_spi.transfer(tx, rx, sizeof(tx));

  return rx[2];
}

void ad9361::write_register(std::uint16_t address, std::uint8_t value) const {
  std::uint16_t control = SPI_WRITE_FLAG | (address & SPI_ADDR_MASK);

  std::uint8_t tx[3] = {static_cast<std::uint8_t>(control >> 8),
                        static_cast<std::uint8_t>(control & 0xFF), value};
  std::uint8_t rx[3] = {0, 0, 0};

  m_spi.transfer(tx, rx, sizeof(tx));
}

std::uint8_t ad9361::read_vendor_id() const {
  return read_register(REG_PRODUCT_ID) & PRODUCT_ID_MASK;
}

} // namespace drivers
