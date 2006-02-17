/*
 *  vercmp.c
 * 
 *  Copyright (c) 2002-2005 by Judd Vinet <jvinet@zeroflux.org>
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
 */

#include <stdio.h>
#include <string.h>
#include "versioncmp.h"

int main(int argc, char *argv[])
{
	char s1[255] = "";
	char s2[255] = "";
	int ret;

	if(argc > 1) {
		strncpy(s1, argv[1], 255);
	}
	if(argc > 2) {
		strncpy(s2, argv[2], 255);
	} else {
		printf("0\n");
		return(0);
	}
	
	ret = _alpm_versioncmp(s1, s2);
	printf("%d\n", ret);
	return(ret);
}
