/**
 * @file hal_spi.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "hal_spi.h"

#include <new>

#include "no_os_error.h"
#include "spi_device.h"

namespace {

    int32_t hal_spi_init(struct no_os_spi_desc **desc, const struct no_os_spi_init_param *param) {
        if (!desc || !param) {
            return -EINVAL;
        }

        hal::spi_config config;
        config.mode = static_cast<std::uint8_t>(param->mode);
        config.speed_hz = param->max_speed_hz;
        config.bits_per_word = 8;

        if (param->extra != nullptr) {
            const auto *extra = static_cast<const hal_spi_extra *>(param->extra);
            if (extra->device_path != nullptr) {
                config.device = extra->device_path;
            }
        }

        auto *descriptor = new (std::nothrow) no_os_spi_desc{};
        if (!descriptor) {
            return -ENOMEM;
        }

        hal::spi_device *spi = new (std::nothrow) hal::spi_device(config);
        if (!spi) {
            delete descriptor;
            return -ENOMEM;
        }

        descriptor->extra = spi;
        *desc = descriptor;

        return 0;
    }

    int32_t hal_spi_write_and_read(struct no_os_spi_desc *desc, uint8_t *data, uint16_t bytes_number) {
        if (!desc || !desc->extra || (!data && bytes_number != 0)) {
            return -EINVAL;
        }

        auto *spi = static_cast<hal::spi_device *>(desc->extra);
        if (!spi->transfer(data, data, bytes_number)) {
            return -EIO;
        }

        return 0;
    }

    int32_t hal_spi_remove(struct no_os_spi_desc *desc) {
        if (!desc) {
            return -EINVAL;
        }

        delete static_cast<hal::spi_device *>(desc->extra);
        delete desc;

        return 0;
    }

} // namespace

extern "C" const struct no_os_spi_platform_ops hal_spi_ops = {
    /* .init = */ &hal_spi_init,
    /* .write_and_read = */ &hal_spi_write_and_read,
    /* .transfer = */ nullptr,
    /* .transfer_dma = */ nullptr,
    /* .transfer_dma_async = */ nullptr,
    /* .remove = */ &hal_spi_remove,
    /* .transfer_abort = */ nullptr,
};
