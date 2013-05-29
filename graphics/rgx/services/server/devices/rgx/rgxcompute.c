/*************************************************************************/ /*!
@File
@Title          RGX Compute routines
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    RGX Compute routines
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

#include "pdump_km.h"
#include "pvr_debug.h"
#include "rgxutils.h"
#include "rgxfwutils.h"
#include "rgxcompute.h"
#include "allocmem.h"
#include "devicemem.h"
#include "devicemem_pdump.h"
#include "osfunc.h"
#include "rgxccb.h"

#include "sync_server.h"
#include "sync_internal.h"

IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXCreateComputeContextKM(PVRSRV_DEVICE_NODE			*psDeviceNode,
											 DEVMEM_MEMDESC 			*psCmpCCBMemDesc,
											 DEVMEM_MEMDESC 			*psCmpCCBCtlMemDesc,
											 RGX_CC_CLEANUP_DATA		**ppsCleanupData,
											 DEVMEM_MEMDESC 			**ppsFWComputeContextMemDesc,
											 IMG_UINT32					ui32Priority,
											 IMG_DEV_VIRTADDR			sMCUFenceAddr,
											 IMG_UINT32					ui32FrameworkRegisterSize,
											 IMG_PBYTE					pbyFrameworkRegisters,
											 IMG_HANDLE					hMemCtxPrivData)
{
	PVRSRV_ERROR			eError = PVRSRV_OK;
	PVRSRV_RGXDEV_INFO 		*psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_FWCOMMONCONTEXT	*psFWComputeContext;
	RGX_CC_CLEANUP_DATA		*psTmpCleanup;
	DEVMEM_MEMDESC 			*psFWFrameworkMemDesc;

	/* Prepare cleanup struct */
	psTmpCleanup = OSAllocMem(sizeof(*psTmpCleanup));
	if (psTmpCleanup == IMG_NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	OSMemSet(psTmpCleanup, 0, sizeof(*psTmpCleanup));
	*ppsCleanupData = psTmpCleanup;

	/* Allocate cleanup sync */
	eError = SyncPrimAlloc(psDeviceNode->hSyncPrimContext,
						   &psTmpCleanup->psCleanupSync);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateComputeContextKM: Failed to allocate cleanup sync (0x%x)",
				eError));
		goto fail_syncalloc;
	}

	/*
		Allocate device memory for the firmware compute context.
	*/
	PDUMPCOMMENT("Allocate RGX firmware compute context");
	
	eError = DevmemFwAllocate(psDevInfo,
							sizeof(*psFWComputeContext),
							RGX_FWCOMCTX_ALLOCFLAGS,
							ppsFWComputeContextMemDesc);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateComputeContextKM: Failed to allocate firmware compute context (0x%x)",
				eError));
		goto fail_contextalloc;
	}
	psTmpCleanup->psFWComputeContextMemDesc = *ppsFWComputeContextMemDesc;
	psTmpCleanup->psDeviceNode = psDeviceNode;
	psTmpCleanup->bDumpedCCBCtlAlready = IMG_FALSE;

	/*
		Temporarily map the firmware compute context to the kernel.
	*/
	eError = DevmemAcquireCpuVirtAddr(*ppsFWComputeContextMemDesc,
									  (IMG_VOID **)&psFWComputeContext);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateComputeContextKM: Failed to map firmware compute context (0x%x)",
				eError));
		goto fail_cpuvirtacquire;
	}

	/* 
	 * Create the FW framework buffer
	 */
	eError = PVRSRVRGXFrameworkCreateKM(psDeviceNode, & psFWFrameworkMemDesc, ui32FrameworkRegisterSize);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateComputeContextKM: Failed to allocate firmware GPU framework state (%u)",
				eError));
		goto fail_frameworkcreate;
	}
	
	psTmpCleanup->psFWFrameworkMemDesc = psFWFrameworkMemDesc;

	/* Copy the Framework client data into the framework buffer */
	eError = PVRSRVRGXFrameworkCopyRegisters(psFWFrameworkMemDesc, pbyFrameworkRegisters, ui32FrameworkRegisterSize);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateComputeContextKM: Failed to populate the framework buffer (%u)",
				eError));
		goto fail_frameworkcopy;
	}

	eError = RGXInitFWCommonContext(psFWComputeContext,
									psCmpCCBMemDesc,
									psCmpCCBCtlMemDesc,
									hMemCtxPrivData,
									psFWFrameworkMemDesc,
									ui32Priority,
									&sMCUFenceAddr,
									&psTmpCleanup->sFWComContextCleanup);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateComputeContextKM: Failed to init firmware common context (%u)",
				eError));
		goto fail_contextinit;
	}

	/*
	 * Dump the Compute and the memory contexts
	 */
	PDUMPCOMMENT("Dump FWComputeContext");
	DevmemPDumpLoadMem(*ppsFWComputeContextMemDesc, 0, sizeof(*psFWComputeContext), PDUMP_FLAGS_CONTINUOUS);

	/* Release address acquired above. */
	DevmemReleaseCpuVirtAddr(*ppsFWComputeContextMemDesc);

	return PVRSRV_OK;
fail_contextinit:
fail_frameworkcopy:
	DevmemFwFree(psFWFrameworkMemDesc);
fail_frameworkcreate:
	DevmemReleaseCpuVirtAddr(*ppsFWComputeContextMemDesc);
fail_cpuvirtacquire:
	DevmemFwFree(*ppsFWComputeContextMemDesc);
fail_contextalloc:
	SyncPrimFree(psTmpCleanup->psCleanupSync);
fail_syncalloc:
	OSFreeMem(psTmpCleanup);
	return eError;
}

IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXDestroyComputeContextKM(RGX_CC_CLEANUP_DATA *psCleanupData)
{
	PVRSRV_ERROR				eError = PVRSRV_OK;
	PRGXFWIF_FWCOMMONCONTEXT	psFWComContextFWAddr;

	RGXSetFirmwareAddress(&psFWComContextFWAddr,
							psCleanupData->psFWComputeContextMemDesc,
							0,
							RFW_FWADDR_NOREF_FLAG);

	eError = RGXFWRequestCommonContextCleanUp(psCleanupData->psDeviceNode,
											  psFWComContextFWAddr,
											  psCleanupData->psCleanupSync,
											  RGXFWIF_DM_CDM);

	/*
		If we get retry error then we can't free this resource
		as it's still in use and we will be called again
	*/
	if (eError != PVRSRV_ERROR_RETRY)
	{
		eError = RGXDeinitFWCommonContext(&psCleanupData->sFWComContextCleanup);
	
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXDestroyComputeContextKM : failed to deinit fw common ctx. Error:%u", eError));
			goto e0;
		}
	
		/* Free the framework buffer */
		DevmemFwFree(psCleanupData->psFWFrameworkMemDesc);
		
		/*
		 * Free the firmware common context.
		 */
		DevmemFwFree(psCleanupData->psFWComputeContextMemDesc);

		/* Free the cleanup sync */
		SyncPrimFree(psCleanupData->psCleanupSync);

		OSFreeMem(psCleanupData);
	}

e0:
	return eError;
}


IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXKickCDMKM(CONNECTION_DATA		*psConnection,
								PVRSRV_DEVICE_NODE	*psDeviceNode,
								DEVMEM_MEMDESC 		*psFWComputeContextMemDesc,
								IMG_UINT32			*pui32cCCBWoffUpdate,
								DEVMEM_MEMDESC 		*pscCCBMemDesc,
								DEVMEM_MEMDESC 		*psCCBCtlMemDesc,
								IMG_UINT32			ui32ServerSyncPrims,
								PVRSRV_CLIENT_SYNC_PRIM_OP**	pasSyncOp,
								SERVER_SYNC_PRIMITIVE **pasServerSyncs,
								IMG_UINT32			ui32CmdSize,
								IMG_PBYTE			pui8Cmd,
								IMG_UINT32			ui32FenceEnd,
								IMG_UINT32			ui32UpdateEnd,
								IMG_BOOL			bPDumpContinuous,
								RGX_CC_CLEANUP_DATA *psCleanupData)
{
	PVRSRV_ERROR			eError;
	RGXFWIF_KCCB_CMD		sCmpKCCBCmd;
	volatile RGXFWIF_CCCB_CTL	*psCCBCtl;
	IMG_UINT8					*pui8CmdPtr;
	IMG_UINT32 i = 0;
	RGXFWIF_UFO					*psUFOPtr;
	IMG_UINT8 *pui8FencePtr = pui8Cmd + ui32FenceEnd; 
	IMG_UINT8 *pui8UpdatePtr = pui8Cmd + ui32UpdateEnd;

	for (i = 0; i < ui32ServerSyncPrims; i++)
	{
		PVRSRV_CLIENT_SYNC_PRIM_OP *psSyncOp = pasSyncOp[i];

			IMG_BOOL bUpdate;
			
			PVR_ASSERT(psSyncOp->ui32Flags != 0);
			if (psSyncOp->ui32Flags & PVRSRV_CLIENT_SYNC_PRIM_OP_UPDATE)
			{
				bUpdate = IMG_TRUE;
			}
			else
			{
				bUpdate = IMG_FALSE;
			}

			eError = PVRSRVServerSyncQueueHWOpKM(pasServerSyncs[i],
												  bUpdate,
												  &psSyncOp->ui32FenceValue,
												  &psSyncOp->ui32UpdateValue);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"PVRSRVServerSyncQueueHWOpKM: Failed (0x%x)", eError));
				return eError;
			}
			
			if(psSyncOp->ui32Flags & PVRSRV_CLIENT_SYNC_PRIM_OP_CHECK)
			{
				psUFOPtr = (RGXFWIF_UFO *) pui8FencePtr;
				psUFOPtr->puiAddrUFO.ui32Addr =  SyncPrimGetFirmwareAddr(psSyncOp->psSync);
				psUFOPtr->ui32Value = psSyncOp->ui32FenceValue;
				PDUMPCOMMENT("TQ client server fence - 0x%x <- 0x%x",
								   psUFOPtr->puiAddrUFO.ui32Addr, psUFOPtr->ui32Value);
				pui8FencePtr += sizeof(*psUFOPtr);
			}
			if(psSyncOp->ui32Flags & PVRSRV_CLIENT_SYNC_PRIM_OP_UPDATE)
			{
				psUFOPtr = (RGXFWIF_UFO *) pui8UpdatePtr;
				psUFOPtr->puiAddrUFO.ui32Addr =  SyncPrimGetFirmwareAddr(psSyncOp->psSync);
				psUFOPtr->ui32Value = psSyncOp->ui32UpdateValue;
				PDUMPCOMMENT("TQ client server update - 0x%x -> 0x%x",
								   psUFOPtr->puiAddrUFO.ui32Addr, psUFOPtr->ui32Value);
				pui8UpdatePtr += sizeof(*psUFOPtr);
			}
		
	}
	
	eError = DevmemAcquireCpuVirtAddr(psCCBCtlMemDesc,
									  (IMG_VOID **)&psCCBCtl);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"CreateCCB: Failed to map client CCB control (0x%x)", eError));
		goto PVRSRVRGXKickCDMKM_Exit;
	}
	/*
	 * Acquire space in the compute CCB for the command.
	 */
	eError = RGXAcquireCCB(psCCBCtl,
						   pui32cCCBWoffUpdate,
						   pscCCBMemDesc,
						   ui32CmdSize,
						   (IMG_PVOID *)&pui8CmdPtr,
						   psCleanupData->bDumpedCCBCtlAlready,
						   bPDumpContinuous);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXKickCDM: Failed to acquire %d bytes in client CCB", ui32CmdSize));
		goto PVRSRVRGXKickCDMKM_Exit;
	}
	
	OSMemCopy(pui8CmdPtr, pui8Cmd, ui32CmdSize);
	
	/*
	 * Release the compute CCB for the command.
	 */
	PDUMPCOMMENT("Compute command");

	eError = RGXReleaseCCB(psDeviceNode, 
						   psCCBCtl,
						   pscCCBMemDesc, 
						   psCCBCtlMemDesc,
						   &psCleanupData->bDumpedCCBCtlAlready,
						   pui32cCCBWoffUpdate,
						   ui32CmdSize, 
						   bPDumpContinuous,
						   psConnection->psSyncConnectionData);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXKickCDM: Failed to release space in Compute CCB"));
		goto PVRSRVRGXKickCDMKM_Exit;
	}
	
	/*
	 * Construct the kernel compute CCB command.
	 * (Safe to release reference to compute context virtual address because
	 * render context destruction must flush the firmware).
	 */
	sCmpKCCBCmd.eCmdType = RGXFWIF_KCCB_CMD_KICK;
	RGXSetFirmwareAddress(&sCmpKCCBCmd.uCmdData.sCmdKickData.psContext,
						  psFWComputeContextMemDesc,
						  0, RFW_FWADDR_NOREF_FLAG);
	sCmpKCCBCmd.uCmdData.sCmdKickData.ui32CWoffUpdate = *pui32cCCBWoffUpdate;


	/*
	 * Submit the compute command to the firmware.
	 */
	eError = RGXScheduleCommand(psDeviceNode->pvDevice,
								RGXFWIF_DM_CDM,
								&sCmpKCCBCmd,
								sizeof(sCmpKCCBCmd),
								bPDumpContinuous);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXKickCDMKM failed to schedule kernel CCB command. (0x%x)", eError));
		goto PVRSRVRGXKickCDMKM_Exit;
	}
	
/*	if (psSyncSubmitData)
	{
		SyncUtilSubmitDataDestroy(psSyncSubmitData);
	}
	*/
	DevmemReleaseCpuVirtAddr(psCCBCtlMemDesc);
PVRSRVRGXKickCDMKM_Exit:
	return eError;
}

IMG_EXPORT PVRSRV_ERROR PVRSRVRGXFlushComputeDataKM(PVRSRV_DEVICE_NODE *psDeviceNode, DEVMEM_MEMDESC *psFWContextMemDesc)
{
	RGXFWIF_KCCB_CMD sFlushCmd;
	PVRSRV_ERROR eError = PVRSRV_OK;
	
#if defined(PDUMP)
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "Submit Compute flush");
#endif
	sFlushCmd.eCmdType = RGXFWIF_KCCB_CMD_SLCFLUSHINVAL;
	sFlushCmd.uCmdData.sSLCFlushInvalData.bInval = IMG_FALSE;
	sFlushCmd.uCmdData.sSLCFlushInvalData.eDM = RGXFWIF_DM_CDM;
	
	RGXSetFirmwareAddress(&sFlushCmd.uCmdData.sSLCFlushInvalData.psContext,
							psFWContextMemDesc,
							0,
							RFW_FWADDR_NOREF_FLAG);
	
	eError = RGXScheduleCommand(psDeviceNode->pvDevice,
						RGXFWIF_DM_GP,
						&sFlushCmd,
						sizeof(sFlushCmd),
						IMG_TRUE);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXFlushComputeDataKM: Failed to schedule SLC flush command with error (%u)", eError));
	}
	else
	{
		/* Wait for the SLC flush to complete */
		eError = RGXWaitForFWOp(psDeviceNode->pvDevice, RGXFWIF_DM_GP, psDeviceNode->psSyncPrim, IMG_TRUE);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXFlushComputeDataKM: Compute flush aborted with error (%u)", eError));
		}
	}
	return eError;
}

/******************************************************************************
 End of file (rgxcompute.c)
******************************************************************************/
