/* Copyright Transarc Corporation 1998 - All Rights Reserved.
 *
 *
 * LWP_WaitForKeystroke - wait indefinitely or for a specified number of
 * seconds for keyboard input.
 *
 * If seconds < 0, LWP_WaitForKeystroke will wait indefinitely. 
 * If seconds == 0, LWP_WaitForKeystroke will just determine if data is now
 *	present.
 * Otherwise, wait "seconds" for data.
 *
 * Return 1 if data available.
 */

#include <afs/param.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <time.h>
#include <conio.h>
#include <assert.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif
#include "lwp.h"

#define LWP_KEYSTROKE_DELAY   250 /* 250ms. Must be < 1000 */
#define LWP_MAXLINELEN  256

#ifdef AFS_NT40_ENV
/* LWP_WaitForKeystroke : Wait until a key has been struck or time (secconds)
 * runs out and return to caller. The NT version of this function will return
 * immediately after a key has been pressed (doesn't wait for cr).
 * Input:
 *   seconds: wait for <seconds> seconds before returning. If seconds < 0,
 *            wait infinitely.
 * Return Value:
 *    1:  Keyboard input available
 *    0:  seconds elapsed. Timeout.
 */
int LWP_WaitForKeystroke(int seconds)
{
  time_t startTime, nowTime;
  double timeleft = 1;
  struct timeval twait;

  time(&startTime); 

  twait.tv_sec = 0;
  twait.tv_usec = LWP_KEYSTROKE_DELAY;

  if (seconds >= 0)
      timeleft = seconds; 

  do
  {
      /* check if we have a keystroke */
      if (_kbhit()) 
	  return 1;
      
      if (timeleft == 0)
	  break;

      /* sleep for  LWP_KEYSTROKE_DELAY ms and let other
       * process run some*/
      IOMGR_Select(0, 0, 0, 0, &twait);

      if (seconds > 0) { /* we only worry about elapsed time if 
			   * not looping forever (seconds < 0) */
	  /* now check elapsed time */
	  time(&nowTime);
	  timeleft = seconds - difftime(nowTime, startTime);
      }
  }
  while(timeleft > 0);

  return 0;
}

/* LWP_GetLine() - Waits indefinitely until a newline has been typed
 * and then returns the line typed.
 * 
 * This is trivial in unix, but requires some processing on NT.
 *   we basically read all chars into a buffer until we hit a newline and
 *   then return it to the user.
 * Return Value:
 *   n - a whole line has been read.(has n chars)
 *   0 - buf not big enough.
 */

int LWP_GetLine(char *linebuf, int len)
{
  int cnt = 0;
  char ch = '\0';

  fflush(stdin);
  /* loop until a new line has been entered */
  while (ch != '\r' && cnt < len-1) 
    { 
      LWP_WaitForKeystroke(-1);
      ch = getch(); 
      if (ch == '\b') {/* print and throw away a backspace */
	if (!cnt) /* if we are at the start of the line don't bspace */
	  continue;
	/* print a space to delete char and move cursor back */
	printf("\b \b");
	cnt--; 
      }
      else {
	putchar(ch);
	linebuf[cnt++] = ch;
      }
    }

  if (ch == '\r') { /* got a cr. translate to nl */
    linebuf[cnt-1] = '\n'; 
    linebuf[cnt]='\0';
    putchar('\n');
    return cnt;
  }
  else { /* buffer too small */
    linebuf[cnt] = '\0';
    return 0;
  }
    
}
#else
/* LWP_WaitForKeystroke(Unix) :Wait until a key has been struck or time (secconds)
 * runs out and return to caller. The Unix version will actually wait until
 * a <cr> has been entered before returning. 
 * Input:
 *   seconds: wait for <seconds> seconds before returning. If seconds < 0,
 *            wait infinitely.
 * Return Value:
 *    1:  Keyboard input available
 *    0:  seconds elapsed. Timeout.
 */
int LWP_WaitForKeystroke(int seconds)
{
    fd_set rdfds;
    int code;
    struct timeval twait;
    struct timeval *tp = NULL;

#ifdef AFS_LINUX20_ENV
    if (stdin->_IO_read_ptr < stdin->_IO_read_end)
	return 1;
#else
    if (stdin->_cnt > 0)
	return 1;
#endif


    FD_ZERO(&rdfds);
    FD_SET(fileno(stdin), &rdfds);

    if (seconds>=0) {
	twait.tv_sec = seconds;
	twait.tv_usec = 0;
	tp = &twait;
    }

    code = IOMGR_Select(1+fileno(stdin), &rdfds, NULL, NULL, tp);

    return (code == 1) ? 1 : 0;
}

/* LWP_GetLine() - Waits indefinitely until a newline has been typed
 * and then returns the line typed.
 * 
 * This is trivial in unix, but requires some processing on NT.
 *   we basically read all chars into a buffer until we hit a newline and
 *   then return it to the user.
 * Return Value:
 *   n - a whole line has been read.(has n chars)
 *   0 - buf not big enough.
 */

int LWP_GetLine(char *linebuf, int len)
{
  int linelen;

  LWP_WaitForKeystroke(-1);

  fgets(linebuf, len, stdin);
  linelen = strlen(linebuf);
  if (linebuf[linelen-1] != '\n') /* buffer too small */
    return 0;
  else
    return linelen;
}
  
#endif /* else NT40*/

/* LWP_GetResponseKey() - Waits for a specified period of time and
 * returns a char when one has been typed by the user.
 * Input:
 *    seconds - how long to wait for a key press.
 *    *key    - char entered by user
 * Return Values: 
 *    0 - Time ran out before the user typed a key.
 *    1 - Valid char is being returned.
 */

int LWP_GetResponseKey(int seconds, char *key)
{
  int rc;

  if (key == NULL)
    return 0;     /* need space to store char */

  
  fflush(stdin); /* flush all existing data and start anew */

  
  rc = LWP_WaitForKeystroke(seconds);
  if (rc == 0) { /* time ran out */
    *key = 0;
    return rc;
  }
  
  /* now read the char. */
#ifdef AFS_NT40_ENV
  *key = getche(); /* get char and echo it to screen */
#else
  *key = getchar();
#endif

  return rc;
}
