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
#include "df_rgx_defs.h"
#include "dev_freq_debug.h"

extern int is_tng_b0;

/**
 * df_rgx_is_valid_freq() - Determines if We are about to use
 * a valid frequency.
 * @freq: frequency to be validated.
 *
 * Function return value: 1 if Valid 0 if not.
 */
unsigned int df_rgx_is_valid_freq(unsigned long int freq)
{
	unsigned int valid = 0;
	int i;
	int aSize = NUMBER_OF_LEVELS;

	if(is_tng_b0)
		aSize = NUMBER_OF_LEVELS_B0;

	DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s freq: %d\n", __func__
			freq);

	for(i = 0; i < aSize; i++)
	{
		if(freq == aAvailableStateFreq[i].freq)
		{
			valid = 1;
			break;
		}
	}

	DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s valid: %d\n", __func__,
			valid);

	return valid;
}

/**
 * df_rgx_get_util_record_index_by_freq() - Obtains the index to a
 * a record from the avalable frequencies table.
 * @freq: frequency to be validated.
 *
 * Function return value: the index if found, -1 if not.
 */
int df_rgx_get_util_record_index_by_freq(unsigned long freq)
{
	int n_levels = NUMBER_OF_LEVELS;
	int i ;

	if(is_tng_b0)
		n_levels = NUMBER_OF_LEVELS_B0;

	for(i = 0; i < n_levels; i++)
	{
		if(freq == aAvailableStateFreq[i].freq)
			break;
	}

	if( i == n_levels)
		i = -1;

	return 	i;
}

/**
 * df_rgx_request_burst() - Decides if dfrgx needs to BURST, UNBURST
 * or keep the current frequency level.
 * @p_freq_table_index: pointer to index of the next frequency
 * to be applied.
 * @util_percentage: percentage of utilization in active state.
 * Function return value: DFRGX_NO_BURST_REQ, DFRGX_BURST_REQ,
 * DFRGX_UNBURST_REQ.
 */
unsigned int df_rgx_request_burst(int *p_freq_table_index, int util_percentage)
{
	int current_index = *p_freq_table_index;
	unsigned long freq = aAvailableStateFreq[current_index].freq;
	int new_index;
	unsigned int burst = DFRGX_NO_BURST_REQ;
	int n_levels = NUMBER_OF_LEVELS;

	if(is_tng_b0)
		n_levels = NUMBER_OF_LEVELS_B0;

	new_index = df_rgx_get_util_record_index_by_freq(freq);

	if(new_index < 0)
		goto out;

	/* Decide unbusrt/burst based on utilization*/

	if(util_percentage > aAvailableStateFreq[current_index].util_th_high && new_index < n_levels - 1)
	{
		/* Provide recommended burst*/
		*p_freq_table_index = new_index+1;
		burst = DFRGX_BURST_REQ;
	}
	else if(util_percentage <  aAvailableStateFreq[current_index].util_th_low && new_index > 0)
	{
		/* Provide recommended unburst*/
		*p_freq_table_index = new_index-1;
		burst = DFRGX_UNBURST_REQ;
	}

out:
	return burst;
}
