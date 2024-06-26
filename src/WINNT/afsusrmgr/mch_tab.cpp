extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "TaAfsUsrMgr.h"
#include "mch_tab.h"
#include "mch_col.h"
#include "command.h"
#include "window.h"


/*
 * RESIZING WINDOWS ___________________________________________________________
 *
 */

rwWindowData awdMachinesTab[] = {
    { IDC_MACHINES_TITLE, raRepaint | raSizeX },
    { IDC_ADVANCED, raMoveX },
    { IDC_MACHINES_PATTERN, raMoveX },
    { IDC_MACHINES_PATTERN_PROMPT, raRepaint | raMoveX },
    { IDC_MACHINES_LIST, raSizeX | raSizeY },
    { M_MACHINE_CREATE, raMoveX | raMoveY },
    { M_MEMBERSHIP, raMoveX | raMoveY },
    { M_PROPERTIES, raMoveX | raMoveY },
    { IDC_STATIC, raRepaint },
    { idENDLIST, 0 }
 };


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define ID_SEARCH_TIMER     0

#define msecSEARCH_TIMER  650

static struct
   {
   DWORD dwTickLastType;
   } l;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Machines_EnableButtons (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK Machines_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (Display_HandleColumnNotify (hDlg, msg, wp, lp, &gr.viewMch))
      return FALSE;

   switch (msg)
      {
      case WM_INITDIALOG:
         {
         RECT rTab;
         GetClientRect (GetParent(hDlg), &rTab);
         TabCtrl_AdjustRect (GetParent (hDlg), FALSE, &rTab); 
         ResizeWindow (hDlg, awdMachinesTab, rwaMoveToHere, &rTab);

         HIMAGELIST hSmall = AfsAppLib_CreateImageList (FALSE);
         HIMAGELIST hLarge = AfsAppLib_CreateImageList (TRUE);
         FastList_SetImageLists (GetDlgItem (hDlg, IDC_MACHINES_LIST), hSmall, hLarge);

         FastList_SetSortFunction (GetDlgItem (hDlg, IDC_MACHINES_LIST), General_ListSortFunction);

         FL_RestoreView (GetDlgItem (hDlg, IDC_MACHINES_LIST), &gr.viewMch);
         FastList_SetTextCallback (GetDlgItem (hDlg, IDC_MACHINES_LIST), Display_GetItemText, (DWORD)&gr.viewMch);
         SetDlgItemText (hDlg, IDC_MACHINES_PATTERN, g.szPatternMachines);
         Machines_EnableButtons(hDlg);
         Display_PopulateMachineList();

         l.dwTickLastType = 0;
         }
         break;

      case WM_HELP:
         WinHelp (hDlg, cszHELPFILENAME, HELP_FINDER, 0);
         break;

      case WM_DESTROY:
         FL_StoreView (GetDlgItem (hDlg, IDC_MACHINES_LIST), &gr.viewMch);
         break;

      case WM_SIZE:
         // if (lp==0), we're minimizing--don't call ResizeWindow().
         //
         if (lp != 0)
            ResizeWindow (hDlg, awdMachinesTab, rwaFixupGuts);
         break;

      case WM_TIMER:
         switch (wp)
            {
            case ID_SEARCH_TIMER:
               if ( (l.dwTickLastType) && (GetTickCount() > l.dwTickLastType + msecSEARCH_TIMER) )
                  {
                  KillTimer (hDlg, ID_SEARCH_TIMER);
                  Display_PopulateMachineList();
                  }
               break;
            }
         break;

      case WM_CONTEXTMENU:
         POINT ptScreen;
         ptScreen.x = LOWORD(lp);
         ptScreen.y = HIWORD(lp);
         OnRightClick (pmMACHINE, GetDlgItem (hDlg, IDC_MACHINES_LIST), &ptScreen);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_MACHINES_PATTERN:
               if (HIWORD(wp) == EN_UPDATE)
                  {
                  l.dwTickLastType = GetTickCount();
                  KillTimer (hDlg, ID_SEARCH_TIMER);
                  SetTimer (hDlg, ID_SEARCH_TIMER, msecSEARCH_TIMER +15, NULL);
                  }
               break;

            default:
               OnContextCommand (LOWORD(wp));
               break;
            }
         break;

      case WM_NOTIFY:
         switch (((LPNMHDR)lp)->code)
            {
            case FLN_ITEMSELECT:
               Main_SetMenus();
               Machines_EnableButtons(hDlg);
               break;

            case FLN_LDBLCLICK:
               PostMessage (hDlg, WM_COMMAND, MAKELONG(M_PROPERTIES,BN_CLICKED), (LPARAM)GetDlgItem (hDlg, M_PROPERTIES));
               break;
            }
         break;
      }

   return FALSE;
}


void Machines_EnableButtons (HWND hDlg)
{
   BOOL fEnable = (Display_GetSelectedCount() != 0) ? TRUE : FALSE;
   EnableWindow (GetDlgItem (hDlg, M_PROPERTIES), fEnable);
   EnableWindow (GetDlgItem (hDlg, M_MEMBERSHIP), fEnable);
}

