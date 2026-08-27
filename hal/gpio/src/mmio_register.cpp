/**
 * @file mmio_register.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-27
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "mmio_register.h"

#include <cstring>
#include <sstream>
#include <stdexcept>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace hal {

    mmio_register::mmio_register(std::uintptr_t physical_address)
        : m_map_base(nullptr), m_map_size(0), m_reg(nullptr) {
        int fd = ::open("/dev/mem", O_RDWR | O_SYNC);
        if (fd < 0) {
            throw std::runtime_error(std::string("mmio_register: failed to open /dev/mem: ") + std::strerror(errno));
        }

        std::uintptr_t page_size = static_cast<std::uintptr_t>(::sysconf(_SC_PAGESIZE));
        std::uintptr_t aligned = physical_address & ~(page_size - 1);
        std::uintptr_t offset = physical_address - aligned;

        void *map = ::mmap(nullptr, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, static_cast<off_t>(aligned));
        ::close(fd);

        if (map == MAP_FAILED) {
            std::ostringstream ss;
            ss << "mmio_register: mmap failed for physical address 0x" << std::hex << physical_address
               << ": " << std::strerror(errno);
            throw std::runtime_error(ss.str());
        }

        m_map_base = map;
        m_map_size = page_size;
        m_reg = reinterpret_cast<volatile std::uint32_t *>(reinterpret_cast<std::uint8_t *>(map) + offset);
    }

    mmio_register::~mmio_register() {
        if (m_map_base != nullptr) {
            ::munmap(m_map_base, m_map_size);
        }
    }

    std::uint32_t mmio_register::read() const {
        return *m_reg;
    }

    void mmio_register::write(std::uint32_t value) const {
        *m_reg = value;
    }

}
