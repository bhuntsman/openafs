extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "svrmgr.h"
#include "cmdline.h"
#include "action.h"
#include "creds.h"


/*
 * VARIABLES __________________________________________________________________
 *
 */

typedef enum {
   swCELL,
   swSUBSET,
   swSERVER,
   swRESET,
   swCONFIRM,
   swUSER,
   swPASSWORD,
   swLOOKUP,
} SWITCH;

static struct {
   TCHAR szSwitch[ cchRESOURCE ];
   BOOL fGetsValue;
   BOOL fPresent;
   TCHAR szValue[ cchRESOURCE ];
} aSWITCHES[] = {
   { TEXT("cell"),      TRUE  },
   { TEXT("subset"),    TRUE  },
   { TEXT("server"),    TRUE  },
   { TEXT("reset"),     FALSE },
   { TEXT("confirm"),   FALSE },
   { TEXT("user"),      TRUE  },
   { TEXT("password"),  TRUE  },
   { TEXT("lookup"),    FALSE }
};

#define nSWITCHES (sizeof(aSWITCHES) / sizeof(aSWITCHES[0]))


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

#define iswhite(_ch) ( ((_ch)==TEXT(' ')) || ((_ch)==TEXT('\t')) )

void cdecl CommandLineHelp (int ids, LPTSTR pszFormat = NULL, ...);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

CMDLINEOP ParseCommandLine (LPTSTR pszCmdLine)
{
   for (size_t ii = 0; ii < nSWITCHES; ++ii)
      aSWITCHES[ ii ].fPresent = FALSE;

   // Search through pszCmdLine for switches; each switch must be
   // preceeded by "/" or "-".
   //
   while (pszCmdLine && *pszCmdLine)
      {
      while (iswhite(*pszCmdLine))
         ++pszCmdLine;
      if (!*pszCmdLine)
         break;

      if ( (*pszCmdLine != '-') && (*pszCmdLine != '/') )
         {
         CommandLineHelp (IDS_CMDLINE_SYNTAX);
         return opCLOSEAPP;
         }

      ++pszCmdLine;

      // Okay, we've found what is probably the start of a switch.
      // See if it matches anything.
      //
      for (ii = 0; ii < nSWITCHES; ++ii)
         {
         size_t cch = lstrlen(aSWITCHES[ ii ].szSwitch);
         if (lstrncmpi (pszCmdLine, aSWITCHES[ ii ].szSwitch, cch))
            continue;

         // If the switch wants a value, it must be followed by ":"
         // or whitespace; if it doesn't, it must be followed by "/"
         // or whitespace.
         //
         LPTSTR pszAfter = &pszCmdLine[ cch ];
         if (iswhite (*pszAfter) || (!*pszAfter) ||
             ((*pszAfter == TEXT(':')) && (aSWITCHES[ ii ].fGetsValue)) ||
             ((*pszAfter == TEXT('/')) && (!aSWITCHES[ ii ].fGetsValue)) )
            {
            break; // found a switch!
            }
         }

      if (ii >= nSWITCHES)
         {
         TCHAR szCopy[ cchRESOURCE ];
         lstrcpy (szCopy, pszCmdLine);
         for (LPTSTR pch = szCopy;
              *pch && !iswhite(*pch) && !(*pch == TEXT('/')) && !(*pch == TEXT(':'));
              ++pch)
            ;
         *pch = TEXT('\0');
         CommandLineHelp (IDS_CMDLINE_UNRECOGNIZED, TEXT("%s"), szCopy);
         return opCLOSEAPP;
         }
      if (aSWITCHES[ ii ].fPresent)
         {
         CommandLineHelp (IDS_CMDLINE_DUPLICATE, TEXT("%s"), aSWITCHES[ ii ].szSwitch);
         return opCLOSEAPP;
         }

      // Woo hoo!  Found what appears to be a valid switch.  Parse it now.
      //
      aSWITCHES[ ii ].fPresent = TRUE;
      pszCmdLine += lstrlen( aSWITCHES[ ii ].szSwitch );
      while (iswhite (*pszCmdLine))
         ++pszCmdLine;

      if (*pszCmdLine == TEXT(':'))
         {
         if (!aSWITCHES[ ii ].fGetsValue)
            {
            CommandLineHelp (IDS_CMDLINE_UNEXPECTVALUE, TEXT("%s"), aSWITCHES[ ii ].szSwitch);
            return opCLOSEAPP;
            }
         for (++pszCmdLine; iswhite (*pszCmdLine); ++pszCmdLine)
            ;
         }

      if (aSWITCHES[ ii ].fGetsValue)
         {
         if ( (*pszCmdLine == TEXT('/')) || (*pszCmdLine == TEXT('\0')) )
            {
            CommandLineHelp (IDS_CMDLINE_MISSINGVAL, TEXT("%s"), aSWITCHES[ ii ].szSwitch);
            return opCLOSEAPP;
            }
         BOOL fQuoted = FALSE;
         for (LPTSTR pszTarget = aSWITCHES[ ii ].szValue;
              *pszCmdLine && !(*pszCmdLine == TEXT('/') && !fQuoted)
                          && !(iswhite(*pszCmdLine) && !fQuoted); )
            {
            if (*pszCmdLine == TEXT('"'))
               {
               ++pszCmdLine;
               if (fQuoted)
                  break;
               fQuoted = TRUE;
               continue;
               }
            *pszTarget++ = *pszCmdLine++;
            }
         *pszTarget++ = TEXT('\0');
         }
      }

   // Was the -CONFIRM switch given?  It works with any other switch
   // combination.
   //
   if (aSWITCHES[ swCONFIRM ].fPresent)
      {
      Action_ShowConfirmations (TRUE);
      }

   // Now test the command-line for syntactical correctness.
   // First test: if the SUBSET switch is given, the CELL switch must
   // be given as well.
   //
   if ( (aSWITCHES[ swSUBSET ].fPresent) &&
        (!aSWITCHES[ swCELL ].fPresent) )
      {
      CommandLineHelp (IDS_CMDLINE_SUBSETNOTCELL);
      return opCLOSEAPP;
      }

   // Similarly, if the SERVER switch is given, the CELL switch must
   // be given as well.
   //
   if ( (aSWITCHES[ swSERVER ].fPresent) &&
        (!aSWITCHES[ swCELL ].fPresent) )
      {
      CommandLineHelp (IDS_CMDLINE_SERVERNOTCELL);
      return opCLOSEAPP;
      }

   // And if the USER or PASSWORD switch is given, the other is required.
   //
   if ( (aSWITCHES[ swUSER ].fPresent && !aSWITCHES[ swPASSWORD ].fPresent) ||
        (aSWITCHES[ swPASSWORD ].fPresent && !aSWITCHES[ swUSER ].fPresent) )
      {
      CommandLineHelp (IDS_CMDLINE_USERPASSWORD);
      return opCLOSEAPP;
      }

   // Implement the command-line switches.
   //
   if (aSWITCHES[ swRESET ].fPresent)
      {
      if (aSWITCHES[ swSERVER ].fPresent)
         {
         ErasePreferences (aSWITCHES[ swCELL ].szValue, aSWITCHES[ swSERVER ].szValue);
         }
      else if (aSWITCHES[ swCELL ].fPresent)
         {
         ErasePreferences (aSWITCHES[ swCELL ].szValue);
         }
      else // neither cell nor server specified--kill just the general stuff
         {
         EraseSettings (REGSTR_SETTINGS_BASE, REGSTR_SETTINGS_PATH, REGVAL_SETTINGS);
         }

      Message (MB_OK | MB_ICONINFORMATION, IDS_CMDLINE_RESET_TITLE, IDS_CMDLINE_RESET_DESC);
      return opCLOSEAPP;
      }

   if (aSWITCHES[ swUSER ].fPresent)
      {
      LPTSTR pszCell = (aSWITCHES[ swCELL ].fPresent) ? (aSWITCHES[ swCELL ].szValue) : NULL;

      ULONG status;
      if (!AfsAppLib_SetCredentials (pszCell, aSWITCHES[ swUSER ].szValue, aSWITCHES[ swPASSWORD ].szValue, &status))
         {
         ImmediateErrorDialog (status, IDS_ERROR_BAD_CREDENTIALS);
         return opCLOSEAPP;
         }
      }

   if (aSWITCHES[ swLOOKUP ].fPresent)
      {
      return opLOOKUPERRORCODE;
      }

   if (aSWITCHES[ swCELL ].fPresent)
      {
      LPOPENCELL_PACKET lpp = New (OPENCELL_PACKET);
      memset (lpp, 0x00, sizeof(OPENCELL_PACKET));

      lstrcpy (lpp->szCell, aSWITCHES[ swCELL ].szValue);
      lpp->fCloseAppOnFail = TRUE;

      if (aSWITCHES[ swSUBSET ].fPresent)
         {
         lpp->sub = Subsets_LoadSubset (lpp->szCell, aSWITCHES[ swSUBSET ].szValue);
         if (lpp->sub == NULL)
            {
            Delete (lpp);
            lpp = NULL;

            CommandLineHelp (IDS_CMDLINE_INVALIDSUBSET, TEXT("%s%s"), aSWITCHES[ swCELL ].szValue, aSWITCHES[ swSUBSET ].szValue);
            return opCLOSEAPP;
            }
         }
      else if (aSWITCHES[ swSERVER ].fPresent)
         {
         lpp->sub = New (SUBSET);
         memset (lpp->sub, 0x0, sizeof(SUBSET));
         FormatMultiString (&lpp->sub->pszMonitored, TRUE, TEXT("%1"), TEXT("%s"), aSWITCHES[ swSERVER ].szValue);
         }

      StartTask (taskOPENCELL, NULL, lpp);
      return opNOCELLDIALOG;
      }

   // Okay--nothing sufficiently special took place to prevent us
   // from running the tool normally.
   //
   return opNORMAL;
}


void cdecl CommandLineHelp (int ids, LPTSTR pszFormat, ...)
{
   va_list   arg;
   va_start (arg, pszFormat);
   vMessage (MB_OK | MB_ICONHAND, IDS_CMDLINE_TITLE, ids, pszFormat, arg);
}

