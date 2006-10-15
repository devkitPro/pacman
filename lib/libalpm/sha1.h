/* Declarations of functions and data types used for SHA1 sum
   library functions.
   Copyright (C) 2000, 2001, 2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#include <limits.h>

#define rol(x,n) ( ((x) << (n)) | ((x) >> (32 -(n))) )
/* The code below is from md5.h (from coreutils), little modifications */
#define UINT_MAX_32_BITS 4294967295U

#if UINT_MAX == UINT_MAX_32_BITS
    typedef unsigned int sha_uint32;
#else
#if USHRT_MAX == UINT_MAX_32_BITS
    typedef unsigned short sha_uint32;
#else
#if ULONG_MAX == UINT_MAX_32_BITS
    typedef unsigned long sha_uint32;
#else
    /* The following line is intended to evoke an error. Using #error is not portable enough.  */
#error "Cannot determine unsigned 32-bit data type"
#endif
#endif
#endif
/* We have to make a guess about the integer type equivalent in size
   to pointers which should always be correct.  */
typedef unsigned long int sha_uintptr;

/* Structure to save state of computation between the single steps.  */
struct sha_ctx
{
  sha_uint32 A;
  sha_uint32 B;
  sha_uint32 C;
  sha_uint32 D;
  sha_uint32 E;

  sha_uint32 total[2];
  sha_uint32 buflen;
  char buffer[128];
};


/* Needed for pacman */
char *_alpm_SHAFile (char *);
