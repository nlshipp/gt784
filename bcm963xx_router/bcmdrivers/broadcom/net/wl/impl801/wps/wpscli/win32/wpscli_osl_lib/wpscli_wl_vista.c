/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_wl_vista.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: Handling OID on Windows Vista
 *
 */
#include "stdafx.h"
#include <oidencap.h>
#include <rpc.h>
#include "wpscli_wlan_api.h"

typedef struct _tWL_QUERY_OID_HEADER {	
	ULONG oid;
	getinformation_t getinfo_hdr;	
} WL_QUERY_OID_HEADER, *PWL_QUERY_OID_HEADER;

// Set oid
typedef struct _tWL_SET_OID_HEADER {
	ULONG oid;
	setinformation_t setinfo_hdr;
} WL_SET_OID_HEADER, *PWL_SET_OID_HEADER;

BOOL QueryOid_Vista(LPCTSTR cszAdapterName, ULONG oid, void* results, ULONG nbytes)
{
	BOOL bRet = FALSE;
	DWORD dwNegotiatedVersion;
	HANDLE hClient = NULL;
    GUID guidIntf;
	DWORD dwStatus = ERROR_SUCCESS;
	DWORD dwBytesReturned = 0;
	PWL_QUERY_OID_HEADER pInBuffer = NULL;
	DWORD dwInBuffSize = sizeof(WL_QUERY_OID_HEADER) + nbytes;			// outData expected from the driver

	if(g_hWlanLib == NULL)
		return FALSE;

	if(cszAdapterName == NULL)
		return FALSE;
	
	// open a handle to the service
	dwStatus = fnWlanOpenHandle(2,NULL,&dwNegotiatedVersion,&hClient);
	if(dwStatus == ERROR_SUCCESS)
	{
		// get the interface GUID
		if (UuidFromString(ValidIfName(cszAdapterName), &guidIntf) == ERROR_SUCCESS)
		{
			// allow one buffer to store a request and reserve space for storing the result
			pInBuffer = (PWL_QUERY_OID_HEADER)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwInBuffSize);		
			if (pInBuffer == NULL)
				return FALSE;

			// initialize the request header
			pInBuffer->oid = OID_BCM_GETINFORMATION;

			// initialize the "getinfo" header
			pInBuffer->getinfo_hdr.oid = oid;
			pInBuffer->getinfo_hdr.len = dwInBuffSize;	// hdr+dataSize
			pInBuffer->getinfo_hdr.cookie = OIDENCAP_COOKIE;

			// followed by the oid-specific input data 
			if (nbytes > 0)
			{	
				// the top 'inDataSize' bytes in pResults are oid-specific data
				memcpy((PCHAR)(pInBuffer+1), results, nbytes);
			}

			// use 1 buffer for input and output
			dwStatus = fnWlanIhvControl(hClient, 
									  &guidIntf, 
									  wlan_ihv_control_type_driver, 
									  dwInBuffSize, 
									  (PVOID) pInBuffer, 
									  (DWORD) dwInBuffSize, 
									  pInBuffer, 
									  &dwBytesReturned);

			// transfer data
			if (dwStatus == ERROR_SUCCESS)
			{
				// note: 1. inBuffer = outBuffer; so let's remove the request-header before returning to the caller.
				//       2. driver should return a "wl" (e.g. wl_channels_in_country) header 
				PCHAR pTmpOutData = (PCHAR) (pInBuffer + 1);		// skip the "request" header
				DWORD outDataSize = dwBytesReturned - sizeof(WL_QUERY_OID_HEADER);
				if (outDataSize > nbytes)
					outDataSize = nbytes;			// only return what caller asks for

				if (outDataSize > 0)
				{
					memcpy(results, (PCHAR)(pInBuffer+1), outDataSize);
					dwBytesReturned = outDataSize;	// remove the inRequest header
				}
				bRet = TRUE;  // Success
			}

			// Free the allocated buffers.	
			if (pInBuffer)
				HeapFree(GetProcessHeap(), 0, (LPVOID)pInBuffer);
		}
	}
	
	if (hClient != NULL) 
		fnWlanCloseHandle(hClient, NULL);

	return bRet;
}

BOOL SetOid_Vista(LPCTSTR cszAdapterName, ULONG oid, PUCHAR pInData, DWORD inlen)
{
	BOOL bRet = FALSE;
	HANDLE hClient = NULL;
    GUID guidIntf;
	DWORD dwNegotiatedVersion;
	DWORD outDataSize = 0;
	DWORD dwInBuffSize;
	PWL_SET_OID_HEADER pInBuffer = NULL;
	DWORD inDataSize = inlen;
	DWORD dwBytesReturned = 0;
	DWORD dwStatus = ERROR_SUCCESS;

	if(g_hWlanLib == NULL)
		return FALSE;

	dwStatus = fnWlanOpenHandle(2,NULL, &dwNegotiatedVersion,&hClient); 
	if(dwStatus != ERROR_SUCCESS)
		return FALSE;

	// get the interface GUID
	if (UuidFromString((RPC_CSTR)(RPC_WSTR)ValidIfName(cszAdapterName), &guidIntf) != RPC_S_OK)
		return FALSE;

	if (pInData == (LPBYTE) NULL)
		inDataSize = FALSE;	

	dwInBuffSize = sizeof(WL_SET_OID_HEADER)		// request header
						+ inDataSize	// oid-specific inData (to the driver)
						+ outDataSize;	// outData expected from the driver

	// allow one buffer to store a request and reserve space for storing the result
	pInBuffer = (PWL_SET_OID_HEADER)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwInBuffSize);		
	if (pInBuffer == NULL)
			return FALSE;


	// setup the request header
	pInBuffer->oid = OID_BCM_SETINFORMATION;

	// setup the SetInfo header
	pInBuffer->setinfo_hdr.oid = oid;
	pInBuffer->setinfo_hdr.cookie = OIDENCAP_COOKIE;

	// followed by the oid-specific input data 
	if (inDataSize > 0)
		memcpy((PCHAR)(pInBuffer+1), pInData, inDataSize);

	// Does driver set a driver-specific error code in the output buffer ?
	dwStatus = fnWlanIhvControl(hClient, 
							  &guidIntf, 
							  wlan_ihv_control_type_driver, 
							  dwInBuffSize, 
							  (PVOID)pInBuffer, 
							  (DWORD)dwInBuffSize, 
							  pInBuffer, 
							  &dwBytesReturned);
	
	// cleanup: Free the allocated buffers.	
	if (pInBuffer)
		HeapFree(GetProcessHeap(), 0, (LPVOID)pInBuffer);

	if (hClient != NULL) 
		fnWlanCloseHandle(hClient, NULL);

	if(dwStatus == ERROR_SUCCESS)
		bRet = TRUE;

	return bRet;
}
