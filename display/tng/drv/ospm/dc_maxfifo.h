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
 *   Vinil Cheeramvelill <vinil.cheeramvelil@intel.com>
 */

#ifndef __DC_MAXFIFO_H__
#define __DC_MAXFIFO_H__

#include "psb_drv.h"

typedef enum {
	S0i1_DISP_STATE_NOT_READY = 0,
	S0i1_DISP_STATE_READY,
	S0i1_DISP_STATE_ENTERED
} S0i1_DISP_STATE;

struct dc_maxfifo {
	struct mutex maxfifo_mtx;

	struct drm_device       *dev_drm;
	bool repeat_frame_interrupt_on;
	int  regs_to_set;
	u32 sa_count; /* holds the number of consecutive sprite a requests */
	u32 so_count; /* holds the number of consecutive sprite a + OVA requests */
	u32 ova_count; /* holds the number of consecutive OVA requests */
	bool maxfifo_active; /* is maxfifo enabled */
	int maxfifo_current_state; /* 0=sprite A; 1=sprite a + ova; 2=ova */
	S0i1_DISP_STATE s0i1_disp_state;
	struct work_struct repeat_frame_interrupt_work;
};

int dc_maxfifo_init(struct drm_device *dev);
void maxfifo_report_repeat_frame_interrupt(struct drm_device * dev);
bool enter_maxfifo_mode(struct drm_device *dev, int mode);
bool exit_maxfifo_mode(struct drm_device *dev);
bool enter_s0i1_display_mode(struct drm_device *dev);
bool exit_s0i1_display_mode(struct drm_device *dev);
void enter_s0i1_display_video_playback(struct drm_device *dev);
void exit_s0i1_display_video_playback(struct drm_device *dev);
bool can_enter_maxfifo_s0i1_display(struct drm_device *dev, int mode);
void enable_repeat_frame_intr(struct drm_device *dev);
void disable_repeat_frame_intr(struct drm_device *dev);
void maxfifo_timer_stop(struct drm_device *dev);
void maxfifo_timer_start(struct drm_device *dev);

int dc_maxfifo_uninit(struct drm_device *dev);

#endif
