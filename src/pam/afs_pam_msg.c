#include <stdio.h>
#include <security/pam_appl.h>
#include "afs_pam_msg.h"
#include "afs_message.h"
#include <stdarg.h>


int pam_afs_printf(struct pam_conv *pam_convp, int error, int fmt_msgid, ...)
{
    va_list args;
    char buf[PAM_MAX_MSG_SIZE];
    char *fmt_msg = NULL;
    int freeit;
    struct pam_message mesg;
    struct pam_message *mesgp = &mesg;
    struct pam_response *resp = NULL;
    int errcode;

    if (pam_convp == NULL || pam_convp->conv == NULL)
	return PAM_CONV_ERR;

    fmt_msg = pam_afs_message(fmt_msgid, &freeit);
    va_start(args, fmt_msgid);
    vsprintf(buf, fmt_msg, args);
    va_end(args);
    if (freeit) free(fmt_msg);

    mesg.msg_style = error ? PAM_ERROR_MSG : PAM_TEXT_INFO;
    mesg.msg = buf;
    errcode = (*(pam_convp->conv))(1, &mesgp, &resp, pam_convp->appdata_ptr);
    if (resp) {
	if (resp->resp) free(resp->resp);
	free(resp);
    }
    return errcode;
}


int pam_afs_prompt(struct pam_conv *pam_convp, char **response,
		   int echo, int fmt_msgid, ...)
{
    va_list args;
    char buf[PAM_MAX_MSG_SIZE];
    char *fmt_msg = NULL;
    int freeit;
    struct pam_message mesg;
    struct pam_message *mesgp = &mesg;
    struct pam_response *resp = NULL;
    int errcode;

    if (pam_convp == NULL || pam_convp->conv == NULL || response == NULL)
	return PAM_CONV_ERR;

    *response = NULL;

    fmt_msg = pam_afs_message(fmt_msgid, &freeit);
    va_start(args, fmt_msgid);
    vsprintf(buf, fmt_msg, args);
    va_end(args);
    if (freeit) free(fmt_msg);

    mesg.msg_style = echo ? PAM_PROMPT_ECHO_ON : PAM_PROMPT_ECHO_OFF;
    mesg.msg = buf;

    errcode = (*(pam_convp->conv))(1, &mesgp, &resp, pam_convp->appdata_ptr);
    if (resp) {
	*response = resp->resp;

	free(resp);		/* but not resp->resp */
    }
    return errcode;
}

