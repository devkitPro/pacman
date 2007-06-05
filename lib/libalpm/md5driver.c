/* MD5DRIVER.C - taken and modified from MDDRIVER.C (license below)  */
/*               for use in pacman.                                  */
/*********************************************************************/ 

/* Copyright (C) 1990-2, RSA Data Security, Inc. Created 1990. All
rights reserved.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
 */

/* The following makes MD default to MD5 if it has not already been
  defined with C compiler flags.
 */
#define MD MD5

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* libalpm */
#include "alpm.h"
#include "log.h"
#include "util.h"
#include "md5.h"

/* Length of test block, number of test blocks.
 */
#define TEST_BLOCK_LEN 1000
#define TEST_BLOCK_COUNT 1000

#define MD_CTX MD5_CTX
#define MDInit _alpm_MD5Init
#define MDUpdate _alpm_MD5Update
#define MDFinal _alpm_MD5Final

/** Get the md5 sum of file.
 * @param name name of the file
 * @return the checksum on success, NULL on error
 * @addtogroup alpm_misc
 */
char SYMEXPORT *alpm_get_md5sum(char *name)
{
	ALPM_LOG_FUNC;

	ASSERT(name != NULL, return(NULL));

	return(_alpm_MDFile(name));
}

char* _alpm_MDFile(char *filename)
{
	FILE *file;
	MD_CTX context;
	int len;
	char hex[3];
	unsigned char buffer[1024], digest[16];

	ALPM_LOG_FUNC;

	if((file = fopen(filename, "rb")) == NULL) {
		_alpm_log(PM_LOG_ERROR, _("md5: %s can't be opened\n"), filename);
	} else {
		char *ret;
		int i;

		MDInit(&context);
		while((len = fread(buffer, 1, 1024, file))) {
			MDUpdate(&context, buffer, len);
		}
		MDFinal(digest, &context);
		fclose(file);

		ret = calloc(33, sizeof(char));
		for(i = 0; i < 16; i++) {
			snprintf(hex, 3, "%02x", digest[i]);
			strncat(ret, hex, 2);
		}

		_alpm_log(PM_LOG_DEBUG, _("md5(%s) = %s"), filename, ret);
		return(ret);
	}
	return(NULL);
}

/* vim: set ts=2 sw=2 noet: */
