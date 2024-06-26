/*
 * (C) Copyright Transarc Corporation 1989
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 */

/*------------------------------------------------------------------------
 * objdict.c
 *
 * Description:
 *	Implementation of the gator object dictionary.
 *
 *------------------------------------------------------------------------*/

#include "gtxobjdict.h"			/*Interface for this module*/
#include <stdio.h>			/*Standard I/O package*/

static char mn[] = "gator_objdict";	/*Module name*/
static int objdict_debug;		/*Is debugging turned on?*/

/*------------------------------------------------------------------------
 * gator_objdict_init
 *
 * Description:
 *	Initialize the gator object dictionary package.
 *
 * Arguments:
 *	int adebug: Is debugging turned on?
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	*** MUST BE THE FIRST ROUTINE CALLED FROM
 *	      THIS PACKAGE ***
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int gator_objdict_init(adebug)
    int adebug;

{ /*gator_objdict_init*/

    static char	rn[] = "gator_objdict_init";	/*Routine name*/

    /*
      * Remember what our debugging setting is.
      */
    objdict_debug = adebug;

    if (objdict_debug)
      fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    /*
     * Finally, return the good news.
     */
    return(0);

} /*gator_objdict_init*/

/*------------------------------------------------------------------------
 * gator_objdict_add
 *
 * Description:
 *	Add an entry to the gator object dictionary.
 *
 * Arguments:
 *	struct onode *objtoadd: Ptr to object to add.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int gator_objdict_add(objtoadd)
    struct onode *objtoadd;

{ /*gator_objdict_add*/

    static char	rn[] = "gator_objdict_add"; /*Routine name*/

    if (objdict_debug)
      fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    /*
     * Finally, return the good news.
     */
    return(0);

} /*gator_objdict_add*/

/*------------------------------------------------------------------------
 * gator_objdict_delete
 *
 * Description:
 *	Delete an entry from the gator object dictionary.
 *
 * Arguments:
 *	struct onode *objtodelete: Ptr to object to delete.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int gator_objdict_delete(objtodelete)
    struct onode *objtodelete;

{ /*gator_objdict_delete*/

    static char	rn[] = "gator_objdict_delete";	/*Routine name*/

    if (objdict_debug)
      fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    /*
     * Finally, return the good news.
     */
    return(0);

} /*gator_objdict_delete*/

/*------------------------------------------------------------------------
 * gator_objdict_lookup
 *
 * Description:
 *	Look up a gator object by name.
 *
 * Arguments:
 *	char *nametofind: String name of desired onode.
 *
 * Returns:
 *	Ptr to desired onode if successful,
 *	Null pointer otherwise.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

struct onode *gator_objdict_lookup(nametofind)
    char *nametofind;

{ /*gator_objdict_lookup*/

    static char	rn[] = "gator_objdict_lookup";	/*Routine name*/

    if (objdict_debug)
      fprintf(stderr, "[%s:%s] Called\n", mn, rn);

    /*
     * Finally, return the good news.
     */
    return((struct onode *)0);

} /*gator_objdict_lookup*/
