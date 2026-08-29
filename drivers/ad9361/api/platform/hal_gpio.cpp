/**
 * @file hal_gpio.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "hal_gpio.h"

#include <new>

#include "mmio_register.h"
#include "no_os_error.h"

namespace {

    struct hal_gpio_desc_extra {
        hal::mmio_register *reg;
        std::uint8_t bit;
    };

    int32_t hal_gpio_get(struct no_os_gpio_desc **desc, const struct no_os_gpio_init_param *param) {
        if (!desc || !param || param->port < 0 || param->number < 0 || param->number > 31) {
            return -EINVAL;
        }

        auto *extra = new (std::nothrow) hal_gpio_desc_extra{};
        if (!extra) {
            return -ENOMEM;
        }

        extra->reg = new (std::nothrow) hal::mmio_register(static_cast<std::uintptr_t>(param->port));
        if (!extra->reg) {
            delete extra;
            return -ENOMEM;
        }
        extra->bit = static_cast<std::uint8_t>(param->number);

        auto *descriptor = new (std::nothrow) no_os_gpio_desc{};
        if (!descriptor) {
            delete extra->reg;
            delete extra;
            return -ENOMEM;
        }

        descriptor->port = param->port;
        descriptor->number = param->number;
        descriptor->pull = param->pull;
        descriptor->extra = extra;

        *desc = descriptor;

        return 0;
    }

    int32_t hal_gpio_get_optional(struct no_os_gpio_desc **desc, const struct no_os_gpio_init_param *param) {
        return hal_gpio_get(desc, param);
    }

    int32_t hal_gpio_remove(struct no_os_gpio_desc *desc) {
        if (!desc) {
            return -EINVAL;
        }

        auto *extra = static_cast<hal_gpio_desc_extra *>(desc->extra);
        if (extra) {
            delete extra->reg;
            delete extra;
        }
        delete desc;

        return 0;
    }

    int32_t hal_gpio_direction_input(struct no_os_gpio_desc * /*desc*/) {
        return -ENOSYS;
    }

    int32_t hal_gpio_set_value(struct no_os_gpio_desc *desc, uint8_t value) {
        if (!desc || !desc->extra) {
            return -EINVAL;
        }

        auto *extra = static_cast<hal_gpio_desc_extra *>(desc->extra);
        std::uint32_t word = extra->reg->read();
        if (!extra->reg->ok()) {
            return -EIO;
        }
        std::uint32_t mask = 1u << extra->bit;

        if (value != 0) {
            word |= mask;
        } else {
            word &= ~mask;
        }

        extra->reg->write(word);

        return 0;
    }

    int32_t hal_gpio_direction_output(struct no_os_gpio_desc *desc, uint8_t value) {
        return hal_gpio_set_value(desc, value);
    }

    int32_t hal_gpio_get_direction(struct no_os_gpio_desc *desc, uint8_t *direction) {
        if (!desc || !direction) {
            return -EINVAL;
        }

        *direction = NO_OS_GPIO_OUT;

        return 0;
    }

    int32_t hal_gpio_get_value(struct no_os_gpio_desc *desc, uint8_t *value) {
        if (!desc || !desc->extra || !value) {
            return -EINVAL;
        }

        auto *extra = static_cast<hal_gpio_desc_extra *>(desc->extra);
        std::uint32_t word = extra->reg->read();
        if (!extra->reg->ok()) {
            return -EIO;
        }

        *value = ((word >> extra->bit) & 0x1u) ? NO_OS_GPIO_HIGH : NO_OS_GPIO_LOW;

        return 0;
    }

} // namespace

extern "C" const struct no_os_gpio_platform_ops hal_gpio_ops = {
    /* .gpio_ops_get = */ &hal_gpio_get,
    /* .gpio_ops_get_optional = */ &hal_gpio_get_optional,
    /* .gpio_ops_remove = */ &hal_gpio_remove,
    /* .gpio_ops_direction_input = */ &hal_gpio_direction_input,
    /* .gpio_ops_direction_output = */ &hal_gpio_direction_output,
    /* .gpio_ops_get_direction = */ &hal_gpio_get_direction,
    /* .gpio_ops_set_value = */ &hal_gpio_set_value,
    /* .gpio_ops_get_value = */ &hal_gpio_get_value,
};
