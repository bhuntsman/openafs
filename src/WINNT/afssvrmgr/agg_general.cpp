extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "svrmgr.h"
#include "agg_general.h"
#include "svr_general.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */


/*
 * AGGREGATE PREFERENCES ______________________________________________________
 *
 */

PVOID Aggregates_LoadPreferences (LPIDENT lpiAggregate)
{
   LPAGGREGATE_PREF pap = New (AGGREGATE_PREF);

   if (!RestorePreferences (lpiAggregate, pap, sizeof(AGGREGATE_PREF)))
      {
      pap->perWarnAggFull = -1;	// use the server's default value
      pap->fWarnAggAlloc = FALSE;

      Alert_SetDefaults (&pap->oa);
      }

   Alert_Initialize (&pap->oa);
   return pap;
}


BOOL Aggregates_SavePreferences (LPIDENT lpiAggregate)
{
   BOOL rc = FALSE;

   PVOID pap = lpiAggregate->GetUserParam();
   if (pap != NULL)
      {
      rc = StorePreferences (lpiAggregate, pap, sizeof(AGGREGATE_PREF));
      }

   return rc;
}


/*
 * ROUTINES ___________________________________________________________________
 *
 */

LPIDENT Aggregates_GetFocused (HWND hDlg)
{
   return (LPIDENT)FL_GetFocusedData (GetDlgItem (hDlg, IDC_AGG_LIST));
}


LPIDENT Aggregates_GetSelected (HWND hDlg)
{
   return (LPIDENT)FL_GetSelectedData (GetDlgItem (hDlg, IDC_AGG_LIST));
}

