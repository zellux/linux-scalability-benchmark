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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STRMAX		256
#define SPACEINC	16384*4

extern int  opt_debug;
char *strSpace(void);	/* allocate an efficient chunk for a string */

typedef struct {
	unsigned long	addr;
	char		*name;
} namespace_entry_t;
#define NAMESPACE_TABLE_CHUNK 2048
static int maxNamespaceEntries = 0;
static int numNamespaceEntries = 0;
static namespace_entry_t *namespace = NULL;

/*
 * Open and read the entire map file once and build an in-memory table of the
 * address-name equivalences for quick reference.  If this is not the first
 * map file we've seen, then merge this file's address/symbol pairs with the
 * previous pairs.
 */
int
openMapFile (char *mapFilename)
{
#define MAX_MAPFILE_REC_LEN 128
	FILE *mapFile;
	char buffer[MAX_MAPFILE_REC_LEN];
	char func_name[MAX_MAPFILE_REC_LEN];
	char mode[MAX_MAPFILE_REC_LEN];
	unsigned long func_addr = 0;
	int i;

	mapFile = fopen(mapFilename,"r");
	if (mapFile == NULL)
		return 0;	/* cannot open map file! */

	rewind(mapFile);
	while (fgets(buffer, MAX_MAPFILE_REC_LEN, mapFile)) {
		/* Module map file begins with "Address" -- discard */
		if (strncmp(buffer, "Address", strlen("Address")) == 0)
			continue;

		if (sscanf(buffer, "%lx %s %s",
			   &func_addr, mode, func_name) != 3) {
		    fprintf(stderr,"Corrupted mapfile");
		    exit(1);
		}
		if (strlen(mode) > 1) {
			/* Module map file has symbolic name as field #2 */
			strcpy(func_name, mode);
		}

		/* Expand the in-memory namespace table, if necessary */
		if (numNamespaceEntries == maxNamespaceEntries) {
		    maxNamespaceEntries += NAMESPACE_TABLE_CHUNK;
		    namespace = (namespace_entry_t *)realloc(
			(void *)namespace,
			(maxNamespaceEntries * sizeof(namespace_entry_t)));
		    if (!namespace) {
			fprintf(stderr,"Cannot malloc in-memory namespace table");	
			exit(1);
		    }
		}

		/*
		 *  Ensure monotomically increasing, non-duplicate addresses.
		 */
		if (numNamespaceEntries == 0) {
			/* the 1st entry */
			namespace[numNamespaceEntries].addr = func_addr;
			namespace[numNamespaceEntries].name = strSpace();
			strcpy(namespace[numNamespaceEntries].name, func_name);
			numNamespaceEntries++;

		} else if (func_addr > namespace[numNamespaceEntries-1].addr) {
			/* this address is beyond the previous last address, so append */
			namespace[numNamespaceEntries].addr = func_addr;
			namespace[numNamespaceEntries].name = strSpace();
			strcpy(namespace[numNamespaceEntries].name, func_name);
			numNamespaceEntries++;
			if (strcmp(func_name, "_end") == 0)
				break;

		} else if (func_addr == namespace[numNamespaceEntries-1].addr) {
			/* duplicates the last entry; use this latter sym name */
			strcpy(namespace[numNamespaceEntries-1].name, func_name);
		} else {
			/*
			 * This address preceeds the previous last address, so insert.
			 * Scan backwards from the end of the list to find the
			 * insertion point, moving entries forward one index position.
			 */
			numNamespaceEntries++;
			for (i = numNamespaceEntries-2; i >= -1; i--) {
				if (i < 0  ||   func_addr > namespace[i].addr) {
					/* found the insertion point */
					namespace[i+1].addr = func_addr;		
					namespace[i+1].name = strSpace();
					strcpy(namespace[i+1].name, func_name);
					break;
				} else {
					namespace[i+1].addr = namespace[i].addr;
					namespace[i+1].name = namespace[i].name;
				}
			}
		}
	    }

	return 1;
}

/*
 * strSpace
 *
 * A crude allocation manager for string space. 
 * Returns pointer to where a string of length 0 .. STRMAX can be placed.
 * On next call, a free space pointer is updated to point to 
 * the location just beyond the last string allocated. 
 *	NOTE: "just beyond" could be 1 byte, but we increment to the
 *		next 8-byte boundary for cleaner copies and compares.
 *	NOTE: does not support expanding a string once allocated &
 *		another call to strspace is made.
 */
char*
strSpace(void)
{
	static char	*space=NULL, *endspace=NULL;
	int		incr;

	if (space) {
		/*
		 * The last caller might not have used this space yet, so
		 * assume the worst case: the max string.
		 */
		if (space[0] == 0) {
			incr = STRMAX;
		} else {
			incr = 9 + strlen(space);
			incr &= ~7;
		}
		space += incr;
	}

	if (space >= endspace) {
		space = (char *)malloc(SPACEINC);
		endspace = space + SPACEINC - STRMAX;
	}
	space[0] = '\0';
	return(space);
}

/*
 * translate an address (adr) into a name (if it matches) or to a name + offset
 * if it doesn't.  Return null if the name is below the lowest address in the
 * map file
 */
void
XlateAddressToSymbol(void *adr, char *namep)
{
	int i;

	if (opt_debug)
		namep += sprintf(namep, "[0x%lx] ", (unsigned long)adr);
	else
		namep[0] = '\0';/* ensure that subsequent strcat() will work */

	for (i = 0; i < numNamespaceEntries; i++) {
	    if ((unsigned long)adr == namespace[i].addr) {
	       strcat(namep, namespace[i].name);
	       return;
	    } else if ((unsigned long)adr < namespace[i].addr) {
		if (i == 0)	/* below the first address in the table */
		    break;
		if (strcmp(namespace[i-1].name, "_end") == 0)
		    break;	/* between "_end" and the modules -- just use numeric */

		sprintf(namep, "%s+0x%x", namespace[i-1].name,
			(unsigned long)adr - namespace[i-1].addr);
		return;
	    }
	}

	/* can't find it */
	if (!opt_debug)
		sprintf(namep, "[0x%lx]", (unsigned long)adr);
}
