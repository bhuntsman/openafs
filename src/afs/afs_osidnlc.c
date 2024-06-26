#include "../afs/param.h"       /*Should be always first*/
#include "../afs/sysincludes.h" /*Standard vendor system headers*/
#include "../afs/afsincludes.h" /*AFS-based standard headers*/
#include "afs/afs.h"
#include "../afs/lock.h"
#include "../afs/afs_stats.h"
#include "../afs/afs_osidnlc.h"

/* Things to do:
 *    also cache failed lookups.
 *    look into interactions of dnlc and readdir.
 *    cache larger names, perhaps by using a better,longer key (SHA) and discarding
 *    the actual name itself.
 *    precompute a key and stuff for @sys, and combine the HandleAtName function with
 *    this, since we're looking at the name anyway.  
 */

struct afs_lock afs_xdnlc;
extern struct afs_lock afs_xvcache;

dnlcstats_t dnlcstats;

#define NCSIZE 300
#define NHSIZE 256 /* must be power of 2. == NHASHENT in dir package */
struct nc *ncfreelist = (struct nc *)0;
static struct nc nameCache[NCSIZE];
struct nc *nameHash[NHSIZE];
/* Hash table invariants:
 *     1.  If nameHash[i] is NULL, list is empty
 *     2.  A single element in a hash bucket has itself as prev and next.
 */

typedef enum {osi_dnlc_enterT, InsertEntryT, osi_dnlc_lookupT, ScavengeEntryT, osi_dnlc_removeT, RemoveEntryT, osi_dnlc_purgedpT, osi_dnlc_purgevpT, osi_dnlc_purgeT } traceevt;

int afs_usednlc = 1; 

struct dt {
  traceevt event;
  unsigned char slot;
};

struct dt dnlctracetable[256]; 
int dnlct;

#define TRACE(e,s) /* if (dnlct == 256) dnlct = 0; dnlctracetable[dnlct].event = e; dnlctracetable[dnlct++].slot = s; */

#define dnlcHash(ts, hval) for (hval=0; *ts; ts++) { hval *= 173;  hval  += *ts;   }

static struct nc * GetMeAnEntry() {
  static unsigned int nameptr = 0; /* next bucket to pull something from */
  struct nc *tnc;
  int j;
  
  if (ncfreelist) {
    tnc = ncfreelist;
    ncfreelist = tnc->next;
    return tnc;
  }

  for (j=0; j<NHSIZE+2; j++, nameptr++) {
    if (nameptr >= NHSIZE) 
      nameptr =0;
    if (nameHash[nameptr])
      break;
  }

  TRACE(ScavengeEntryT, nameptr);
  tnc = nameHash[nameptr];
  if (!tnc)   /* May want to consider changing this to return 0 */
    osi_Panic("null tnc in GetMeAnEntry");

  if (tnc->prev == tnc) { /* only thing in list, don't screw around */
    nameHash[nameptr] = (struct nc *) 0;
    return (tnc);
  }

  tnc = tnc->prev;      /* grab oldest one in this bucket */
  /* remove it from list */
  tnc->next->prev = tnc->prev;
  tnc->prev->next = tnc->next;

  return (tnc);
}

static void InsertEntry(tnc)
  struct nc *tnc;
{
  unsigned int key; 
  key = tnc->key & (NHSIZE -1);

  TRACE(InsertEntryT, key);
  if(!nameHash[key]) {
    nameHash[key] = tnc;
    tnc->next = tnc->prev = tnc;
  }
  else {
    tnc->next = nameHash[key];
    tnc->prev = tnc->next->prev;
    tnc->next->prev = tnc;
    tnc->prev->next = tnc;
    nameHash[key] = tnc;
  }
}


int osi_dnlc_enter ( adp, aname, avc, avno )
  struct vcache *adp;
  char          *aname;
  struct vcache *avc;
  afs_hyper_t   *avno;
{
  struct nc *tnc;
  unsigned int key, skey;
  char *ts = aname;
  int safety;

  if (!afs_usednlc)
    return 0;
  
  TRACE(osi_dnlc_enterT, 0);
  dnlcHash( ts, key );  /* leaves ts pointing at the NULL */
  if (ts - aname >= AFSNCNAMESIZE) {
    return 0;
  }
  skey = key & (NHSIZE -1);
  dnlcstats.enters++;

retry:
  ObtainWriteLock(&afs_xdnlc,222);

  /* Only cache entries from the latest version of the directory */
  if (!(adp->states & CStatd) || !hsame(*avno, adp->m.DataVersion)) {
    ReleaseWriteLock(&afs_xdnlc);
    return 0;
  }

  /*
   * Make sure each directory entry gets cached no more than once.
   */
  for (tnc = nameHash[skey], safety=0; tnc; tnc = tnc->next, safety++ ) {
    if ((tnc->dirp == adp) && (!strcmp((char *)tnc->name, aname))) {
      /* duplicate entry */
      break;
    }
    else if (tnc->next == nameHash[skey]) { /* end of list */
      tnc = NULL;
      break;
    }
    else if (safety >NCSIZE) {
      afs_warn("DNLC cycle");
      dnlcstats.cycles++;
      ReleaseWriteLock(&afs_xdnlc);
      osi_dnlc_purge();
      goto retry;
    }
  }

  if (tnc == NULL) {
    tnc = GetMeAnEntry();

    tnc->dirp = adp;
    tnc->vp = avc;
    tnc->key = key;
    bcopy (aname, (char *)tnc->name, ts-aname+1); /* include the NULL */

    InsertEntry(tnc);
  } else {
    /* duplicate */
    tnc->vp = avc;
  }
  ReleaseWriteLock(&afs_xdnlc);

return 0;
}


struct vcache *
osi_dnlc_lookup ( adp, aname, locktype )
  struct vcache *adp;
  char          *aname;
  int           locktype;
{
  struct vcache * tvc;
  int LRUme;
  unsigned int key, skey;
  char *ts = aname;
  struct nc * tnc, *tnc1=0;
  int safety;
  
  if (!afs_usednlc)
    return 0;

  dnlcHash( ts, key );  /* leaves ts pointing at the NULL */
  if (ts - aname >= AFSNCNAMESIZE) {
    return 0;
  }
  skey = key & (NHSIZE -1);

  TRACE(osi_dnlc_lookupT, skey);
  dnlcstats.lookups++;

  ObtainReadLock(&afs_xvcache);	
  ObtainReadLock(&afs_xdnlc);

  for ( tvc = (struct vcache *) 0, tnc = nameHash[skey], safety=0; 
       tnc; tnc = tnc->next, safety++ ) {
    if ( /* (tnc->key == key)  && */ (tnc->dirp == adp) 
	&& (!strcmp((char *)tnc->name, aname))) {
      tvc = tnc->vp; 
      tnc1 = tnc;
      break;
    }
    else if (tnc->next == nameHash[skey]) { /* end of list */
      break;
    }
    else if (safety >NCSIZE) {
      afs_warn("DNLC cycle");
      dnlcstats.cycles++;
      ReleaseReadLock(&afs_xdnlc);
      ReleaseReadLock(&afs_xvcache);	
      osi_dnlc_purge();
      return(0);
    }
  }

  LRUme = 0; /* (tnc != nameHash[skey]); */
  ReleaseReadLock(&afs_xdnlc);

  if (!tvc) {
     ReleaseReadLock(&afs_xvcache);	
     dnlcstats.misses++;
  }
  else {
#ifdef	AFS_OSF_ENV
     VN_HOLD((vnode_t *)tvc);
#else
      osi_vnhold(tvc, 0);
#endif
      ReleaseReadLock(&afs_xvcache);	

#ifdef	notdef
    /* 
     * XX If LRUme ever is non-zero change the if statement around because
     * aix's cc with optimizer on won't necessarily check things in order XX
     */
    if (LRUme && (0 == NBObtainWriteLock(&afs_xdnlc))) {  
      /* don't block to do this */
      /* tnc might have been moved during race condition, */
      /* but it's always in a legit hash chain when a lock is granted, 
       * or else it's on the freelist so prev == NULL, 
       * so at worst this is redundant */
      /* Now that we've got it held, and a lock on the dnlc, we 
       * should check to be sure that there was no race, and 
       * bail out if there was. */
      if (tnc->prev) {
	/* special case for only two elements on list - relative ordering
	 * doesn't change */
	if (tnc->prev != tnc->next) {
	  /* remove from old location */
	  tnc->prev->next = tnc->next;
	  tnc->next->prev = tnc->prev;
	  /* insert into new location */
	  tnc->next = nameHash[skey];
	  tnc->prev = tnc->next->prev;
	  tnc->next->prev = tnc;
	  tnc->prev->next = tnc;
	}
	nameHash[skey] = tnc;
      }
      ReleaseWriteLock(&afs_xdnlc);
  }
#endif
  }

return tvc;
}


static void
RemoveEntry (tnc, key)
  struct nc    *tnc;
  unsigned int key;
{
  if (!tnc->prev) /* things on freelist always have null prev ptrs */
    osi_Panic("bogus free list");

  TRACE(RemoveEntryT, key);
  if (tnc == tnc->next) { /* only one in list */
    nameHash[key] = (struct nc *) 0;
  }
  else {
    if (tnc == nameHash[key])
      nameHash[key]  = tnc->next;
    tnc->prev->next = tnc->next;
    tnc->next->prev = tnc->prev;
  }

  tnc->prev = (struct nc *) 0; /* everything not in hash table has 0 prev */
  tnc->key = 0; /* just for safety's sake */
}


int 
osi_dnlc_remove ( adp, aname, avc )
  struct vcache *adp;
  char          *aname;
  struct vcache *avc;
{
  unsigned int key, skey;
  char *ts = aname;
  struct nc *tnc;
  
  if (!afs_usednlc)
    return 0;

  TRACE( osi_dnlc_removeT, skey);
  dnlcHash( ts, key );  /* leaves ts pointing at the NULL */
  if (ts - aname >= AFSNCNAMESIZE) {
    return 0;
  }
  skey = key & (NHSIZE -1);
  TRACE( osi_dnlc_removeT, skey);
  dnlcstats.removes++;
  ObtainReadLock(&afs_xdnlc);

  for (tnc = nameHash[skey]; tnc; tnc = tnc->next) {
    if ((tnc->dirp == adp) && (tnc->key == key)
	&& (!strcmp((char *)tnc->name, aname))) {
      tnc->dirp = (struct vcache *) 0; /* now it won't match anything */
      break;      
    }
    else if (tnc->next == nameHash[skey]) { /* end of list */
      tnc = (struct nc *) 0;
      break;
    }
  }
  ReleaseReadLock(&afs_xdnlc);

  if (!tnc)
    return 0;

  /* there is a little race condition here, but it's relatively
   * harmless.  At worst, I wind up removing a mapping that I just
   * created. */
  if (EWOULDBLOCK == NBObtainWriteLock(&afs_xdnlc,1)) {
    return 0; /* no big deal, tnc will get recycled eventually */
  }
  RemoveEntry(tnc, skey);
  tnc->next = ncfreelist;
  ncfreelist = tnc;
  ReleaseWriteLock(&afs_xdnlc);

return 0;
}

/* remove anything pertaining to this directory.  I can invalidate
 * things without the lock, since I am just looking through the array,
 * but to move things off the lists or into the freelist, I need the
 * write lock */
int 
osi_dnlc_purgedp (adp)
  struct vcache *adp;
{
  int i;
  int writelocked;

  if (!afs_usednlc)
    return 0;

  dnlcstats.purgeds++;
  TRACE( osi_dnlc_purgedpT, 0);
  writelocked = (0 == NBObtainWriteLock(&afs_xdnlc,2));

  for (i=0; i<NCSIZE; i++) {
    if ((nameCache[i].dirp == adp) || (nameCache[i].vp == adp)) {
      nameCache[i].dirp = nameCache[i].vp = (struct vcache *) 0;
      if (writelocked && nameCache[i].prev) {
	RemoveEntry(&nameCache[i], nameCache[i].key & (NHSIZE-1));
	nameCache[i].next = ncfreelist;
	ncfreelist = &nameCache[i];
      }
    }
  }
  if (writelocked)
    ReleaseWriteLock(&afs_xdnlc);

return 0;
}

/* remove anything pertaining to this file */
int 
osi_dnlc_purgevp ( avc )
  struct vcache *avc;
{
  int i;
  int writelocked;

  if (!afs_usednlc)
    return 0;

  dnlcstats.purgevs++;
  TRACE( osi_dnlc_purgevpT, 0);
  writelocked = (0 == NBObtainWriteLock(&afs_xdnlc,3));

  for (i=0; i<NCSIZE; i++) {
    if ((nameCache[i].vp == avc)) {
      nameCache[i].dirp = nameCache[i].vp = (struct vcache *) 0;
      /* can't simply break; because of hard links -- might be two */
      /* different entries with same vnode */ 
      if (writelocked && nameCache[i].prev) {
	RemoveEntry(&nameCache[i], nameCache[i].key & (NHSIZE-1));
	nameCache[i].next = ncfreelist;
	ncfreelist = &nameCache[i];
      }
    }
  }
  if (writelocked)
    ReleaseWriteLock(&afs_xdnlc);

return 0;
}

/* remove everything */
int osi_dnlc_purge()
{
  int i;

  dnlcstats.purges++;
  TRACE( osi_dnlc_purgeT, 0);
  if (EWOULDBLOCK == NBObtainWriteLock(&afs_xdnlc,4)) { /* couldn't get lock */
    for (i=0; i<NCSIZE; i++) 
      nameCache[i].dirp = nameCache[i].vp = (struct vcache *) 0;
  }
  else {  /* did get the lock */
    ncfreelist = (struct nc *) 0;
    bzero ((char *)nameCache, sizeof(struct nc) * NCSIZE);
    bzero ((char *)nameHash, sizeof(struct nc *) * NHSIZE);
    for (i=0; i<NCSIZE; i++) {
      nameCache[i].next = ncfreelist;
      ncfreelist = &nameCache[i];
    }
    ReleaseWriteLock(&afs_xdnlc);
  }

return 0;
}

/* remove everything referencing a specific volume */
int
osi_dnlc_purgevol( fidp )
  struct VenusFid *fidp;
{

  dnlcstats.purgevols++;
  osi_dnlc_purge();

return 0;
}

int osi_dnlc_init()
{
int i;

  Lock_Init(&afs_xdnlc);
  bzero ((char *)&dnlcstats, sizeof(dnlcstats));
  bzero ((char *)dnlctracetable, sizeof(dnlctracetable));
  dnlct=0;
  ObtainWriteLock(&afs_xdnlc,223);
  ncfreelist = (struct nc *) 0;
  bzero ((char *)nameCache, sizeof(struct nc) * NCSIZE);
  bzero ((char *)nameHash, sizeof(struct nc *) * NHSIZE);
  for (i=0; i<NCSIZE; i++) {
    nameCache[i].next = ncfreelist;
    ncfreelist = &nameCache[i];
  }
  ReleaseWriteLock(&afs_xdnlc);

return 0;
}

int osi_dnlc_shutdown()
{

return 0;
}




