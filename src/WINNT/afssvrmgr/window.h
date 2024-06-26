#ifndef WINDOW_H
#define WINDOW_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define WORKING_FPS  8     // try for X frames per second


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Main_DialogProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Main_OnPreviewPane (BOOL fPreviewNew, BOOL fVertNew);
void Main_OnServerView (int lvsNew);
void Main_OnViewSets (void);

void Main_SetFilesetMenus (void);
void Main_SetActionMenus (void);
HWND Main_GetTabChild (HWND hTab);

DWORD WINAPI Main_Redraw_ThreadProc (PVOID lp);  // pass TRUE to invalidate

HWND GetTabDialog (void);

void Main_StartWorking (void);
void Main_StopWorking (void);

void Main_AnimateIcon (HWND hIcon, int *piFrameLast = NULL);

void Main_SetServerViewMenus (void);


#endif

