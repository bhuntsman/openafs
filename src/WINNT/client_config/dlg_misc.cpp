extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}
#include <stdio.h>
#include "afs_config.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define nLANA_MIN             0
#define nLANA_MAX             10

#define csecPROBE_MIN         1
#define csecPROBE_MAX         600

#define cTHREADS_MIN          1
#define cTHREADS_MAX          128

#define cDAEMONS_MIN          1
#define cDAEMONS_MAX          128


// Our dialog data
BOOL fFirstTime = TRUE;
DWORD nLanAdapter;
DWORD csecProbe;
DWORD nThreads;
DWORD nDaemons;
TCHAR szSysName[ MAX_PATH ];
TCHAR szRootVolume[ MAX_PATH ];
TCHAR szMountDir[ MAX_PATH ];


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Misc_OnInitDialog (HWND hDlg);
void Misc_OnOK(HWND hDlg);
void Misc_OnCancel(HWND hDlg);
BOOL Misc_OnApply();


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK Misc_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         Misc_OnInitDialog (hDlg);
         break;

      case WM_CTLCOLORSTATIC:
         if ((HWND)lp == GetDlgItem (hDlg, IDC_CHUNK_SIZE))
            {
            if (IsWindowEnabled ((HWND)lp))
               {
               static HBRUSH hbrStatic = CreateSolidBrush (GetSysColor (COLOR_WINDOW));
               SetTextColor ((HDC)wp, GetSysColor (COLOR_WINDOWTEXT));
               SetBkColor ((HDC)wp, GetSysColor (COLOR_WINDOW));
               return (BOOL)hbrStatic;
               }
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDHELP:
               Misc_DlgProc (hDlg, WM_HELP, 0, 0);
               break;

            case IDOK:
                Misc_OnOK(hDlg);
                break;
                
            case IDCANCEL:
                Misc_OnCancel(hDlg);
                break;
            }
         break;

      case WM_HELP:
         WinHelp (hDlg, g.szHelpFile, HELP_CONTEXT, IDH_AFSCONFIG_ADVANCED_MISC);
         break;
      }

   return FALSE;
}


void Misc_OnInitDialog (HWND hDlg)
{
   if (fFirstTime) {
      Config_GetLanAdapter(&g.Configuration.nLanAdapter);
      Config_GetProbeInt (&g.Configuration.csecProbe);
      Config_GetNumThreads (&g.Configuration.nThreads);
      Config_GetNumDaemons (&g.Configuration.nDaemons);
      Config_GetSysName (g.Configuration.szSysName);
      Config_GetRootVolume (g.Configuration.szRootVolume);
      Config_GetMountRoot (g.Configuration.szMountDir);

      nLanAdapter = g.Configuration.nLanAdapter;
      csecProbe = g.Configuration.csecProbe;
      nThreads = g.Configuration.nThreads;
      nDaemons = g.Configuration.nDaemons;
      lstrcpy(szSysName, g.Configuration.szSysName);
      lstrcpy(szRootVolume, g.Configuration.szRootVolume);
      lstrcpy(szMountDir, g.Configuration.szMountDir);

      fFirstTime = FALSE;
   }

   CreateSpinner (GetDlgItem (hDlg, IDC_LAN_ADAPTER), 10, FALSE, nLANA_MIN, nLanAdapter, nLANA_MAX);
   CreateSpinner (GetDlgItem (hDlg, IDC_PROBE), 10, FALSE, csecPROBE_MIN, csecProbe, csecPROBE_MAX);
   CreateSpinner (GetDlgItem (hDlg, IDC_THREADS), 10, FALSE, cTHREADS_MIN, nThreads, cTHREADS_MAX);
   CreateSpinner (GetDlgItem (hDlg, IDC_DAEMONS), 10, FALSE, cDAEMONS_MIN, nDaemons, cDAEMONS_MAX);
   
   SetDlgItemText (hDlg, IDC_SYSNAME, szSysName);
   SetDlgItemText (hDlg, IDC_ROOTVOLUME, szRootVolume);
   SetDlgItemText (hDlg, IDC_MOUNTDIR, szMountDir);
}

void Misc_OnOK (HWND hDlg)
{
   nLanAdapter = SP_GetPos (GetDlgItem (hDlg, IDC_LAN_ADAPTER));

   csecProbe = SP_GetPos (GetDlgItem (hDlg, IDC_PROBE));
  
   nThreads = SP_GetPos (GetDlgItem (hDlg, IDC_THREADS));
   
   nDaemons = SP_GetPos (GetDlgItem (hDlg, IDC_DAEMONS));
   
   GetDlgItemText (hDlg, IDC_SYSNAME, szSysName, sizeof(szSysName));
   GetDlgItemText (hDlg, IDC_ROOTVOLUME, szRootVolume, sizeof(szRootVolume));
   GetDlgItemText (hDlg, IDC_MOUNTDIR, szMountDir, sizeof(szMountDir));
   
   EndDialog(hDlg, IDOK);
}


BOOL Misc_OnApply()
{
   if (fFirstTime)
      return TRUE;
   
   if (nLanAdapter != g.Configuration.nLanAdapter) {
      if (!Config_SetLanAdapter (nLanAdapter))
         return FALSE;
      g.Configuration.nLanAdapter = nLanAdapter;
   }

   if (csecProbe != g.Configuration.csecProbe) {
      if (!Config_SetProbeInt (csecProbe))
         return FALSE;
      g.Configuration.csecProbe = csecProbe;
   }
   
   if (nThreads != g.Configuration.nThreads) {
      if (!Config_SetNumThreads (nThreads))
         return FALSE;
      g.Configuration.nThreads = nThreads;
   }
   
   if (nDaemons != g.Configuration.nDaemons) {
      if (!Config_SetNumDaemons (nDaemons))
         return FALSE;
      g.Configuration.nDaemons = nDaemons;
   }
   
   if (lstrcmp(szSysName, g.Configuration.szSysName) != 0) {
      if (!Config_SetSysName (szSysName))
         return FALSE;
      lstrcpy(g.Configuration.szSysName, szSysName);
   }
   
   if (lstrcmp(szRootVolume, g.Configuration.szRootVolume) != 0) {
      if (!Config_SetRootVolume (szRootVolume))
         return FALSE;
      lstrcpy(g.Configuration.szRootVolume, szRootVolume);
   }
   
   if (lstrcmp(szMountDir, g.Configuration.szMountDir) != 0) {
      if (!Config_SetMountRoot (szMountDir))
         return FALSE;
      lstrcpy(g.Configuration.szMountDir, szMountDir);
   }

   return TRUE;
}

void Misc_OnCancel(HWND hDlg)
{
   fFirstTime = TRUE;

   EndDialog(hDlg, IDCANCEL);
}

