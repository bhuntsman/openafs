extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "TaAfsUsrMgr.h"
#include "mch_delete.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Machine_Delete_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Machine_Delete_OnInitDialog (HWND hDlg);
void Machine_Delete_OnDestroy (HWND hDlg);
void Machine_Delete_OnCheck (HWND hDlg);
void Machine_Delete_OnOK (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Machine_ShowDelete (LPASIDLIST pMachineList)
{
   ModalDialogParam (IDD_MACHINE_DELETE, g.hMain, Machine_Delete_DlgProc, (LPARAM)pMachineList);
}


BOOL CALLBACK Machine_Delete_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_MACHINE_DELETE, hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG:
         SetWindowLong (hDlg, DWL_USER, lp);
         Machine_Delete_OnInitDialog (hDlg);
         break;

      case WM_DESTROY:
         Machine_Delete_OnDestroy (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
               Machine_Delete_OnOK (hDlg);
               EndDialog (hDlg, IDOK);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;
            }
         break;
      }

   return FALSE;
}


void Machine_Delete_OnInitDialog (HWND hDlg)
{
   LPASIDLIST pMachineList = (LPASIDLIST)GetWindowLong (hDlg, DWL_USER);

   // Fix the title of the dialog
   //
   if (pMachineList->cEntries == 1)
      {
      ULONG status;
      TCHAR szName[ cchNAME ];
      asc_ObjectNameGet_Fast (g.idClient, g.idCell, pMachineList->aEntries[0].idObject, szName, &status);

      TCHAR szText[ cchRESOURCE ];
      GetDlgItemText (hDlg, IDC_DELETE_TITLE, szText, cchRESOURCE);

      LPTSTR pszText = FormatString (szText, TEXT("%s"), szName);
      SetDlgItemText (hDlg, IDC_DELETE_TITLE, pszText);
      FreeString (pszText);
      }
   else
      {
      LPTSTR pszNames = CreateNameList (pMachineList);

      LPTSTR pszText = FormatString (IDS_MACHINE_DELETE_MULTIPLE, TEXT("%s"), pszNames);
      SetDlgItemText (hDlg, IDC_DELETE_TITLE, pszText);
      FreeString (pszText);

      FreeString (pszNames);
      }
}


void Machine_Delete_OnDestroy (HWND hDlg)
{
   LPASIDLIST pMachineList = (LPASIDLIST)GetWindowLong (hDlg, DWL_USER);
   asc_AsidListFree (&pMachineList);
}


void Machine_Delete_OnOK (HWND hDlg)
{
   LPASIDLIST pMachineList = (LPASIDLIST)GetWindowLong (hDlg, DWL_USER);

   // Start a background task to do all the work.
   //
   LPUSER_DELETE_PARAMS pTask = New (USER_DELETE_PARAMS);
   memset (pTask, 0x00, sizeof(USER_DELETE_PARAMS));
   pTask->fDeleteKAS = FALSE;
   pTask->fDeletePTS = TRUE;
   asc_AsidListCopy (&pTask->pUserList, &pMachineList);
   StartTask (taskUSER_DELETE, NULL, pTask);
}

