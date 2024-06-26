extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "afscreds.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define ID_REMIND_TIMER      1000
#define ID_SERVICE_TIMER     1001

#define cREALLOC_TABS        4

#define dwTABPARAM_MOUNT     (LPTSTR)0
#define dwTABPARAM_ADVANCED  (LPTSTR)1
#define ISCELLTAB(_psz)      ((HIWORD((LONG)(_psz))) != 0)


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Main_OnInitDialog (HWND hDlg);
void Main_OnCheckMenuRemind (void);
void Main_OnRemindTimer (void);
void Main_OnMouseOver (void);
void Main_OnSelectTab (void);
void Main_OnCheckTerminate (void);
HWND Main_CreateTabDialog (HWND hTab, size_t iTab);

BOOL CALLBACK Terminate_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Terminate_OnInitDialog (HWND hDlg);
void Terminate_OnOK (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK Main_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   static UINT msgCheckTerminate = 0;
   if (msgCheckTerminate == 0)
      msgCheckTerminate = RegisterWindowMessage (TEXT("AfsCredsCheckTerminate"));

   if (msg == msgCheckTerminate)
      {
      Main_OnCheckTerminate();
      }
   else switch (msg)
      {
      case WM_INITDIALOG:
         g.hMain = hDlg;
         Main_OnInitDialog (hDlg);
         break;

      case WM_DESTROY:
         Creds_CloseLibraries();
         ChangeTrayIcon (NIM_DELETE);
         break;

      case WM_ACTIVATEAPP:
         if (wp)
            {
            Main_RepopulateTabs (FALSE);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
            case IDCANCEL:
               Main_Show (FALSE);
               break;

            case M_ACTIVATE:
               if (g.fIsWinNT || IsServiceRunning())
                  {
                  if (!lp) // Got here from "/show" parameter? switch tabs.
                     {
                     HWND hTab = GetDlgItem (g.hMain, IDC_TABS);
                     TabCtrl_SetCurSel (hTab, 0);
                     Main_OnSelectTab();
                     }
                  Main_Show (TRUE);
                  }
               else
                  {
                  Message (MB_ICONHAND, IDS_UNCONFIG_TITLE_95, IDS_UNCONFIG_DESC_95);
                  }
               break;

            case M_TERMINATE:
               if (g.fIsWinNT && IsServiceRunning())
                  ModalDialog (IDD_TERMINATE, NULL, (DLGPROC)Terminate_DlgProc);
               else if (g.fIsWinNT)
                  ModalDialog (IDD_TERMINATE_SMALL, NULL, (DLGPROC)Terminate_DlgProc);
               else // (!g.fIsWinNT)
                  ModalDialog (IDD_TERMINATE_SMALL_95, NULL, (DLGPROC)Terminate_DlgProc);
               break;

            case M_TERMINATE_NOW:
               Quit();
               break;

            case M_REMIND:
               Main_OnCheckMenuRemind();
               break;
            }
         break;

      case WM_TIMER:
         Main_OnRemindTimer();
         break;

      case WM_NOTIFY:
         switch (((NMHDR*)lp)->code)
            {
            case TCN_SELCHANGE:
               Main_OnSelectTab();
               break;
            }
         break;

      case WM_TRAYICON:
         switch (lp)
            {
            case WM_LBUTTONDOWN:
               if (IsServiceRunning() || !IsServiceConfigured())
                  Main_Show (TRUE);
               else if (!g.fIsWinNT)
                  Message (MB_ICONHAND, IDS_UNCONFIG_TITLE_95, IDS_UNCONFIG_DESC_95);
               else
                  ShowStartupWizard();
               break;

            case WM_RBUTTONDOWN:
               HMENU hm;
               if ((hm = TaLocale_LoadMenu (MENU_TRAYICON)) != 0)
                  {
                  POINT pt;
                  GetCursorPos(&pt);

                  HMENU hmDummy = CreateMenu();
                  InsertMenu (hmDummy, 0, MF_POPUP, (UINT)hm, NULL);

                  BOOL fRemind = FALSE;
                  for (size_t iCreds = 0; iCreds < g.cCreds; ++iCreds)
                     {
                     if (g.aCreds[ iCreds ].fRemind)
                        fRemind = TRUE;
                     }
                  CheckMenuItem (hm, M_REMIND, MF_BYCOMMAND | ((fRemind) ? MF_CHECKED : MF_UNCHECKED));

                  TrackPopupMenu (GetSubMenu (hmDummy, 0),
                                  TPM_RIGHTALIGN | TPM_RIGHTBUTTON,
                                  pt.x, pt.y, NULL, hDlg, NULL);

                  DestroyMenu (hmDummy);
                  }
               break;

            case WM_MOUSEMOVE:
               Main_OnMouseOver();
               break;
            }
         break;
      }

   return FALSE;
}


void Main_Show (BOOL fShow)
{
   if (fShow)
      {
      ShowWindow (g.hMain, SW_SHOW);
      SetWindowPos (g.hMain, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      SetForegroundWindow (g.hMain);
      }
   else
      {
      SetWindowPos (g.hMain, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
      }
}


void Main_OnInitDialog (HWND hDlg)
{
   if (!g.fIsWinNT)
      {
      TCHAR szTitle[256];
      GetString (szTitle, IDS_TITLE_95);
      SetWindowText (hDlg, szTitle);
      }

   TCHAR szVersion[256];
   DWORD dwPatch = 0;
   TCHAR szUser[256];
   GetString (szVersion, IDS_UNKNOWN);
   GetString (szUser, IDS_UNKNOWN);

   HKEY hk;
   if (RegOpenKey (HKEY_LOCAL_MACHINE, REGSTR_PATH_AFS, &hk) == 0)
      {
      DWORD dwSize = sizeof(szVersion);
      DWORD dwType = REG_SZ;
      RegQueryValueEx (hk, REGVAL_AFS_VERSION, NULL, &dwType, (PBYTE)szVersion, &dwSize);

      dwSize = sizeof(dwPatch);
      dwType = REG_DWORD;
      RegQueryValueEx (hk, REGVAL_AFS_PATCH, NULL, &dwType, (PBYTE)&dwPatch, &dwSize);
      RegCloseKey (hk);
      }

   BOOL fFoundUserName = FALSE;
   if (RegOpenKey (HKEY_LOCAL_MACHINE, TEXT("Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon"), &hk) == 0)
      {
      DWORD dwSize = sizeof(szUser);
      DWORD dwType = REG_SZ;
      if (RegQueryValueEx (hk, TEXT("DefaultUserName"), NULL, &dwType, (PBYTE)szUser, &dwSize) == 0)
         fFoundUserName = TRUE;
      RegCloseKey (hk);
      }
   if (!fFoundUserName)
      {
      if (RegOpenKey (HKEY_LOCAL_MACHINE, TEXT("Network\\Logon"), &hk) == 0)
         {
         DWORD dwSize = sizeof(szUser);
         DWORD dwType = REG_SZ;
         if (RegQueryValueEx (hk, TEXT("UserName"), NULL, &dwType, (PBYTE)szUser, &dwSize) == 0)
            fFoundUserName = TRUE;
         RegCloseKey (hk);
         }
      }

   TCHAR szSource[ cchRESOURCE ];
   TCHAR szTarget[ cchRESOURCE ];

   GetString (szSource, (dwPatch) ? IDS_TITLE_VERSION : IDS_TITLE_VERSION_NOPATCH);
   wsprintf (szTarget, szSource, szVersion, dwPatch);
   SetDlgItemText (hDlg, IDC_TITLE_VERSION, szTarget);

   GetDlgItemText (hDlg, IDC_TITLE_NT, szSource, cchRESOURCE);
   wsprintf (szTarget, szSource, szUser);
   SetDlgItemText (hDlg, IDC_TITLE_NT, szTarget);
}


void Main_OnCheckMenuRemind (void)
{
   BOOL fRemind = FALSE;
   for (size_t iCreds = 0; iCreds < g.cCreds; ++iCreds)
      {
      if (g.aCreds[ iCreds ].fRemind)
         fRemind = TRUE;
      }

   fRemind = !fRemind;
   for (iCreds = 0; iCreds < g.cCreds; ++iCreds)
      {
      if (g.aCreds[ iCreds ].fRemind != fRemind)
         {
         g.aCreds[ iCreds ].fRemind = fRemind;
         SaveRemind (iCreds);
         }
      }

   // Check the active tab, and fix its checkbox if necessary
   //
   HWND hTab = GetDlgItem (g.hMain, IDC_TABS);
   LPTSTR pszTab = (LPTSTR)GetTabParam (hTab, TabCtrl_GetCurSel(hTab));
   if (ISCELLTAB(pszTab) && (*pszTab))
      {
      HWND hDlg = GetTabChild (hTab);
      if (hDlg)
         CheckDlgButton (hDlg, IDC_CREDS_REMIND, fRemind);
      }

   // Make sure the reminder timer is going
   //
   Main_EnableRemindTimer (TRUE);
}


void Main_OnRemindTimer (void)
{
   Main_RepopulateTabs (TRUE);

   // See if anything is close to expiring; if so, display a warning
   // dialog. Make sure we never display a warning more than once.
   //
   size_t iExpired;
   if ((iExpired = Main_FindExpiredCreds()) != -1)
      {
      if (InterlockedIncrement (&g.fShowingMessage) != 1)
         InterlockedDecrement (&g.fShowingMessage);
      else
         ShowObtainCreds (TRUE, g.aCreds[ iExpired ].szCell);
      }
}


void Main_OnMouseOver (void)
{
   if ((GetTickCount() - g.tickLastRetest) > cmsecMOUSEOVER)
      {
      Main_RepopulateTabs (TRUE);
      }
}


void Main_OnSelectTab (void)
{
   HWND hTab = GetDlgItem (g.hMain, IDC_TABS);
   size_t iTab = TabCtrl_GetCurSel (hTab);

   HWND hDlgOld = GetTabChild (hTab);

   HWND hDlgNew;
   if ((hDlgNew = Main_CreateTabDialog (hTab, iTab)) != NULL)
      ShowWindow (hDlgNew, SW_SHOW);

   if (hDlgOld)
      DestroyWindow (hDlgOld);
}


void Main_OnCheckTerminate (void)
{
   HKEY hk;
   if (RegOpenKey (HKEY_LOCAL_MACHINE, TEXT("System\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters"), &hk) == 0)
      {
      DWORD dwSize = sizeof(g.fStartup);
      DWORD dwType = REG_DWORD;
      RegQueryValueEx (hk, TEXT("ShowTrayIcon"), NULL, &dwType, (PBYTE)&g.fStartup, &dwSize);
      RegCloseKey (hk);
      }

   Shortcut_FixStartup (cszSHORTCUT_NAME, g.fStartup);

   if (!g.fStartup)
      Quit();
}


HWND Main_CreateTabDialog (HWND hTab, size_t iTab)
{
   HWND hDlg = NULL;
   LPTSTR psz = NULL;

   TC_ITEM Item;
   memset (&Item, 0x00, sizeof(Item));
   Item.mask = TCIF_PARAM;
   if (TabCtrl_GetItem (hTab, iTab, &Item))
      {
      psz = (LPTSTR)(Item.lParam);
      }

   if (psz == dwTABPARAM_ADVANCED)    // Advanced tab
      {
      hDlg = ModelessDialog (IDD_TAB_ADVANCED, hTab, (DLGPROC)Advanced_DlgProc);
      }
   else if (psz == dwTABPARAM_MOUNT)  // Mount Points tab
      {
      hDlg = ModelessDialog (IDD_TAB_MOUNT, hTab, (DLGPROC)Mount_DlgProc);
      }
   else if (ISCELLTAB(psz) && !*psz)  // No Creds tab
      {
      hDlg = ModelessDialogParam (IDD_TAB_NOCREDS, hTab, (DLGPROC)Creds_DlgProc, (LPARAM)psz);
      }
   else if (ISCELLTAB(psz) && *psz)   // Creds tab for a particular cell
      {
      hDlg = ModelessDialogParam (IDD_TAB_CREDS, hTab, (DLGPROC)Creds_DlgProc, (LPARAM)psz);
      }

   return hDlg;
}


void Main_RepopulateTabs (BOOL fDestroyInvalid)
{
   static BOOL fInHere = FALSE;
   if (!fInHere)
      {
      fInHere = TRUE;

      if (IsWindowVisible (g.hMain))
         fDestroyInvalid = FALSE;
      Main_EnableRemindTimer (FALSE);

      // First we'll have to look around and see what credentials we currently
      // have. This call just updates g.aCreds[]; it doesn't do anything else.
      //
      (void)GetCurrentCredentials();

      // We want one tab on the main dialog for each g.aCredentials entry,
      // and one tab for Advanced.
      //
      HWND hTab = GetDlgItem (g.hMain, IDC_TABS);

      // Generate a list of the lParams we'll be giving tabs...
      //
      LPTSTR *aTabs = NULL;
      size_t cTabs = 0;
      size_t iTabOut = 0;

      size_t nCreds = 0;
      for (size_t iCreds = 0; iCreds < g.cCreds; ++iCreds)
         {
         if (g.aCreds[ iCreds ].szCell[0])
            ++nCreds;
         }
      if (!nCreds)
         {
         fDestroyInvalid = TRUE;
         }

      if (!fDestroyInvalid)
         {
         int nTabs = TabCtrl_GetItemCount(hTab);
         for (int iTab = 0; iTab < nTabs; ++iTab)
            {
            LPTSTR pszTab = (LPTSTR)GetTabParam (hTab, iTab);
            if (ISCELLTAB(pszTab) && (*pszTab))
               {
               if (REALLOC (aTabs, cTabs, 1+iTabOut, cREALLOC_TABS))
                  aTabs[ iTabOut++ ] = CloneString(pszTab);
               }
            }
         }

      if (nCreds == 0)
         {
         if (REALLOC (aTabs, cTabs, 1+iTabOut, cREALLOC_TABS))
            aTabs[ iTabOut++ ] = CloneString (TEXT(""));
         }
      else for (iCreds = 0; iCreds < g.cCreds; ++iCreds)
         {
         if (g.aCreds[ iCreds ].szCell[0])
            {
            for (size_t ii = 0; ii < iTabOut; ++ii)
               {
               if (!ISCELLTAB (aTabs[ii]))
                  continue;
               if (!lstrcmpi (g.aCreds[ iCreds ].szCell, aTabs[ ii ]))
                  break;
               }
            if (ii == iTabOut)
               {
               if (REALLOC (aTabs, cTabs, 1+iTabOut, cREALLOC_TABS))
                  aTabs[ iTabOut++ ] = CloneString (g.aCreds[ iCreds ].szCell);
               }
            }
         }

      if (REALLOC (aTabs, cTabs, 1+iTabOut, cREALLOC_TABS))
         aTabs[ iTabOut++ ] = dwTABPARAM_MOUNT;

      if (g.fIsWinNT)
         {
         if (REALLOC (aTabs, cTabs, 1+iTabOut, cREALLOC_TABS))
            aTabs[ iTabOut++ ] = dwTABPARAM_ADVANCED;
         }

      // Now erase the current tabs, and re-add new ones. Remember which tab is
      // currently selected, so we can try to go back to it later.
      //
      int iTabSel = 0;
      if (TabCtrl_GetItemCount(hTab))
         {
         LPTSTR pszTabSel = (LPTSTR)GetTabParam (hTab, TabCtrl_GetCurSel(hTab));
         for (size_t iSel = 0; iSel < iTabOut; ++iSel)
            {
            if ((!ISCELLTAB(pszTabSel))  && (!ISCELLTAB(aTabs[iSel])) && (pszTabSel == aTabs[iSel]))
               iTabSel = iSel;
            else if (ISCELLTAB(pszTabSel) && ISCELLTAB(aTabs[iSel]) && !lstrcmpi (pszTabSel, aTabs[iSel]))
               iTabSel = iSel;
            }
         }

      int nTabs = TabCtrl_GetItemCount(hTab);
      for (int iTab = 0; iTab < nTabs; ++iTab)
         {
         LPTSTR pszTab = (LPTSTR)GetTabParam (hTab, iTab);
         if (ISCELLTAB(pszTab))
            FreeString (pszTab);
         }
      TabCtrl_DeleteAllItems (hTab);

      for (size_t ii = 0; ii < iTabOut; ++ii)
         {
         TCHAR szTitle[cchRESOURCE];
         if (aTabs[ii] == dwTABPARAM_ADVANCED)
            GetString (szTitle, IDS_ADVANCED);
         else if (aTabs[ii] == dwTABPARAM_MOUNT)
            GetString (szTitle, IDS_MOUNT);
         else if ((nCreds <= 1) || (aTabs[ii][0] == TEXT('\0')))
            GetString (szTitle, IDS_CREDENTIALS);
         else
            lstrcpy (szTitle, aTabs[ii]);

         TC_ITEM Item;
         memset (&Item, 0x00, sizeof(Item));
         Item.mask = TCIF_PARAM | TCIF_TEXT;
         Item.pszText = szTitle;
         Item.cchTextMax = cchRESOURCE;
         Item.lParam = (LPARAM)(aTabs[ii]);

         TabCtrl_InsertItem (hTab, ii, &Item);
         }

      if (aTabs != NULL)
         Free (aTabs);

      TabCtrl_SetCurSel (hTab, iTabSel);
      Main_OnSelectTab ();
      Main_EnableRemindTimer (TRUE);

      fInHere = FALSE;
      }
}


void Main_EnableRemindTimer (BOOL fEnable)
{
   KillTimer (g.hMain, ID_REMIND_TIMER);

   if (fEnable)
      SetTimer (g.hMain, ID_REMIND_TIMER, (ULONG)cmsec1MINUTE * cminREMIND_TEST, NULL);
}


size_t Main_FindExpiredCreds (void)
{
   for (size_t iCreds = 0; iCreds < g.cCreds; ++iCreds)
      {
      if (!g.aCreds[ iCreds ].szCell[0])
         continue;
      if (!g.aCreds[ iCreds ].fRemind)
         continue;

      SYSTEMTIME stNow;
      GetLocalTime (&stNow);

      FILETIME ftNow;
      SystemTimeToFileTime (&stNow, &ftNow);

      FILETIME ftExpires;
      SystemTimeToFileTime (&g.aCreds[ iCreds ].stExpires, &ftExpires);

      LONGLONG llNow = (((LONGLONG)ftNow.dwHighDateTime) << 32) + (LONGLONG)(ftNow.dwLowDateTime);
      LONGLONG llExpires = (((LONGLONG)ftExpires.dwHighDateTime) << 32) + (LONGLONG)(ftExpires.dwLowDateTime);

      llNow /= c100ns1SECOND;
      llExpires /= c100ns1SECOND;

      if (llExpires <= (llNow + (LONGLONG)cminREMIND_WARN * csec1MINUTE))
         return iCreds;
      }

   return (size_t)-1;
}


BOOL CALLBACK Terminate_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         Terminate_OnInitDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               Terminate_OnOK (hDlg);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;
            }
         break;
      }

   return FALSE;
}


void Terminate_OnInitDialog (HWND hDlg)
{
   BOOL fPersistent = IsServicePersistent();

   CheckDlgButton (hDlg, IDC_STARTUP, g.fStartup);
   CheckDlgButton (hDlg, IDC_LEAVE, fPersistent);
   CheckDlgButton (hDlg, IDC_STOP, !fPersistent);
}


void Terminate_OnOK (HWND hDlg)
{
   if (IsServiceRunning())
      {
      if (IsDlgButtonChecked (hDlg, IDC_STOP))
         {
         SC_HANDLE hManager;
         if ((hManager = OpenSCManager (NULL, NULL, SC_MANAGER_ALL_ACCESS)) != NULL)
            {
            SC_HANDLE hService;
            if ((hService = OpenService (hManager, TEXT("TransarcAFSDaemon"), SERVICE_ALL_ACCESS)) != NULL)
               {
               SERVICE_STATUS Status;
               ControlService (hService, SERVICE_CONTROL_STOP, &Status);

               CloseServiceHandle (hService);
               }

            CloseServiceHandle (hManager);
            }
         }
      }

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

   Quit();
   EndDialog (hDlg, IDOK);
}

