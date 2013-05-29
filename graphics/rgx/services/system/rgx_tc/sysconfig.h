/*************************************************************************/ /*!
@File
@Title          System Description Header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    This header provides system-specific declarations and macros
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#if !defined(__SYSCONFIG_H__)
#define __SYSCONFIG_H__

#include "pvrsrv_device.h"
#include "rgxdevice.h"

#define TC_SYSTEM_NAME			"Rogue Test Chip"

/* Valid values for the TC_MEMORY_CONFIG configuration option */
#define TC_MEMORY_LOCAL			(1)
#define TC_MEMORY_HOST			(2)
#define TC_MEMORY_HYBRID		(3)

#define RGX_TC_CORE_CLOCK_SPEED		(90000000)
#define RGX_TC_MEM_CLOCK_SPEED		(65000000)

/* Memory reserved for use by the PDP DC. */
#define RGX_TC_RESERVE_DC_MEM_SIZE	(32 * 1024 * 1024)
#if defined(SUPPORT_ION)
/* Memory reserved for use by ion. */
#define RGX_TC_RESERVE_ION_MEM_SIZE (128 * 1024 * 1024)
#endif

#define PCI_BASEREG_OFFSET_DWORDS	(3)

/* Apollo reg on base register 0 */
#define SYS_APOLLO_REG_PCI_BASENUM	(0)
#define SYS_APOLLO_REG_PCI_OFFSET	(SYS_APOLLO_REG_PCI_BASENUM + PCI_BASEREG_OFFSET_DWORDS)
#define SYS_APOLLO_REG_REGION_SIZE	(0x00010000)

#define SYS_APOLLO_REG_SYS_OFFSET	(0x0000)
#define SYS_APOLLO_REG_SYS_SIZE		(0x0400)

#define SYS_APOLLO_REG_PLL_OFFSET	(0x1000)
#define SYS_APOLLO_REG_PLL_SIZE		(0x0400)

/* RGX reg on base register 1 */
#define SYS_RGX_REG_PCI_BASENUM		(1)
#define SYS_RGX_REG_PCI_OFFSET		(SYS_RGX_REG_PCI_BASENUM + PCI_BASEREG_OFFSET_DWORDS)
#define SYS_RGX_REG_REGION_SIZE		(0x00004000)

/* RGX mem (including HP mapping) on base register 2 */
#define SYS_RGX_MEM_PCI_BASENUM		(2)
#define SYS_RGX_MEM_PCI_OFFSET		(SYS_RGX_MEM_PCI_BASENUM + PCI_BASEREG_OFFSET_DWORDS)
#define SYS_RGX_DEV1_MEM_REGION_SIZE	(0x10000000)
#define SYS_RGX_DEV2_MEM_REGION_SIZE	(0x20000000)


static RGX_TIMING_INFORMATION gsRGXTimingInfo = 
{
	.ui32CoreClockSpeed		= RGX_TC_CORE_CLOCK_SPEED,
	.bEnableActivePM		= IMG_FALSE,
	.bEnableRDPowIsland		= IMG_FALSE,
};

static RGX_DATA gsRGXData =
{
	.psRGXTimingInfo = &gsRGXTimingInfo,
};

static PVRSRV_DEVICE_CONFIG gsDevices[] = 
{
	{
		.pszName                	= "RGX",
		.eDeviceType            	= PVRSRV_DEVICE_TYPE_RGX,

		/* Device setup information */
		.sRegsCpuPBase.uiAddr   	= 0,
		.ui32RegsSize           	= 0,

		.ui32IRQ                	= 0,
		.bIRQIsShared           	= IMG_TRUE,

		.hDevData               	= &gsRGXData,
		.hSysData               	= IMG_NULL,

		.ui32PhysHeapID			= 0,

		/* FIXME */
		.pfnPrePowerState       	= IMG_NULL,
		.pfnPostPowerState      	= IMG_NULL,

		.pfnClockFreqGet        	= IMG_NULL,

		.pfnInterruptHandled		= IMG_NULL,
	}
};

#if (TC_MEMORY_CONFIG == TC_MEMORY_LOCAL)
static IMG_VOID TCLocalCpuPAddrToDevPAddr(IMG_HANDLE hPrivData,
					  IMG_DEV_PHYADDR *psDevPAddr,
					  IMG_CPU_PHYADDR *psCpuPAddr);

static IMG_VOID TCLocalDevPAddrToCpuPAddr(IMG_HANDLE hPrivData,
					  IMG_CPU_PHYADDR *psCpuPAddr,
					  IMG_DEV_PHYADDR *psDevPAddr);
#elif (TC_MEMORY_CONFIG == TC_MEMORY_HOST) || (TC_MEMORY_CONFIG == TC_MEMORY_HYBRID)
static IMG_VOID TCSystemCpuPAddrToDevPAddr(IMG_HANDLE hPrivData,
					   IMG_DEV_PHYADDR *psDevPAddr,
					   IMG_CPU_PHYADDR *psCpuPAddr);

static IMG_VOID TCSystemDevPAddrToCpuPAddr(IMG_HANDLE hPrivData,
					   IMG_CPU_PHYADDR *psCpuPAddr,
					   IMG_DEV_PHYADDR *psDevPAddr);
#endif /* (TC_MEMORY_CONFIG == TC_MEMORY_HOST) || (TC_MEMORY_CONFIG == TC_MEMORY_HYBRID) */

#if (TC_MEMORY_CONFIG == TC_MEMORY_LOCAL)
static PHYS_HEAP_FUNCTIONS gsLocalPhysHeapFuncs =
{
	.pfnCpuPAddrToDevPAddr	= TCLocalCpuPAddrToDevPAddr,
	.pfnDevPAddrToCpuPAddr	= TCLocalDevPAddrToCpuPAddr,
};

static PHYS_HEAP_CONFIG	gsPhysHeapConfig[] =
{
	{
		.ui32PhysHeapID		= 0,
		.eType			= PHYS_HEAP_TYPE_LMA,
		.sStartAddr		= { 0 },
		.uiSize			= 0,
		.pszPDumpMemspaceName	= "LMA",
		.psMemFuncs		= &gsLocalPhysHeapFuncs,
		.hPrivData		= IMG_NULL,
	},
	{
		.ui32PhysHeapID		= 1,
		.eType			= PHYS_HEAP_TYPE_LMA,
		.sStartAddr		= { 0 },
		.uiSize			= 0,
		.pszPDumpMemspaceName	= "LMA",
		.psMemFuncs		= &gsLocalPhysHeapFuncs,
		.hPrivData		= IMG_NULL,
	},
#if defined(SUPPORT_ION)
	{
		/* ui32PhysHeapID */
		2,
		/* eType */
		PHYS_HEAP_TYPE_LMA,
		/* sStartAddr */
		{ 0 },
		/* uiSize */
		0,
		/* pszPDumpMemspaceName */
		"LMA",
		/* psMemFuncs */
		&gsLocalPhysHeapFuncs,
		/* hPrivData */
		IMG_NULL,
	},
#endif
};
#elif (TC_MEMORY_CONFIG == TC_MEMORY_HOST)
static PHYS_HEAP_FUNCTIONS gsSystemPhysHeapFuncs =
{
	.pfnCpuPAddrToDevPAddr	= TCSystemCpuPAddrToDevPAddr,
	.pfnDevPAddrToCpuPAddr	= TCSystemDevPAddrToCpuPAddr,
};

static PHYS_HEAP_CONFIG	gsPhysHeapConfig[] =
{
	{
		.ui32PhysHeapID		= 0,
		.eType			= PHYS_HEAP_TYPE_UMA,
		.sStartAddr		= { 0 },
		.uiSize			= 0,
		.pszPDumpMemspaceName	= "SYSMEM",
		.psMemFuncs		= &gsSystemPhysHeapFuncs,
		.hPrivData		= IMG_NULL,
	}
};
#elif (TC_MEMORY_CONFIG == TC_MEMORY_HYBRID)
static PHYS_HEAP_FUNCTIONS gsHybridPhysHeapFuncs =
{
	.pfnCpuPAddrToDevPAddr	= TCSystemCpuPAddrToDevPAddr,
	.pfnDevPAddrToCpuPAddr	= TCSystemDevPAddrToCpuPAddr,
};

static PHYS_HEAP_CONFIG	gsPhysHeapConfig[] =
{
	{
		.ui32PhysHeapID		= 0,
		.eType			= PHYS_HEAP_TYPE_UMA,
		.sStartAddr		= { 0 },
		.uiSize			= 0,
		.pszPDumpMemspaceName	= "SYSMEM",
		.psMemFuncs		= &gsHybridPhysHeapFuncs,
		.hPrivData		= IMG_NULL,
	},
	{
		.ui32PhysHeapID		= 1,
		.eType			= PHYS_HEAP_TYPE_LMA,
		.sStartAddr		= { 0 },
		.uiSize			= 0,
		.pszPDumpMemspaceName	= "LMA",
		.psMemFuncs		= &gsHybridPhysHeapFuncs,
		.hPrivData		= IMG_NULL,
	}
};
#else
#error "TC_MEMORY_CONFIG not valid"
#endif

static PVRSRV_SYSTEM_CONFIG gsSysConfig =
{
	.pszSystemName			= TC_SYSTEM_NAME,
	.uiDeviceCount			= sizeof(gsDevices) / sizeof(PVRSRV_DEVICE_CONFIG),
	.pasDevices			= &gsDevices[0],

	/* Physcial memory heaps */
	.ui32PhysHeapCount		= sizeof(gsPhysHeapConfig) / sizeof(PHYS_HEAP_CONFIG),
	.pasPhysHeaps			= &gsPhysHeapConfig[0],

	/* No power management on no HW system */
	.pfnSysPrePowerState		= IMG_NULL,
	.pfnSysPostPowerState		= IMG_NULL,

	/* no cache snooping */
	.bHasCacheSnooping		= IMG_FALSE,
};

/*****************************************************************************
 * system specific data structures
 *****************************************************************************/

#endif /* !defined(__SYSCONFIG_H__) */
