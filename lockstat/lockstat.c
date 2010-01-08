/*
 *
 *  Copyright (C) 1999-2001 Silicon Graphics, Inc.
 *
 *  Written by John Hawkes (hawkes@sgi.com)
 *  Based on lockstat.c by Jack Steiner (steiner@sgi.com)
 *
 *  Modifications by Ray Bryant (raybry@us.ibm.com) Feb-Mar 2000
 *  Changes Copyright (C) 2000 IBM, Inc.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

static char lockstat_version[]  = "1.4.11";

#include "lockstat.h"

#include <linux/config.h>
#include <linux/utsname.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <linux/lockmeter.h>

extern void	closeFiles(void);
extern void	getKernelData(lstat_user_request_t **, lstat_cpu_counts_t **,
			      lstat_directory_entry_t **,
			      lstat_read_lock_cpu_counts_t **, int);
extern int	isProcFile(void);
extern void	XlateAddressToSymbol(void *, char *);
extern int	openKernelData(char *);
extern int	openMapFile(char *);
extern void	setCollectionState(int);
extern void	setMirrorOutFilename(char *);
extern char *	strSpace(void);

#ifdef TIMER
extern void cycletrace(char*);
#define TSTAMP(s)	cycletrace(s);
#else
#define TSTAMP(s)
#endif

#define DEBUGBUG

#define TRUE 1
#define FALSE 0

/* this program only reads version 5 lockstat data */
#define THIS_VERSION 5

char *helpstr[] = {
"Name"
"       lockstat - display kernel lock statistics",
"",
"SYNOPSIS",
"       lockstat [options] on/off/get/print/off+print/reset/release/query",
"",
"DESCRIPTION",
"       When run with a Lockmeter-enabled kernel, this command will display",
"       (or save for later display) statistics about the kernel locks that",
"       are being used.",
"",
"   lockstat on     turn ON the Lockmeter data gathering.",
"                   The first time this happens, the kernel also allocates the",
"                   storage required for its Lockmeter data structures.",
"   lockstat off    turn OFF the Lockmeter data gathering.",
"                   Each time data gathering is turned ON and OFF, a count",
"                   of the number of measurement intervals is incremented and",
"                   the total stats enabled time is updated.",
"                   Use 'lockstat reset' to reset counts and data to 0.",
"   lockstat get    fetch the Lockmeter data and writes it to lockstat.tmp,",
"                   or to the filename specified by the -T option (see below).",
"   lockstat print  fetch the Lockmeter data, then generate and print a report.",
"                   If neither -X nor -P is specified, then Lockmeter data",
"                   gathering must either be currently ON, or else the number of",
"                   stored Lockmeter measurement intervals must be > 0.",
"                   -X and -P cause data gathering to be turned ON (if not",
"                   already ON), and then to be turned OFF at the end of the",
"                   measurement run.  See -X and -P options below for details.",
"   lockstat off+print same as 'lockstat off' followed by 'lockstat print'.",
"   lockstat reset  reset the kernel's Lockmeter data to zero.  Otherwise, data",
"                   recorded by the next 'lockstat on' is accumulative.",
"   lockstat release  tell the kernel to release storage used to record",
"                   Lockmeter data.  Typically only used when you are done with",
"                   measurement.  In the default configuration these structures",
"                   occupy approximately 300 KB of kernel storage per processor.",
"   lockstat query  return the current state of data collection and whether or",
"                   not data is available to be 'print'ed or 'get'ed.",
"                   Each 'lockstat on';'lockstat off' pair increments an", 
"                   interval counter in the Lockmeter data.",
"                   Any time data gathering is turned ON, or when the number of",
"                   intervals is >0, you can print all the data thus far",
"                   recorded using 'lockstat print'.",
"",
"OPTIONS",
"   -m mapfile      Location of map file(s) to use for symbol table.  If not",
"                   specified, defaults to /usr/src/linux/System.map.",
"   -L file         Get the Lockmeter data from file 'file'.  If unspecified,",
"                   then the default is /proc/lockmeter.",
"                   Typically, a named disk file was previously produced",
"                   by using the -T option.  Only the 'print' keyword is allowed",
"                   if this file is specified and is not /proc/lockmeter.",
"   -T file         Used with the 'get' directive to specify a file into which",
"                   the collected data is stored.  If unspecified, defaults to",
"                   'lockstat.tmp'.  Data can later be read using the -L option.",
"   -X 'commandargs'  Run the command with the specified args and collect",
"                   Lockmeter data while the command runs.  Print a report after",
"                   the command terminates if the 'print' keyword is given,",
"                   or save the data where the -T option specifies if the 'get'",
"                   keyword is given.  Only the 'print' or 'get' keywords can be",
"                   specified with -X.",
"                   The single quotes around commandandargs are REQUIRED if",
"                   the commandandargs string contains any blanks.",
"                   If statistics are not ON, they are turned ON before the",
"                   command is executed, then back OFF after the command",
"                   terminates.  Otherwise, the current statistics collection",
"                   state is unchanged.",
"                   The -P option is not allowed with the -X option.",
"   -i string       String to print as part of the title for the report.",
"   -c cpu          By default, the statistics for all cpus are summed",
"                   together.  If \"-c a\", then the full summation report is",
"                   printed, as well as a separate report for each cpu in the",
"                   system.  Otherwise, 'cpu' identifies a specific cpu,",
"                   numbered from zero to N, and a report is generated for",
"                   just that cpu.",
"   -C              Report lock requests-per-second instead of lock utilization.",
"   -t              <Now the default!> 'lockstat -t' is equivalent to",
"                   'lockstat print'.  Prints 'total' statistics.",
"   -P sec          Report periodic statistics every 'sec' seconds.",
"                   Print the report if the 'print' keyword is specified, or save",
"                   the data where the -T option specifies if also using the",
"                   'get' keyword.  Only the 'print' or 'get' keywords can be",
"                   specified with -P, and not the -X option.",
"                   If data gathering is not ON, then it is turned ON for the",
"                   duration of the periodic report and thereafter turned back",
"                   OFF after the end of the report.  Otherwise, the current",
"                   statistics collection state remains unchanged.",
"                   The -P option is ignored if data is being read from a file",
"                   (using -L).",
"                   If multiple data gathering intervals have already been",
"                   generated and saved, then the saved data must be reset (via",
"                   'lockstat reset') before a periodic report can be started.",
"   -R count        repeat the periodic statistics report count times.",
"                   If -R is not used, 'count' defaults to 10.",
"                   The -R option is allowed with the -L option to read the",
"                   results of a periodic report that was saved in a file using",
"                   the 'get' keyword.",
#ifdef notyet
"   -S              Show semaphore information.  This is not selected by default",
"                   because I am not sure it is useful.",
#endif
" ",
"   The following options control which locks are described in the report:",
"",
"   -p persec       Report only on locks set more than 'persec' times per",
"                   second.",
"   -k percent      Report locks with more than 'percent' contention.",
"   -w              Report on \"warm\" or \"hot\" locks only, i.e., on all",
"                   locks that exhibit any contention.",
"                   This option is the same as -p 0 -k 0.",
"   -h              Report on \"hot\" locks only, which are defined as if",
"                   you specified '-p 100 -k 5'",
"   -u percent      Report locks with more than 'percent' utilization.",
"   -O              Default behavior is to report on locks that meet all of the",
"                   specified conditions.  Specify the -O flag to get a report",
"                   for locks that meet any ONE of the specified conditions.",
"   -v              Display the lockstat version.",
"",
"REPORT",
"       The report is divided into several sections:",
"               SPINLOCKS      Stats for simple spinlock_t spinlocks",
#ifdef notyet
"               MRLOCKS",
#endif
"               RWLOCK READS   Stats for READ  locks on rwlock_t spinlocks.",
"               RWLOCK WRITES  Stats for WRITE locks on rwlock_t spinlocks.",
#ifdef notyet
"               SEMAPHORES (if -S is selected)",
#endif
"",
"       The following data is collected for each lock:",
"",
"       TOT/SEC Number of times per second that the lock was set.",
"       UTIL    % of time that the lock was busy during the measurement.",
"               (Only one of the above two is printed.  The -C flag selects",
"               which one is printed.  The default is to print UTIL.)",
"       CON     Amount of contention that occurred for the lock.  The ",
"               number represents the percent of time that lock was NOT",
"               acquired without spinning or sleeping.",
#ifdef notyet
"               Note: for semaphores, the number represents the % of",
"               the time psema slept.",
#endif
"       HOLD    Mean and max lock hold times, in microseconds (us) or",
"               milliseconds (ms), and the percentage of total CPU cycles",
"               consumed by the waiting.",
"               This is reported per caller for spin locks and",
"               for write mode locks on rwlocks.  It is reported",
"               over all callers for read mode locks on rwlocks.",
"       MAXRDR  Maximum number of readers for a rwlock.",
"       RDR BUSY PERIOD Length (in microseconds) of 'reader busy' periods",
"               for a rwlock.  A reader busy period starts when the first",
"               reader lock is set and ends when the last reader releases the",
"               lock.  This statistic is reported over all read lock callers",
"               on a rwlock (not on a per caller basis).",
"       WAIT    Mean spin-wait wait time, in microseconds.",
"       WAIT WW For write locks, we also report wait time that a",
"               a writer waited for a writer holding the lock.",
"       TOTAL   Total number of times that the lock was set.",
"       NOWAIT  Percentage of times the lock was acquired without waiting",
"       SPIN    Percentage of times it was necessary to spin waiting for",
"               a spinlock.",
"       SPIN WW Percentage of times a writer could not get a rwlock because",
"               another writer was holding the lock.",
#ifdef notyet
"       SLEEP   Number of times it was necessary to sleep for a lock",
#endif
"       RJECT   Percentage of times a \"trylock\" failed.",
"       NAME    Identifies the lock and/or the routine that set the lock.",
"               If the lock is statically defined and not part of an array,",
"               both the lock name and the functions that set the ",
"               lock are listed.  If the lock is dynamically allocated,",
"               only the function name that set the lock will be listed.",
"               If spin lock code at a particular address sets more than one",
"               lock (e. g. if a procedure sets a spin lock in an argument)",
"               then stats for all such locks are combined and reported by",
"               calling address (procedure and offset) only.  No lock name",
"               or address is given in that case.",
"",
NULL};

char   env_var[] = "LOCKSTAT";
char   env_val[] = "V2";
static char defaultGetFilename[]  = "lockstat.tmp";
int dataFile;

static char defaultMapFilename[]  = "/usr/src/linux/System.map";

#define MAXCPUS			512

#define perrorx(s)		{if (errno != 0) perror(s); else fprintf(stderr,"%s:%s\n",progname,s); exit(1);}
#define fatalx(m)		fprintf(stderr, "%s: ERROR - %s\n", progname, m), exit(1)
#define min(a,b)		(((a)>(b)) ? (b) : (a))
#define max(a,b)		(((a)<(b)) ? (b) : (a))


typedef enum	{Buf_Previous, Buf_Current} get_data_buffer_enum ;
typedef enum	{Null_Entry, Spin_Entry, RLspin_Entry, WLspin_Entry, Sema_Entry} entry_type_enum;

typedef struct {
	lstat_lock_counts_t  counts;
	uint32_t	total;
	double		contention;
	double		persec;
	double          utilization;
	void		*lock_ptr;
} lock_summary_t;
	
typedef struct {
	void		*caller_ra;
	char		*caller_name;
	char		*lock_name;
	char		multilock;
	char		title_index;
	char		caller_name_len;
	entry_type_enum	entry_type;
} directory_entry_t;

directory_entry_t	directory[LSTAT_MAX_STAT_INDEX];
int			next_free_dir_index;
lstat_read_lock_counts_t read_lock_counts[LSTAT_MAX_READ_LOCK_INDEX];
#ifdef DEBUGBUG
int         read_lock_entry_used[LSTAT_MAX_READ_LOCK_INDEX]={LSTAT_MAX_READ_LOCK_INDEX * 0};
#endif
int         next_free_read_lock_index;                              

int directory_overflows=0, read_lock_overflows=0;

lstat_directory_entry_t	*kernel_directory;
lstat_directory_entry_t	*prev_kernel_directory;

lstat_cpu_counts_t	*kernel_counts;
lstat_cpu_counts_t	*prev_counts;

lstat_read_lock_cpu_counts_t *kernel_read_lock_counts;                      
lstat_read_lock_cpu_counts_t *prev_read_lock_counts;                        

lstat_lock_counts_t	total_counts[LSTAT_MAX_STAT_INDEX]; 
lstat_read_lock_counts_t rwlock_counts[LSTAT_MAX_READ_LOCK_INDEX]; 
lstat_user_request_t kernel_request, prev_request;

/* the maximum number of readers possible is the number of cpus, but sometimes a cpu may lock a */
/* read lock more than once, so, give the kernel some room for error. ......................... */
#define ABSURD_MAX_COUNT (10*numcpus)
int read_lock_increment[LSTAT_MAX_READ_LOCK_INDEX];
int prev_read_lock_increment[LSTAT_MAX_READ_LOCK_INDEX];

short			sorti[LSTAT_MAX_STAT_INDEX+2];

int			numcpus = 0;
int			state, valid;
float			cycles_per_usec;
#ifdef notyet
void			*kernel_magic_addr;
void			*kernel_end_addr;
#endif
time_t			first_start_time, start_time, end_time;
double          	deltatime;
int			skipline = 0, multiflag = 0;
char			*current_header, *last_header, *current_dashes;

struct new_utsname uts;
int period=0, count=10;
int keyword_valid = 0;
int turned_on_this_time=0;

unsigned long long    started_cycles64, ending_cycles64, enabled_cycles64;
int         intervals;

char	*dashes[] = {
    "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n",
    "- - - - - - - - - - - -  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n",
    "- - - - - - - - - - -  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n"
	""};

/* these titles are used if the -C option is specified */
char	*special_titlesC[] = {
	"SPINLOCKS         HOLD            WAIT\n"
	"TOT/SEC CON    MEAN(  MAX )   MEAN(  MAX )(% CPU)     TOTAL NOWAIT SPIN RJECT  NAME",
	"RWLOCK READS   HOLD    MAX  RDR BUSY PERIOD      WAIT\n"
    "TOT/SEC CON    MEAN   RDRS   MEAN(  MAX )   MEAN(  MAX )( %CPU)     TOTAL NOWAIT SPIN  NAME",
	"RWLOCK WRITES     HOLD           WAIT (ALL)           WAIT (WW) \n"
    "TOT/SEC CON    MEAN(  MAX )   MEAN(  MAX )( %CPU)   MEAN(  MAX )     TOTAL NOWAIT SPIN(  WW )  NAME",
	"SEMAPHORES  \n  TOT/SEC  CON         TOTAL      NOWAIT       SPIN    SLEEP  NAME",
	""};

/* these titles are used if the -C option is NOT specified */
char	*special_titlesU[] = {
	"SPINLOCKS         HOLD            WAIT\n"
	"  UTIL  CON    MEAN(  MAX )   MEAN(  MAX )(% CPU)     TOTAL NOWAIT SPIN RJECT  NAME",
	"RWLOCK READS   HOLD    MAX  RDR BUSY PERIOD      WAIT\n"
    "  UTIL  CON    MEAN   RDRS   MEAN(  MAX )   MEAN(  MAX )( %CPU)     TOTAL NOWAIT SPIN  NAME",
	"RWLOCK WRITES     HOLD           WAIT (ALL)           WAIT (WW) \n"
    "  UTIL  CON    MEAN(  MAX )   MEAN(  MAX )( %CPU)   MEAN(  MAX )     TOTAL NOWAIT SPIN(  WW )  NAME",
	"SEMAPHORES  \n    UTIL    CON         TOTAL      NOWAIT       SPIN    SLEEP  NAME",
	""};
char **special_titles;

void	do_report(char *, int);
void	print_stats(char *, char *);
void	set_header(char *,char *);
void	reset_counts(lock_summary_t *);
void	add_counts(lock_summary_t *, lstat_lock_counts_t *);
int	sum_counts(lock_summary_t *, entry_type_enum);
int	set_counts(lock_summary_t *, lstat_lock_counts_t *, entry_type_enum);
void	do_help(void);
void	print_header(void);
void	print_title(char *, char *);
void	print_lock(char *, lock_summary_t *, int, entry_type_enum);
void	print_percentage(char, double, char);
void	print_time(double);
void	get_collection_state(int);
void	get_kernel_data(get_data_buffer_enum,int);
void	build_sort_directory(void);
int	sortcmp(const void *, const void *);
void	sum_data (int, int);
int	read_diff_file(void);
void	write_diff_file(void);
int     write_tmp=0;

char    *command;
char    *progname;
extern int	optind, opterr, errno;
extern char	*optarg;

int	semaopt = 0, topt = 0, Popt=0, Ropt=0, Xopt=0, Lopt=0, Copt=0, debugopt = 0, opt_debug = 0;
int Oopt = 0;
double	opt_contention = -1.0, opt_persec = -1.0, opt_utilization=-1.0;
char	*debugname=NULL;

char	*ident = 0;

char    *tmpFilename=defaultGetFilename;
char	*dataFilename;
char	*mapFilename;

char	cpulist[MAXCPUS];

int
main(int argc, char **argv)
{
#ifdef notyet
	static char	optstr[] = "Swdhvk:l:L:Op:tCc:i:m:D:P:R:T:u:X:";
#endif
	static char	optstr[] = "wdhvk:l:L:Op:tCc:i:m:D:P:R:T:u:X:";
	int		c, err = 0;
	int		args, cpunum;
	char		title[120];
	int     tmpargc;
	char    **tmpargv;

	dataFilename = defaultDataFilename;
	mapFilename  = NULL;

	progname = (char *) malloc(strlen(argv[0])+1);
	strcpy(progname, argv[0]);

	/* we make -t the default now */
	topt = 1;
	/* -P is used to specify a periodic report */

	/* special case help */
	if ((argc==1) || ((argc==2) && (!strcmp(argv[1],"--help")))) {
		do_help();
		return(0);
	}

	special_titles = special_titlesU;
	while ((c = getopt(argc, argv, optstr)) != EOF)
		switch (c) {
		case 'D':
			debugopt = 1;
			debugname = optarg;
			break;
		case 'c':
			if ((optarg == NULL) || (*optarg == '-')) {
				fprintf(stderr,"%s: Value(s) missing for -c option\n",progname);
				exit(0);
			}
			if (*optarg == 'a') {
				for (cpunum = 0; cpunum< MAXCPUS; cpunum++)
					cpulist[cpunum]++;
			} else {
				cpunum = atoi(optarg);
				if (cpunum < 0 || cpunum >= MAXCPUS)
					fatalx("invalid cpu number specified");
				cpulist[cpunum]++;
			}
			break;
		case 'C':
			Copt = 1;
			special_titles = special_titlesC;
			break;
		case 'd':
			opt_debug = 1;
			break;
		case 'w':
			opt_persec = 0.0;
			opt_contention = 0.00;
			opt_utilization = 0.0;
			break;
		case 'h':
			opt_persec = 100.0;
			opt_contention = 5.0;
			opt_utilization = 50.0;
			break;
		case 'p':
			sscanf(optarg,"%lg",&opt_persec);
			break;
		case 'k':
			if ((optarg == NULL) || (*optarg == '-')) {
				fprintf(stderr,"%s: Value missing for -k option\n",progname);
				exit(0);
			}
			sscanf(optarg,"%lg",&opt_contention);
			break;
		case 'u':
			if ((optarg == NULL) || (*optarg == '-')) {
				fprintf(stderr,"%s: Value missing for -u option\n",progname);
				exit(0);
			}
			sscanf(optarg,"%lg",&opt_utilization);
			break;
		case 'i':
			if ((optarg == NULL) || (*optarg == '-')) {
				fprintf(stderr,"%s: Value missing for -i option\n",progname);
				exit(0);
			}
			ident = optarg;
			break;
		case 'l':
		case 'L':
			if ((optarg == NULL) || (*optarg == '-')) {
				fprintf(stderr,"%s: File name missing for -L option\n",progname);
				exit(0);
			}
			Lopt = 1;
			dataFilename = optarg;
			break;
#ifdef notyet
		case 'S':
			semaopt++;
			break;
#endif
		case 't':
			topt++;
			break;
		case 'T':
			write_tmp++;
			if ((optarg == NULL) || (*optarg == '-')) {
				fprintf(stderr,"%s: File name missing for -T option\n",progname);
				exit(0);
			}
			tmpFilename = optarg;
			setMirrorOutFilename(tmpFilename);
			break;
		case 'm':
			if ((optarg == NULL) || (*optarg == '-')) {
				fprintf(stderr,"%s: File name missing for -m option\n",progname);
				exit(0);
			}
			mapFilename = optarg;	/* remember we've seen -m */
			if (!openMapFile(optarg)) {
				fprintf(stderr,"%s: Cannot open mapfile '%s'\n",
					progname, optarg);
				exit(1);
			}
			break;
		case 'P':
			if ((optarg == NULL) || (*optarg == '-')) {
				fprintf(stderr,"%s: Value missing for -P option\n",progname);
				exit(0);
			}
			Popt = 1;
			period = atoi(optarg);
			break;
		case 'R':
			if ((optarg == NULL) || (*optarg == '-')) {
				fprintf(stderr,"%s: Count missing for -R option\n",progname);
				exit(0);
			}
			Ropt = 1;
			count = atoi(optarg);
			break;
		case 'O':
			/* set flag to report on locks that meet any ONE of the conditions...  */
			Oopt = 1;
			/* update defaults so an OR of the conditions doesn't print every lock */
			/* have to do the if test to handle the case where -O is encountered   */
			/* after an option that specified a value for the indicated parameter..*/
			if (opt_utilization < 0) opt_utilization = 200.00;
			if (opt_persec < 0)      opt_persec      = 1000000000.0;
			if (opt_contention < 0)  opt_contention  = 200.00;
			break;
		case 'v':
			printf("lockstat version %s\n", lockstat_version);
			exit(1);
		case 'X':
			if ((optarg == NULL) || (*optarg == '-')) {
				fprintf(stderr,"%s: Command missing for -X option\n",progname);
				exit(0);
			}
			Xopt = 1;
			command = optarg;
			break;
		case '?':
			err = 1;
			break;
		}

	if (Popt && !Ropt && !Lopt) 
		fprintf(stderr, "%s: -P option but no -R option; count defaults to %d\n",progname,count);

	dataFile = openKernelData(dataFilename);
	if (dataFile == -1)
		{
			perror(dataFilename);
			exit(1);
		}

	TSTAMP("start");
	if (debugopt && read_diff_file())
		debugopt = 2;
	else
		get_collection_state(0);

	TSTAMP("inited");
	args = argc - optind;
	if (err || args < 0)
		fatalx("invalid arguments specified");

	if (!args) {
		fprintf(stderr,"%s: A keyword argument is required.  Type 'lockstat --help' for help.\n",progname);
		return(0);
	}


	/* process action words specified on the command line */
	if (args) {

		/* the only keywords allowed with -X and -P are 'get' and 'print' */
		if ((Xopt || Popt) && strcmp(argv[optind],"get") && strcmp(argv[optind],"print")) {
			fprintf(stderr,"%s: The %s keyword may not be specified with -X or -P\n",progname, argv[optind]);
			exit(999);
		}

		/* the only keyword allowed with -L is print */
		if (Lopt && !isProcFile()  && strcmp(argv[optind],"print")) {
			fprintf(stderr,"%s: The %s keyword may not be specified with -L\n",progname,argv[optind]);
			exit(999);
		}

		/* turn statistics on, and if 1st time allocate storage  */
		/* for lockmeter statistics. ..........................  */
		if (strcmp(argv[optind], "on") == 0) {
			setCollectionState(LSTAT_ON);
			return(0);

		/* reset the statistics */
		} else if (strcmp(argv[optind], "reset") == 0) {
			setCollectionState(LSTAT_RESET);
			return(0);

		/* release the lockmeter storage in the kernel */
		} else if (strcmp(argv[optind], "release") == 0) {
			setCollectionState(LSTAT_RELEASE);
			return(0);

		/* stop measurement.  can be resumed by 'lockstat on' */
		/* data is accumulated until 'lockstat reset'         */
		} else if (strcmp(argv[optind], "off") == 0)  {
			setCollectionState(LSTAT_OFF);
			return(0);

		/* stop measurement and print results.  can be resumed by 'lockstat on' */
		/* data is accumulated until 'lockstat reset'         */
		} else if (strcmp(argv[optind], "off+print") == 0)  {
			setCollectionState(LSTAT_OFF);
			keyword_valid=1;

		/* print just causes topt to be set unless Popt is set */
		} else if (strcmp(argv[optind], "print") == 0)  {
			if (!Popt) topt++;
			keyword_valid=1;

		/* get just causes write_tmp to be set */
		} else if (strcmp(argv[optind], "get") == 0)  {
			write_tmp++;	
			keyword_valid=1;
			setMirrorOutFilename(tmpFilename);

		/* query just prints the state as obtained from get_collection_state(0) */
		} else if (strcmp(argv[optind], "query") == 0) {
			if (state)
				fprintf(stderr,"Lockmeter statistics are currently ON\n" 
					"%6.2f seconds of data are available for print/get.\n",deltatime);
			else {
				fprintf(stderr,"Lockmeter statistics are currently OFF.\n");
				if (intervals>0)
					fprintf(stderr,"Stored Lockmeter data is available for print/get:\n\t" 
						"%d intervals and a total of %6.2f seconds of data are available.\n",
						intervals,deltatime);
				else
					fprintf(stderr,"No stored Lockmeter data is available for print/get.\n");
				}
			return(0);
		}
	}

	if (!keyword_valid && (args)) {
		fprintf(stderr,"%s: Unknown keyword '%s' on command line.\n",progname,argv[optind]);
		return(999);
	}

	if (!valid) {
		fprintf(stderr,"No Lockmeter data available.\n");
		fprintf(stderr,"Either:\n");
		fprintf(stderr,"(1) Statistics are not 'on' and this is a periodic sample run (-P), or\n");
		fprintf(stderr,"(2) Statistics are not 'on' and this is a command run (-X), or\n");
		fprintf(stderr,"(3) You have yet to do a 'lockstat on; lockstat off'\n");
		fprintf(stderr,"    cycle since system boot or most recent 'lockstat reset'.\n");
		fprintf(stderr,"(1) and (2) can be fixed by either doing a 'lockstat on' and repeating\n");
		fprintf(stderr,"the command or by adding the keyword 'on' at the end of the command.\n");
		return(999);
	}

	/* it is pointless to require a mapFilename if we are just writing the */
	/* lockmeter data out into a file..................................... */
	if (!write_tmp  &&  mapFilename == NULL) {
		mapFilename  = defaultMapFilename;
		if (!openMapFile(mapFilename)) {
			fprintf(stderr, "%s: Cannot open mapfile '%s'\n",
				progname, mapFilename);
			exit(1);
		}
	}

	/* topt is true by default -- turn it off if we want a periodic report */
	/* either from /proc/lockmeter or a saved data file. */
	if (Popt || (Ropt && Lopt)) topt=0;

	if (!Xopt) {
		int		i, sleepsec=0, sleepcnt=1;
		/* it is pointless to sleep if we are just reading from a file */
		/* so if user specifed -L and -P, ignore the -P */
		if (Lopt) {
			sleepsec = 0;
			if (Popt)
				fprintf(stderr,"%s: -L option causes -P option to be ignored\n",progname);
		}
		else
			sleepsec = period;
		/* one can use the -R option without the -P to specify a count if -L*/ 
		/* is specified */
		if (Ropt) {
			if (Popt || Lopt)
				sleepcnt = count;
			else 
				fprintf(stderr,"%s: -R ignored; -R option requires -P or -L\n",progname);
		}
		if (Popt && !state) {
			turned_on_this_time = 1;
			setCollectionState(LSTAT_ON);
			get_collection_state(0);
		}
		for (i=1; i<= sleepcnt; i++) {
			if (!topt)
				get_kernel_data(Buf_Previous,i==1);
			if (sleepsec)
				sleep(sleepsec);
			get_kernel_data(Buf_Current,0);
			/* in this case all we do is copy the data out to the temp file */
			/* inside of get_kernel_data, so we can skip the reporting phase*/
			if (write_tmp) continue;
			if (i > 1)
				printf("\n\n");
			if (topt)
				sprintf(title, "Total counts\n");
			else 
				if (!Lopt)
					sprintf(title, "Periodic sample %d of %d. Sample period: %d secs\n",
						i, sleepcnt, sleepsec);
				else
					sprintf(title, "Periodic sample %d of %d. Sample period: %d secs\n",
						i, sleepcnt, end_time-start_time);
			do_report(title, (i == sleepcnt));
		}

		/* if we turned on stats this time, turn them off when done */
		if (turned_on_this_time) 
			setCollectionState(LSTAT_OFF);
		else if (Popt) 
			/* print a message so the user knows what state stats are in */
			fprintf(stderr,"%s: Lockmeter statistics are ON after end of periodic run.\n",progname);

	} else {
		int		pid, stat;

		if (Popt || Lopt) {
			fprintf(stderr,"%s: Can't specify -X option with -L or -P options\n",progname);
			return(0);
		}

		/* get the statistitics to a known state */
		if (!state) {
			setCollectionState(LSTAT_ON);
			get_collection_state(0);
			turned_on_this_time = 1;
		}

		get_kernel_data (Buf_Previous,TRUE);
		if ((pid=fork()) == 0) {
			execvp(command, &command);
			fprintf(stderr,"%s: unable to exec command=%s\n",progname,command);
			exit(1);
		} else if (pid < 0) {
			perrorx("fork failed");
		}
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		while (wait(&stat) != pid);
		if((stat&0377) != 0)
			fprintf(stderr,"%s: Command terminated abnormally.\n",progname);
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		get_kernel_data(Buf_Current,FALSE);
		/* don't create a report if we are copying data to a tmp file */
		if (!write_tmp) { 
			strcpy(title, "Command: ");
			for (; optind < argc; optind++) {
				if(5+strlen(title)+strlen(argv[optind]) > sizeof(title)) {
					strcat(title, " ...");
					break;
				}
				strcat(title, argv[optind]);
				strcat(title, " ");
			}
			do_report(title, 1);
		}

		if (turned_on_this_time)
			setCollectionState(LSTAT_OFF);
		else
			fprintf(stderr,"%s: Lockmeter statistics are ON after command execution.\n",progname);
	}

	if (write_tmp)
		closeFiles();

	TSTAMP("done");
	return(0);
}


void
do_help(void)
{
	char	**p;

	for (p=helpstr; *p; p++) 
		printf("%s\n", *p);
}


#ifdef DEBUGBUG
int number_read_locks=0;
#endif

void
do_report(char *title, int last_report)
{
	int		cpu;
	char		name[100];

	TSTAMP("start do report");
	build_sort_directory();

	TSTAMP("finish sort");
	sum_data(0, numcpus - 1);
	sprintf(name, "All (%d) CPUs", numcpus);
	print_stats(title, name);

	for (cpu = 0; cpu < numcpus; cpu++) {
		if (cpulist[cpu] == 0)
			continue;
		sum_data(cpu, cpu);
		sprintf(name, "CPU:%d", cpu);
		print_stats(title, name);
	}
	if (last_report) {
		printf(
		"_________________________________________________________________________________________________________________________\n");
		#ifdef DEBUGBUG
		printf("Number of read locks found=%d\n",number_read_locks);
		#endif
	}
	fflush(stdout);
	TSTAMP("end do report");
}


void
print_stats(char *title1, char *title2)
{
	entry_type_enum		entry_type, current_entry_type = Null_Entry;
	lock_summary_t		sum_count;
	int			i, j, k, si, sj;


	print_title(title1, title2);
	multiflag = 1;

	for (i = 1; i < next_free_dir_index; i++) {
		si = sorti[i];
		entry_type = directory[si].entry_type;
		if (entry_type == Sema_Entry && !semaopt)
			continue;
		set_header (dashes[directory[si].title_index],special_titles[directory[si].title_index]);

		if (entry_type != current_entry_type) {
			current_entry_type = entry_type;
			reset_counts (&sum_count);
			for (j = i; j < next_free_dir_index; j++) {
				sj = sorti[j];
				if (directory[sj].entry_type != entry_type)
					break;
				add_counts (&sum_count, &total_counts[sj]);
			}
			sum_counts (&sum_count, entry_type);
			if (sum_count.total > 0) {
				skipline = 1;
				print_lock("*TOTAL*", &sum_count, 0, entry_type);
				skipline = 1;
			}
		}

		if (!directory[si].multilock) {
			k = i;
			reset_counts (&sum_count);
			while (k < next_free_dir_index &&
			       strcmp(directory[sorti[k]].lock_name,directory[si].lock_name) == 0) {
				add_counts (&sum_count, &total_counts[sorti[k]]);
				k++;
			}
			skipline = 1;
			/* don't print anything if there are no locks of this entry_type */
			sum_count.lock_ptr = (void *)kernel_directory[si].lock_ptr;
			if (sum_counts (&sum_count, entry_type)) {
				/* this line prints the summary statistics */
				print_lock(directory[si].lock_name,  &sum_count, 0, entry_type);
#ifdef DEBUGBUG
				if (entry_type == RLspin_Entry) {
					/* printf("Lockname=%s\n",directory[si].lock_name); */
					number_read_locks++;
				}
#endif
				for (j = i; j < k; j++) {
					sj = sorti[j];
					set_counts (&sum_count, &total_counts[sj], entry_type);
					/* total is the total number of requests -- skip if zero */
					if (sum_count.total)
						/* this line prints the per call statistics for this lock */
						print_lock(directory[sj].caller_name, &sum_count, 1, entry_type);
				}
			}
			i = k - 1;
			skipline = 1;
		} else {
		    /* don't print summary info for readlocks in the multi case */
			if (entry_type == RLspin_Entry) multiflag=0;
			else multiflag=1;
			if (set_counts (&sum_count, &total_counts[si], entry_type))
				print_lock(directory[si].caller_name, &sum_count, 0, entry_type);
		}
	}
}



void
reset_counts(lock_summary_t *sump)
{
	int		j;
	sump->counts.cum_hold_ticks = 0;
	sump->counts.cum_wait_ticks = 0;
	sump->counts.max_wait_ticks = 0;
	sump->counts.max_hold_ticks = 0;
	sump->counts.cum_wait_ww_ticks = 0;
	sump->counts.max_wait_ww_ticks = 0;
	for (j = 0; j < LSTAT_ACT_MAX_VALUES; j++)
		sump->counts.count[j] = 0;
}


int
set_counts(lock_summary_t *sump, lstat_lock_counts_t *countp, entry_type_enum entry_type)
{
	int		j;

	sump->counts.cum_hold_ticks = countp->cum_hold_ticks;
	sump->counts.cum_wait_ticks = countp->cum_wait_ticks;
	sump->counts.max_wait_ticks = countp->max_wait_ticks;
	sump->counts.max_hold_ticks = countp->max_hold_ticks;
	sump->counts.cum_wait_ww_ticks = countp->cum_wait_ww_ticks;
	sump->counts.max_wait_ww_ticks = countp->max_wait_ww_ticks;
	for (j = 0; j < LSTAT_ACT_MAX_VALUES; j++)
		sump->counts.count[j] = countp->count[j];
	return(sum_counts(sump, entry_type));
}


void
add_counts(lock_summary_t *sump, lstat_lock_counts_t *countp)
{
	int		j;

	sump->counts.cum_hold_ticks += countp->cum_hold_ticks;
	sump->counts.cum_wait_ticks += countp->cum_wait_ticks;
	if (sump->counts.max_wait_ticks < countp->max_wait_ticks)
	    sump->counts.max_wait_ticks = countp->max_wait_ticks;
	if (sump->counts.max_hold_ticks < countp->max_hold_ticks)
	    sump->counts.max_hold_ticks = countp->max_hold_ticks;
	sump->counts.cum_wait_ww_ticks += countp->cum_wait_ww_ticks;
	if (sump->counts.max_wait_ww_ticks < countp->max_wait_ww_ticks)
	    sump->counts.max_wait_ww_ticks = countp->max_wait_ww_ticks;
	for (j = 0; j < LSTAT_ACT_MAX_VALUES; j++)
		sump->counts.count[j] += countp->count[j];
}


int
sum_counts(lock_summary_t *sump, entry_type_enum entry_type)
{
	int		total, i, j;
	double	contention;
	double  utilization;

	for (total = 0, j = 0; j < LSTAT_ACT_MAX_VALUES; j++)
		total += sump->counts.count[j];
	sump->total = total;
	sump->persec = total / deltatime;

	contention = (total) ? 100.0 - (100.0 * sump->counts.count[LSTAT_ACT_NO_WAIT] / total)
			     : 0.0;
	sump->contention = contention;

	switch(entry_type) {
		case Spin_Entry: 
		case WLspin_Entry:
		    utilization = (double) (sump->counts.cum_hold_ticks)/(cycles_per_usec*1000000.0*deltatime)*100.00;	
			break;
		case RLspin_Entry:
			/* we have to find the read block info for this lock pointer */
			for(i=1;i<next_free_read_lock_index;i++)
				if (read_lock_counts[i].lock_ptr == sump->lock_ptr) {
					utilization=(double)read_lock_counts[i].busy_ticks/(deltatime*cycles_per_usec*1000000.0)*100.00;
					goto exit_switch;
				}
			/* oops -- didn't find the read lock entry for this lock -- set bogus value */
			utilization = -1.0;
			break;
		default:
			fprintf(stderr,"%s: Internal Error at line %d\n",progname,__LINE__);
			exit(999);
	}
exit_switch:
	sump->utilization = utilization;

	if (!Oopt)
		return (total && sump->persec > opt_persec && 
				(contention > opt_contention)
				&& (utilization > opt_utilization)
				);
	else
		return (total && sump->persec > opt_persec || 
				(contention > opt_contention)
				|| (utilization > opt_utilization)
				);
}


void
set_header(char *dashes, char *header)
{
	if (header != last_header) {
		current_dashes = dashes;
		current_header = header;
		last_header = header;
	}
}


void
print_header(void)
{
	if (current_header != NULL) {
		printf("\n%s", current_dashes);
		printf("%s\n", current_header);
		current_header = NULL;
	}
}


void
print_title(char *title1, char *title2)
{
	static int have_printed_warning = 0;

	printf("___________________________________________________________________________________________\n");
	printf("System: %s %s %s %s %s\n",
	     uts.sysname, uts.nodename, uts.release, uts.version, uts.machine);
	if (ident)
		printf("Ident: %s\n", ident);
	printf("%s\n", title1);
	printf("%s\n", title2);
	if (!Oopt) {
		/* compare params to defaults to see if any were set by options */
		/* defaults set by initializer unless -O is specified. ........ */
		if (opt_persec > -1.0 || opt_contention > -1.0 || opt_utilization > -1.0) {
				printf("Selecting locks that meet ALL of the following:\n");
				if (opt_persec > -1.0 )
					printf("\trequests/sec:\t>%6.2lf/sec\n", opt_persec);
				if (opt_contention > -1.0)
					printf("\tcontention  :\t>%6.2lf%%\n", opt_contention);
				if (opt_utilization > -1.0)
					printf("\tutilization :\t>%6.2lf%%\n", opt_utilization);
				printf("\n");
		}
	} else /* compare params to defaults to see if any were set by options ......... */
	       /* note that -O causes the defaults to be changed (see option processing) */
		if (opt_persec < 1000000000.0 || opt_contention < 200.0 || opt_utilization < 200.0) {
			printf("Selecting locks that meet ONE of the following:\n");
			if (opt_persec < 1000000000.0)
				printf("\trequests/sec:\t>%6.2lf/sec\n", opt_persec);
			if (opt_contention < 200.0)
				printf("\tcontention  :\t>%6.2lf%%\n", opt_contention);
			if (opt_utilization < 200.0)
				printf("\tutilization :\t>%6.2lf%%\n", opt_utilization);
			printf("\n");
		}
	printf("\n");
	if (intervals > 1) {
		printf("First Start time: %s",ctime(&first_start_time));
		printf("Last Start  time: %s", ctime(&start_time));
		printf("Last End    time: %s", ctime(&end_time));
		printf("Measurements enabled for: %.2f sec in %d intervals\n",deltatime,intervals);
	} else {
		printf("Start time: %s", ctime(&start_time));
		printf("End   time: %s", ctime(&end_time));
		printf("Delta Time: %.2f sec.\n",  deltatime);
	}
	printf("Hash table slots in use:      %d.\n",  next_free_dir_index-1);
	printf("Global read lock slots in use: %d.\n", next_free_read_lock_index-1);

	if (directory_overflows + read_lock_overflows  &&  !have_printed_warning) {
		fprintf(stderr,"%s: One or more warnings were printed with the report.\n",progname);
		printf("\n*************************** Warnings! ******************************\n");
		if (directory_overflows)
			printf("\tDirectory table overflowed.\n");
		if (read_lock_overflows)
			printf("\tRead Lock table overflowed.\n");
		printf("\n\tThe data in this report may be in error due to this.\n");
		printf("************************ End of Warnings! **************************\n\n");
		have_printed_warning = 1;
	}

}

/*
 *  Printf a percentage value to fit nicely into 4-7 columns:
 *	leading char, 4 digits, percent sign, trailing char
 */
void
print_percentage(char lead_char, double percent, char trail_char)
{
	if (lead_char) printf("%c",lead_char);
	if (percent > 99.9  ||  percent == 0.0)
		printf("%4.0f%%", percent);
	else if (percent > 1)
		printf("%4.1f%%", percent);
	else	printf("%4.2f%%", percent);
	if (trail_char) printf("%c",trail_char);
}

/*
 *  Printf a time value to fit nicely into 6 columns
 */
void
print_time(double timeval)
{
	if (timeval < 10)
	    printf("%4.1fus", timeval);
	else if (timeval <  10000)
	    printf("%4.0fus", timeval);
	else
	    printf("%4.0fms", timeval/1000);
}

void
print_lock(char *name, lock_summary_t *sump, int indent, entry_type_enum entry_type)
{
	int entry_skipline=skipline,i;
	float mean_hold_time, max_hold_time;
	float wait_time, mean_wait_time, max_wait_time, mean_wait_ww_time, max_wait_ww_time;
	double temp_time;
	uint32_t total_spin_count = 0;

	print_header();

	if (skipline)
		printf("\n");
	skipline = 0;

	/* print lock count or utilization based on the presence or absence of the -C flag..... */
	if (Copt)
		printf((sump->persec < 100) ? "%6.1f" : "%6.0f", sump->persec);
	else {
		/* the TOTAL of the lock utilizations is a useless number so don't print it........ */
		if (!strcmp(name,"*TOTAL*")) {
			printf("      ");
		}
			/* if the entry type is Spin_Entry or WLspin_Entry, or this is the summary line for */
			/* a READ mode lock, then print the utilization.  Elsewise we are printing the per  */
			/* caller info for a READ mode lock and there is no per caller util data available  */
			/* so print blanks. ............................................................... */
		else if ((entry_type != RLspin_Entry) || (entry_skipline == 1) && multiflag) {
				print_percentage(' ',sump->utilization,0);
			} else 
				printf("      ");
	}

	print_percentage(' ',sump->contention,0);

	if (entry_type == Spin_Entry || entry_type == RLspin_Entry || entry_type == WLspin_Entry) {
		mean_hold_time = sump->counts.cum_hold_ticks / cycles_per_usec / sump->total;
		max_hold_time = sump->counts.max_hold_ticks / cycles_per_usec;
		wait_time = sump->counts.cum_wait_ticks / cycles_per_usec;
		max_wait_time = sump->counts.max_wait_ticks / cycles_per_usec;
		total_spin_count = sump->counts.count[LSTAT_ACT_SPIN];
		if (entry_type == WLspin_Entry) {
			wait_time += sump->counts.cum_wait_ww_ticks / cycles_per_usec;
			max_wait_time = max(max_wait_time, sump->counts.max_wait_ww_ticks / cycles_per_usec);
			total_spin_count += sump->counts.count[LSTAT_ACT_WW_SPIN];
		}
		if (wait_time > 0) {
			if (LSTAT_ACT_SLEPT < LSTAT_ACT_MAX_VALUES  &&
			    sump->counts.count[LSTAT_ACT_SLEPT]) {
				mean_wait_time = wait_time / sump->counts.count[LSTAT_ACT_SLEPT];
			} else if (total_spin_count) {
				mean_wait_time = wait_time / total_spin_count;
			} else {
				mean_wait_time = 0;
				wait_time = 0;
			}
		}
		
		if ((entry_type == Spin_Entry) || (entry_type == WLspin_Entry)) {
			if (mean_hold_time > 0) {
				printf(" ");
				print_time(mean_hold_time);
				printf("("); print_time(max_hold_time); printf(")");
			} else { /* no hold_time data, so just print blanks */
				printf("               ");
			}
		}

		if (entry_type == RLspin_Entry) {
			/* entry_skipline == 1 indicates this is the first line printed for this lock */
			/* so we print statistics related to the global read stats for this lock,     */
			/* unless this is a *TOTAL* entry, for which those quantities make no sense.. */
			/* also if this is a multilock entry, these quantities make no sense. ....... */
		    if ((entry_skipline == 1) && strcmp(name,"*TOTAL*") && multiflag) {
			    for(i=1;i<next_free_read_lock_index;i++) {
					if(read_lock_counts[i].lock_ptr == sump->lock_ptr) {
						/* keep track of which entries we found directory entries for.... */
						read_lock_entry_used[i]++;
						if (read_lock_increment[i] >= 0) {
							printf("%6.1fus",
								(float)read_lock_counts[i].cum_hold_ticks/
									(cycles_per_usec*
										(read_lock_counts[i].read_lock_count+read_lock_increment[i])));
							/* insert a marker to indicate that we estimated the stats   */
							/* for this lock because the data contained a negative sum   */
							if ((read_lock_increment[i] > 0) || (prev_read_lock_increment[i]>0))
								printf("??");
							else
								printf("  ");
							/* print the max number of readers statistic  */
							printf("%4d ",read_lock_counts[i].max_readers);
						}
						/* most likely a lock was held in read mode when we sampled the  */
						/* data.  sorry about that.  we did the best we could........... */
						else /* read_lock_increment < 0 */
							printf("  Data Sync Error ");
						/* print the average and max rdr busy period times.............. */
						/* (we know busy_period > 0 else why is this lock in the table?) */
						temp_time = read_lock_counts[i].busy_ticks/(read_lock_counts[i].busy_periods
								*cycles_per_usec),
						print_time(temp_time);
						temp_time = read_lock_counts[i].max_busy/cycles_per_usec;
						printf("("); print_time(temp_time); printf(")");
						goto found_it;
					}
				}
				/* RLEntry but no entry in the read_lock_counts table */
				printf("RWLOCK LookupError                  ");
			} else {
				printf("                             ");
			}
		}
	found_it:

		if (wait_time > 0) {
			printf(" ");
			print_time(mean_wait_time);
			printf("("); print_time(max_wait_time); printf(")");
			print_percentage('(',100.0*wait_time/(numcpus*deltatime*1000000),')');
		} else {  /* wait_time is zero */
			printf("    0us               ");
		}

		if (entry_type == WLspin_Entry) {
			int i,sum;
			if (sump->counts.count[LSTAT_ACT_WW_SPIN] > 0)
				mean_wait_ww_time = 
					sump->counts.cum_wait_ww_ticks / cycles_per_usec 
						/ sump->counts.count[LSTAT_ACT_WW_SPIN];
			else 
				mean_wait_ww_time = 0;

			if (mean_wait_ww_time > 0) {
				printf(" ");
				print_time(mean_wait_ww_time);
				max_wait_ww_time = sump->counts.max_wait_ww_ticks / cycles_per_usec;
				printf("("); print_time(max_wait_ww_time); printf(")");
			} else {  /* wait_time is zero */
				printf("    0us        ");
			}

			/* FIXME -- delete this debugging code***************
			sum=0;
			for(i=0;i<LSTAT_ACT_MAX_VALUES;i++) {
				printf(" sump->counts.count[%d]=%d",i,sump->counts.count[i]);
				sum += sump->counts.count[i];
			}
			printf("sum=%d sump->total=%d",sum,sump->total);
			 **************************************************** */
		}
	}

	/* following done for ALL types of locks, except as indicated */
	printf(" %9d", sump->total);
	print_percentage(' ',100.0*(double)sump->counts.count[LSTAT_ACT_NO_WAIT]/(double)sump->total,0);
	print_percentage(' ',100.0*(double)sump->counts.count[LSTAT_ACT_SPIN]/(double)sump->total,0);
	if (entry_type == WLspin_Entry) {
		print_percentage('(',100.0*(double)sump->counts.count[LSTAT_ACT_WW_SPIN]/(double)sump->total,
				 ')');
	}
#ifdef notyet
	printf("%9d", sump->counts.count[LSTAT_ACT_SLEPT]);
#endif
	if (entry_type == Spin_Entry) {
		/* We only see these for spinlock_t, and even then it's rare. */
		print_percentage(' ',100.0*(double)sump->counts.count[LSTAT_ACT_REJECT]/(double)sump->total,0);
	}
	printf("%s  %s", (indent ? "  ":""), name);
	printf("\n");
}

void
get_collection_state(int verify_unix)
{
	lstat_user_request_t	request;
	lstat_user_request_t *	request_ptr = &request;
	
	getKernelData(&request_ptr,NULL,NULL,NULL,TRUE);

	if (request.lstat_version != LSTAT_VERSION) {
	    fprintf(stderr,"Lockstat version does not match that of /usr/include/linux/lockmeter.h!",progname);
		fprintf(stderr,"Lockstat version %d != Kernel Version %d\n",progname);
	    exit(1);
	}

	if (request.lstat_version != THIS_VERSION) {
	    fprintf(stderr,"Lockstat version %d data found; this program only reads version %d data\n",
			request.lstat_version, THIS_VERSION);
	    exit(1);
	}

	if (debugopt) {
		printf("get_collection_state: sizeof(lstat_user_request_t)=%d\n",sizeof(lstat_user_request_t));
		printf("get_collection_state: sizeof(lstat_cpu_counts_t)=%d\n",sizeof(lstat_cpu_counts_t));
		printf("get_collection_state: sizeof(lstat_read_lock_cpu_counts_t)=%d\n",sizeof(lstat_read_lock_cpu_counts_t));
		printf("get_collection_state: request.maxcpus=%d\n",request.maxcpus);
		printf("get_collection_state: LSTAT_MAX_STAT_INDEX=%d sizeof(lstat_directory_entry_t)=%d\n",
			LSTAT_MAX_STAT_INDEX, sizeof(lstat_directory_entry_t));
	}

	numcpus = request.maxcpus;
	intervals = request.intervals;
	state = request.state;
	valid = request.state || (intervals>0);
	cycles_per_usec = (float)request.cycleval / 1000000.0;
#ifdef notyet
	kernel_magic_addr = request.kernel_magic_addr;
	kernel_end_addr = request.kernel_end_addr;
#endif
	deltatime = (double) request.enabled_cycles64 / cycles_per_usec / 1000000.0;
	if (debugopt) {
		printf("get_collection_state: deltatime=%6.2f\n", deltatime);
	}

	/* this is a mess, but try to do the best thing here */
	if((intervals>0) && ((Popt) || (Ropt))) {
		fprintf(stderr,"Summary of %d intervals stored in kernel lockmeter data\n",intervals);
		if (Lopt) {
			fprintf(stderr,"It appears that the lockmeter data file %s was not generated\n"
						   "via a lockstat -P command.  Remove the -P and -R options and\n" 
						   "try again, or generate a new lockmeter data file.\n",
							dataFilename);
		} else {
			fprintf(stderr,"You cannot request a period report (-P) report of this data.\n");
			fprintf(stderr,"Reset the stats with 'lockstat reset' and try again.\n");
		}
		exit(999);
	}
}

/*
 * read the data out of the kernel via the proc file system
 * copy the data either to kernel_{counts,directory,_read_lock_counts}
 * or to prev_{counts,directory,,_read_locks_counts} depending
 * on the value of bufid (Buf_Previous or Buf_Current)
 */
void	
get_kernel_data(get_data_buffer_enum bufid, int first)
{
	lstat_user_request_t	request;
	lstat_user_request_t *	request_ptr = &request;
	int		ret_count;
	int		cpu_counts_len;
	int             read_lock_counts_len;
	int             i,cpu;

	if (debugopt == 2)
		return;

	if (debugopt) {
		printf("get_kernel_data: bufid=");
		if (bufid == Buf_Previous) printf(" Buf_Previous");
		else printf(" Buf_Current");
		printf(" first=%d\n",first);
	}

	if (bufid == Buf_Current || (bufid == Buf_Previous && first)) {

	  	cpu_counts_len = numcpus * sizeof(lstat_cpu_counts_t);
		read_lock_counts_len = numcpus * sizeof(lstat_read_lock_cpu_counts_t);  	

		if (bufid == Buf_Previous) {
			getKernelData(&request_ptr, &prev_counts,
				      &prev_kernel_directory,
				      &prev_read_lock_counts, FALSE);

			/* we must be in a -P case or a -R + -L case */
			start_time = request.ending_time;
			started_cycles64  = request.ending_cycles64;                             
		} else {
			getKernelData(&request_ptr, &kernel_counts,
				      &kernel_directory,
				      &kernel_read_lock_counts, FALSE);

			end_time = request.ending_time;
			ending_cycles64 = request.ending_cycles64;                                
			if (topt) {
				first_start_time = request.first_started_time;
				start_time = request.started_time;
				deltatime = (double) request.enabled_cycles64
						/ cycles_per_usec / 1000000.0;
			} else {
				deltatime = (double)
				   (ending_cycles64-started_cycles64)
					/ cycles_per_usec / 1000000.0;
			}
		}

		uts=request.uts;

		next_free_dir_index = request.next_free_dir_index;
		next_free_read_lock_index = request.next_free_read_lock_index;              
		directory_overflows += request.dir_overflow;
		read_lock_overflows += request.rwlock_overflow;
		/* BUSTED */
		if (debugopt && bufid == Buf_Current) 
			write_diff_file();
	} else {
		if (write_tmp) return;
		/* copy the kern data to the prev data and set up control vars appropriately */
	  	cpu_counts_len = numcpus * sizeof(lstat_cpu_counts_t);
		read_lock_counts_len = numcpus * sizeof(lstat_read_lock_cpu_counts_t);  	
		prev_request = kernel_request;
		fflush(stderr);
		memcpy(prev_counts,kernel_counts, cpu_counts_len);
		memcpy(prev_read_lock_counts,kernel_read_lock_counts,read_lock_counts_len);                                
		start_time = prev_request.ending_time;
		started_cycles64  = prev_request.ending_cycles64;                             
	}


#ifdef LOCKMETER_DEBUG_PRINT
	if (bufid == Buf_Current) {
		printf("Data from Buf_Current:\n");
		for(cpu=0;cpu<=numcpus-1;cpu++)
			for(i=1;i<next_free_read_lock_index;i++) {
				printf("Data from kernel_read_lock_counts[%d][%d]:\n",
					cpu, i);
#define printit(name,fmt) printf(#name"="#fmt"\n",request.##name)
				printit(started_cycles64,%Ld);
				printit(ending_cycles64,%Ld);
				printit(enabled_cycles64,%Ld);
				printit(intervals,%d);
#undef printit
#define printit(name,fmt) printf(#name"="#fmt"\n",kernel_read_lock_counts[cpu][i].##name)
				printit(cum_hold_ticks,%Ld);	
				printit(lock_ptr,%X);
				printit(read_lock_count,%d);
				printit(lock_val,%X);
				printit(increments,%d);
				printit(decrements,%d);
				printit(read_lock_calls,%d);
				printit(read_unlock_calls,%d);
				printit(initial_lock_val,%d);
				printit(last_read_lock_caller,%X);
				printit(last_read_unlock_caller,%X);
				printit(read_lock_reset_index,%d);
				printit(read_unlock_stats_off,%d);
				printit(write_lock_calls,%d);
				printit(write_unlock_calls,%d);
				printit(last_write_lock_caller,%X);
				printit(last_write_unlock_caller,%X);
				printit(write_lock_reset_index,%d);
#undef printit
			}
	}
	else {
		printf("Data from Buf_Previous:\n");
#define printit(name,fmt) printf(#name"="#fmt"\n",prev_read_lock_counts[cpu][i].##name)
		for(cpu=0;cpu<=numcpus-1;cpu++)
			for(i=1;i<next_free_read_lock_index;i++) {
				printf("Data from prev_read_lock_counts[%d][%d]:\n",
					cpu, i);
				printit(cum_hold_ticks,%Ld);	
				printit(lock_ptr,%X);
				printit(read_lock_count,%d);
				printit(lock_val,%X);
				printit(increments,%d);
				printit(decrements,%d);
				printit(read_lock_calls,%d);
				printit(read_unlock_calls,%d);
				printit(initial_lock_val,%d);
				printit(last_read_lock_caller,%X);
				printit(last_read_unlock_caller,%X);
				printit(read_lock_reset_index,%d);
				printit(read_unlock_stats_off,%d);
				printit(write_lock_calls,%d);
				printit(write_unlock_calls,%d);
				printit(last_write_lock_caller,%X);
				printit(last_write_unlock_caller,%X);
				printit(write_lock_reset_index,%d);
		}
	}
#undef printit
#endif

}


void
build_sort_directory(void)
{
	static int	last_next_free_dir_index = 1;
	int		i, lowbits;
	char		*namep;

#ifdef ZZZ
	{
		int	chain[64];
		int	i, j, k, n;
		for (i=0; i<64; i++) 
			chain[i] = 0;
		for (i = 0; i < next_free_dir_index; i++) {
			for (j=kernel_directory[i].next_stat_index, n=0; j; j = kernel_directory[j].next_stat_index)
				n++;
			chain[n]++;
		}	
		printf ("Total entries %d\n", next_free_dir_index);
		for (i=0; i<64; i++) 
			printf("Chain %3d, %5d\n", i, chain[i]);
		exit(0);	
	}
#endif
	for (i = last_next_free_dir_index; i < next_free_dir_index; i++) {
		sorti[i] = i;
		lowbits = ((unsigned long)kernel_directory[i].caller_ra & 3UL);
		directory[i].caller_ra = (void *)((unsigned long)kernel_directory[i].caller_ra & ~3UL);
		directory[i].caller_name = strSpace();
		XlateAddressToSymbol((void*)directory[i].caller_ra, directory[i].caller_name);
		namep = strchr(directory[i].caller_name, '+');
		if (namep)
			directory[i].caller_name_len = namep - directory[i].caller_name;
		else
			directory[i].caller_name_len = strlen(directory[i].caller_name);

		directory[i].lock_name = NULL;
		directory[i].multilock = 0;
		if (kernel_directory[i].lock_ptr != LSTAT_MULTI_LOCK_ADDRESS) {
			namep = strSpace();			 /* ZZZ */
			XlateAddressToSymbol(kernel_directory[i].lock_ptr, namep);
			if (!isdigit(*namep))
				directory[i].lock_name = namep;
			else
				*namep = '\0';
		}
		switch (lowbits) {
		case LSTAT_RA_SPIN:
			directory[i].entry_type = Spin_Entry;
			directory[i].title_index = 0;
			break;
		case LSTAT_RA_READ:
			directory[i].entry_type = RLspin_Entry;
			directory[i].title_index = 1;
			break;
		case LSTAT_RA_WRITE:
			directory[i].entry_type = WLspin_Entry;
			directory[i].title_index = 2;
			break;
#ifdef notyet
		case LSTAT_RA_SEMA:
			directory[i].entry_type = Sema_Entry;
			directory[i].title_index = 3;
			break;
#endif
		default:
			directory[i].entry_type = Null_Entry;
			directory[i].title_index = 0;
		}
		if (directory[i].lock_name == NULL) {
			directory[i].lock_name = directory[i].caller_name;
			directory[i].multilock = 1;
		}
	}
	last_next_free_dir_index = next_free_dir_index;

	qsort((void *) &sorti[1], next_free_dir_index - 1, sizeof(sorti[0]), &sortcmp);

}


int
sortcmp(const void *ip, const void *jp)
{
	int		si0, si1, k;

	si0 = *(short *)ip;
	si1 = *(short *)jp;

	k = directory[si0].entry_type - directory[si1].entry_type;
	if (k)
		return (k);


	k = directory[si0].multilock - directory[si1].multilock;
	if (k)
		return (k);

	k = strcmp(directory[si0].lock_name, directory[si1].lock_name);
	if (k)
		return (k);

	k = strncmp(directory[si0].caller_name, directory[si1].caller_name,
			min(directory[si0].caller_name_len,
				directory[si1].caller_name_len));
	if (k)
		return (k);

	k = directory[si0].caller_ra - directory[si1].caller_ra;
	return(k);

}


/*
 * sum up report data over all cpu's or a sequential subset
 * called either with all cpu's or just one cpu specified
 */
void
sum_data (int start_cpu, int end_cpu)
{
	int	i, j, k, m, cpu, start;
	for (i = 0; i < next_free_dir_index; i++) {
		total_counts[i].cum_hold_ticks = 0;
		total_counts[i].cum_wait_ticks = 0;
		total_counts[i].max_hold_ticks = 0;
		total_counts[i].max_wait_ticks = 0;
		total_counts[i].cum_wait_ww_ticks = 0;
		total_counts[i].max_wait_ww_ticks = 0;
		for (j = 0; j < LSTAT_ACT_MAX_VALUES; j++)
			total_counts[i].count[j] = 0;
		for (cpu = start_cpu; cpu <= end_cpu; cpu++) {
			total_counts[i].cum_hold_ticks += kernel_counts[cpu][i].cum_hold_ticks - 
				(topt ? 0 : prev_counts[cpu][i].cum_hold_ticks);
			total_counts[i].cum_wait_ticks += kernel_counts[cpu][i].cum_wait_ticks - 
				(topt ? 0 : prev_counts[cpu][i].cum_wait_ticks);
			total_counts[i].cum_wait_ww_ticks += kernel_counts[cpu][i].cum_wait_ww_ticks -
				(topt ? 0 : prev_counts[cpu][i].cum_wait_ww_ticks);
			if (total_counts[i].max_hold_ticks < kernel_counts[cpu][i].max_hold_ticks)
			    total_counts[i].max_hold_ticks = kernel_counts[cpu][i].max_hold_ticks;
			if (total_counts[i].max_wait_ticks < kernel_counts[cpu][i].max_wait_ticks)
			    total_counts[i].max_wait_ticks = kernel_counts[cpu][i].max_wait_ticks;
			if (total_counts[i].max_wait_ww_ticks < kernel_counts[cpu][i].max_wait_ww_ticks)
			    total_counts[i].max_wait_ww_ticks = kernel_counts[cpu][i].max_wait_ww_ticks;
			for (j = 0; j < LSTAT_ACT_MAX_VALUES; j++)
				total_counts[i].count[j] += kernel_counts[cpu][i].count[j] -  
						(topt ? 0 : prev_counts[cpu][i].count[j]);
		}
	}

	for (i = 1; i < next_free_read_lock_index; i++) {
		int tmp, kernel_incr, prev_incr;
		long long kernel_cum_hold_ticks, prev_cum_hold_ticks;
		long long tmp_kernel_cum_hold_ticks, tmp_prev_cum_hold_ticks;
		long long kernel_busy_ticks, prev_busy_ticks;
		int kernel_busy_periods, prev_busy_periods;
		bzero(&read_lock_counts[i],sizeof(lstat_read_lock_counts_t));
		kernel_cum_hold_ticks = 0;
		prev_cum_hold_ticks   = 0;
		kernel_busy_ticks     = 0;
		prev_busy_ticks       = 0;
		kernel_busy_periods   = 0;
		prev_busy_periods     = 0;
		read_lock_counts[i].max_readers = 0;
		read_lock_counts[i].max_busy    = 0;
		read_lock_increment[i] = 0;
		prev_read_lock_increment[i] = 0;
		for(cpu = start_cpu; cpu <= end_cpu; cpu++) {
			tmp_kernel_cum_hold_ticks = kernel_read_lock_counts[cpu][i].cum_hold_ticks;
			#ifdef DEBUGBUG
		    if (tmp_kernel_cum_hold_ticks < 0) 
				printf("Bad kernel cum_hold_ticks=%Ld (%LX) for cpu=%d index=%d\n",
				     tmp_kernel_cum_hold_ticks,  tmp_kernel_cum_hold_ticks, cpu, i);
			#endif
			/* 
			 * if the current lock is still held by one or more readers when data collected,
			 * the cum_hold_ticks will be negative
			 * update stats as if all of these callers released the lock at time ending_cycles64
			 */
			tmp = 0;
			while ((tmp_kernel_cum_hold_ticks<0) && (tmp<ABSURD_MAX_COUNT)) {
				tmp_kernel_cum_hold_ticks += ending_cycles64;
				tmp++;
			}
			read_lock_increment[i] += tmp;
#ifdef DEBUGBUG
				if (tmp>0) printf("For cpu %d entry %d incremented kernel count=%d\n",cpu,i,tmp);
#endif
			kernel_cum_hold_ticks   += tmp_kernel_cum_hold_ticks;
			kernel_busy_ticks       += kernel_read_lock_counts[cpu][i].busy_ticks;
			kernel_busy_periods     += kernel_read_lock_counts[cpu][i].busy_periods;
			if (!topt) {
				tmp_kernel_cum_hold_ticks = prev_read_lock_counts[cpu][i].cum_hold_ticks;
				if ( tmp_kernel_cum_hold_ticks < 0) 
					printf("Bad prev cum_hold_ticks=%Ld (%LX) for cpu=%d index=%d\n",
						 tmp_kernel_cum_hold_ticks, tmp_kernel_cum_hold_ticks, cpu, i);
				tmp = 0;
				while ((tmp_kernel_cum_hold_ticks<0) && (tmp<ABSURD_MAX_COUNT)) {
					/* in all the cases where this matters, the starting_cycles64 */
					/* is the ending time value for the previous cycle, so in     */
					/* spite of the fact that this looks idiotic, I think it is   */
					/* corect. .................................................. */
					tmp_kernel_cum_hold_ticks += started_cycles64;
					tmp++;
				}
			    prev_read_lock_increment[i] += tmp;
#ifdef DEBUGBUG
					if (tmp>0) printf("For cpu %d entry %d incremented prev count=%d\n",cpu,i,tmp);
#endif
				prev_cum_hold_ticks += tmp_kernel_cum_hold_ticks;
				prev_busy_ticks     += prev_read_lock_counts[cpu][i].busy_ticks;
				prev_busy_periods   += prev_read_lock_counts[cpu][i].busy_periods;
			}
			read_lock_counts[i].read_lock_count +=
				kernel_read_lock_counts[cpu][i].read_lock_count -
					(topt ? 0 : prev_read_lock_counts[cpu][i].read_lock_count);
			if (kernel_read_lock_counts[cpu][i].max_readers > read_lock_counts[i].max_readers)
				read_lock_counts[i].max_readers = kernel_read_lock_counts[cpu][i].max_readers;
			if (kernel_read_lock_counts[cpu][i].max_busy > read_lock_counts[i].max_busy)
				read_lock_counts[i].max_busy = kernel_read_lock_counts[cpu][i].max_busy;
		}

		/* if we reached the ABSURD_MAX_COUNT the data is probably NFG */
		/* mark the data as "out of synch" */
		if ((read_lock_increment[i] >= ABSURD_MAX_COUNT) || 
	    	(prev_read_lock_increment[i] >= ABSURD_MAX_COUNT)) read_lock_increment[i] = -1;

		read_lock_counts[i].cum_hold_ticks = kernel_cum_hold_ticks - prev_cum_hold_ticks; 
		read_lock_counts[i].busy_ticks = kernel_busy_ticks - prev_busy_ticks; 
		read_lock_counts[i].busy_periods = kernel_busy_periods - prev_busy_periods;
		read_lock_counts[i].lock_ptr = kernel_read_lock_counts[start_cpu][i].lock_ptr;
	}
}

void
verify_diff_file (void)
{
	int		i;

	for (i = 0; i < next_free_dir_index; i++) {
		if (kernel_directory[i].caller_ra != prev_kernel_directory[i].caller_ra &&  prev_kernel_directory[i].caller_ra != 0) {
			fprintf(stderr, "%s: caller address mismatch: index:%d, old:%llx, new:%llx", 
					progname, i, prev_kernel_directory[i].caller_ra, kernel_directory[i].caller_ra);
			perrorx("caller address mismatch");
		}
	}
}

#define WRITE_STR(s)	if (write(fd, (char *)&s, sizeof(s)) != sizeof(s))	\
				perrorx("write diff stats");
#define READ_STR(s)	if (read(fd, (char *)&s, sizeof(s)) != sizeof(s))	\
				perrorx("read diff stats s ");
#define WRITEN(s,n)	if (write(fd, (char *)&s, (n)) != (n))			\
				perrorx("write diff stats");
#define READN(s,n)	if (read(fd, (char *)&s, (n)) != (n))			\
				perrorx("read diff stats");
int
read_diff_file (void)
{
	int		fd;

	if ((fd = open(debugname, O_RDONLY, 0)) < 0)
		return (0);

	READ_STR(numcpus);
	prev_counts = (lstat_cpu_counts_t *)
			malloc(numcpus*sizeof(lstat_cpu_counts_t));
	kernel_counts = (lstat_cpu_counts_t *)
			malloc(numcpus*sizeof(lstat_cpu_counts_t));
	READ_STR(start_time);
	READ_STR(end_time);
	READ_STR(deltatime);
	READ_STR(next_free_dir_index);
	READN(kernel_directory[0], next_free_dir_index*sizeof(lstat_directory_entry_t));
	READN(kernel_counts[0], numcpus*sizeof(lstat_cpu_counts_t));
	READN(prev_counts[0], numcpus*sizeof(lstat_cpu_counts_t));
#ifdef notyet
	READ_STR(kernel_magic_addr);
	READ_STR(kernel_end_addr);
#endif

	close(fd);
	verify_diff_file ();
	valid = LSTAT_ON;
	return(1);
}


void
write_diff_file (void)
{
	int	fd;

	if ((fd = open(debugname, O_WRONLY | O_CREAT, 0666)) < 0)
		perrorx("cant create diff file");

	WRITE_STR(numcpus);
	WRITE_STR(start_time);
	WRITE_STR(end_time);
	WRITE_STR(deltatime);
	WRITE_STR(next_free_dir_index);
	WRITEN(kernel_directory[0], next_free_dir_index*sizeof(lstat_directory_entry_t));
	WRITEN(kernel_counts[0], numcpus*sizeof(lstat_cpu_counts_t));
	WRITEN(prev_counts[0], numcpus*sizeof(lstat_cpu_counts_t));
#ifdef notyet
	WRITE_STR(kernel_magic_addr);
	WRITE_STR(kernel_end_addr);
#endif

	close(fd);
	exit(0);
}
