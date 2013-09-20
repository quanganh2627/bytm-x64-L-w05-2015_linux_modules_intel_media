/**************************************************************************
 * Copyright (c) 2012, Intel Corporation.
 * All Rights Reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Javier Torres Castillo <javier.torres.castillo@intel.com>
 */

#if !defined DF_RGX_DEFS_H
#define DF_RGX_DEFS_H

#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/hrtimer.h>

/**
 * THERMAL_COOLING_DEVICE_MAX_STATE - The maximum cooling state that this
 * driver (as a thermal cooling device by reducing frequency) supports.
 */
#define THERMAL_COOLING_DEVICE_MAX_STATE	4
#define NUMBER_OF_LEVELS_B0 			8
#define NUMBER_OF_LEVELS			4

#define DF_RGX_FREQ_KHZ_MIN             200000
#define DF_RGX_FREQ_KHZ_MAX             533000

#define DF_RGX_FREQ_KHZ_MIN_INITIAL     DF_RGX_FREQ_KHZ_MIN

#define DF_RGX_FREQ_KHZ_MAX_INITIAL     320000

#define DF_RGX_INITIAL_FREQ_KHZ         320000

#define DF_RGX_THERMAL_LIMITED_FREQ_KHZ 200000

typedef enum _DFRGX_FREQ_
{
	DFRGX_FREQ_200_MHZ = 200000,
	DFRGX_FREQ_213_MHZ = 213000,
	DFRGX_FREQ_266_MHZ = 266000,
	DFRGX_FREQ_320_MHZ = 320000,
	DFRGX_FREQ_355_MHZ = 355000,
	DFRGX_FREQ_400_MHZ = 400000,
	DFRGX_FREQ_457_MHZ = 457000,
	DFRGX_FREQ_533_MHZ = 533000,
} DFRGX_FREQ;

typedef enum _DFRGX_BURST_MODE_
{
	DFRGX_NO_BURST_REQ	= 0, /* No need to perform burst/unburst*/
	DFRGX_BURST_REQ		= 1, /* Burst RGX*/
	DFRGX_UNBURST_REQ	= 2, /* Unburst RGX*/
} DFRGX_BURST_MODE;

struct gpu_util_stats {
	unsigned int				bValid;				/* if TRUE, statistict are valid, otherwise there was not enough data to calculate the ratios */
	unsigned int				ui32GpuStatActive;	/* GPU active  ratio expressed in 0,01% units */
	unsigned int				ui32GpuStatBlocked; /* GPU blocked ratio expressed in 0,01% units */
	unsigned int				ui32GpuStatIdle;    /* GPU idle    ratio expressed in 0,01% units */
};

/**
 * struct gpu_profiling_record - profiling information
 */
struct gpu_profiling_record{
	ktime_t		last_timestamp_ns;
	long long	time_ms;
};

struct gpu_utilization_record {
	unsigned long		freq;
	int			code;
	int			util_th_low;	/*lower limit utilization percentage, unburst it!*/
	int			util_th_high;	/*upper limit utilization percentage, burst it!*/
};

struct gpu_data {
	unsigned long int     freq_limit;
};

/**
 * struct df_rgx_data_s - dfrgx burst private data
 */
struct df_rgx_data_s {

	struct busfreq_data		*bus_freq_data;
	struct hrtimer          	g_timer;

	/* gbp_task - pointer to task structure for work thread or NULL. */
	struct task_struct     		*g_task;

	/* gbp_hrt_period - Period for timer interrupts as a ktime_t. */
	ktime_t                 	g_hrt_period;
	int                     	g_initialized;
	int                     	g_suspended;
	int                     	g_thread_check_utilization;


	/* gbp_enable - Usually 1.  If 0, gpu burst is disabled. */
	int                     	g_enable;
	int				g_profiling_enable;
	int                     	g_timer_is_enabled;

	struct mutex            	g_mutex_sts;
	unsigned long			g_recommended_freq_level;
	int 				gpu_utilization_record_index;
};


struct busfreq_data {
	struct df_rgx_data_s g_dfrgx_data;
	struct device        *dev;
	struct devfreq       *devfreq;
	struct notifier_block pm_notifier;
	struct mutex          lock;
	bool                  disabled;
	unsigned long int     bf_freq_mhz_rlzd;

	struct thermal_cooling_device *gbp_cooldv_hdl;
	int                   gbp_cooldv_state_cur;
	int                   gbp_cooldv_state_prev;
	int                   gbp_cooldv_state_highest;
	int                   gbp_cooldv_state_override;
	struct gpu_data	      gpudata[THERMAL_COOLING_DEVICE_MAX_STATE];
};

/*Available states - freq mapping table*/
const static struct gpu_utilization_record aAvailableStateFreq[] = {
									{DFRGX_FREQ_200_MHZ, 0xF, 0, 95},
									{DFRGX_FREQ_213_MHZ, 0xE, 85, 95}, /*Need a proper value for this freq*/
									{DFRGX_FREQ_266_MHZ, 0xB, 80, 95},
									{DFRGX_FREQ_320_MHZ, 0x9, 83, 95},
									{DFRGX_FREQ_355_MHZ, 0x8, 90, 95},
									{DFRGX_FREQ_400_MHZ, 0x7, 88, 95},
									{DFRGX_FREQ_457_MHZ, 0x6, 87, 95},
									{DFRGX_FREQ_533_MHZ, 0x5, 85, 95}
									};


unsigned int df_rgx_is_valid_freq(unsigned long int freq);
unsigned int df_rgx_request_burst(int *p_freq_table_index, int util_percentage);
int df_rgx_get_util_record_index_by_freq(unsigned long freq);
long set_desired_frequency_khz(struct busfreq_data *bfdata,
	unsigned long freq_khz);

#endif /*DF_RGX_DEFS_H*/
