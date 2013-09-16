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
 *    Dale B. Stimson <dale.b.stimson@intel.com>
 *    Jari Luoma-aho  <jari.luoma-aho@intel.com>
 *    Jari Nippula    <jari.nippula@intel.com>
 *    Javier Torres Castillo <javier.torres.castillo@intel.com>
 */

#include <linux/devfreq.h>
#include <ospm/gfx_freq.h>
#include <dfrgx_utilstats.h>
#include "dev_freq_debug.h"
#include "df_rgx_defs.h"
#include "df_rgx_burst.h"
#include "dfrgx_interface.h"

#define MAX_NUM_SAMPLES		10

/**
 * set_desired_frequency_khz() - Set gpu frequency.
 * @bfdata: Pointer to private data structure
 * @freq_khz: Desired frequency in KHz (not MHz).
 * Returns: <0 if error, 0 if success, but no frequency update, or
 * realized frequency in KHz.
 */
long set_desired_frequency_khz(struct busfreq_data *bfdata,
	unsigned long freq_khz)
{
	int sts;
	struct devfreq *df;
	unsigned long freq_req;
	unsigned long freq_limited;
	unsigned long freq_mhz;
	unsigned int freq_mhz_quantized;
	u32 freq_code;
	int thermal_state;

	sts = 0;

	/* Warning - this function may be called from devfreq_add_device,
	 * but if it is, bfdata->devfreq will not yet be set.
	 */
	df = bfdata->devfreq;

	if (!df) {
	    /*
	     * Initial call, so set initial frequency.	Limits from min_freq
	     * and max_freq would not have been applied by caller.
	     */
	    freq_req = DF_RGX_INITIAL_FREQ_KHZ;
	}
	else if ((freq_khz == 0) && df->previous_freq)
		freq_req = df->previous_freq;
	else
		freq_req = freq_khz;

	DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s: entry, caller requesting %luKHz\n",
		__func__, freq_khz);

	if (freq_req < DF_RGX_FREQ_KHZ_MIN)
		freq_limited = DF_RGX_FREQ_KHZ_MIN;
	else if (freq_req > DF_RGX_FREQ_KHZ_MAX)
		freq_limited = DF_RGX_FREQ_KHZ_MAX;
	else
		freq_limited = freq_req;

	if (df && (freq_limited == df->previous_freq))
		return df->previous_freq;

	freq_mhz = freq_limited / 1000;

	mutex_lock(&bfdata->lock);

	if (bfdata->disabled)
		goto out;

	freq_code = gpu_freq_mhz_to_code(freq_mhz, &freq_mhz_quantized);

	if (bfdata->bf_freq_mhz_rlzd != freq_mhz_quantized) {
		sts = gpu_freq_set_from_code(freq_code);
		if (sts < 0) {
			DFRGX_DPF(DFRGX_DEBUG_MED,
				"%s: error (%d) from gpu_freq_set_from_code for %uMHz\n",
				__func__, sts, freq_mhz_quantized);
			goto out;
		} else {
			bfdata->bf_freq_mhz_rlzd = sts;
			DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s: freq MHz(requested, realized) = %u, %lu\n",
				__func__, freq_mhz_quantized,
				bfdata->bf_freq_mhz_rlzd);
		}

		if (df) {
			/*
			 * Setting df->previous_freq will be redundant
			 * when called from target dispatch function, but
			 * not otherwise.
			 */
			df->previous_freq = bfdata->bf_freq_mhz_rlzd * 1000;
		}
	}

	sts = bfdata->bf_freq_mhz_rlzd * 1000;

	/* Update our record accordingly*/
	bfdata->g_dfrgx_data.gpu_utilization_record_index = df_rgx_get_util_record_index_by_freq(sts);

out:
	mutex_unlock(&bfdata->lock);

	return sts;
}


/**
 * wake_thread() - Wake the work thread.
 * @df_rgx_data_s: dfrgx burst handle.
 */
static void dfrgx_add_sample_data(struct df_rgx_data_s * g_dfrgx, struct gpu_util_stats util_stats_sample)
{
	DFRGX_DPF(DFRGX_DEBUG_LOW, "%s: !\n",
				__func__);
	static int num_samples = 0;
	static int sum_samples_active = 0;
	int average_active_util;
	unsigned int burst;
	int ret;

	sum_samples_active += (util_stats_sample.ui32GpuStatActive / 100);
	num_samples++;

	/* When we collect MAX_NUM_SAMPLES samples we need to decide bursting or unbursting*/
	if(num_samples == MAX_NUM_SAMPLES)
	{

		average_active_util = sum_samples_active / MAX_NUM_SAMPLES;

		/* Reset */
		sum_samples_active = 0;
		num_samples = 0;

		DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s: Average Active: %d !\n",
		__func__, average_active_util);

		burst = df_rgx_request_burst(&g_dfrgx->gpu_utilization_record_index, average_active_util);
				
		if( burst == DFRGX_NO_BURST_REQ )
		{
			DFRGX_DPF(DFRGX_DEBUG_LOW, "%s: NO BURST REQ!\n",
				__func__);
		}
		else{
			ret = set_desired_frequency_khz(g_dfrgx->bus_freq_data,
				aAvailableStateFreq[g_dfrgx->gpu_utilization_record_index].freq);

			if (ret <= 0)
			{
				DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s: Failed to burst/unburst at : %l !\n",
				__func__,aAvailableStateFreq[g_dfrgx->gpu_utilization_record_index].freq);

			}
		}
	}
}


/**
 * wake_thread() - Wake the work thread.
 * @df_rgx_data_s: dfrgx burst handle.
 */
static void wake_thread(struct df_rgx_data_s *g_dfrgx)
{
	DFRGX_DPF(DFRGX_DEBUG_LOW, "%s: time to wake up!  !\n",
			__func__);
	if (g_dfrgx->g_task)
		wake_up_process(g_dfrgx->g_task);
}

/**
 * df_rgx_action() - Perform utilization stats polling and freq burst.
 * @df_rgx_data_s: dfrgx burst handle.
 */
static int df_rgx_action(struct df_rgx_data_s * g_dfrgx)
{
	struct gpu_util_stats utilStats;
	/* Get GPU utilisation numbers*/

	//smp_rmb();
	DFRGX_DPF(DFRGX_DEBUG_LOW, "%s: !\n",
		__func__);

	/*Let's check if We can query the utilisation numbers now*/
	if(!g_dfrgx->g_initialized)
	{
		//smp_wmb();
		DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s: Not initialized yet !\n",
		__func__);
		goto go_out;
	}

	if(!df_rgx_is_active())
	{
		DFRGX_DPF(DFRGX_DEBUG_LOW, "%s: RGX not Active !\n",
		__func__);
		goto go_out;
	}

	if(gpu_rgx_get_util_stats(&utilStats))
	{
		DFRGX_DPF(DFRGX_DEBUG_LOW, "%s: Active: %d, Blocked: %d, Idle: %d !\n",
		__func__, utilStats.ui32GpuStatActive, utilStats.ui32GpuStatBlocked, utilStats.ui32GpuStatIdle);
		dfrgx_add_sample_data(g_dfrgx, utilStats);
	}
	else
	{
		DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s: Invalid Util stats !\n",
		__func__);
	}
go_out:
	return 1;
}

/**
 * work_thread_loop() - the main loop for the worker thread.
 * @pvd: The "void *" private data provided to kthread_create.
 *       This can be cast to the gbprv handle.
 *
 * Upon return, thread will exit.
 */
static int df_rgx_worker_thread(void *pvd)
{
	struct df_rgx_data_s *g_dfrgx = (struct df_rgx_data_s *) pvd;
	int rva;

	DFRGX_DPF(DFRGX_DEBUG_LOW, "%s: kernel thread started !\n",
		__func__);

	while(1) {

		/**
		 * Synchronization is via a call to:
		 * int wake_up_process(struct task_struct *p)
		 */
		set_current_state(TASK_INTERRUPTIBLE);

		schedule();

		/* Do We really need this?*/
		//if (kthread_should_stop())
		//	break;

		rva = df_rgx_action(g_dfrgx);
		if (rva == 0) {
			/* Thread exit requested */
			break;
		}

		if (kthread_should_stop())
			break;
	}

	DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s: kernel thread stopping !\n",
		__func__);

	return 0;
}

/**
 * df_rgx_create_worker_thread() - Create work thread.
 * @df_rgx_data_s: dfrgx burst handle.
 *
 * This thread is not truely a "real-time" thread, in that there will be no
 * catastrophe if its execution is somewhat delayed.  However, knowing that
 * the nominal execution interval for this timer-woken thread is 5 msecs and
 * knowing that the thread execution will be very short, it seems appropriate
 * to request an elevated scheduling priority.  Perhaps a consensus will be
 * reached as to whether or not this is truely a good idea.
 *
 * Function return value:  < 0 if error, otherwise 0.
 */
static int df_rgx_create_worker_thread(struct df_rgx_data_s *g_dfrgx)
{

	struct task_struct *tskhdl;
	DFRGX_DPF(DFRGX_DEBUG_LOW, "%s: creating the thread !\n",
		__func__);

	if (!g_dfrgx->g_task) {
		tskhdl = kthread_create(df_rgx_worker_thread,
			(void *) g_dfrgx, "kdfrgx");

		/* Keep a reference on task structure. */
		get_task_struct(tskhdl);

		if (IS_ERR(tskhdl) || !tskhdl) {
			DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s: kernel thread create fail !\n",
		__func__);
			return PTR_ERR(tskhdl);
		}

/* Do We really need this?*/
#if 0
		{
			/**
			 *  Potential values for policy:
			 *    SCHED_NORMAL
			 *    SCHED_FIFO
			 *    SCHED_RR
			 *    SCHED_BATCH
			 *    SCHED_IDLE
			 * optionally OR'd with
			 *   SCHED_RESET_ON_FORK
			 * Valid priorities for SCHED_FIFO and SCHED_RR are
			 * 1..MAX_USER_RT_PRIO-1,
			 * valid priority for SCHED_NORMAL, SCHED_BATCH, and
			 * SCHED_IDLE is 0.
			 */

			/**
			 * An alternative should normal thread priority be
			 * desired would be the following.
			 *     static const int sc_policy = SCHED_NORMAL;
			 *     static const struct sched_param sc_param = {
			 *             .sched_priority = 0,
			 *     };
			 */

			/**
			 * It seems advisable to run our kernel thread
			 * at elevated priority.
			 */
			static const int sc_policy = SCHED_RR;
			static const struct sched_param sc_param = {
				.sched_priority = 1,
			};
			int rva;

			rva = sched_setscheduler_nocheck(tskhdl,
				sc_policy, &sc_param);
			if (rva < 0) {
				DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s: task priority set failed %d !\n",
				__func__,-rva);
			}
		}
#endif
		g_dfrgx->g_task = tskhdl;
		wake_up_process(tskhdl);

	}

	return 0;
}

/**
 * df_rgx_stop_worker_thread - kill the worker thread.
 * @df_rgx_data_s: dfrgx burst handle.
 */
static void df_rgx_stop_worker_thread(struct df_rgx_data_s *g_dfrgx)
{
	if (g_dfrgx->g_task) {
		/* kthread_stop will not return until the thread is gone. */
		kthread_stop(g_dfrgx->g_task);

		put_task_struct(g_dfrgx->g_task);
		g_dfrgx->g_task = NULL;
	}
}

/**
 * hrt_start() - start (or restart) timer.
 * @df_rgx_data_s: dfrgx burst handle.
 */
static void hrt_start(struct df_rgx_data_s *g_dfrgx)
{
	if (g_dfrgx->g_enable) {
		if (g_dfrgx->g_timer_is_enabled) {
			/* Due to the gbp_timer is auto-restart timer
			 * in most case, we must use hrtimer_cancel
			 * it at first if it is in active state, to avoid
			 * hitting the BUG_ON(timer->state !=
			 * HRTIMER_STATE_CALLBACK) in hrtimer.c.
			*/
			hrtimer_cancel(&g_dfrgx->g_timer);
		} else {
			g_dfrgx->g_timer_is_enabled = 1;
		}

		hrtimer_start(&g_dfrgx->g_timer, g_dfrgx->g_hrt_period,
			HRTIMER_MODE_REL);
	}
}

/**
 * hrt_cancel() - cancel a timer.
 * @df_rgx_data_s: dfrgx burst handle.
 */
static void hrt_cancel(struct df_rgx_data_s *g_dfrgx)
{
	/* The timer can be restarted with hrtimer_start. */
	hrtimer_cancel(&g_dfrgx->g_timer);

	g_dfrgx->g_timer_is_enabled = 0;
}

/**
 * hrt_event_processor() - Process timer-driven things.
 * Called by kernel hrtimer system when the timer expires.
 * @hrthdl: Pointer to the associated hrtimer struct.
 *
 * Execution context: hard irq level.
 * Invoked via interrupt/callback.
 */
static enum hrtimer_restart hrt_event_processor(struct hrtimer *hrthdl)
{
	struct df_rgx_data_s *g_dfrgx =
		container_of(hrthdl, struct df_rgx_data_s, g_timer);
	ktime_t mc_now;

	DFRGX_DPF(DFRGX_DEBUG_LOW, "%s: time is up! -- init %d, enable %d, suspended %d !\n",
				__func__, g_dfrgx->g_initialized, g_dfrgx->g_enable, g_dfrgx->g_suspended);
	//smp_rmb();
	if (g_dfrgx->g_initialized && g_dfrgx->g_enable &&
		!g_dfrgx->g_suspended) {
		//smp_wmb();
		wake_thread(g_dfrgx);
	}

	DFRGX_DPF(DFRGX_DEBUG_LOW, "%s: time is up! -- timer_enabled %d !\n",
				__func__, g_dfrgx->g_timer_is_enabled);

	if (!g_dfrgx->g_timer_is_enabled)
		return HRTIMER_NORESTART;

	mc_now = ktime_get();
	hrtimer_forward(hrthdl, mc_now, g_dfrgx->g_hrt_period);

	return HRTIMER_RESTART;
}

/**
 * dfrgx_burst_resume() - Callback for gfx hw transition from state D3.
 * @df_rgx_data_s: dfrgx burst handle.
 *
 * Device power on.  Assume the device has retained no state.
 * Invoked via interrupt/callback.
 * Execution context: non-atomic
 */
static int dfrgx_burst_resume(struct df_rgx_data_s *g_dfrgx)
{
	//smp_rmb();

	if (!g_dfrgx|| !g_dfrgx->g_initialized)
		return 0;

	DFRGX_DPF(DFRGX_DEBUG_LOW, "%s: resume!\n",
				__func__);


	g_dfrgx->g_suspended = 0;

	//smp_wmb();

	//mutex_lock(&g_dfrgx->g_mutex_sts);
	hrt_start(g_dfrgx);
	//mutex_unlock(&g_dfrgx->g_mutex_sts);

	return 0;
}

/**
 * dfrgx_burst_suspend() - Callback for gfx hw transition from state D3.
 * @df_rgx_data_s: dfrgx burst handle.
 *
 * Device power on.  Assume the device has retained no state.
 * Invoked via interrupt/callback.
 * Execution context: non-atomic
 */
static int dfrgx_burst_suspend(struct df_rgx_data_s *g_dfrgx)
{
	//smp_rmb();

	if (!g_dfrgx|| !g_dfrgx->g_initialized)
		return 0;

	DFRGX_DPF(DFRGX_DEBUG_LOW, "%s: suspend!\n",
				__func__);

	//smp_wmb();

	//mutex_lock(&g_dfrgx->g_mutex_sts);
	g_dfrgx->g_suspended = 1;
	hrt_cancel(g_dfrgx);
	//mutex_unlock(&g_dfrgx->g_mutex_sts);

	return 0;
}


/**
 * dfrgx_power_state_set() - Callback informing of gfx hw power state change.
 * @df_rgx_data_s: dfrgx burst handle.
 * @st_on: 1 if powering on, 0 if powering down.
 */
static void dfrgx_power_state_set(struct df_rgx_data_s *g_dfrgx, int st_on)
{
	if (g_dfrgx->g_enable) {
		if (st_on)
			dfrgx_burst_resume(g_dfrgx);
		else
			dfrgx_burst_suspend(g_dfrgx);
	}
}


/**
 * dfrgx_burst_init() - dfrgx burst module initialization.
 * @df_rgx_data_s: dfrgx burst handle.
 *
 * Invokes sub-function to initialize.  If failure, invokes cleanup.
 *
 * Function return value: negative to abort module installation.
 */
int dfrgx_burst_init(struct df_rgx_data_s *g_dfrgx)
{
	int sts;
	unsigned int error = 0;

	DFRGX_DPF(DFRGX_DEBUG_LOW, "%s:gpu burst mode initialization -- begin !\n",
				__func__);

	mutex_init(&g_dfrgx->g_mutex_sts);

	/* No 3D activity is initialized to run until kernel finish
	 * its initialization. so here make the gburst status consistent
	 * with HW
	 * */
	g_dfrgx->g_suspended = 1;

	g_dfrgx->g_hrt_period = ktime_set(0,
		DFRGX_BURST_TIMER_PERIOD_DEFAULT_USECS * NSEC_PER_USEC);

	{
		struct dfrgx_interface_s dfrgx_interface;

		dfrgx_interface.dfrgx_priv = g_dfrgx;
		dfrgx_interface.dfrgx_power_state_set = dfrgx_power_state_set;

		dfrgx_interface_set_data(&dfrgx_interface);
	}

	/* Initialize timer.  This does not start the timer. */
	hrtimer_init(&g_dfrgx->g_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	g_dfrgx->g_timer.function = &hrt_event_processor;

	/* FIXME: Need to re-think this*/
	msleep(500);

	error = gpu_rgx_utilstats_init_obj();
	if(error)
	{
		DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s:gpu_rgx_utilstats_init_obj -- failed!\n",
				__func__);
		sts = -EAGAIN;
		goto error_init_obj;
	}

	g_dfrgx->g_suspended = 0;

	if (g_dfrgx->g_enable) {
		hrt_start(g_dfrgx);
		sts = df_rgx_create_worker_thread(g_dfrgx);
		if (sts < 0) {
			/* abort init if unable to create thread. */
			DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s:thread creation failed %d !\n",
				__func__,-sts);
			goto err_thread_creation;

		}
	}

	//smp_wmb();

	g_dfrgx->g_initialized = 1;

	//smp_wmb();

	DFRGX_DPF(DFRGX_DEBUG_LOW, "%s:gpu burst mode initialization -- done!\n",
				__func__);

	return 0;

error_init_obj:
	df_rgx_stop_worker_thread(g_dfrgx);

err_thread_creation:

	{
		struct dfrgx_interface_s dfrgx_interface;
		memset(&dfrgx_interface, 0, sizeof(struct dfrgx_interface_s));
		dfrgx_interface_set_data(&dfrgx_interface);
	}

	mutex_destroy(&g_dfrgx->g_mutex_sts);

	return sts;
}

/**
 * dfrgx_burst_deinit() - dfrgx burst module initialization.
 * @df_rgx_data_s: dfrgx burst handle.
 *
 * Invokes sub-function to initialize.  If failure, invokes cleanup.
 *
 * Function return value: negative to abort module installation.
 */
void dfrgx_burst_deinit(struct df_rgx_data_s *g_dfrgx)
{

	DFRGX_DPF(DFRGX_DEBUG_LOW, "%s:gpu burst mode deinitialization -- begin !\n",
				__func__);

	{
		struct dfrgx_interface_s dfrgx_interface;
		memset(&dfrgx_interface, 0, sizeof(struct dfrgx_interface_s));
		dfrgx_interface_set_data(&dfrgx_interface);
	}

	gpu_rgx_utilstats_deinit_obj();

	hrt_cancel(g_dfrgx);

	g_dfrgx->g_timer.function = NULL;

	df_rgx_stop_worker_thread(g_dfrgx);

	mutex_destroy(&g_dfrgx->g_mutex_sts);
	g_dfrgx->g_initialized = 0;

	DFRGX_DPF(DFRGX_DEBUG_LOW, "%s:gpu burst mode deinitialization -- done!\n",
				__func__);

	return;

}



