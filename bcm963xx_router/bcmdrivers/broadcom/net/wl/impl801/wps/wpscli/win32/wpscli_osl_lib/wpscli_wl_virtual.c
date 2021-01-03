/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_wl_virtual.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: Implement functionalities handling SoftAP virtual interface
 *
 */
#include "stdafx.h"
#include "vendor.h"
#include <winioctl.h>
#include <ntddndis.h>
#include <io.h>
#include <irelay.h>
#include <epictrl.h>

HANDLE m_hVirtualDev = NULL;
static BOOL m_bInitialized;
static BOOL LoadRelayDriver(LPCTSTR lpszDriverService);
static DWORD CallIRelay(HANDLE hDevice, PRelayHeader prh);
static DWORD LoadAndOpen(IN LPCTSTR lpszDriverName, IN LPCTSTR lpszServiceExe);
static DWORD InstallDriver(IN SC_HANDLE SchSCManager, IN LPCTSTR lpszDriverName, IN LPCTSTR lpszServiceExe);
static DWORD StartDriver(IN SC_HANDLE SchSCManager, IN LPCTSTR lpszDriverName);

DWORD Initialize_Virtual()
{
    WINERR dwStatus = NO_ERROR;
    HANDLE hDevHandle = INVALID_HANDLE_VALUE;
	HANDLE* pirh = &m_hVirtualDev;
	LPTSTR lpDevName = RELAY_NT_FILE;
	LPTSTR lpDevBase = RELAY_DEV_BASE;

    if (!pirh)
	{
		TUTRACE((TUTRACE_INFO, "Unable to initialize interface to IR, handle is not NULL\n"));
		return ERROR_INVALID_HANDLE;
	}

	hDevHandle = CreateFile(lpDevName, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (hDevHandle == INVALID_HANDLE_VALUE)
	{
		if (LoadRelayDriver(lpDevBase))
			hDevHandle = CreateFile(lpDevName, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	}

	if (hDevHandle == INVALID_HANDLE_VALUE) 
	    dwStatus = GetLastError();

    if (dwStatus == NO_ERROR)
	{
		dll_private_t* ip;
		ip = (dll_private_t*) malloc(sizeof(dll_private_t));
		if (ip != NULL)
		{
			memset(ip, 0, sizeof(dll_private_t));
			ip->handle = hDevHandle;
			*pirh = (HANDLE)ip;
		}
    }
	else
		TUTRACE((TUTRACE_INFO, "Unable to initialize interface to IR, error %d\n", dwStatus));

    return dwStatus;
}

DWORD Uninitialize()
{
    WINERR dwStatus = NO_ERROR;
    dll_private_t* ip = (dll_private_t*)m_hVirtualDev;
    
    if (!m_hVirtualDev || m_hVirtualDev == INVALID_HANDLE_VALUE)
		return ERROR_INVALID_HANDLE;
    
    if (ip->handle !=  INVALID_HANDLE_VALUE && !CloseHandle(ip->handle))
		dwStatus = GetLastError();

	free(ip);

	if (dwStatus != NO_ERROR)
		TUTRACE((TUTRACE_INFO, "Unable to uninitialize interface to IR, error %d\n", dwStatus));

    return dwStatus;
}

BOOL IsValid()
{
	if (m_hVirtualDev && m_hVirtualDev != INVALID_HANDLE_VALUE)
		return TRUE;
	else
		return FALSE;
}

DWORD SendIoctl(DWORD dwIoctl, PCHAR pBuf, PDWORD pdwLen, DWORD dwMillis)
{
    DWORD dwStatus = NO_ERROR, dwWait;
    int iOk;
    OVERLAPPED ovlp = { 0, 0, 0, 0, 0 };
    HANDLE hEvent = 0;
    dll_private_t* ip = (dll_private_t*)m_hVirtualDev;

    hEvent = CreateEvent(0, TRUE, 0, NULL);
    if (hEvent == NULL)
        return GetLastError();

	ovlp.hEvent = hEvent;
    iOk = DeviceIoControl(ip->handle, dwIoctl, pBuf, *pdwLen, pBuf, *pdwLen, pdwLen, &ovlp);
    if (!iOk)
	{
		dwStatus = GetLastError();
		if (dwStatus == ERROR_IO_PENDING)
		{
			dwWait = WaitForSingleObject(hEvent, dwMillis);
			switch (dwWait)
			{
			case WAIT_OBJECT_0:
				if (!GetOverlappedResult(ip->handle, &ovlp, pdwLen, TRUE))
					dwStatus = GetLastError();
				else
				{
					if (ovlp.Internal != 0)
						dwStatus = ovlp.Internal;
					else
						dwStatus = ERROR_SUCCESS;
				}
				break;

			case WAIT_FAILED:
				dwStatus = GetLastError();
				break;

			case WAIT_TIMEOUT:
				*pdwLen = 0;
				dwStatus = ERROR_TIMEOUT;
				break;

			default:
				TUTRACE((TUTRACE_INFO, "Received unexpected status from WaitForSingleObject = 0x%x", dwWait));
				dwStatus = ERROR_INVALID_FUNCTION;
				break;
			}
		}
    }

    CloseHandle(hEvent);
    return dwStatus;
}

DWORD Bind(LPCTSTR lpszDeviceName)
{ 
    DWORD dwStatus = ERROR_NOT_SUPPORTED, dwBrlen;
    BindRequest br;

	if (!m_hVirtualDev || m_hVirtualDev == INVALID_HANDLE_VALUE)
		return ERROR_INVALID_HANDLE;

	// convert to an ascii string
	strcpy(br.name, lpszDeviceName);
	dwBrlen = sizeof(br);
	dwStatus = SendIoctl(IOCTL_BIND, (PCHAR) &br, &dwBrlen, INFINITE);

    return dwStatus;
}

DWORD Unbind()
{
    DWORD dwLen = 0, dwStatus = ERROR_NOT_SUPPORTED;

	if (!m_hVirtualDev || m_hVirtualDev == INVALID_HANDLE_VALUE)
		return ERROR_INVALID_HANDLE;

	dwStatus = SendIoctl(IOCTL_UNBIND, NULL, &dwLen, INFINITE);
    
    return dwStatus;
}

DWORD QueryInformation(ULONG uOid, PUCHAR pInbuf, PDWORD dwInlen)
{
    dll_private_t* ip = (dll_private_t*)m_hVirtualDev;
	DWORD dwStatus = ERROR_NOT_SUPPORTED;
	PIRELAY pr;

	if (!m_hVirtualDev || m_hVirtualDev == INVALID_HANDLE_VALUE)
		return ERROR_INVALID_HANDLE;

	pr = (PIRELAY) malloc(sizeof(IRELAY) + *dwInlen);
	memset(pr, 0, sizeof(IRELAY) + *dwInlen);
	pr->rh.OID = uOid;
	pr->rh.IsQuery = TRUE;
	pr->rh.Status = ERROR_SUCCESS;

	memcpy(pr->GenOid.Buffer, pInbuf, *dwInlen);
	pr->rh.BufferLength = *dwInlen;

	dwStatus = CallIRelay(ip->handle, (PRelayHeader)pr);
	if (dwStatus == ERROR_SUCCESS)
	{
		*dwInlen = pr->rh.BufferLength;
		memcpy(pInbuf, pr->Buffer, *dwInlen);
	}
	else
	{
		if (dwStatus == ERROR_MORE_DATA)
			*dwInlen = pr->rh.BufferLength;
	}

	free(pr);

	return dwStatus;
}

DWORD SetInformation(ULONG uOid, PUCHAR pInbuf, PDWORD dwInlen)
{
    dll_private_t* ip = (dll_private_t*)m_hVirtualDev;
	DWORD dwStatus = ERROR_NOT_SUPPORTED;
	PIRELAY pr;

	if (!m_hVirtualDev || m_hVirtualDev == INVALID_HANDLE_VALUE)
		return ERROR_INVALID_HANDLE;

	pr = (PIRELAY) malloc(sizeof(IRELAY) + *dwInlen);
	memset(pr, 0, sizeof(IRELAY) + *dwInlen);
	pr->rh.OID = uOid;
	pr->rh.IsQuery = FALSE;
	pr->rh.Status = ERROR_SUCCESS;

	memcpy(pr->GenOid.Buffer, pInbuf, *dwInlen);
	pr->rh.BufferLength = *dwInlen;

	dwStatus = CallIRelay(ip->handle, (PRelayHeader)pr);

	free(pr);

	return dwStatus;
}

DWORD CallIRelay(HANDLE hDevice, PRelayHeader prh)
{
    OVERLAPPED  ovlp = { 0, 0, 0, 0, 0 };
    HANDLE hEvent = 0;
	DWORD dwIosize, dwStatus, dwCount;

    hEvent = CreateEvent(0, TRUE, 0, NULL);
    if (hEvent == NULL)
        return 0;

	ovlp.hEvent = hEvent;
    dwIosize = sizeof(RelayHeader) + prh->BufferLength;
	
	dwCount = 0;
	SetLastError(NO_ERROR);
	if (!DeviceIoControl(hDevice, IOCTL_OID_RELAY, (PVOID)prh, dwIosize, (PVOID)prh, dwIosize, &dwCount, &ovlp))
	{
		dwStatus = GetLastError();
		if (dwStatus == ERROR_IO_PENDING)
		{
			if (!GetOverlappedResult(hDevice, &ovlp, &dwCount, TRUE))
				dwStatus = GetLastError();
			else
				dwStatus = ERROR_SUCCESS;
		}

		prh->BufferLength = dwCount - sizeof(RelayHeader);
	}

	CloseHandle(hEvent);

	return dwStatus;
}

BOOL LoadRelayDriver(LPCTSTR lpszDriverService)
{    
    DWORD dwStatus;
    TCHAR cDriverExe[MAX_PATH];

	_sntprintf(cDriverExe, sizeof(cDriverExe), _T("system32\\drivers\\%s.sys"), lpszDriverService);
	dwStatus = LoadAndOpen(lpszDriverService, cDriverExe);
	switch (dwStatus)
	{
		case ERROR_SUCCESS:
		/* Fall-Through */
		case ERROR_SERVICE_EXISTS:
		return TRUE;
		
		default:
		return FALSE;
	}

	return FALSE;
}

DWORD LoadAndOpen(IN LPCTSTR lpszDriverName, IN LPCTSTR lpszServiceExe)
{
    SC_HANDLE schSCManager;
    DWORD dwError = ERROR_SUCCESS;

    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (schSCManager == NULL)
		dwError = GetLastError();
	else
	{
		InstallDriver(schSCManager, lpszDriverName, lpszServiceExe);
		StartDriver(schSCManager, lpszDriverName);
		CloseServiceHandle(schSCManager);
    }
    
	return dwError;
}

DWORD InstallDriver(IN SC_HANDLE SchSCManager, IN LPCTSTR lpszDriverName, IN LPCTSTR lpszServiceExe)
{
    SC_HANDLE schService;
    WINERR dwStatus = ERROR_SUCCESS;

    schService = CreateService (SchSCManager,          // SCManager database
                                lpszDriverName,        // name of service
                                lpszDriverName,        // name to display
                                SERVICE_ALL_ACCESS,    // desired access
                                SERVICE_KERNEL_DRIVER, // service type
                                SERVICE_DEMAND_START,  // start type
                                SERVICE_ERROR_NORMAL,  // error control type
                                lpszServiceExe,        // service's binary
                                NULL,                  // no load ordering group
                                NULL,                  // no tag identifier
                                NULL,                  // no dependencies
                                NULL,                  // LocalSystem account
                                NULL                   // no password
                                );

    if (schService == NULL)
        dwStatus = GetLastError();

    return dwStatus;
}

DWORD RemoveDriver(IN SC_HANDLE  SchSCManager, IN LPCTSTR lpszDriverName)
{
    SC_HANDLE schService;
    DWORD dwError = 0;
    BOOL bReturn;

    schService = OpenService (SchSCManager, lpszDriverName, SERVICE_ALL_ACCESS);
    if (schService != NULL)
	{
		bReturn = DeleteService (schService);
		if (bReturn == 0)
			dwError = GetLastError(); 

		CloseServiceHandle (schService);
    }

    return dwError;
}

DWORD StartDriver(IN SC_HANDLE SchSCManager, IN LPCTSTR lpszDriverName)
{
    SC_HANDLE schService;
    BOOL bReturn;
    DWORD dwError = 0;

    schService = OpenService (SchSCManager, lpszDriverName, SERVICE_ALL_ACCESS);
    if (schService == NULL)
		dwError = GetLastError();
	else
	{
		bReturn = StartService (schService, 0, NULL);
		if (bReturn == 0)
			dwError = GetLastError();

		CloseServiceHandle (schService);
    }

    return dwError;
}

DWORD StopDriver(IN SC_HANDLE SchSCManager, IN LPCTSTR lpszDriverName)
{
    SC_HANDLE schService;
    BOOL bReturn; 
    DWORD dwError = 0;
    SERVICE_STATUS serviceStatus;

    schService = OpenService(SchSCManager, lpszDriverName, SERVICE_ALL_ACCESS);
    if (schService == NULL)
		dwError = GetLastError();
	else
	{
		bReturn = ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus);
		if (bReturn == 0)
		{
			dwError = GetLastError();
			TUTRACE((TUTRACE_INFO, "Unable to stop driver error (0x%02x)\n", dwError));
		}

		CloseServiceHandle(schService);
    }

    return dwError;
}


BOOL QueryOid_Virtual(ULONG oid, PCHAR pResults, PULONG nbytes)
{
	BOOL bRet = FALSE;
	DWORD dwStatus = ERROR_DEV_NOT_EXIST;
	
	dwStatus = Bind(getShortAdapterName());
	if(dwStatus == ERROR_SUCCESS) {
		dwStatus = QueryInformation(oid, (PUCHAR)pResults, nbytes);
		Unbind();
		bRet = (dwStatus == ERROR_SUCCESS);
	}

	return bRet;
}

BOOL SetOid_Virtual(ulong oid, void* data, ulong nbytes)
{
	BOOL bRet = FALSE;
	DWORD dwStatus;
	
	dwStatus = Bind(getShortAdapterName());
	if(dwStatus == ERROR_SUCCESS) {
		dwStatus = SetInformation(oid, data, &nbytes);
		Unbind();
		bRet = (dwStatus == ERROR_SUCCESS);
	}
	
	return bRet;
}
