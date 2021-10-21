#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <Winsvc.h>
#include <time.h>

#include <stdio.h>
//---------------------------------------------------------------------------
#ifndef TEST
#define TEST
#endif
//---------------------------------------------------------------------------
#ifndef TEST
#pragma comment(linker, "/ENTRY:NewMain")
#endif
//---------------------------------------------------------------------------
#define REDEMPTION_SERVICE_NAME												L"TestService"
#define REDEMPTION_DISPLAY_NAME												L"TestService"
//---------------------------------------------------------------------------
SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE ServiceStatusHandle;
//---------------------------------------------------------------------------
void WINAPI ServicelHandlerProc(DWORD control)
{
	SERVICE_STATUS_HANDLE hss = ServiceStatusHandle;
	SERVICE_STATUS *pss = &ServiceStatus;

	switch (control)
	{
	case SERVICE_CONTROL_PAUSE:
		pss->dwCurrentState = SERVICE_PAUSED;
		break;
	case SERVICE_CONTROL_CONTINUE:
		pss->dwCurrentState = SERVICE_RUNNING;
		break;
	case SERVICE_CONTROL_STOP:
		pss->dwWin32ExitCode = 0;
		pss->dwCurrentState = SERVICE_STOPPED;
		pss->dwCheckPoint = 0;
		pss->dwWaitHint = 0;
		break;
	case SERVICE_CONTROL_INTERROGATE:
		break;
	default:
		break;
	}

	SetServiceStatus(hss, pss);
}

void WINAPI ServiceMain(DWORD argc, LPWSTR *argv)
{
	SERVICE_STATUS_HANDLE hss;
	SERVICE_STATUS *pss = &ServiceStatus;
	DWORD status;

	pss->dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	pss->dwCurrentState = SERVICE_START_PENDING;
	pss->dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	pss->dwWin32ExitCode = 0;
	pss->dwServiceSpecificExitCode = 0;
	pss->dwCheckPoint = 0;
	pss->dwWaitHint = 1000;

	ServiceStatusHandle = hss = RegisterServiceCtrlHandler(argv[0], ServicelHandlerProc);
	if (hss)
	{
		pss->dwCurrentState = SERVICE_RUNNING;
		pss->dwCheckPoint = 0;
		pss->dwWaitHint = 0;
		if (SetServiceStatus(hss, pss))
		{
			while (pss->dwCurrentState != SERVICE_STOPPED)
			{
				Sleep(1000);
			}
		}
	}
}
//---------------------------------------------------------------------------
int ServiceCreate(const WCHAR *binarypathname, const WCHAR *servicename, const WCHAR *displayname, const WCHAR *description)
{
	SC_HANDLE hscmanager;
	SC_HANDLE hservice;
	HKEY hkey;
	WCHAR keyname[256];
	unsigned int l;
	int result = 0;

	hscmanager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hscmanager)
	{
		hservice = CreateService(hscmanager,
			servicename,
			displayname, // service name to display
			SERVICE_ALL_ACCESS, // desired access 
			SERVICE_WIN32_OWN_PROCESS, // service type 
			SERVICE_AUTO_START, // start type 
			SERVICE_ERROR_NORMAL, // error control type 
			binarypathname, // service's binary 
			NULL, // no load ordering group 
			NULL, // no tag identifier 
			NULL, // no dependencies
			NULL, // LocalSystem account
			NULL); // no password
		if (hservice)
		{
			CloseServiceHandle(hservice);

			if (description)
			{
				wcscpy(keyname, L"SYSTEM\\CurrentControlSet\\Services\\");
				l = wcslen(keyname);

				wcscpy(keyname + l, servicename);

				if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyname, 0, KEY_SET_VALUE, &hkey) == ERROR_SUCCESS)
				{
					RegSetValueEx(hkey, L"Description", 0, REG_SZ, (CONST BYTE *)description, (wcslen(description) + 1) * sizeof(WCHAR));

					RegCloseKey(hkey);
				}
			}

			result = 1;
		}

		CloseServiceHandle(hscmanager);
	}

	return(result);
}
int ServiceDelete(const WCHAR *servicename)
{
	SC_HANDLE hscmanager;
	SC_HANDLE hservice;
	SERVICE_STATUS ss;
	int result = 0;
	UINT i;

	hscmanager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (hscmanager)
	{
		hservice = OpenService(hscmanager, servicename, SERVICE_ALL_ACCESS);
		if (hservice)
		{
			QueryServiceStatus(hservice, &ss);
			if (ss.dwCurrentState != SERVICE_STOPPED)
			{
				ControlService(hservice, SERVICE_CONTROL_STOP, &ss);
			}
			for (i = 0; i < 28 && QueryServiceStatus(hservice, &ss); i++)
			{
				if (ss.dwCurrentState == SERVICE_STOPPED)
				{
					break;
				}

				Sleep(1000);
			}
			result = DeleteService(hservice);

			CloseServiceHandle(hservice);
		}

		CloseServiceHandle(hscmanager);
	}

	return(result);
}
int ServiceStart(const WCHAR *servicename)
{
	SC_HANDLE hscmanager;
	SC_HANDLE hservice;
	int result = 0;

	hscmanager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (hscmanager)
	{
		hservice = OpenService(hscmanager, servicename, SERVICE_START);
		if (hservice)
		{
			result = StartService(hservice, 0, NULL);

			CloseServiceHandle(hservice);
		}

		CloseServiceHandle(hscmanager);
	}

	return(result);
}
//---------------------------------------------------------------------------
#ifndef TEST
VOID APIENTRY NewMain(VOID)
#else
int wmain(int argc, WCHAR *argv[])
#endif
{
	SERVICE_TABLE_ENTRY service_table_entries[] =
	{
		{ (LPWSTR)REDEMPTION_SERVICE_NAME, ServiceMain },
		{ NULL, NULL }
	};
	WCHAR filename[1024];
	LPWSTR p;
#ifndef TEST
	LPWSTR *argv;
	int argc;
#endif
	unsigned int l;
	int j;
	int launch = 1;

#ifndef TEST
	argc = 0;
	argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv)
#endif
	{
		if (argc > 1)
		{
			launch = 0;

			p = argv[1];
			if (p[0] == '-' && (p[1] == 'I' || p[1] == 'i') && p[2] == '\0')
			{
				l = GetModuleFileName(NULL, filename, sizeof(filename) / sizeof(filename[0]));
				if (l && l < sizeof(filename) / sizeof(filename[0]))
				{
					if (ServiceCreate(filename, REDEMPTION_SERVICE_NAME, REDEMPTION_DISPLAY_NAME, L"Redemptioner"))
					{
						ServiceStart(REDEMPTION_SERVICE_NAME);

						MessageBox(NULL, L"\n\nService Installed Sucessfully\n", L"", MB_OK);
					}
					else
					{
						MessageBox(NULL, L"\n\nError Installing Service\n", L"", MB_OK);
					}
				}
			}
			else if (p[0] == '-' && (p[1] == 'D' || p[1] == 'd') && p[2] == '\0')
			{
				if (ServiceDelete(REDEMPTION_SERVICE_NAME))
				{
					MessageBox(NULL, L"\n\nService UnInstalled Sucessfully\n", L"", MB_OK);
				}
				else
				{
					MessageBox(NULL, L"\n\nError UnInstalling Service\n", L"", MB_OK);
				}
			}
		}

#ifndef TEST
		LocalFree(argv);
#endif
	}

	if (launch)
	{
		StartServiceCtrlDispatcher(service_table_entries);
	}

#ifndef TEST
	ExitProcess(0);
#else
	return(0);
#endif
}
//---------------------------------------------------------------------------