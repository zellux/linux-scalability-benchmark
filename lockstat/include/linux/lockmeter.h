/*
 *  Copyright (C) 1999,2000 Silicon Graphics, Inc.
 *
 *  Written by John Hawkes (hawkes@sgi.com)
 *  Based on klstat.h by Jack Steiner (steiner@sgi.com)
 *  
 *  Modified by Ray Bryant (raybry@us.ibm.com) Feb-Apr 2000
 *  Changes Copyright (C) 2000 IBM, Inc.
 *  Added save of index in spinlock_t to improve efficiency
 *  of "hold" time reporting for spinlocks
 *  Added support for hold time statistics for read and write
 *  locks.
 *  Moved machine dependent code to include/asm/lockmeter.h.
 *
 */

#ifndef _LINUX_LOCKMETER_H
#define _LINUX_LOCKMETER_H


/*---------------------------------------------------
 *	architecture-independent lockmeter.h
 *-------------------------------------------------*/

/* 
 * raybry -- version 2: added efficient hold time statistics
 *           requires lstat recompile, so flagged as new version
 * raybry -- version 3: added global reader lock data
 * hawkes -- version 4: removed some unnecessary fields to simplify mips64 port
 */
#define LSTAT_VERSION	5

int	lstat_update(void*, void*, int);
int	lstat_update_time(void*, void*, int, uint32_t);

/*
 * Currently, the mips64 and sparc64 kernels talk to a 32-bit lockstat, so we
 * need to force compatibility in the inter-communication data structure.
 */

#if defined(CONFIG_MIPS32_COMPAT)
#define TIME_T		uint32_t
#elif defined(CONFIG_SPARC32_COMPAT)
#define TIME_T		uint64_t
#else
#define TIME_T		time_t
#endif

#if defined(__KERNEL__) || (!defined(CONFIG_MIPS32_COMPAT) && !defined(CONFIG_SPARC32_COMPAT)) || (_MIPS_SZLONG==32)
#define POINTER		void *
#else
#define	POINTER		int64_t
#endif

/*
 * Values for the "action" parameter passed to lstat_update.
 *	ZZZ - do we want a try-success status here??? 
 */
#define LSTAT_ACT_NO_WAIT	0
#define LSTAT_ACT_SPIN		1
#define LSTAT_ACT_REJECT	2
#define LSTAT_ACT_WW_SPIN       3
#define LSTAT_ACT_SLEPT		4 /* UNUSED */

#define LSTAT_ACT_MAX_VALUES	4 /* NOTE: Increase to 5 if use ACT_SLEPT */

/*
 * Special values for the low 2 bits of an RA passed to
 * lstat_update.
 */
/* we use these values to figure out what kind of lock data */
/* is stored in the statistics table entry at index ....... */
#define LSTAT_RA_SPIN           0  /* spin lock data */
#define LSTAT_RA_READ           1  /* read lock statistics */
#define LSTAT_RA_SEMA		2  /* RESERVED */
#define LSTAT_RA_WRITE          3  /* write lock statistics*/

#define LSTAT_RA(n)	\
	((void*)( ((unsigned long)__builtin_return_address(0) & ~3) | n) )

/*
 * Constants used for lock addresses in the lstat_directory
 * to indicate special values of the lock address. 
 */
#define	LSTAT_MULTI_LOCK_ADDRESS	NULL

/*
 * Maximum size of the lockstats tables. Increase this value 
 * if its not big enough. (Nothing bad happens if its not
 * big enough although some locks will not be monitored.)
 * We record overflows of this quantity in lstat_control.dir_overflows
 *
 * Note:  The max value here must fit into the field set
 * and obtained by the macro's PUT_INDEX() and GET_INDEX().
 * This value depends on how many bits are available in the 
 * lock word in the particular machine implementation we are on.
 */
#define LSTAT_MAX_STAT_INDEX		2000

/* 
 * Size and mask for the hash table into the directory.
 */
#define LSTAT_HASH_TABLE_SIZE		4096		/* must be 2**N */
#define LSTAT_HASH_TABLE_MASK		(LSTAT_HASH_TABLE_SIZE-1)

#define DIRHASH(ra)      ((unsigned long)(ra)>>2 & LSTAT_HASH_TABLE_MASK)

/*
 *	This defines an entry in the lockstat directory. It contains
 *	information about a lock being monitored.
 *	A directory entry only contains the lock identification - 
 *	counts on usage of the lock are kept elsewhere in a per-cpu
 *	data structure to minimize cache line pinging.
 */
typedef struct {
	POINTER	caller_ra;		  /* RA of code that set lock */
	POINTER	lock_ptr;		  /* lock address */
	ushort	next_stat_index;  /* Used to link multiple locks that have the same hash table value */
} lstat_directory_entry_t;

/*
 *	A multi-dimensioned array used to contain counts for lock accesses.
 *	The array is 3-dimensional:
 *		- CPU number. Keep from thrashing cache lines between CPUs
 *		- Directory entry index. Identifies the lock
 *		- Action. Indicates what kind of contention occurred on an
 *		  access to the lock.
 *
 *	The index of an entry in the directory is the same as the 2nd index
 *	of the entry in the counts array.
 */
/* 
 *  This table contains data for spin_locks, write locks, and read locks
 *  Not all data is used for all cases.  In particular, the hold time   
 *  information is not stored here for read locks since that is a global
 *  (e. g. cannot be separated out by return address) quantity. 
 *  See the lstat_read_lock_counts_t structure for the global read lock
 *  hold time.
 */ 
typedef struct {
	uint64_t    cum_wait_ticks;	/* sum of wait times               */
	                                /* for write locks, sum of time a  */
					/* writer is waiting for a reader  */
	int64_t	    cum_hold_ticks;	/* cumulative sum of holds         */
	                                /* not used for read mode locks    */
					/* must be signed. ............... */
	uint32_t    max_wait_ticks;	/* max waiting time                */
	uint32_t    max_hold_ticks;	/* max holding time                */
	uint64_t    cum_wait_ww_ticks;  /* sum times writer waits on writer*/
	uint32_t    max_wait_ww_ticks;  /* max wait time writer vs writer  */
	                                /* prev 2 only used for write locks*/
	uint32_t    acquire_time;       /* time lock acquired this CPU     */
	uint32_t    count[LSTAT_ACT_MAX_VALUES];
} lstat_lock_counts_t;

typedef lstat_lock_counts_t	lstat_cpu_counts_t[LSTAT_MAX_STAT_INDEX];

/*
 * User request to:
 *	- turn statistic collection on/off, or to reset
 */
#define LSTAT_OFF	 0
#define LSTAT_ON	 1
#define LSTAT_RESET      2
#define LSTAT_RELEASE    3

#define LSTAT_MAX_READ_LOCK_INDEX 1000
typedef struct {
	POINTER	    lock_ptr;            /* address of lock for output stats */
	uint32_t    read_lock_count;          
	int64_t     cum_hold_ticks;       /* sum of read lock hold times over */
	                                  /* all callers. ....................*/
	uint32_t    write_index;          /* last write lock hash table index */
	uint32_t    busy_periods;         /* count of busy periods ended this */
	uint64_t    start_busy;           /* time this busy period started. ..*/
	uint64_t    busy_ticks;           /* sum of busy periods this lock. ..*/
	uint64_t    max_busy;             /* longest busy period for this lock*/
	uint32_t    max_readers;          /* maximum number of readers ...... */
#ifdef USER_MODE_TESTING
	rwlock_t    entry_lock;           /* lock for this read lock entry... */
	                                  /* avoid having more than one rdr at*/
	                                  /* needed for user space testing... */
	                                  /* not needed for kernel 'cause it  */
					  /* is non-preemptive. ............. */
#endif
} lstat_read_lock_counts_t;
typedef lstat_read_lock_counts_t	lstat_read_lock_cpu_counts_t[LSTAT_MAX_READ_LOCK_INDEX];

#if defined(__KERNEL__) || defined(USER_MODE_TESTING)

#ifndef USER_MODE_TESTING
#include <asm/lockmeter.h>
#else
#include "asm_newlockmeter.h"
#endif

/* 
 * Size and mask for the hash table into the directory.
 */
#define LSTAT_HASH_TABLE_SIZE		4096		/* must be 2**N */
#define LSTAT_HASH_TABLE_MASK		(LSTAT_HASH_TABLE_SIZE-1)

#define DIRHASH(ra)      ((unsigned long)(ra)>>2 & LSTAT_HASH_TABLE_MASK)

/*
 * This version eliminates the per processor lock stack.  What we do is to
 * store the index of the lock hash structure in unused bits in the lock  
 * itself.  Then on unlock we can find the statistics record without doing
 * any additional hash or lock stack lookup.  This works for spin_locks.  
 * Hold time reporting is now basically as cheap as wait time reporting
 * so we ignore the difference between LSTAT_ON_HOLD and LSTAT_ON_WAIT
 * as in version 1.1.* of lockmeter.
 *
 * For rw_locks, we store the index of a global reader stats structure in 
 * the lock and the writer index is stored in the latter structure.       
 * For read mode locks we hash at the time of the lock to find an entry  
 * in the directory for reader wait time and the like.
 * At unlock time for read mode locks, we update just the global structure
 * so we don't need to know the reader directory index value at unlock time.
 *
 */

/* 
 * Protocol to change lstat_control.state
 *   This is complicated because we don't want the cum_hold_time for
 * a rw_lock to be decremented in _read_lock_ without making sure it
 * is incremented in _read_lock_ and vice versa.  So here is the    
 * way we change the state of lstat_control.state:                  
 * I.  To Turn Statistics On
 *     After allocating storage, set lstat_control.state non-zero.
 * This works because we don't start updating statistics for in use
 * locks until the reader lock count goes to zero.
 * II. To Turn Statistics Off:
 * (0)  Disable interrupts on this CPU                                          
 * (1)  Seize the lstat_control.directory_lock                            
 * (2)  Obtain the current value of lstat_control.next_free_read_lock_index   
 * (3)  Store a zero in lstat_control.state.
 * (4)  Release the lstat_control.directory_lock                          
 * (5)  For each lock in the read lock list up to the saved value   
 *      (well, -1) of the next_free_read_lock_index, do the following:        
 *      (a)  Check validity of the stored lock address
 *           by making sure that the word at the saved addr
 *           has an index that matches this entry.  If not 
 *           valid, then skip this entry.
 *      (b)  If there is a write lock already set on this lock,
 *           skip to (d) below.
 *      (c)  Set a non-metered write lock on the lock          
 *      (d)  set the cached INDEX in the lock to zero
 *      (e)  Release the non-metered write lock.                    
 * (6)  Re-enable interrupts
 *
 * These rules ensure that a read lock will not have its statistics      
 * partially updated even though the global lock recording state has    
 * changed.  See put_lockmeter_info() for implementation.
 *
 * The reason for (b) is that there may be write locks set on the
 * syscall path to put_lockmeter_info() from user space.  If we do
 * not do this check, then we can deadlock.  A similar problem would
 * occur if the lock was read locked by the current CPU.  At the 
 * moment this does not appear to happen.
 */

/*
 * Main control structure for lockstat. Used to turn statistics on/off
 * and to maintain directory info.
 */
typedef struct {
	int				state;
	spinlock_t		control_lock;		/* used to serialize turning statistics on/off   */
	spinlock_t		directory_lock;		/* for serialize adding entries to directory     */
	volatile int	next_free_dir_index;/* next free entry in the directory */
	/* FIXME not all of these fields are used / needed .............. */
                /* the following fields represent data since     */
		/* first "lstat on" or most recent "lstat reset" */
	TIME_T      first_started_time;     /* time when measurement first enabled */
	TIME_T      started_time;           /* time when measurement last started  */
	TIME_T      ending_time;            /* time when measurement last disabled */
	uint64_t    started_cycles64;       /* cycles when measurement last started          */
	uint64_t    ending_cycles64;        /* cycles when measurement last disabled         */
	uint64_t    enabled_cycles64;       /* total cycles with measurement enabled         */
	int         intervals;              /* number of measurement intervals recorded      */
	                                    /* i. e. number of times did lstat on;lstat off  */
	lstat_directory_entry_t	*dir;		/* directory */
	int         dir_overflow;           /* count of times ran out of space in directory  */
	int         rwlock_overflow;        /* count of times we couldn't allocate a rw block*/
	ushort		*hashtab;		 	    /* hash table for quick dir scans */
	lstat_cpu_counts_t	*counts[NR_CPUS];	 /* Array of pointers to per-cpu stats */
    int         next_free_read_lock_index;   /* next rwlock reader (global) stats block  */
    lstat_read_lock_cpu_counts_t *read_lock_counts[NR_CPUS]; /* per cpu read lock stats  */
} lstat_control_t;

#endif	/* defined(__KERNEL__) || defined(USER_MODE_TESTING) */

typedef struct {
	short		lstat_version;		/* version of the data */
	short		state;			/* the current state is returned */
	int		maxcpus;		/* Number of cpus present */
	int		next_free_dir_index;	/* index of the next free directory entry */
	TIME_T          first_started_time;	/* when measurement enabled for first time */
	TIME_T          started_time;		/* time in secs since 1969 when stats last turned on  */
	TIME_T		ending_time;		/* time in secs since 1969 when stats last turned off */
	uint32_t	cycleval;		/* cycles per second */
#ifdef notyet
	void		*kernel_magic_addr;	/* address of kernel_magic */
	void		*kernel_end_addr;	/* contents of kernel magic (points to "end") */
#endif
	int              next_free_read_lock_index; /* index of next (global) read lock stats struct */
	uint64_t         started_cycles64;	/* cycles when measurement last started        */
	uint64_t         ending_cycles64;	/* cycles when stats last turned off           */
	uint64_t         enabled_cycles64;	/* total cycles with measurement enabled       */
	int              intervals;		/* number of measurement intervals recorded      */
						/* i.e. number of times we did lstat on;lstat off*/
	int              dir_overflow;		/* number of times we wanted more space in directory */
	int              rwlock_overflow;	/* # of times we wanted more space in read_locks_count */
	struct new_utsname   uts;		/* info about machine where stats are measured */
						/* -T option of lockstat allows data to be     */
						/* moved to another machine. ................. */
} lstat_user_request_t;

#endif /* _LINUX_LOCKMETER_H */
