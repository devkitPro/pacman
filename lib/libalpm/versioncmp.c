/*
 *  versioncmp.c
 *
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005, 2006 by Miklos Vajna <vmiklos@frugalware.org>
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
/* pacman */
#include "versioncmp.h"

#ifndef HAVE_STRVERSCMP
/* GNU's strverscmp() function, taken from glibc 2.3.2 sources
 */

/* Compare strings while treating digits characters numerically.
   Copyright (C) 1997, 2002 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Jean-François Bignolles <bignolle@ecoledoc.ibp.fr>, 1997.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

/* states: S_N: normal, S_I: comparing integral part, S_F: comparing
           fractionnal parts, S_Z: idem but with leading Zeroes only */
#define  S_N    0x0
#define  S_I    0x4
#define  S_F    0x8
#define  S_Z    0xC

/* result_type: CMP: return diff; LEN: compare using len_diff/diff */
#define  CMP    2
#define  LEN    3

/* Compare S1 and S2 as strings holding indices/version numbers,
   returning less than, equal to or greater than zero if S1 is less than,
   equal to or greater than S2 (for more info, see the texinfo doc).
*/

static int strverscmp (s1, s2)
     const char *s1;
     const char *s2;
{
  const unsigned char *p1 = (const unsigned char *) s1;
  const unsigned char *p2 = (const unsigned char *) s2;
  unsigned char c1, c2;
  int state;
  int diff;

  /* Symbol(s)    0       [1-9]   others  (padding)
     Transition   (10) 0  (01) d  (00) x  (11) -   */
  static const unsigned int next_state[] =
  {
      /* state    x    d    0    - */
      /* S_N */  S_N, S_I, S_Z, S_N,
      /* S_I */  S_N, S_I, S_I, S_I,
      /* S_F */  S_N, S_F, S_F, S_F,
      /* S_Z */  S_N, S_F, S_Z, S_Z
  };

  static const int result_type[] =
  {
      /* state   x/x  x/d  x/0  x/-  d/x  d/d  d/0  d/-
                 0/x  0/d  0/0  0/-  -/x  -/d  -/0  -/- */

      /* S_N */  CMP, CMP, CMP, CMP, CMP, LEN, CMP, CMP,
                 CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP,
      /* S_I */  CMP, -1,  -1,  CMP, +1,  LEN, LEN, CMP,
                 +1,  LEN, LEN, CMP, CMP, CMP, CMP, CMP,
      /* S_F */  CMP, CMP, CMP, CMP, CMP, LEN, CMP, CMP,
                 CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP,
      /* S_Z */  CMP, +1,  +1,  CMP, -1,  CMP, CMP, CMP,
                 -1,  CMP, CMP, CMP
  };

  if (p1 == p2)
    return 0;

  c1 = *p1++;
  c2 = *p2++;
  /* Hint: '0' is a digit too.  */
  state = S_N | ((c1 == '0') + (isdigit (c1) != 0));

  while ((diff = c1 - c2) == 0 && c1 != '\0')
    {
      state = next_state[state];
      c1 = *p1++;
      c2 = *p2++;
      state |= (c1 == '0') + (isdigit (c1) != 0);
    }

  state = result_type[state << 2 | (((c2 == '0') + (isdigit (c2) != 0)))];

  switch (state)
  {
    case CMP:
      return diff;

    case LEN:
      while (isdigit (*p1++))
	if (!isdigit (*p2++))
	  return 1;

      return isdigit (*p2) ? -1 : diff;

    default:
      return state;
  }
}

#endif

/* this function was taken from rpm 4.0.4 and rewritten */
int _alpm_versioncmp(const char *a, const char *b)
{
	char str1[64], str2[64];
	char *ptr1, *ptr2;
	char *one, *two;
	char *rel1 = NULL, *rel2 = NULL;
	char oldch1, oldch2;
	int is1num, is2num;
	int rc;

	if(!strcmp(a,b)) {
		return(0);
	}

	strncpy(str1, a, 64);
	str1[63] = 0;
	strncpy(str2, b, 64);
	str2[63] = 0;

	/* lose the release number */
	for(one = str1; *one && *one != '-'; one++);
	if(one) {
		*one = '\0';
		rel1 = ++one;
	}
	for(two = str2; *two && *two != '-'; two++);
	if(two) {
		*two = '\0';
		rel2 = ++two;
	}

	one = str1;
	two = str2;

	while(*one || *two) {
		while(*one && !isalnum((int)*one)) one++;
		while(*two && !isalnum((int)*two)) two++;

		ptr1 = one;
		ptr2 = two;

		/* find the next segment for each string */
		if(isdigit((int)*ptr1)) {
			is1num = 1;
			while(*ptr1 && isdigit((int)*ptr1)) ptr1++;
		} else {
			is1num = 0;
			while(*ptr1 && isalpha((int)*ptr1)) ptr1++;
		}
		if(isdigit((int)*ptr2)) {
			is2num = 1;
			while(*ptr2 && isdigit((int)*ptr2)) ptr2++;
		} else {
			is2num = 0;
			while(*ptr2 && isalpha((int)*ptr2)) ptr2++;
		}

		oldch1 = *ptr1;
		*ptr1 = '\0';
		oldch2 = *ptr2;
		*ptr2 = '\0';

		/* see if we ran out of segments on one string */
		if(one == ptr1 && two != ptr2) {
			return(is2num ? -1 : 1);
		}
		if(one != ptr1 && two == ptr2) {
			return(is1num ? 1 : -1);
		}

		/* see if we have a type mismatch (ie, one is alpha and one is digits) */
		if(is1num && !is2num) return(1);
		if(!is1num && is2num) return(-1);

		if(is1num) while(*one == '0') one++;
		if(is2num) while(*two == '0') two++;

		rc = strverscmp(one, two);
		if(rc) return(rc);

		*ptr1 = oldch1;
		*ptr2 = oldch2;
		one = ptr1;
		two = ptr2;
	}

	if((!*one) && (!*two)) {
		/* compare release numbers */
		if(rel1 && rel2) return(_alpm_versioncmp(rel1, rel2));
		return(0);
	}

	return(*one ? 1 : -1);
}

/* vim: set ts=2 sw=2 noet: */
