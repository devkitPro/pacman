/*
 *  signing.c
 *
 *  Copyright (c) 2008-2011 Pacman Development Team <pacman-dev@archlinux.org>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h> /* setlocale() */
#include <gpgme.h>

/* libalpm */
#include "signing.h"
#include "package.h"
#include "util.h"
#include "log.h"
#include "alpm.h"

#define CHECK_ERR(void) do { \
		if(err != GPG_ERR_NO_ERROR) { goto error; } \
	} while(0)

static int gpgme_init(void)
{
	static int init = 0;
	const char *version;
	gpgme_error_t err;
	gpgme_engine_info_t enginfo;

	ALPM_LOG_FUNC;

	if(init) {
		/* we already successfully initialized the library */
		return 0;
	}

	if(!alpm_option_get_signaturedir()) {
		RET_ERR(PM_ERR_SIG_MISSINGDIR, 1);
	}

	/* calling gpgme_check_version() returns the current version and runs
	 * some internal library setup code */
	version = gpgme_check_version(NULL);
	_alpm_log(PM_LOG_DEBUG, "GPGME version: %s\n", version);
	gpgme_set_locale(NULL, LC_CTYPE, setlocale(LC_CTYPE, NULL));
#ifdef LC_MESSAGES
	gpgme_set_locale(NULL, LC_MESSAGES, setlocale(LC_MESSAGES, NULL));
#endif
	/* NOTE:
	 * The GPGME library installs a SIGPIPE signal handler automatically if
	 * the default signal hander is in use. The only time we set a handler
	 * for SIGPIPE is in dload.c, and we reset it when we are done. Given that
	 * we do this, we can let GPGME do its automagic. However, if we install
	 * a library-wide SIGPIPE handler, we will have to be careful.
	 */

	/* check for OpenPGP support (should be a no-brainer, but be safe) */
	err = gpgme_engine_check_version(GPGME_PROTOCOL_OpenPGP);
	CHECK_ERR();

	/* set and check engine information */
	err = gpgme_set_engine_info(GPGME_PROTOCOL_OpenPGP, NULL,
			alpm_option_get_signaturedir());
	CHECK_ERR();
	err = gpgme_get_engine_info(&enginfo);
	CHECK_ERR();
	_alpm_log(PM_LOG_DEBUG, "GPGME engine info: file=%s, home=%s\n",
			enginfo->file_name, enginfo->home_dir);

	init = 1;
	return 0;

error:
	_alpm_log(PM_LOG_ERROR, _("GPGME error: %s\n"), gpgme_strerror(err));
	RET_ERR(PM_ERR_GPGME, 1);
}

/**
 * Check the PGP package signature for the given file.
 * @param path the full path to a file
 * @param sig PGP signature data in raw form (already decoded)
 * @return a int value : 0 (valid), 1 (invalid), -1 (an error occured)
 */
int _alpm_gpgme_checksig(const char *path, const pmpgpsig_t *sig)
{
	int ret = 0;
	gpgme_error_t err;
	gpgme_ctx_t ctx;
	gpgme_data_t filedata, sigdata;
	gpgme_verify_result_t result;
	gpgme_signature_t gpgsig;
	FILE *file = NULL, *sigfile = NULL;

	ALPM_LOG_FUNC;

	if(!sig || !sig->data) {
		 RET_ERR(PM_ERR_SIG_UNKNOWN, -1);
	}
	if(!path || access(path, R_OK) != 0) {
		RET_ERR(PM_ERR_NOT_A_FILE, -1);
	}
	if(gpgme_init()) {
		/* pm_errno was set in gpgme_init() */
		return -1;
	}

	_alpm_log(PM_LOG_DEBUG, "checking signature for %s\n", path);

	memset(&ctx, 0, sizeof(ctx));
	memset(&sigdata, 0, sizeof(sigdata));
	memset(&filedata, 0, sizeof(filedata));

	err = gpgme_new(&ctx);
	CHECK_ERR();

	/* create our necessary data objects to verify the signature */
	file = fopen(path, "rb");
	if(file == NULL) {
		pm_errno = PM_ERR_NOT_A_FILE;
		ret = -1;
		goto error;
	}
	err = gpgme_data_new_from_stream(&filedata, file);
	CHECK_ERR();

	/* next create data object for the signature */
	err = gpgme_data_new_from_mem(&sigdata, (char *)sig->data, sig->len, 0);
	CHECK_ERR();

	/* here's where the magic happens */
	err = gpgme_op_verify(ctx, sigdata, filedata, NULL);
	CHECK_ERR();
	result = gpgme_op_verify_result(ctx);
	gpgsig = result->signatures;
	if(!gpgsig || gpgsig->next) {
		_alpm_log(PM_LOG_ERROR, _("Unexpected number of signatures\n"));
		ret = -1;
		goto error;
	}
	_alpm_log(PM_LOG_DEBUG, "summary=%x\n", gpgsig->summary);
	_alpm_log(PM_LOG_DEBUG, "fpr=%s\n", gpgsig->fpr);
	_alpm_log(PM_LOG_DEBUG, "status=%d\n", gpgsig->status);
	_alpm_log(PM_LOG_DEBUG, "timestamp=%lu\n", gpgsig->timestamp);
	_alpm_log(PM_LOG_DEBUG, "wrong_key_usage=%u\n", gpgsig->wrong_key_usage);
	_alpm_log(PM_LOG_DEBUG, "pka_trust=%u\n", gpgsig->pka_trust);
	_alpm_log(PM_LOG_DEBUG, "chain_model=%u\n", gpgsig->chain_model);
	_alpm_log(PM_LOG_DEBUG, "validity=%d\n", gpgsig->validity);
	_alpm_log(PM_LOG_DEBUG, "validity_reason=%d\n", gpgsig->validity_reason);
	_alpm_log(PM_LOG_DEBUG, "key=%d\n", gpgsig->pubkey_algo);
	_alpm_log(PM_LOG_DEBUG, "hash=%d\n", gpgsig->hash_algo);

	if(gpgsig->summary & GPGME_SIGSUM_VALID) {
		/* good signature, continue */
		_alpm_log(PM_LOG_DEBUG, _("File %s has a valid signature.\n"),
				path);
	} else if(gpgsig->summary & GPGME_SIGSUM_GREEN) {
		/* 'green' signature, not sure what to do here */
		_alpm_log(PM_LOG_WARNING, _("File %s has a green signature.\n"),
				path);
	} else if(gpgsig->summary & GPGME_SIGSUM_KEY_MISSING) {
		pm_errno = PM_ERR_SIG_UNKNOWN;
		_alpm_log(PM_LOG_WARNING, _("File %s has a signature from an unknown key.\n"),
				path);
		ret = -1;
	} else {
		/* we'll capture everything else here */
		pm_errno = PM_ERR_SIG_INVALID;
		_alpm_log(PM_LOG_ERROR, _("File %s has an invalid signature.\n"),
				path);
		ret = 1;
	}

error:
	gpgme_data_release(sigdata);
	gpgme_data_release(filedata);
	gpgme_release(ctx);
	if(sigfile) {
		fclose(sigfile);
	}
	if(file) {
		fclose(file);
	}
	if(err != GPG_ERR_NO_ERROR) {
		_alpm_log(PM_LOG_ERROR, _("GPGME error: %s\n"), gpgme_strerror(err));
		RET_ERR(PM_ERR_GPGME, -1);
	}
	return ret;
}

/**
 * Load the signature from the given path into the provided struct.
 * @param sigfile the signature to attempt to load
 * @param pgpsig the struct to place the data in
 *
 * @return 0 on success, 1 on file not found, -1 on error
 */
int _alpm_load_signature(const char *file, pmpgpsig_t *pgpsig) {
	struct stat st;
	char *sigfile;
	int ret = -1;

	/* look around for a PGP signature file; load if available */
	MALLOC(sigfile, strlen(file) + 5, RET_ERR(PM_ERR_MEMORY, -1));
	sprintf(sigfile, "%s.sig", file);

	if(access(sigfile, R_OK) == 0 && stat(sigfile, &st) == 0) {
		FILE *f;
		size_t bytes_read;

		if(st.st_size > 4096 || (f = fopen(sigfile, "rb")) == NULL) {
			free(sigfile);
			return ret;
		}
		CALLOC(pgpsig->data, st.st_size, sizeof(unsigned char),
				RET_ERR(PM_ERR_MEMORY, -1));
		bytes_read = fread(pgpsig->data, sizeof(char), st.st_size, f);
		if(bytes_read == (size_t)st.st_size) {
			pgpsig->len = bytes_read;
			_alpm_log(PM_LOG_DEBUG, "loaded gpg signature file, location %s\n",
					sigfile);
			ret = 0;
		} else {
			_alpm_log(PM_LOG_WARNING, _("Failed reading PGP signature file %s"),
					sigfile);
			FREE(pgpsig->data);
		}

		fclose(f);
	} else {
		_alpm_log(PM_LOG_DEBUG, "signature file %s not found\n", sigfile);
		/* not fatal...we return a different error code here */
		ret = 1;
	}

	free(sigfile);
	return ret;
}

/**
 * Determines the necessity of checking for a valid PGP signature
 * @param db the sync database to query
 *
 * @return signature verification level
 */
pgp_verify_t _alpm_db_get_sigverify_level(pmdb_t *db)
{
	ALPM_LOG_FUNC;
	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, PM_PGP_VERIFY_UNKNOWN));

	if(db->pgp_verify != PM_PGP_VERIFY_UNKNOWN) {
		return db->pgp_verify;
	} else {
		return alpm_option_get_default_sigverify();
	}
}

/**
 * Check the PGP package signature for the given package file.
 * @param pkg the package to check
 * @return a int value : 0 (valid), 1 (invalid), -1 (an error occurred)
 */
int SYMEXPORT alpm_pkg_check_pgp_signature(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;
	ASSERT(pkg != NULL, return 0);

	return _alpm_gpgme_checksig(alpm_pkg_get_filename(pkg),
			alpm_pkg_get_pgpsig(pkg));
}

/**
 * Check the PGP package signature for the given database.
 * @param db the database to check
 * @return a int value : 0 (valid), 1 (invalid), -1 (an error occurred)
 */
int SYMEXPORT alpm_db_check_pgp_signature(pmdb_t *db)
{
	ALPM_LOG_FUNC;
	ASSERT(db != NULL, return 0);

	return _alpm_gpgme_checksig(_alpm_db_path(db),
			_alpm_db_pgpsig(db));
}

/* vim: set ts=2 sw=2 noet: */
