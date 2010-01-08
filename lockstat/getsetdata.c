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

#include "lockstat.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <linux/utsname.h>
#include <linux/lockmeter.h>

static int dataFile = 0;
static int isProcfileFlag = 0;
static char *dataFilename;

static int mirrorOutFile = 0;
static char *mirrorOutFilename = NULL;

static char *generalReadBuf = NULL;
static int  generalReadBufLen = 0;

static int currentState = LSTAT_OFF;
static int maxCpusCount = 0;

/* must match what is in include/linux/lockmeter.h */
char *command_name[] = {"OFF","ON","RESET","RELEASED",""};

void closeFiles(void);
void getKernelData(lstat_user_request_t **, lstat_cpu_counts_t **,
		   lstat_directory_entry_t **, lstat_read_lock_cpu_counts_t **,
		   int);
int  isProcFile(void);
int  openKernelData(char *);
void setCollectionState(int);
void setMirrorOutFilename(char *);


void
closeFiles()
{
    int result;

    /*
     *  The only file that needs to be closed is the mirrored output file,
     *  if it exists.
     */
    if (mirrorOutFile) {
	close(mirrorOutFile);

	/* fix up file permissions */
	result = chmod(mirrorOutFilename,S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
	if (result) {
	    fprintf(stderr,"closeFiles: cannot chmod '%s'\n",mirrorOutFilename);
	}
    }
}

/*
 *  getKernelData
 *
 *	This is the general interface to read lockmetering data from a
 *	lockmeter-capable kernel.
 *	The args are pointers to pointers of buffers.  If the caller
 *	supplies a non-null pointer to a buffer, then the appropriate data
 *	is passed back to the caller; if the caller supplies no buffers,
 *	then getKernelData() malloc's buffers and returns those back to
 *	the user.  It is the caller's responsibility to reuse these buffers
 *	on a subsequent call, or to free() the buffers.
 *	If the caller specifies readHeaderOnly=1, then only the initial
 *	lstat_user_request_t information is returned; otherwise, all four
 *	subparts of the data is returned.
 */
void
getKernelData(
	lstat_user_request_t **		request_ptr,
	lstat_cpu_counts_t **		cpu_counts_ptr,
	lstat_directory_entry_t **	directory_ptr,
	lstat_read_lock_cpu_counts_t **	read_lock_counts_ptr,
	int				readHeaderOnly)
{
    int	read_count;
    int buf_size;
    void *memcpySrcAddr;
    int  memcpyLen;

    if (!dataFile) {
	fprintf(stderr,"getKernelData: data file is not open!\n");
	exit(1);
    }

    if (readHeaderOnly) {
	buf_size = sizeof(lstat_user_request_t);  /* easier to read */
	if (*request_ptr == NULL) {
	    *request_ptr = (lstat_user_request_t *)malloc(buf_size);
	    if (*request_ptr == NULL) {
		fprintf(stderr,"getKernelData: cannot malloc %d bytes for"
			" lstat_user_request_t buffer", buf_size);
		exit(1);
	    }
	}
	read_count = read(dataFile, *request_ptr, buf_size);
	if (read_count != buf_size) {
	    fprintf(stderr,"getKernelData: "
		"read of %d bytes of lstat_user_request_t from file '%s' "
		"gets %d bytes\n", buf_size, dataFilename, read_count);
	    exit(1);
	}

	/*
	 *  If reading from something other than the /proc file,
	 *  then rewind to undo the previous read()
	 */
	if (!isProcfileFlag) {
	    read_count = (int) lseek(dataFile, -buf_size, SEEK_CUR);
	    if (read_count < 0) {
		fprintf(stderr, "getKernelData: lseek failure\n");
		exit(1);
	    }
	}

	/* If we don't have a big general read buffer yet, alloc it now */
	if (!generalReadBuf) {
	    generalReadBufLen =
		(sizeof(lstat_user_request_t))
	      + ((*request_ptr)->maxcpus * sizeof(lstat_cpu_counts_t))
	      + (LSTAT_MAX_STAT_INDEX * sizeof(lstat_directory_entry_t))
	      + ((*request_ptr)->maxcpus
				      * sizeof(lstat_read_lock_cpu_counts_t));

	    generalReadBuf = (char *)malloc(generalReadBufLen);
	    if (!generalReadBuf) {
		fprintf(stderr,"getKernelData:"
		    "Unable to malloc %d bytes for generalReadBuf\n",
		    generalReadBufLen);
		exit(1);
	    }
	}

	currentState = (*request_ptr)->state;

	if (maxCpusCount == 0)
	    maxCpusCount = (*request_ptr)->maxcpus;
	else if (maxCpusCount != (*request_ptr)->maxcpus) {
	    fprintf(stderr,"getCollectionState: "
		"supposedly constant 'maxcpus' has changed from %d to %d!\n",
		maxCpusCount, (*request_ptr)->maxcpus);
	    exit(1);
	}

	return;	/* done! */

    } 

    if (!generalReadBuf) {
	fprintf(stderr,"getKernelData: first use must be readHeaderOnly\n");
	exit(1);
    }

    /* Read the full concatenated data from the kernel */
    read_count = read(dataFile, generalReadBuf, generalReadBufLen);
    if (read_count != generalReadBufLen) {
	fprintf(stderr,"getKernelData: "
		"read requested %d bytes, received %d\n",
		generalReadBufLen, read_count);
	exit(1);
    }

    /* Mirror the input data, if requested */
    if (mirrorOutFilename) {
	int write_count;
	if (!mirrorOutFile) {
	    mirrorOutFile = open(mirrorOutFilename, O_RDWR|O_CREAT);
	    if (mirrorOutFile < 0) {
		fprintf(stderr,"getKernelData: "
			"cannot open mirror output file '%s', errno=%d\n",
			mirrorOutFilename, errno);
		exit(1);
	    }
	}
	write_count = write(mirrorOutFile, generalReadBuf, generalReadBufLen);
	if (write_count != read_count) {
	    fprintf(stderr,"getKernelData: attempted to write %d bytes "
		    "to mirror file, only %d bytes succeeded\n",
		    generalReadBufLen, write_count);
	    exit(1);
	}
    }

    /*
     *  Separate the data into the various discrete chunks.
     *  Malloc buffers as necessary.
     */

    memcpySrcAddr = generalReadBuf;
    memcpyLen = sizeof(lstat_user_request_t);
    if (*request_ptr == NULL) {
	*request_ptr = (lstat_user_request_t *)malloc(memcpyLen);
	if (*request_ptr == NULL) {
	    fprintf(stderr,"getKernelData: cannot malloc %d bytes for"
		    " lstat_user_request_t buffer", memcpyLen);
		exit(1);
	    }
	}
    memcpy((void *)*request_ptr, memcpySrcAddr, memcpyLen);
    memcpySrcAddr += memcpyLen;		/* bump for next memcpy */

    memcpyLen = maxCpusCount * sizeof(lstat_cpu_counts_t);
    if (*cpu_counts_ptr == NULL) {
	*cpu_counts_ptr = (lstat_cpu_counts_t *)malloc(memcpyLen);
	if (*cpu_counts_ptr == NULL) {
	    fprintf(stderr,"getKernelData: cannot malloc %d bytes for"
		    " lstat_cpu_counts_t buffer", memcpyLen);
		exit(1);
	    }
	}
    memcpy((void *)*cpu_counts_ptr, memcpySrcAddr, memcpyLen);
    memcpySrcAddr += memcpyLen;

    memcpyLen = LSTAT_MAX_STAT_INDEX * sizeof(lstat_directory_entry_t);
    if (*directory_ptr == NULL) {
	*directory_ptr = (lstat_directory_entry_t *)malloc(memcpyLen);
	if (*directory_ptr == NULL) {
	    fprintf(stderr,"getKernelData: cannot malloc %d bytes for"
		    " lstat_directory_entry_t buffer", memcpyLen);
		exit(1);
	    }
	}
    memcpy((void *)*directory_ptr, memcpySrcAddr, memcpyLen);
    memcpySrcAddr += memcpyLen;

    memcpyLen = maxCpusCount * sizeof(lstat_read_lock_cpu_counts_t);
    if (*read_lock_counts_ptr == NULL) {
	*read_lock_counts_ptr = (lstat_read_lock_cpu_counts_t *)malloc(memcpyLen);
	if (*read_lock_counts_ptr == NULL) {
	    fprintf(stderr,"getKernelData: cannot malloc %d bytes for"
		    " lstat_read_lock_cpu_counts_t buffer", memcpyLen);
		exit(1);
	    }
	}
    memcpy((void *)*read_lock_counts_ptr, memcpySrcAddr, memcpyLen);
}

int
isProcFile()
{
    return isProcfileFlag;
}

/*
 * openKernelData
 *	Return 1 if kernel data file can be opened, 0 if it cannot.
 *	Open read-write if /proc/lockmeter node, readonly otherwise.
 */
int
openKernelData(char *filename)
{
    dataFilename = (filename) ? filename : defaultDataFilename;
    isProcfileFlag = (strcmp(dataFilename, defaultDataFilename) == 0);

    /* need to be root if reading /proc/lockmeter */
    if (isProcfileFlag)
	setuid(0);

    dataFile = open(dataFilename, (isProcfileFlag) ? O_RDWR : O_RDONLY);
    return dataFile;
}

void
setCollectionState(int newState)
{
    char writeBuf[1];

    switch(newState) {
	case LSTAT_ON:
		if (currentState == LSTAT_ON) {
			fprintf(stderr,"Lockmeter statistics already ON.\n");
			exit(1);
		}
		break;
	case LSTAT_OFF:
		if (currentState == LSTAT_OFF) {
			fprintf(stderr,"Lockmeter statistics already OFF.\n");
			exit(1);
		}
		break;
	case LSTAT_RESET:
		if (currentState == LSTAT_ON) {
			fprintf(stderr,"Lockmeter statistics are ON;"
				" turn them off before reset.\n");
			exit(1);
		}
		break;
	case LSTAT_RELEASE:
		if (currentState == LSTAT_ON) {
			fprintf(stderr,"Lockmeter statistics ON;"
				" turn them off before release.\n");
			exit(1);
		}
		break;
	default:
		fprintf(stderr,"setCollectionState: unknown newState=%d\n",
			newState);
		exit(1);
    }

    writeBuf[0] = newState;
    if (write(dataFile, writeBuf, 1) != 1) {
	fprintf(stderr,"setCollectionState:"
		" error %d setting lockmeter statistics to:%d\n",
		errno,newState);
	exit(1);
    } else {
	printf("Lockmeter statistics are now %s\n", command_name[newState]);

	if (newState == LSTAT_ON)
		currentState = LSTAT_ON;
	else if (newState == LSTAT_OFF)
		currentState = LSTAT_OFF;
    }
}

void
setMirrorOutFilename(char *filename)
{
    mirrorOutFilename = (char *)malloc(strlen(filename) + 1);
    if (!mirrorOutFilename) {
	fprintf(stderr,"setMirrorOutFilename: cannot malloc %d bytes\n",
		strlen(filename)+1);
	exit(1);
    }
    strcpy(mirrorOutFilename, filename);
}
