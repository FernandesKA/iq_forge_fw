/**
 * @file ad9361_conv_stub.c
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "ad9361.h"
#include "no_os_error.h"

int32_t ad9361_hdl_loopback(struct ad9361_rf_phy *phy, bool enable)
{
	(void)phy;
	(void)enable;
	return -ENODEV;
}

int32_t ad9361_dig_tune(struct ad9361_rf_phy *phy, uint32_t max_freq,
			 enum dig_tune_flags flags)
{
	(void)phy;
	(void)max_freq;
	(void)flags;
	return 0;
}

int32_t ad9361_post_setup(struct ad9361_rf_phy *phy)
{
	(void)phy;
	return 0;
}
