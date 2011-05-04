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

#if HAVE_LIBGPGME
#include <locale.h> /* setlocale() */
#include <gpgme.h>
#include "base64.h"
#endif

/* libalpm */
#include "signing.h"
#include "package.h"
#include "util.h"
#include "log.h"
#include "alpm.h"

#if HAVE_LIBGPGME
#define CHECK_ERR(void) do { \
		if(err != GPG_ERR_NO_ERROR) { goto error; } \
	} while(0)

static const char *gpgme_string_validity(gpgme_validity_t validity)
{
	switch(validity) {
		case GPGME_VALIDITY_UNKNOWN:
			return "unknown";
		case GPGME_VALIDITY_UNDEFINED:
			return "undefined";
		case GPGME_VALIDITY_NEVER:
			return "never";
		case GPGME_VALIDITY_MARGINAL:
			return "marginal";
		case GPGME_VALIDITY_FULL:
			return "full";
		case GPGME_VALIDITY_ULTIMATE:
			return "ultimate";
	}
	return "???";
}

static alpm_list_t *sigsum_test_bit(gpgme_sigsum_t sigsum, alpm_list_t *summary,
		gpgme_sigsum_t bit, const char *value)
{
	if(sigsum & bit) {
		summary = alpm_list_add(summary, (void *)value);
	}
	return summary;
}

static alpm_list_t *gpgme_list_sigsum(gpgme_sigsum_t sigsum)
{
	alpm_list_t *summary = NULL;
	/* The docs say this can be a bitmask...not sure I believe it, but we'll code
	 * for it anyway and show all possible flags in the returned string. */

	/* The signature is fully valid.  */
	summary = sigsum_test_bit(sigsum, summary, GPGME_SIGSUM_VALID, "valid");
	/* The signature is good.  */
	summary = sigsum_test_bit(sigsum, summary, GPGME_SIGSUM_GREEN, "green");
	/* The signature is bad.  */
	summary = sigsum_test_bit(sigsum, summary, GPGME_SIGSUM_RED, "red");
	/* One key has been revoked.  */
	summary = sigsum_test_bit(sigsum, summary, GPGME_SIGSUM_KEY_REVOKED, "key revoked");
	/* One key has expired.  */
	summary = sigsum_test_bit(sigsum, summary, GPGME_SIGSUM_KEY_EXPIRED, "key expired");
	/* The signature has expired.  */
	summary = sigsum_test_bit(sigsum, summary, GPGME_SIGSUM_SIG_EXPIRED, "sig expired");
	/* Can't verify: key missing.  */
	summary = sigsum_test_bit(sigsum, summary, GPGME_SIGSUM_KEY_MISSING, "key missing");
	/* CRL not available.  */
	summary = sigsum_test_bit(sigsum, summary, GPGME_SIGSUM_CRL_MISSING, "crl missing");
	/* Available CRL is too old.  */
	summary = sigsum_test_bit(sigsum, summary, GPGME_SIGSUM_CRL_TOO_OLD, "crl too old");
	/* A policy was not met.  */
	summary = sigsum_test_bit(sigsum, summary, GPGME_SIGSUM_BAD_POLICY, "bad policy");
	/* A system error occured.  */
	summary = sigsum_test_bit(sigsum, summary, GPGME_SIGSUM_SYS_ERROR, "sys error");
	/* Fallback case */
	if(!sigsum) {
		summary = alpm_list_add(summary, (void *)"(empty)");
	}
	return summary;
}

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
 * Decode a loaded signature in base64 form.
 * @param base64_data the signature to attempt to decode
 * @param data the decoded data; must be freed by the caller
 * @param data_len the length of the returned data
 * @return 0 on success, 1 on failure to properly decode
 */
static int decode_signature(const char *base64_data,
		unsigned char **data, int *data_len) {
	unsigned char *usline;
	int len;

	len = strlen(base64_data);
	usline = (unsigned char *)base64_data;
	int ret, destlen = 0;
	/* get the necessary size for the buffer by passing 0 */
	ret = base64_decode(NULL, &destlen, usline, len);
	if(ret != 0 && ret != POLARSSL_ERR_BASE64_BUFFER_TOO_SMALL) {
		goto error;
	}
	/* alloc our memory and repeat the call to decode */
	MALLOC(*data, (size_t)destlen, goto error);
	ret = base64_decode(*data, &destlen, usline, len);
	if(ret != 0) {
		goto error;
	}
	*data_len = destlen;
	return 0;

error:
	*data = NULL;
	*data_len = 0;
	return 1;
}

/**
 * Check the PGP signature for the given file.
 * @param path the full path to a file
 * @param base64_sig PGP signature data in base64 encoding; if NULL, expect a
 * signature file next to 'path'
 * @return a int value : 0 (valid), 1 (invalid), -1 (an error occured)
 */
int _alpm_gpgme_checksig(const char *path, const char *base64_sig)
{
	int ret = 0;
	gpgme_error_t err;
	gpgme_ctx_t ctx;
	gpgme_data_t filedata, sigdata;
	gpgme_verify_result_t result;
	gpgme_signature_t gpgsig;
	char *sigpath = NULL;
	unsigned char *decoded_sigdata = NULL;
	FILE *file = NULL, *sigfile = NULL;

	ALPM_LOG_FUNC;

	if(!path || access(path, R_OK) != 0) {
		RET_ERR(PM_ERR_NOT_A_FILE, -1);
	}

	if(!base64_sig) {
		size_t len = strlen(path) + 5;
		CALLOC(sigpath, len, sizeof(char), RET_ERR(PM_ERR_MEMORY, -1));
		snprintf(sigpath, len, "%s.sig", path);

		if(!access(sigpath, R_OK) == 0) {
			FREE(sigpath);
			RET_ERR(PM_ERR_SIG_UNKNOWN, -1);
		}
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
	if(base64_sig) {
		/* memory-based, we loaded it from a sync DB */
		int data_len;
		int decode_ret = decode_signature(base64_sig,
				&decoded_sigdata, &data_len);
		if(decode_ret) {
			ret = -1;
			goto error;
		}
		err = gpgme_data_new_from_mem(&sigdata,
				(char *)decoded_sigdata, data_len, 0);
	} else {
		/* file-based, it is on disk */
		sigfile = fopen(sigpath, "rb");
		if(sigfile == NULL) {
			pm_errno = PM_ERR_NOT_A_FILE;
			ret = -1;
			goto error;
		}
		err = gpgme_data_new_from_stream(&sigdata, sigfile);
	}
	CHECK_ERR();

	/* here's where the magic happens */
	err = gpgme_op_verify(ctx, sigdata, filedata, NULL);
	CHECK_ERR();
	result = gpgme_op_verify_result(ctx);
	gpgsig = result->signatures;
	if(!gpgsig || gpgsig->next) {
		int count = 0;
		while(gpgsig) {
			count++;
			gpgsig = gpgsig->next;
		}
		_alpm_log(PM_LOG_ERROR, _("Unexpected number of signatures (%d)\n"),
				count);
		ret = -1;
		goto error;
	}

	{
		alpm_list_t *summary_list, *summary;

		_alpm_log(PM_LOG_DEBUG, "fingerprint: %s\n", gpgsig->fpr);
		summary_list = gpgme_list_sigsum(gpgsig->summary);
		for(summary = summary_list; summary; summary = summary->next) {
			_alpm_log(PM_LOG_DEBUG, "summary: %s\n", (const char *)summary->data);
		}
		alpm_list_free(summary_list);
		_alpm_log(PM_LOG_DEBUG, "status: %s\n", gpgme_strerror(gpgsig->status));
		_alpm_log(PM_LOG_DEBUG, "timestamp: %lu\n", gpgsig->timestamp);
		_alpm_log(PM_LOG_DEBUG, "exp_timestamp: %lu\n", gpgsig->exp_timestamp);
		_alpm_log(PM_LOG_DEBUG, "validity: %s\n",
				gpgme_string_validity(gpgsig->validity));
		_alpm_log(PM_LOG_DEBUG, "validity_reason: %s\n",
				gpgme_strerror(gpgsig->validity_reason));
		_alpm_log(PM_LOG_DEBUG, "pubkey algo: %s\n",
				gpgme_pubkey_algo_name(gpgsig->pubkey_algo));
		_alpm_log(PM_LOG_DEBUG, "hash algo: %s\n",
				gpgme_hash_algo_name(gpgsig->hash_algo));
	}

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
	FREE(sigpath);
	FREE(decoded_sigdata);
	if(err != GPG_ERR_NO_ERROR) {
		_alpm_log(PM_LOG_ERROR, _("GPGME error: %s\n"), gpgme_strerror(err));
		RET_ERR(PM_ERR_GPGME, -1);
	}
	return ret;
}
#else
int _alpm_gpgme_checksig(const char *path, const char *base64_sig)
{
	return -1;
}
#endif

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
 * Check the PGP signature for the given package file.
 * @param pkg the package to check
 * @return a int value : 0 (valid), 1 (invalid), -1 (an error occurred)
 */
int SYMEXPORT alpm_pkg_check_pgp_signature(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;
	ASSERT(pkg != NULL, return 0);

	return _alpm_gpgme_checksig(alpm_pkg_get_filename(pkg), pkg->base64_sig);
}

/**
 * Check the PGP signature for the given database.
 * @param db the database to check
 * @return a int value : 0 (valid), 1 (invalid), -1 (an error occurred)
 */
int SYMEXPORT alpm_db_check_pgp_signature(pmdb_t *db)
{
	ALPM_LOG_FUNC;
	ASSERT(db != NULL, return 0);

	return _alpm_gpgme_checksig(_alpm_db_path(db), NULL);
}

/* vim: set ts=2 sw=2 noet: */
