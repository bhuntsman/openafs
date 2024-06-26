
extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "afscreds.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Advanced_OnInitDialog (HWND hDlg);
void Advanced_StartTimer (HWND hDlg);
void Advanced_OnServiceTimer (HWND hDlg);
void Advanced_OnOpenCPL (HWND hDlg);
void Advanced_OnChangeService (HWND hDlg, WORD wCmd);
void Advanced_OnStartup (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK Advanced_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         RECT rTab;
         GetClientRect (GetParent(hDlg), &rTab);
         TabCtrl_AdjustRect (GetParent (hDlg), FALSE, &rTab); 
         SetWindowPos (hDlg, NULL, rTab.left, rTab.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);

         Advanced_OnInitDialog (hDlg);
         break;

      case WM_TIMER:
         Advanced_OnServiceTimer (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_SERVICE_STOP:
            case IDC_SERVICE_START:
            case IDC_SERVICE_AUTO:
               Advanced_OnChangeService (hDlg, LOWORD(wp));
               break;

            case IDC_OPEN_CPL:
               Advanced_OnOpenCPL (hDlg);
               break;

            case IDC_STARTUP:
               Advanced_OnStartup (hDlg);
               break;

            case IDHELP:
               Advanced_DlgProc (hDlg, WM_HELP, 0, 0);
               break;
            }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCREDS_TAB_ADVANCED);
         break;
      }
   return FALSE;
}


void Advanced_OnInitDialog (HWND hDlg)
{
   CheckDlgButton (hDlg, IDC_STARTUP, g.fStartup);
   Advanced_OnServiceTimer (hDlg);
   Advanced_StartTimer (hDlg);
}


void Advanced_StartTimer (HWND hDlg)
{
   KillTimer (hDlg, ID_SERVICE_TIMER);
   SetTimer (hDlg, ID_SERVICE_TIMER, (ULONG)cmsecSERVICE, NULL);
}


void Advanced_OnServiceTimer (HWND hDlg)
{
   BOOL fFinal = TRUE;
   BOOL fFound = FALSE;

   struct {
      QUERY_SERVICE_CONFIG Config;
      BYTE buf[1024];
   } Config;
   memset (&Config, 0x00, sizeof(Config));
   Config.Config.dwStartType = SERVICE_DEMAND_START;

   SERVICE_STATUS Status;
   memset (&Status, 0x00, sizeof(Status));
   Status.dwCurrentState = SERVICE_STOPPED;

   SC_HANDLE hManager;
   if ((hManager = OpenSCManager (NULL, NULL, GENERIC_READ)) != NULL)
      {
      SC_HANDLE hService;
      if ((hService = OpenService (hManager, TEXT("TransarcAFSDaemon"), GENERIC_READ)) != NULL)
         {
         fFound = TRUE;
         DWORD dwSize = sizeof(Config);
         QueryServiceConfig (hService, (QUERY_SERVICE_CONFIG*)&Config, sizeof(Config), &dwSize);
         QueryServiceStatus (hService, &Status);

         CloseServiceHandle (hService);
         }

      CloseServiceHandle (hManager);
      }

   if (fFound)
      {
      if ((Status.dwCurrentState == SERVICE_STOP_PENDING) ||
          (Status.dwCurrentState == SERVICE_START_PENDING))
         {
         fFinal = FALSE;
         }
      }

   EnableWindow (GetDlgItem (hDlg, IDC_SERVICE_START), fFound && (Status.dwCurrentState == SERVICE_STOPPED));
   EnableWindow (GetDlgItem (hDlg, IDC_SERVICE_STOP), fFound && (Status.dwCurrentState == SERVICE_RUNNING));
   EnableWindow (GetDlgItem (hDlg, IDC_SERVICE_AUTO), fFound);
   CheckDlgButton (hDlg, IDC_SERVICE_AUTO, fFound && (Config.Config.dwStartType == SERVICE_AUTO_START));

   TCHAR szStatus[cchRESOURCE];
   if (!fFound)
      GetString (szStatus, IDS_SERVICE_BROKEN);
   else if (Status.dwCurrentState == SERVICE_RUNNING)
      GetString (szStatus, IDS_SERVICE_RUNNING);
   else if (Status.dwCurrentState == SERVICE_STOPPED)
      GetString (szStatus, IDS_SERVICE_STOPPED);
   else if (Status.dwCurrentState == SERVICE_STOP_PENDING)
      GetString (szStatus, IDS_SERVICE_STOPPING);
   else if (Status.dwCurrentState == SERVICE_START_PENDING)
      GetString (szStatus, IDS_SERVICE_STARTING);
   else
      GetString (szStatus, IDS_SERVICE_UNKNOWN);
   SetDlgItemText (hDlg, IDC_SERVICE_STATUS, szStatus);

   if (fFinal && GetWindowLong (hDlg, DWL_USER))
      {
      SetWindowLong (hDlg, DWL_USER, 0);
      Main_RepopulateTabs (FALSE);
      }

   if (fFinal)
      {
      KillTimer (hDlg, ID_SERVICE_TIMER);
      }
}


void Advanced_OnChangeService (HWND hDlg, WORD wCmd)
{
   BOOL fSuccess = FALSE;
   ULONG error = 0;

   SC_HANDLE hManager;
   if ((hManager = OpenSCManager (NULL, NULL, SC_MANAGER_ALL_ACCESS)) != NULL)
      {
      SC_HANDLE hService;
      if ((hService = OpenService (hManager, TEXT("TransarcAFSDaemon"), SERVICE_ALL_ACCESS)) != NULL)
         {
         switch (wCmd)
            {
            case IDC_SERVICE_AUTO:
               DWORD StartType;
               StartType = (IsDlgButtonChecked (hDlg, wCmd)) ? SERVICE_AUTO_START : SERVICE_DEMAND_START;

               if (ChangeServiceConfig (hService, SERVICE_NO_CHANGE, StartType, SERVICE_NO_CHANGE, 0, 0, 0, 0, 0, 0, 0))
                  fSuccess = TRUE;
               break;

            case IDC_SERVICE_START:
               if (StartService (hService, 0, 0))
                  fSuccess = TRUE;
               break;

            case IDC_SERVICE_STOP:
               SERVICE_STATUS Status;
               if (ControlService (hService, SERVICE_CONTROL_STOP, &Status))
                  fSuccess = TRUE;
               break;
            }

         CloseServiceHandle (hService);
         }

      CloseServiceHandle (hManager);
      }

   if (fSuccess)
      {
      if (wCmd == IDC_SERVICE_STOP)
         SetWindowLong (hDlg, DWL_USER, TRUE);
      Advanced_OnServiceTimer (hDlg);
      Advanced_StartTimer (hDlg);
      }
   else
      {
      if (!error)
         error = GetLastError();

      int ids;
      switch (wCmd)
         {
         case IDC_SERVICE_START:
            ids = IDS_SERVICE_FAIL_START;
            break;
         case IDC_SERVICE_STOP:
            ids = IDS_SERVICE_FAIL_STOP;
            break;
         default:
            ids = IDS_SERVICE_FAIL_CONFIG;
            break;
         }

      Message (MB_OK | MB_ICONHAND, IDS_SERVICE_ERROR, ids, TEXT("%08lX"), error);
      }
}


void Advanced_OnOpenCPL (HWND hDlg)
{
   WinExec ("afs_config.exe", SW_SHOW);
}


void Advanced_OnStartup (HWND hDlg)
{
   g.fStartup = IsDlgButtonChecked (hDlg, IDC_STARTUP);

   HKEY hk;
   if (RegCreateKey (HKEY_LOCAL_MACHINE, TEXT("System\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters"), &hk) == 0)
      {
      DWORD dwSize = sizeof(g.fStartup);
      DWORD dwType = REG_DWORD;
      RegSetValueEx (hk, TEXT("ShowTrayIcon"), NULL, dwType, (PBYTE)&g.fStartup, dwSize);
      RegCloseKey (hk);
      }

   Shortcut_FixStartup (cszSHORTCUT_NAME, g.fStartup);
}

