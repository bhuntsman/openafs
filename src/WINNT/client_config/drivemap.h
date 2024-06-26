#ifndef DRIVEMAP_H
#define DRIVEMAP_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define chDRIVE_A   TEXT('A')
#define chDRIVE_B   TEXT('B')
#define chDRIVE_C   TEXT('C')
#define chDRIVE_D   TEXT('D')
#define chDRIVE_E   TEXT('E')
#define chDRIVE_F   TEXT('F')
#define chDRIVE_G   TEXT('G')
#define chDRIVE_H   TEXT('H')
#define chDRIVE_I   TEXT('I')
#define chDRIVE_J   TEXT('J')
#define chDRIVE_K   TEXT('K')
#define chDRIVE_L   TEXT('L')
#define chDRIVE_M   TEXT('M')
#define chDRIVE_N   TEXT('N')
#define chDRIVE_O   TEXT('O')
#define chDRIVE_P   TEXT('P')
#define chDRIVE_Q   TEXT('Q')
#define chDRIVE_R   TEXT('R')
#define chDRIVE_S   TEXT('S')
#define chDRIVE_T   TEXT('T')
#define chDRIVE_U   TEXT('U')
#define chDRIVE_V   TEXT('V')
#define chDRIVE_W   TEXT('W')
#define chDRIVE_X   TEXT('X')
#define chDRIVE_Y   TEXT('Y')
#define chDRIVE_Z   TEXT('Z')

typedef struct
   {
   TCHAR chDrive;	// chDRIVE_A through chDRIVE_Z
   TCHAR szMapping[ MAX_PATH ];
   TCHAR szSubmount[ MAX_PATH ];
   BOOL fPersistent;
   BOOL fActive;
   } DRIVEMAP, *PDRIVEMAP;

typedef struct
   {
   TCHAR szSubmount[ MAX_PATH ];
   TCHAR szMapping[ MAX_PATH ];
   BOOL fInUse;
   } SUBMOUNT, *PSUBMOUNT;

typedef struct
   {
   DRIVEMAP aDriveMap[26];	// Drive[0]=A, Drive[1]=B, etc
   PSUBMOUNT aSubmounts;
   size_t cSubmounts;
   } DRIVEMAPLIST, *PDRIVEMAPLIST;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL IsValidSubmountName (LPTSTR pszSubmount);
void QueryDriveMapList (PDRIVEMAPLIST pList);
void WriteDriveMappings (PDRIVEMAPLIST pList);
void FreeDriveMapList (PDRIVEMAPLIST pList);
BOOL ActivateDriveMap (TCHAR chDrive, LPTSTR pszMapping, LPTSTR pszSubmountReq, BOOL fPersistent, DWORD *pdwStatus = NULL);
BOOL InactivateDriveMap (TCHAR chDrive, DWORD *pdwStatus = NULL);
void AddSubMount (LPTSTR pszSubmount, LPTSTR pszMapping);
void RemoveSubMount (LPTSTR pszSubmount);
void AdjustAfsPath (LPTSTR pszTarget, LPCTSTR pszSource, BOOL fWantAFS, BOOL fWantForwardSlashes);
BOOL GetDriveSubmount (TCHAR chDrive, LPTSTR pszSubmountNow);
BOOL SubmountToPath (PDRIVEMAPLIST pList, LPTSTR pszPath, LPTSTR pszSubmount, BOOL fMarkInUse);
BOOL PathToSubmount (LPTSTR pszSubmount, LPTSTR pszMapping, LPTSTR pszSubmountReq, ULONG *pStatus);

#endif

