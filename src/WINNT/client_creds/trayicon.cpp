extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "afscreds.h"



/*
 * ROUTINES ___________________________________________________________________
 *
 */

void ChangeTrayIcon (int nim)
{
   static BOOL fAdded = FALSE;
   static BOOL fDeleted = FALSE;
   if ((nim == NIM_MODIFY) && (!fAdded))
      nim = NIM_ADD;
   if ((nim == NIM_MODIFY) && (fDeleted))
      return;

   if ((nim != NIM_DELETE) || (IsWindow (g.hMain)))
      {
      static HICON ICON_CREDS_YES = TaLocale_LoadIcon (IDI_CREDS_YES);
      static HICON ICON_CREDS_NO  = TaLocale_LoadIcon (IDI_CREDS_NO);

      size_t iExpired = Main_FindExpiredCreds();

      NOTIFYICONDATA nid;
      memset (&nid, 0x00, sizeof(NOTIFYICONDATA));
      nid.cbSize = sizeof(NOTIFYICONDATA);
      nid.hWnd = g.hMain;
      nid.uID = 0;
      nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
      nid.uCallbackMessage = WM_TRAYICON;
      nid.hIcon = ((g.cCreds != 0) && (iExpired == (size_t)-1)) ? ICON_CREDS_YES : ICON_CREDS_NO;
      GetString (nid.szTip, (g.fIsWinNT) ? IDS_TOOLTIP : IDS_TOOLTIP_95);
      Shell_NotifyIcon (nim, &nid);
      }

   if (nim == NIM_ADD)
      fAdded = TRUE;
   if (nim == NIM_DELETE)
      fDeleted = TRUE;
}

