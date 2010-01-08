/*
 *
 *  Copyright (C) 1999,2000 Silicon Graphics, Inc.
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

#ifndef _LOCKSTAT_H_
#define _LOCKSTAT_H_

#ifdef mips
#define CONFIG_MIPS32_COMPAT 1
#endif

#include <stdint.h>

static char defaultDataFilename[] = "/proc/lockmeter";

#endif /* _LOCKSTAT_H_ */
