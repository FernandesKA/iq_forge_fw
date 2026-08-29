/**
 * @file ad9361_shim.c
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "ad9361_shim.h"

#include <stddef.h>

#include "ad9361_api.h"
#include "no_os_gpio.h"
#include "no_os_spi.h"

#include "hal_gpio.h"
#include "hal_spi.h"

extern const AD9361_InitParam ad9361_default_init_param;

#define AD9361_CTRL_GPIO_RESETB_BIT 0

int32_t ad9361_transceiver_shim_init(void **out_phy,
				      const char *spi_device_path,
				      uint32_t spi_speed_hz,
				      uint8_t spi_mode,
				      int32_t ctrl_gpio_port,
				      int has_ctrl_gpio)
{
	struct hal_spi_extra spi_extra = {
		.device_path = spi_device_path,
	};

	struct no_os_spi_init_param spi_param = {
		.device_id = 0,
		.max_speed_hz = spi_speed_hz,
		.chip_select = 0,
		.mode = (enum no_os_spi_mode)spi_mode,
		.platform_ops = &hal_spi_ops,
		.extra = &spi_extra,
	};

	struct no_os_gpio_init_param gpio_resetb_param = {
		.number = -1,
	};
	if (has_ctrl_gpio) {
		gpio_resetb_param.port = ctrl_gpio_port;
		gpio_resetb_param.number = AD9361_CTRL_GPIO_RESETB_BIT;
		gpio_resetb_param.platform_ops = &hal_gpio_ops;
	}

	struct no_os_gpio_init_param gpio_unused_param = {
		.number = -1,
	};

	AD9361_InitParam init_param = ad9361_default_init_param;
	init_param.gpio_resetb = gpio_resetb_param;
	init_param.gpio_sync = gpio_unused_param;
	init_param.gpio_cal_sw1 = gpio_unused_param;
	init_param.gpio_cal_sw2 = gpio_unused_param;
	init_param.spi_param = spi_param;

	struct ad9361_rf_phy *phy = NULL;
	int32_t ret = ad9361_init(&phy, &init_param);
	if (ret < 0)
		return ret;

	*out_phy = phy;
	return 0;
}

void ad9361_transceiver_shim_remove(void *phy)
{
	if (phy)
		ad9361_remove((struct ad9361_rf_phy *)phy);
}

int32_t ad9361_transceiver_shim_set_tx_lo_freq(void *phy, uint64_t lo_freq_hz)
{
	return ad9361_set_tx_lo_freq((struct ad9361_rf_phy *)phy, lo_freq_hz);
}

int32_t ad9361_transceiver_shim_set_rx_gain_control_mode(void *phy, uint8_t ch, uint8_t gc_mode)
{
	return ad9361_set_rx_gain_control_mode((struct ad9361_rf_phy *)phy, ch, gc_mode);
}

int32_t ad9361_transceiver_shim_enable_tx(void *phy)
{
	return ad9361_set_en_state_machine_mode((struct ad9361_rf_phy *)phy, ENSM_MODE_FDD);
}
