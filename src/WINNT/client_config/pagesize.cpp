/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
 */
extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <windows.h>
#include <stdlib.h>
#include "pagesize.h"


ULONG ExtractPageSize (LPCTSTR psz)
{
   LPCTSTR pch = &psz[ lstrlen(psz) ];
   while ((pch > psz) && (isdigit(pch[-1])))
      pch--;
   return atol(pch);
}


ULONG GetPagingSpace (void)
{
   ULONG ckPageSpace = 0;

   HKEY hk;
   if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, TEXT("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management"), 0, KEY_QUERY_VALUE, &hk) == ERROR_SUCCESS)
      {
      TCHAR mszData[1024] = TEXT("");
      DWORD dwSize = sizeof(mszData);
      DWORD dwType = REG_MULTI_SZ;

      if (RegQueryValueEx (hk, TEXT("PagingFiles"), 0, &dwType, (PBYTE)mszData, &dwSize) == ERROR_SUCCESS)
         {
         for (LPTSTR psz = mszData; *psz; psz += 1+lstrlen(psz))
            {
            ckPageSpace += ExtractPageSize (psz);
            }
         }

      RegCloseKey (hk);
      }

   return ckPageSpace * 1024;
}

