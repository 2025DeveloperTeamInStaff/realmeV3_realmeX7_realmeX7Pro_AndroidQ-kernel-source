/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "gpio.h"
#ifdef VENDOR_EDIT
//Feiping.Li@Cam.Drv, 20200106, add for 19040 camera drvier porting
#include "soc/oppo/oppo_project.h"
#endif

#ifndef VENDOR_EDIT
#define VENDOR_EDIT
#endif

struct GPIO_PINCTRL gpio_pinctrl_list_cam[
			GPIO_CTRL_STATE_MAX_NUM_CAM] = {
	/* Main */
	{"pnd1"},
	{"pnd0"},
	{"rst1"},
	{"rst0"},
	{"ldo_vcama_1"},
	{"ldo_vcama_0"},
	{"ldo_vcamd_1"},
	{"ldo_vcamd_0"},
	{"ldo_vcamio_1"},
	{"ldo_vcamio_0"},
        #ifdef VENDOR_EDIT
        /* Feiping.Li@Cam.Drv, 20190912, driver porting */
	{"ldo_vcama1_1"},
	{"ldo_vcama1_0"},
        #endif
};

#ifdef MIPI_SWITCH
struct GPIO_PINCTRL gpio_pinctrl_list_switch[
			GPIO_CTRL_STATE_MAX_NUM_SWITCH] = {
	{"cam_mipi_switch_en_1"},
	{"cam_mipi_switch_en_0"},
	{"cam_mipi_switch_sel_1"},
	{"cam_mipi_switch_sel_0"}
};
#endif

#ifdef VENDOR_EDIT
/* Shipei.Chen@Cam.Drv, 20200727, modify for dali enable Fan53870*/
struct GPIO_PINCTRL gpio_pinctrl_list_ldo_enable[2] = {
	{"fan53870_chip_enable"},
	{"fan53870_chip_disable"}
};
#endif

static struct GPIO gpio_instance;

static enum IMGSENSOR_RETURN gpio_init(
	void *pinstance,
	struct IMGSENSOR_HW_DEVICE_COMMON *pcommon)
{
	int    i, j;
	struct GPIO            *pgpio            = (struct GPIO *)pinstance;
	enum   IMGSENSOR_RETURN ret              = IMGSENSOR_RETURN_SUCCESS;
	char str_pinctrl_name[LENGTH_FOR_SNPRINTF];
	char *lookup_names = NULL;

	pgpio->pgpio_mutex = &pcommon->pinctrl_mutex;

	pgpio->ppinctrl = devm_pinctrl_get(&pcommon->pplatform_device->dev);
	if (IS_ERR(pgpio->ppinctrl)) {
		PK_PR_ERR("%s : Cannot find camera pinctrl!", __func__);
		return IMGSENSOR_RETURN_ERROR;
	}

	for (j = IMGSENSOR_SENSOR_IDX_MIN_NUM;
		j < IMGSENSOR_SENSOR_IDX_MAX_NUM;
		j++) {
		for (i = 0 ; i < GPIO_CTRL_STATE_MAX_NUM_CAM; i++) {
			lookup_names =
			gpio_pinctrl_list_cam[i].ppinctrl_lookup_names;

			if (lookup_names) {
				snprintf(str_pinctrl_name,
				sizeof(str_pinctrl_name),
				"cam%d_%s",
				j,
				lookup_names);
				pgpio->ppinctrl_state_cam[j][i] =
					pinctrl_lookup_state(
						pgpio->ppinctrl,
						str_pinctrl_name);
			}

			if (pgpio->ppinctrl_state_cam[j][i] == NULL ||
				IS_ERR(pgpio->ppinctrl_state_cam[j][i])) {
				pr_info(
					"ERROR: %s : pinctrl err, %s\n",
					__func__,
					str_pinctrl_name);
				ret = IMGSENSOR_RETURN_ERROR;
			}
		}
	}

	#ifdef VENDOR_EDIT
	/* Shipei.Chen@Cam.Drv, 20200727, modify for dali enable Fan53870*/
	for (i=0; i<2; i++)
	if (is_project(OPPO_20601) || is_project(OPPO_20660) || is_project(OPPO_20605)) {
		if (gpio_pinctrl_list_ldo_enable[i].ppinctrl_lookup_names) {
			pgpio->pinctrl_state_ldo_enable[i] = pinctrl_lookup_state(
				pgpio->ppinctrl,
				gpio_pinctrl_list_ldo_enable[i].ppinctrl_lookup_names);
		}
		if (pgpio->pinctrl_state_ldo_enable[i] == NULL) {
			PK_PR_ERR("%s : pinctrl err, %s\n", __func__,
				gpio_pinctrl_list_ldo_enable[i].ppinctrl_lookup_names);
			ret = IMGSENSOR_RETURN_ERROR;
		}
	}
	#endif

#ifdef MIPI_SWITCH
	for (i = 0; i < GPIO_CTRL_STATE_MAX_NUM_SWITCH; i++) {
		if (gpio_pinctrl_list_switch[i].ppinctrl_lookup_names) {
			pgpio->ppinctrl_state_switch[i] =
				pinctrl_lookup_state(
					pgpio->ppinctrl,
			gpio_pinctrl_list_switch[i].ppinctrl_lookup_names);
		}

		if (pgpio->ppinctrl_state_switch[i] == NULL ||
			IS_ERR(pgpio->ppinctrl_state_switch[i])) {
			PK_PR_ERR("%s : pinctrl err, %s\n", __func__,
			gpio_pinctrl_list_switch[i].ppinctrl_lookup_names);
			ret = IMGSENSOR_RETURN_ERROR;
		}
	}
#endif

	return ret;
}

static enum IMGSENSOR_RETURN gpio_release(void *pinstance)
{
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN gpio_set(
	void *pinstance,
	enum IMGSENSOR_SENSOR_IDX   sensor_idx,
	enum IMGSENSOR_HW_PIN       pin,
	enum IMGSENSOR_HW_PIN_STATE pin_state)
{
	struct pinctrl_state  *ppinctrl_state;
	struct GPIO           *pgpio = (struct GPIO *)pinstance;
	enum   GPIO_STATE      gpio_state;

	/* PK_DBG("%s :debug pinctrl ENABLE, PinIdx %d, Val %d\n",
	 *	__func__, pin, pin_state);
	 */

#ifdef VENDOR_EDIT
	/* Shipei.Chen@Cam.Drv, 20200727, modify for dali enable Fan53870*/
	if (is_project(OPPO_20601) || is_project(OPPO_20660) || is_project(OPPO_20605)) {
		if (pin < IMGSENSOR_HW_PIN_PDN ||
			pin > IMGSENSOR_HW_PIN_FAN53870 ||
			pin_state < IMGSENSOR_HW_PIN_STATE_LEVEL_0 ||
			pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH)
			return IMGSENSOR_RETURN_ERROR;
	} else {
		if (pin < IMGSENSOR_HW_PIN_PDN ||
#ifdef MIPI_SWITCH
			pin > IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL ||
#else
			pin > IMGSENSOR_HW_PIN_AVDD_1 ||
#endif
			pin_state < IMGSENSOR_HW_PIN_STATE_LEVEL_0 ||
			pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH)
			return IMGSENSOR_RETURN_ERROR;
	}
#else /* VENDOR_EDIT */

	if (pin < IMGSENSOR_HW_PIN_PDN ||
#ifdef MIPI_SWITCH
	    pin > IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL ||
#else
	    // #ifdef VENDOR_EDIT
	    //Feiping.Li@Cam.Drv, 20200102, modify for porting 19040 camera sensor
	    pin > IMGSENSOR_HW_PIN_AVDD_1 ||
	    // #endif
#endif
	   pin_state < IMGSENSOR_HW_PIN_STATE_LEVEL_0 ||
	   pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH)
		return IMGSENSOR_RETURN_ERROR;
#endif /* VENDOR_EDIT */

	gpio_state = (pin_state > IMGSENSOR_HW_PIN_STATE_LEVEL_0)
		? GPIO_STATE_H : GPIO_STATE_L;

#ifdef MIPI_SWITCH
	if (pin == IMGSENSOR_HW_PIN_MIPI_SWITCH_EN)
		ppinctrl_state = pgpio->ppinctrl_state_switch[
			GPIO_CTRL_STATE_MIPI_SWITCH_EN_H + gpio_state];
	else if (pin == IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL)
		ppinctrl_state = pgpio->ppinctrl_state_switch[
			GPIO_CTRL_STATE_MIPI_SWITCH_SEL_H + gpio_state];
	else
#endif
	{
		#ifdef VENDOR_EDIT
		/* Shipei.Chen@Cam.Drv, 20200727, modify for dali enable Fan53870*/
		if ((pin == IMGSENSOR_HW_PIN_FAN53870) && (is_project(OPPO_20601) || is_project(OPPO_20660) || is_project(OPPO_20605))) {
			ppinctrl_state = pgpio->pinctrl_state_ldo_enable[gpio_state];
		} else {
			ppinctrl_state =
				pgpio->ppinctrl_state_cam[sensor_idx][
			    ((pin - IMGSENSOR_HW_PIN_PDN) << 1) + gpio_state];
		}
		#else
		ppinctrl_state =
			pgpio->ppinctrl_state_cam[sensor_idx][
			((pin - IMGSENSOR_HW_PIN_PDN) << 1) + gpio_state];
		#endif

	}

	mutex_lock(pgpio->pgpio_mutex);

	if (ppinctrl_state != NULL && !IS_ERR(ppinctrl_state))
		pinctrl_select_state(pgpio->ppinctrl, ppinctrl_state);
	else
		PK_PR_ERR("%s : pinctrl err, PinIdx %d, Val %d\n",
			__func__, pin, pin_state);

	mutex_unlock(pgpio->pgpio_mutex);

	return IMGSENSOR_RETURN_SUCCESS;
}

static struct IMGSENSOR_HW_DEVICE device = {
	.id        = IMGSENSOR_HW_ID_GPIO,
	.pinstance = (void *)&gpio_instance,
	.init      = gpio_init,
	.set       = gpio_set,
	.release   = gpio_release
};

enum IMGSENSOR_RETURN imgsensor_hw_gpio_open(
	struct IMGSENSOR_HW_DEVICE **pdevice)
{
	*pdevice = &device;
	return IMGSENSOR_RETURN_SUCCESS;
}

