/*
 *  WinInetd by Davide Libenzi ( Inetd-like daemon for Windows )
 *  Modified 2018 by Saratoga Data Systems, Inc.
 *  Copyright 2013  Ilya Basin
 *  Copyright (C) 2003  Davide Libenzi
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>

#include <tchar.h>

#include <locale.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wininetd.h"

// depend both on the TCP/IP stack and the Ancillary Function Driver for Winsock
#define SVCDEPS "Tcpip\0Afd\0\0"

static char **get_ascii_argv(int argc, LPWSTR *argv);
static void WINAPI service_main(DWORD argc, LPTSTR *argv);
static void WINAPI service_ctrl(DWORD cctrl);
static int report_to_scm(DWORD cstate, DWORD excode, DWORD whint);
static void log_message(LPTSTR logmsg);
static int install_service(void);
static int remove_service(void);
static int debug_service(int argc, char const **argv);
static BOOL WINAPI ctrl_handler(DWORD ctrlc);
static LPTSTR get_last_errmsg(LPTSTR buf, DWORD size);

static int winet_argc;
static char const **winet_argv;
static SERVICE_STATUS_HANDLE hsstat;
static SERVICE_STATUS sstat;
static DWORD lsterr;
static _TCHAR lsterrs[512];
static int dbgsvc = 0;

static char **get_ascii_argv(int argc, LPWSTR *argv) {
  int i;
  size_t wlen;
  int len;
  char **aargv;

  if (!(aargv = (char **)malloc(sizeof(char *) * ((size_t)argc + 1))))
    return NULL;
  aargv[argc] = NULL;
  for (i = 0; i < argc; i++) {
    wlen = wcslen(argv[i]);
    // get length for the converted arg
    if (!(len = WideCharToMultiByte(CP_ACP, 0, argv[i], (int)wlen, NULL, 0,
                                    NULL, NULL))) {
      return NULL;
    }
    if (!(*(aargv + i) = (char *)malloc(len)))
      return NULL;
    if (!WideCharToMultiByte(CP_ACP, 0, argv[i], (int)wlen, *(aargv + i), len,
                             NULL, NULL)) {
      return NULL;
    }
  }
  return aargv;
}

int main(int argc, char const *argv[]) {
  SERVICE_TABLE_ENTRY disptab[] = {
      {_TEXT(WINET_APPNAME), (LPSERVICE_MAIN_FUNCTION)service_main},
      {NULL, NULL}};

  winet_argc = argc;
  winet_argv = argv;
  printf("(%s) version %s by Saratoga Data Systems, Inc.\n", WINET_APPNAME,
         WINET_VERSION);

  if (argc > 1) {
    if (!strcmp(argv[1], "--install"))
      return install_service();
    else if (!strcmp(argv[1], "--remove"))
      return remove_service();
    else if (!strcmp(argv[1], "--debug")) {
      dbgsvc = 1;
      return debug_service(argc - 1, argv + 1);
    }
  }

  printf("%s --install         to install the service\n", WINET_APPNAME);
  printf("%s --remove          to remove the service\n", WINET_APPNAME);
  printf("%s --debug <params>  to run as a console app for debugging\n",
         WINET_APPNAME);

  if (!StartServiceCtrlDispatcher(disptab))
    log_message(_TEXT("StartServiceCtrlDispatcher failed."));

  return 0;
}

static void WINAPI service_main(DWORD argc, LPTSTR *argv) {
  if ((hsstat = RegisterServiceCtrlHandler(_TEXT(WINET_APPNAME),
                                           service_ctrl)) != 0) {
    sstat.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    sstat.dwServiceSpecificExitCode = 0;

    if (report_to_scm(SERVICE_RUNNING, NO_ERROR, 4000)) {
      winet_main(winet_argc, winet_argv);
    }

    report_to_scm(SERVICE_STOPPED, lsterr, 0);
  }
}

static void WINAPI service_ctrl(DWORD cctrl) {
  switch (cctrl) {
  case SERVICE_CONTROL_STOP:
    report_to_scm(SERVICE_STOP_PENDING, NO_ERROR, 4000);
    winet_stop_service();
    return;

  case SERVICE_CONTROL_INTERROGATE:
    break;
  }

  report_to_scm(sstat.dwCurrentState, NO_ERROR, 0);
}

static int report_to_scm(DWORD cstate, DWORD excode, DWORD whint) {
  int resc = 1;
  static DWORD chkp = 1;

  if (!dbgsvc) {
    if (cstate == SERVICE_START_PENDING)
      sstat.dwControlsAccepted = 0;
    else
      sstat.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    sstat.dwCurrentState = cstate;
    sstat.dwWin32ExitCode = excode;
    sstat.dwWaitHint = whint;

    if (cstate == SERVICE_RUNNING || cstate == SERVICE_STOPPED)
      sstat.dwCheckPoint = 0;
    else
      sstat.dwCheckPoint = chkp++;

    if (!(resc = SetServiceStatus(hsstat, &sstat)))
      log_message(_TEXT("SetServiceStatus"));
  }

  return resc;
}

static void log_message(LPTSTR logmsg) {
  HANDLE hesrc;
  _TCHAR lmsg[256];
  LPCTSTR strs[2];

  if (!dbgsvc) {
    lsterr = GetLastError();

    hesrc = RegisterEventSource(NULL, _TEXT(WINET_APPNAME));

    _stprintf(lmsg, _TEXT("%s error: %d"), _TEXT(WINET_APPNAME), lsterr);
    strs[0] = lmsg;
    strs[1] = logmsg;

    if (hesrc != NULL) {
      ReportEvent(hesrc, EVENTLOG_ERROR_TYPE, 0, 0, NULL, 2, 0, strs, NULL);

      DeregisterEventSource(hesrc);
    }
  }
}

static int install_service(void) {
  SC_HANDLE schsvc, schscm;
  _TCHAR path[512];

  if (GetModuleFileName(NULL, path, COUNTOF(path)) == 0) {
    _tprintf(_TEXT("Unable to install %s - %s\n"), _TEXT(WINET_APPNAME),
             get_last_errmsg(lsterrs, COUNTOF(lsterrs)));
    return -1;
  }

  if ((schscm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) != NULL) {
    schsvc = CreateService(schscm, _TEXT(WINET_APPNAME), _TEXT(WINET_APPNAME),
                           SERVICE_ALL_ACCESS, SERVICE_USER_OWN_PROCESS,
                           SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, path, NULL,
                           NULL, _TEXT(SVCDEPS), NULL, NULL);

    if (schsvc) {
      // Change the service description.
      SERVICE_DESCRIPTION sd;
      LPTSTR szDesc = TEXT("Flume inetd daemon");
      sd.lpDescription = szDesc;

      if (!ChangeServiceConfig2(
              schsvc,                     // handle to service
              SERVICE_CONFIG_DESCRIPTION, // change: description
              &sd)) {                     // new description
        printf("ChangeServiceConfig2 description failed\n");
      }

      // Change the restart behavior
      SC_ACTION sa;
      sa.Type = SC_ACTION_RESTART;
      sa.Delay = 1 * 60 * 1000; // 1 minute in msec
      SERVICE_FAILURE_ACTIONS sfa;
      sfa.dwResetPeriod = INFINITE;
      sfa.lpRebootMsg = NULL;
      sfa.lpCommand = NULL;
      sfa.cActions = 1;
      sfa.lpsaActions = &sa;

      if (!ChangeServiceConfig2(
              schsvc,                         // handle to service
              SERVICE_CONFIG_FAILURE_ACTIONS, // change: failure actions
              &sfa)) {
        printf("ChangeServiceConfig2 failure actions failed\n");
      }

      SERVICE_FAILURE_ACTIONS_FLAG sfaf;
      sfaf.fFailureActionsOnNonCrashFailures = TRUE;

      if (!ChangeServiceConfig2(
              schsvc,                              // handle to service
              SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, // change: failure actions
              &sfaf)) {
        printf("ChangeServiceConfig2 failure actions flag failed\n");
      }

      _tprintf(_TEXT("%s installed.\n"), _TEXT(WINET_APPNAME));

      if (StartService(schsvc, 0, NULL)) {
        _tprintf(_TEXT("%s started.\n"), _TEXT(WINET_APPNAME));
      } else {
        _tprintf(_TEXT("StartService failed - %s\n"),
                 get_last_errmsg(lsterrs, COUNTOF(lsterrs)));
      }

      CloseServiceHandle(schsvc);
    } else {
      _tprintf(_TEXT("CreateService failed - %s\n"),
               get_last_errmsg(lsterrs, COUNTOF(lsterrs)));
    }
    CloseServiceHandle(schscm);
  } else {
    _tprintf(_TEXT("OpenSCManager failed - %s\n"),
             get_last_errmsg(lsterrs, COUNTOF(lsterrs)));
  }

  return 0;
}

static int remove_service(void) {
  SC_HANDLE schsvc, schscm;

  if ((schscm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) != NULL) {
    if ((schsvc = OpenService(schscm, _TEXT(WINET_APPNAME),
                              SERVICE_ALL_ACCESS)) != NULL) {
      if (ControlService(schsvc, SERVICE_CONTROL_STOP, &sstat)) {
        _tprintf(_TEXT("Stopping %s."), _TEXT(WINET_APPNAME));
        Sleep(1000);

        while (QueryServiceStatus(schsvc, &sstat)) {
          if (sstat.dwCurrentState == SERVICE_STOP_PENDING) {
            _tprintf(_TEXT("."));
            Sleep(1000);
          } else
            break;
        }

        if (sstat.dwCurrentState == SERVICE_STOPPED)
          _tprintf(_TEXT("\n%s stopped.\n"), _TEXT(WINET_APPNAME));
        else
          _tprintf(_TEXT("\n%s failed to stop.\n"), _TEXT(WINET_APPNAME));
      }

      if (DeleteService(schsvc))
        _tprintf(_TEXT("%s removed.\n"), _TEXT(WINET_APPNAME));
      else
        _tprintf(_TEXT("DeleteService failed - %s\n"),
                 get_last_errmsg(lsterrs, COUNTOF(lsterrs)));

      CloseServiceHandle(schsvc);
    } else
      _tprintf(_TEXT("OpenService failed - %s\n"),
               get_last_errmsg(lsterrs, COUNTOF(lsterrs)));

    CloseServiceHandle(schscm);
  } else
    _tprintf(_TEXT("OpenSCManager failed - %s\n"),
             get_last_errmsg(lsterrs, COUNTOF(lsterrs)));

  return 0;
}

static int debug_service(int argc, char const **argv) {
  setlocale(LC_ALL, ".ACP");

  _tprintf(_TEXT("Debugging %s.\n"), _TEXT(WINET_APPNAME));

  SetConsoleCtrlHandler(ctrl_handler, TRUE);

  winet_main(argc, argv);

  return 0;
}

static BOOL WINAPI ctrl_handler(DWORD ctrlc) {
  switch (ctrlc) {
  case CTRL_BREAK_EVENT:
  case CTRL_C_EVENT:
    _tprintf(_TEXT("Stopping %s.\n"), _TEXT(WINET_APPNAME));
    winet_stop_service();
    return TRUE;
  }

  return FALSE;
}

static LPTSTR get_last_errmsg(LPTSTR buf, DWORD size) {
  DWORD sres;
  LPTSTR tmp = NULL;

  sres = FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_ARGUMENT_ARRAY,
      NULL, GetLastError(), LANG_NEUTRAL, (LPTSTR)&tmp, 0, NULL);

  if (!sres || size < sres + 14)
    buf[0] = _TEXT('\0');
  else {
    tmp[lstrlen(tmp) - 2] = _TEXT('\0');
    _stprintf(buf, _TEXT("%s (0x%x)"), tmp, GetLastError());
  }

  if (tmp)
    LocalFree((HLOCAL)tmp);

  return buf;
}
