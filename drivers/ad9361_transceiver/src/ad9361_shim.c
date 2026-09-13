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

int32_t ad9361_transceiver_shim_get_tx_lo_freq(void *phy, uint64_t *lo_freq_hz)
{
	return ad9361_get_tx_lo_freq((struct ad9361_rf_phy *)phy, lo_freq_hz);
}

int32_t ad9361_transceiver_shim_set_tx_attenuation(void *phy, uint8_t ch, uint32_t attenuation_mdb)
{
	return ad9361_set_tx_attenuation((struct ad9361_rf_phy *)phy, ch, attenuation_mdb);
}

int32_t ad9361_transceiver_shim_get_tx_attenuation(void *phy, uint8_t ch, uint32_t *attenuation_mdb)
{
	return ad9361_get_tx_attenuation((struct ad9361_rf_phy *)phy, ch, attenuation_mdb);
}

int32_t ad9361_transceiver_shim_set_rx_gain_control_mode(void *phy, uint8_t ch, uint8_t gc_mode)
{
	return ad9361_set_rx_gain_control_mode((struct ad9361_rf_phy *)phy, ch, gc_mode);
}

int32_t ad9361_transceiver_shim_enable_tx(void *phy)
{
	return ad9361_set_en_state_machine_mode((struct ad9361_rf_phy *)phy, ENSM_MODE_FDD);
}

int32_t ad9361_transceiver_shim_disable_tx(void *phy)
{
	return ad9361_set_en_state_machine_mode((struct ad9361_rf_phy *)phy, ENSM_MODE_ALERT);
}

extern uint8_t ad9361_ensm_get_state(struct ad9361_rf_phy *phy);

int32_t ad9361_transceiver_shim_get_ensm_state(void *phy, uint8_t *state)
{
	*state = ad9361_ensm_get_state((struct ad9361_rf_phy *)phy);
	return 0;
}

extern int32_t ad9361_set_tx_clock_data_delay(struct ad9361_rf_phy *phy,
					       uint8_t fb_clk_delay, uint8_t tx_data_delay);
extern int32_t ad9361_get_tx_clock_data_delay(struct ad9361_rf_phy *phy,
					       uint8_t *fb_clk_delay, uint8_t *tx_data_delay);

int32_t ad9361_transceiver_shim_set_tx_clock_data_delay(void *phy, uint8_t fb_clk_delay, uint8_t tx_data_delay)
{
	return ad9361_set_tx_clock_data_delay((struct ad9361_rf_phy *)phy, fb_clk_delay, tx_data_delay);
}

int32_t ad9361_transceiver_shim_get_tx_clock_data_delay(void *phy, uint8_t *fb_clk_delay, uint8_t *tx_data_delay)
{
	return ad9361_get_tx_clock_data_delay((struct ad9361_rf_phy *)phy, fb_clk_delay, tx_data_delay);
}

#define AD9361_SHIM_TX_QUAD_CAL (1 << 4)

int32_t ad9361_transceiver_shim_calibrate_tx_quad(void *phy)
{
	return ad9361_do_calib((struct ad9361_rf_phy *)phy, AD9361_SHIM_TX_QUAD_CAL, -1);
}

int32_t ad9361_transceiver_shim_set_tx_sampling_freq(void *phy, uint32_t sampling_freq_hz)
{
	return ad9361_set_tx_sampling_freq((struct ad9361_rf_phy *)phy, sampling_freq_hz);
}

int32_t ad9361_transceiver_shim_get_tx_sampling_freq(void *phy, uint32_t *sampling_freq_hz)
{
	return ad9361_get_tx_sampling_freq((struct ad9361_rf_phy *)phy, sampling_freq_hz);
}

extern int32_t ad9361_set_lvds_invert(struct ad9361_rf_phy *phy, uint8_t ctrl1, uint8_t ctrl2);
extern int32_t ad9361_get_lvds_invert(struct ad9361_rf_phy *phy, uint8_t *ctrl1, uint8_t *ctrl2);

int32_t ad9361_transceiver_shim_set_lvds_invert(void *phy, uint8_t ctrl1, uint8_t ctrl2)
{
	return ad9361_set_lvds_invert((struct ad9361_rf_phy *)phy, ctrl1, ctrl2);
}

int32_t ad9361_transceiver_shim_get_lvds_invert(void *phy, uint8_t *ctrl1, uint8_t *ctrl2)
{
	return ad9361_get_lvds_invert((struct ad9361_rf_phy *)phy, ctrl1, ctrl2);
}

int32_t ad9361_transceiver_shim_bist_tone(void *phy, int32_t mode, uint32_t freq_hz, uint32_t level_db,
                                           uint32_t mask)
{
	return ad9361_bist_tone((struct ad9361_rf_phy *)phy, (enum ad9361_bist_mode)mode, freq_hz, level_db, mask);
}

int32_t ad9361_transceiver_shim_bist_prbs(void *phy, int32_t mode)
{
	return ad9361_bist_prbs((struct ad9361_rf_phy *)phy, (enum ad9361_bist_mode)mode);
}

int32_t ad9361_transceiver_shim_bist_loopback(void *phy, int32_t mode)
{
	return ad9361_bist_loopback((struct ad9361_rf_phy *)phy, mode);
}
