extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "TaAfsUsrMgr.h"
#include "usr_tab.h"
#include "usr_col.h"
#include "usr_search.h"
#include "command.h"
#include "window.h"


/*
 * RESIZING WINDOWS ___________________________________________________________
 *
 */

rwWindowData awdUsersTab[] = {
    { IDC_USERS_TITLE, raRepaint | raSizeX },
    { IDC_ADVANCED, raMoveX },
    { IDC_USERS_PATTERN, raMoveX },
    { IDC_USERS_PATTERN_PROMPT, raRepaint | raMoveX },
    { IDC_USERS_LIST, raSizeX | raSizeY },
    { M_USER_CREATE, raMoveX | raMoveY },
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

void Users_EnableButtons (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK Users_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (Display_HandleColumnNotify (hDlg, msg, wp, lp, &gr.viewUsr))
      return FALSE;

   switch (msg)
      {
      case WM_INITDIALOG:
         {
         RECT rTab;
         GetClientRect (GetParent(hDlg), &rTab);
         TabCtrl_AdjustRect (GetParent (hDlg), FALSE, &rTab); 
         ResizeWindow (hDlg, awdUsersTab, rwaMoveToHere, &rTab);

         HIMAGELIST hSmall = AfsAppLib_CreateImageList (FALSE);
         HIMAGELIST hLarge = AfsAppLib_CreateImageList (TRUE);
         FastList_SetImageLists (GetDlgItem (hDlg, IDC_USERS_LIST), hSmall, hLarge);

         FastList_SetSortFunction (GetDlgItem (hDlg, IDC_USERS_LIST), General_ListSortFunction);

         FL_RestoreView (GetDlgItem (hDlg, IDC_USERS_LIST), &gr.viewUsr);
         FastList_SetTextCallback (GetDlgItem (hDlg, IDC_USERS_LIST), Display_GetItemText, (DWORD)&gr.viewUsr);
         SetDlgItemText (hDlg, IDC_USERS_PATTERN, g.szPatternUsers);
         Users_EnableButtons(hDlg);
         Display_PopulateUserList();

         l.dwTickLastType = 0;
         }
         break;

      case WM_HELP:
         WinHelp (hDlg, cszHELPFILENAME, HELP_FINDER, 0);
         break;

      case WM_DESTROY:
         FL_StoreView (GetDlgItem (hDlg, IDC_USERS_LIST), &gr.viewUsr);
         break;

      case WM_SIZE:
         // if (lp==0), we're minimizing--don't call ResizeWindow().
         //
         if (lp != 0)
            ResizeWindow (hDlg, awdUsersTab, rwaFixupGuts);
         break;

      case WM_TIMER:
         switch (wp)
            {
            case ID_SEARCH_TIMER:
               if ( (l.dwTickLastType) && (GetTickCount() > l.dwTickLastType + msecSEARCH_TIMER) )
                  {
                  KillTimer (hDlg, ID_SEARCH_TIMER);
                  Display_PopulateUserList();
                  }
               break;
            }
         break;

      case WM_CONTEXTMENU:
         POINT ptScreen;
         ptScreen.x = LOWORD(lp);
         ptScreen.y = HIWORD(lp);
         OnRightClick (pmUSER, GetDlgItem (hDlg, IDC_USERS_LIST), &ptScreen);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_USERS_PATTERN:
               if (HIWORD(wp) == EN_UPDATE)
                  {
                  l.dwTickLastType = GetTickCount();
                  KillTimer (hDlg, ID_SEARCH_TIMER);
                  SetTimer (hDlg, ID_SEARCH_TIMER, msecSEARCH_TIMER +15, NULL);
                  }
               break;

            case IDC_ADVANCED:
               Users_ShowAdvancedSearch (g.hMain);
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
               Users_EnableButtons(hDlg);
               break;

            case FLN_LDBLCLICK:
               PostMessage (hDlg, WM_COMMAND, MAKELONG(M_PROPERTIES,BN_CLICKED), (LPARAM)GetDlgItem (hDlg, M_PROPERTIES));
               break;
            }
         break;
      }

   return FALSE;
}


void Users_EnableButtons (HWND hDlg)
{
   BOOL fEnable = (Display_GetSelectedCount() != 0) ? TRUE : FALSE;
   EnableWindow (GetDlgItem (hDlg, M_PROPERTIES), fEnable);
   EnableWindow (GetDlgItem (hDlg, M_MEMBERSHIP), fEnable);
}

