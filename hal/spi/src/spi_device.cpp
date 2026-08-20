/**
 * @file spi_device.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "spi_device.h"

#include <cstring>
#include <stdexcept>

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace hal {

    spi_device::spi_device(const spi_config &config)
        : m_path(config.device), m_fd(-1), m_mode(config.mode),
          m_bits_per_word(config.bits_per_word), m_speed_hz(config.speed_hz) {
    }

    spi_device::~spi_device() {
        if (m_fd >= 0) {
            ::close(m_fd);
        }
    }

    void spi_device::ensure_open() const {
        if (m_fd >= 0) {
            return;
        }

        m_fd = ::open(m_path.c_str(), O_RDWR);
        if (m_fd < 0) {
            throw std::runtime_error("spi_device: failed to open " + m_path + ": " + std::strerror(errno));
        }

        if (::ioctl(m_fd, SPI_IOC_WR_MODE, &m_mode) < 0) {
            ::close(m_fd);
            m_fd = -1;
            throw std::runtime_error("spi_device: failed to set mode on " + m_path + ": " + std::strerror(errno));
        }

        if (::ioctl(m_fd, SPI_IOC_WR_BITS_PER_WORD, &m_bits_per_word) < 0) {
            ::close(m_fd);
            m_fd = -1;
            throw std::runtime_error("spi_device: failed to set bits per word on " + m_path + ": " + std::strerror(errno));
        }

        if (::ioctl(m_fd, SPI_IOC_WR_MAX_SPEED_HZ, &m_speed_hz) < 0) {
            ::close(m_fd);
            m_fd = -1;
            throw std::runtime_error("spi_device: failed to set speed on " + m_path + ": " + std::strerror(errno));
        }
    }

    bool spi_device::transfer(const std::uint8_t *tx, std::uint8_t *rx, std::size_t len) const {
        ensure_open();

        struct spi_ioc_transfer tr;
        std::memset(&tr, 0, sizeof(tr));

        tr.tx_buf = reinterpret_cast<std::uint64_t>(tx);
        tr.rx_buf = reinterpret_cast<std::uint64_t>(rx);
        tr.len = static_cast<std::uint32_t>(len);
        tr.speed_hz = m_speed_hz;
        tr.bits_per_word = m_bits_per_word;

        return ::ioctl(m_fd, SPI_IOC_MESSAGE(1), &tr) >= 0;
    }

}
