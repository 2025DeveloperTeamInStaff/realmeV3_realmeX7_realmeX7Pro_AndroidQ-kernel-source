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

#include <linux/delay.h>
#include <linux/string.h>

#include "kd_camera_typedef.h"
#include "kd_camera_feature.h"

#include "imgsensor_sensor.h"
#include "imgsensor_hw.h"

#ifdef VENDOR_EDIT
//Feiping.Li@Cam.Drv, 20200106, add for 19040 camera drvier porting
#include "soc/oppo/oppo_project.h"
#endif
#ifdef CONFIG_MACH_MT6885
/* Shipei.Chen@Cam.Drv, 20200727, modify for dali enable Fan53870*/
int camera_status = 0;
extern int is_check_lcm_status_ana6705(void);
extern int is_check_lcm_status_sofe03f(void);
#endif
enum IMGSENSOR_RETURN imgsensor_hw_init(struct IMGSENSOR_HW *phw)
{
	struct IMGSENSOR_HW_SENSOR_POWER      *psensor_pwr;
	struct IMGSENSOR_HW_CFG               *pcust_pwr_cfg;
	struct IMGSENSOR_HW_CUSTOM_POWER_INFO *ppwr_info;
	int i, j;
	char str_prop_name[LENGTH_FOR_SNPRINTF];
	struct device_node *of_node
		= of_find_compatible_node(NULL, NULL, "mediatek,imgsensor");

	mutex_init(&phw->common.pinctrl_mutex);

	for (i = 0; i < IMGSENSOR_HW_ID_MAX_NUM; i++) {
		if (hw_open[i] != NULL)
			(hw_open[i]) (&phw->pdev[i]);

		if (phw->pdev[i]->init != NULL)
			(phw->pdev[i]->init)(
				phw->pdev[i]->pinstance, &phw->common);
	}

	for (i = 0; i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++) {
		psensor_pwr = &phw->sensor_pwr[i];

		#ifdef VENDOR_EDIT
		//Feiping.Li@Cam.Drv, 20200106, add for 19040 camera drvier porting
		//wenhui.chen@Cam.Drv, 20200402, add for porting 20601 sensor
		/* Shipei.Chen@Cam.Drv, 20200502,  sensor porting!*/
		if (is_project(OPPO_19040) || is_project(OPPO_20601) || is_project(OPPO_20660)
		|| is_project(OPPO_20611)|| is_project(OPPO_20610) || is_project(OPPO_20613)||is_project(OPPO_20605)
		|| is_project(OPPO_20680)) {
			pcust_pwr_cfg = imgsensor_custom_config_19040;
		} else if (is_project(OPPO_20630)|| is_project(OPPO_20631)|| is_project(OPPO_20632)|| is_project(OPPO_20633)|| is_project(OPPO_20634)|| is_project(OPPO_20635)|| is_project(OPPO_206B4)){
			pcust_pwr_cfg = imgsensor_custom_config_20630;
		} else if (is_project(OPPO_20625)|| is_project(OPPO_20626)){
			pcust_pwr_cfg = imgsensor_custom_config_20625;
		} else
		{
			pcust_pwr_cfg = imgsensor_custom_config;
		}
		#else
		pcust_pwr_cfg = imgsensor_custom_config;
		#endif
		while (pcust_pwr_cfg->sensor_idx != i &&
		       pcust_pwr_cfg->sensor_idx != IMGSENSOR_SENSOR_IDX_NONE)
			pcust_pwr_cfg++;

		if (pcust_pwr_cfg->sensor_idx == IMGSENSOR_SENSOR_IDX_NONE)
			continue;

		ppwr_info = pcust_pwr_cfg->pwr_info;
		while (ppwr_info->pin != IMGSENSOR_HW_PIN_NONE) {
			for (j = 0;
				j < IMGSENSOR_HW_ID_MAX_NUM &&
					ppwr_info->id != phw->pdev[j]->id;
				j++) {
			}

			psensor_pwr->id[ppwr_info->pin] = j;
			ppwr_info++;
		}
	}

	for (i = 0; i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++) {
		memset(str_prop_name, 0, sizeof(str_prop_name));
		snprintf(str_prop_name,
					sizeof(str_prop_name),
					"cam%d_%s",
					i,
					"enable_sensor");
		if (of_property_read_string(
			of_node, str_prop_name,
			&phw->enable_sensor_by_index[i]) < 0) {
			PK_DBG("Property cust-sensor not defined\n");
			phw->enable_sensor_by_index[i] = NULL;
		}
	}

	return IMGSENSOR_RETURN_SUCCESS;
}

enum IMGSENSOR_RETURN imgsensor_hw_release_all(struct IMGSENSOR_HW *phw)
{
	int i;

	for (i = 0; i < IMGSENSOR_HW_ID_MAX_NUM; i++) {
		if (phw->pdev[i]->release != NULL)
			(phw->pdev[i]->release)(phw->pdev[i]->pinstance);
	}
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN imgsensor_hw_power_sequence(
		struct IMGSENSOR_HW             *phw,
		enum   IMGSENSOR_SENSOR_IDX      sensor_idx,
		enum   IMGSENSOR_HW_POWER_STATUS pwr_status,
		struct IMGSENSOR_HW_POWER_SEQ   *ppower_sequence,
		char *pcurr_idx)
{
	struct IMGSENSOR_HW_SENSOR_POWER *psensor_pwr =
					&phw->sensor_pwr[sensor_idx];
	struct IMGSENSOR_HW_POWER_SEQ    *ppwr_seq = ppower_sequence;
	struct IMGSENSOR_HW_POWER_INFO   *ppwr_info;
	struct IMGSENSOR_HW_DEVICE       *pdev;
	int                               pin_cnt = 0;

	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 30);

#ifdef CONFIG_FPGA_EARLY_PORTING  /*for FPGA*/
	if (1) {
		PK_DBG("FPGA return true for power control\n");
		return IMGSENSOR_RETURN_SUCCESS;
	}
#endif

	while (ppwr_seq < ppower_sequence + IMGSENSOR_HW_SENSOR_MAX_NUM &&
		ppwr_seq->name != NULL) {
		if (!strcmp(ppwr_seq->name, PLATFORM_POWER_SEQ_NAME)) {
			if (sensor_idx == ppwr_seq->_idx)
				break;
		} else {
			if (!strcmp(ppwr_seq->name, pcurr_idx))
				break;
		}
		ppwr_seq++;
	}

	if (ppwr_seq->name == NULL)
		return IMGSENSOR_RETURN_ERROR;

	ppwr_info = ppwr_seq->pwr_info;

    #ifdef CONFIG_MACH_MT6885
	/* Shipei.Chen@Cam.Drv, 20200727, modify for dali enable Fan53870*/
	if ((pwr_status == IMGSENSOR_HW_POWER_STATUS_ON) && (is_project(OPPO_20601) || is_project(OPPO_20660) || is_project(OPPO_20605))) {
		if((is_check_lcm_status_sofe03f() && is_check_lcm_status_ana6705()) || camera_status == 1) {
			printk("camera enable: the gpiod_set_value is 1, last_camera_status%d, lcm_status%d\n", camera_status, (is_check_lcm_status_sofe03f() && is_check_lcm_status_ana6705()));
		} else {
			pdev = phw->pdev[IMGSENSOR_HW_ID_GPIO];
			if (pdev->set != NULL) {
				printk("camera enable: the gpiod_set_value is 1, last_camera_status%d, lcm_status%d\n", camera_status, (is_check_lcm_status_sofe03f() && is_check_lcm_status_ana6705()));
				pdev->set(pdev->pinstance, IMGSENSOR_SENSOR_IDX_MAIN, IMGSENSOR_HW_PIN_FAN53870, Vol_High);
			}
		}
		camera_status = 1;
	}
	#endif
	while (ppwr_info->pin != IMGSENSOR_HW_PIN_NONE &&
	       ppwr_info < ppwr_seq->pwr_info + IMGSENSOR_HW_POWER_INFO_MAX) {

		if (pwr_status == IMGSENSOR_HW_POWER_STATUS_ON) {
			if (ppwr_info->pin != IMGSENSOR_HW_PIN_UNDEF) {
				pdev =
				phw->pdev[psensor_pwr->id[ppwr_info->pin]];

				if (__ratelimit(&ratelimit))
					PK_DBG(
					"sensor_idx %d, ppwr_info->pin %d, ppwr_info->pin_state_on %d",
					sensor_idx,
					ppwr_info->pin,
					ppwr_info->pin_state_on);

				if (pdev->set != NULL)
					pdev->set(pdev->pinstance,
					sensor_idx,
				    ppwr_info->pin, ppwr_info->pin_state_on);
			}

			mdelay(ppwr_info->pin_on_delay);
		}

		ppwr_info++;
		pin_cnt++;
	}

	if (pwr_status == IMGSENSOR_HW_POWER_STATUS_OFF) {
		while (pin_cnt) {
			ppwr_info--;
			pin_cnt--;

			if (__ratelimit(&ratelimit))
				PK_DBG(
				"sensor_idx %d, ppwr_info->pin %d, ppwr_info->pin_state_off %d",
				sensor_idx,
				ppwr_info->pin,
				ppwr_info->pin_state_off);

			if (ppwr_info->pin != IMGSENSOR_HW_PIN_UNDEF) {
				pdev =
				phw->pdev[psensor_pwr->id[ppwr_info->pin]];

				if (pdev->set != NULL)
					pdev->set(pdev->pinstance,
					sensor_idx,
				ppwr_info->pin, ppwr_info->pin_state_off);
			}
			#ifdef VENDOR_EDIT
			// Wenjun.Wu@Camera.Drv, 20200204, add for add avdd more delay time
			if (is_project(OPPO_19131) || is_project(OPPO_19132) || is_project(OPPO_19133) || is_project(OPPO_19420)) {
				PK_DBG("19131 avdd need more delay time");
				mdelay(ppwr_info->pin_off_delay);
			} else {
				mdelay(ppwr_info->pin_on_delay);
			}
			#else
			mdelay(ppwr_info->pin_on_delay);
			#endif
		}
	}
    #ifdef CONFIG_MACH_MT6885
	/* Shipei.Chen@Cam.Drv, 20200727, modify for dali enable Fan53870*/
	if ((pwr_status == IMGSENSOR_HW_POWER_STATUS_OFF) && (is_project(OPPO_20601) || is_project(OPPO_20660) || is_project(OPPO_20605))) {
		if((is_check_lcm_status_sofe03f() && is_check_lcm_status_ana6705())) {
			printk("camera disable: the gpiod_set_value is 1, last_camera_status%d, lcm_status%d\n", camera_status, (is_check_lcm_status_sofe03f() && is_check_lcm_status_ana6705()));
		} else if(camera_status == 0) {
			printk("camera disable: the gpiod_set_value is 0, last_camera_status%d, lcm_status%d\n", camera_status, (is_check_lcm_status_sofe03f() && is_check_lcm_status_ana6705()));
		} else {
			pdev = phw->pdev[IMGSENSOR_HW_ID_GPIO];
			if (pdev->set != NULL) {
				printk("camera disable: the gpiod_set_value is 0, camera_status%d, lcm_status%d\n", camera_status, (is_check_lcm_status_sofe03f() && is_check_lcm_status_ana6705()));
				pdev->set(pdev->pinstance, IMGSENSOR_SENSOR_IDX_MAIN, IMGSENSOR_HW_PIN_FAN53870, Vol_Low);
			}
		}
		camera_status = 0;
	}
	#endif
	return IMGSENSOR_RETURN_SUCCESS;
}
#ifdef CONFIG_MACH_MT6885
/* Shipei.Chen@Cam.Drv, 20200727, modify for dali enable Fan53870*/
int is_check_camera_status(void)
{
	printk("For checking camera_status: %d \n", camera_status);
	return camera_status;
}
EXPORT_SYMBOL(is_check_camera_status);
#endif
enum IMGSENSOR_RETURN imgsensor_hw_power(
		struct IMGSENSOR_HW *phw,
		struct IMGSENSOR_SENSOR *psensor,
		enum IMGSENSOR_HW_POWER_STATUS pwr_status)
{
	enum IMGSENSOR_SENSOR_IDX sensor_idx = psensor->inst.sensor_idx;
	char *curr_sensor_name = psensor->inst.psensor_list->name;
	char str_index[LENGTH_FOR_SNPRINTF];

	PK_DBG("sensor_idx %d, power %d curr_sensor_name %s, enable list %s\n",
		sensor_idx,
		pwr_status,
		curr_sensor_name,
		phw->enable_sensor_by_index[sensor_idx] == NULL
		? "NULL"
		: phw->enable_sensor_by_index[sensor_idx]);
	#ifdef VENDOR_EDIT
	/* Shipei.Chen@Cam.Drv, 20200525,  modify for compare fail!*/
	/* Shengguang.Zhu@Cam.Drv, 20200722,  modify for distinguish athens-B && athens-C!*/
	if (is_project(OPPO_20630)|| is_project(OPPO_20631)|| is_project(OPPO_20632)
		|| is_project(OPPO_20633)|| is_project(OPPO_20634)|| is_project(OPPO_20635)|| is_project(OPPO_206B4)) {
		if (sensor_idx == 0) {
			char *main_a = "s5kgw3_mipi_raw";
			char *main_b = "athensc_s5kgm1st_mipi_raw";
			if((strcmp(main_a, curr_sensor_name) == 0)&&(is_project(OPPO_20630)|| is_project(OPPO_20631)|| is_project(OPPO_20632)|| is_project(OPPO_206B4))) {
			    if (phw->enable_sensor_by_index[sensor_idx] && strcmp(main_a, curr_sensor_name))
				return IMGSENSOR_RETURN_ERROR;
			} else if ((strcmp(main_b, curr_sensor_name) == 0)&&(is_project(OPPO_20633)|| is_project(OPPO_20634)|| is_project(OPPO_20635))) {
			    if (phw->enable_sensor_by_index[sensor_idx] && strcmp(main_b, curr_sensor_name))
			    return IMGSENSOR_RETURN_ERROR;
			} else {
			    return IMGSENSOR_RETURN_ERROR;
			}
		} else if (sensor_idx == 1) {
			char *sub_a = "athensb_ov32a1q_mipi_raw";
			char *sub_b = "athensc_imx471_mipi_raw";
			if((strcmp(sub_a, curr_sensor_name) == 0)&&(is_project(OPPO_20630)|| is_project(OPPO_20631)|| is_project(OPPO_20632)|| is_project(OPPO_206B4))) {
			    if (phw->enable_sensor_by_index[sensor_idx] && strcmp(sub_a, curr_sensor_name))
				return IMGSENSOR_RETURN_ERROR;
			}else if ((strcmp(sub_b, curr_sensor_name) == 0)&&(is_project(OPPO_20633)|| is_project(OPPO_20634)|| is_project(OPPO_20635))) {
			    if (phw->enable_sensor_by_index[sensor_idx] && strcmp(sub_b, curr_sensor_name))
			    return IMGSENSOR_RETURN_ERROR;
			}else{
			    return IMGSENSOR_RETURN_ERROR;
			}
		} else {
			if (phw->enable_sensor_by_index[sensor_idx] && strcmp(phw->enable_sensor_by_index[sensor_idx], curr_sensor_name))
			return IMGSENSOR_RETURN_ERROR;
		}
	} else {
		if (phw->enable_sensor_by_index[sensor_idx] &&
		!strstr(phw->enable_sensor_by_index[sensor_idx], curr_sensor_name))
			return IMGSENSOR_RETURN_ERROR;
	}
	#else
	if (phw->enable_sensor_by_index[sensor_idx] &&
	!strstr(phw->enable_sensor_by_index[sensor_idx], curr_sensor_name))
		return IMGSENSOR_RETURN_ERROR;
	#endif

	snprintf(str_index, sizeof(str_index), "%d", sensor_idx);
	imgsensor_hw_power_sequence(
			phw,
			sensor_idx,
			pwr_status,
			platform_power_sequence,
			str_index);

	imgsensor_hw_power_sequence(
			phw,
			sensor_idx,
			pwr_status, sensor_power_sequence, curr_sensor_name);

	return IMGSENSOR_RETURN_SUCCESS;
}

enum IMGSENSOR_RETURN imgsensor_hw_dump(struct IMGSENSOR_HW *phw)
{
	int i;

	for (i = 0; i < IMGSENSOR_HW_ID_MAX_NUM; i++) {
		if (phw->pdev[i]->dump != NULL)
			(phw->pdev[i]->dump)(phw->pdev[i]->pinstance);
	}
	return IMGSENSOR_RETURN_SUCCESS;
}

