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
#include "handle.h"

#if HAVE_LIBGPGME
#define CHECK_ERR(void) do { \
		if(gpg_err_code(err) != GPG_ERR_NO_ERROR) { goto error; } \
	} while(0)

static const char *string_validity(gpgme_validity_t validity)
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

static void sigsum_test_bit(gpgme_sigsum_t sigsum, alpm_list_t **summary,
		gpgme_sigsum_t bit, const char *value)
{
	if(sigsum & bit) {
		*summary = alpm_list_add(*summary, (void *)value);
	}
}

static alpm_list_t *list_sigsum(gpgme_sigsum_t sigsum)
{
	alpm_list_t *summary = NULL;
	/* The docs say this can be a bitmask...not sure I believe it, but we'll code
	 * for it anyway and show all possible flags in the returned string. */

	/* The signature is fully valid.  */
	sigsum_test_bit(sigsum, &summary, GPGME_SIGSUM_VALID, "valid");
	/* The signature is good.  */
	sigsum_test_bit(sigsum, &summary, GPGME_SIGSUM_GREEN, "green");
	/* The signature is bad.  */
	sigsum_test_bit(sigsum, &summary, GPGME_SIGSUM_RED, "red");
	/* One key has been revoked.  */
	sigsum_test_bit(sigsum, &summary, GPGME_SIGSUM_KEY_REVOKED, "key revoked");
	/* One key has expired.  */
	sigsum_test_bit(sigsum, &summary, GPGME_SIGSUM_KEY_EXPIRED, "key expired");
	/* The signature has expired.  */
	sigsum_test_bit(sigsum, &summary, GPGME_SIGSUM_SIG_EXPIRED, "sig expired");
	/* Can't verify: key missing.  */
	sigsum_test_bit(sigsum, &summary, GPGME_SIGSUM_KEY_MISSING, "key missing");
	/* CRL not available.  */
	sigsum_test_bit(sigsum, &summary, GPGME_SIGSUM_CRL_MISSING, "crl missing");
	/* Available CRL is too old.  */
	sigsum_test_bit(sigsum, &summary, GPGME_SIGSUM_CRL_TOO_OLD, "crl too old");
	/* A policy was not met.  */
	sigsum_test_bit(sigsum, &summary, GPGME_SIGSUM_BAD_POLICY, "bad policy");
	/* A system error occured.  */
	sigsum_test_bit(sigsum, &summary, GPGME_SIGSUM_SYS_ERROR, "sys error");
	/* Fallback case */
	if(!sigsum) {
		summary = alpm_list_add(summary, (void *)"(empty)");
	}
	return summary;
}

static int init_gpgme(alpm_handle_t *handle)
{
	static int init = 0;
	const char *version, *sigdir;
	gpgme_error_t err;
	gpgme_engine_info_t enginfo;

	if(init) {
		/* we already successfully initialized the library */
		return 0;
	}

	sigdir = handle->gpgdir;

	if (_alpm_access(handle, sigdir, "pubring.gpg", R_OK)
			|| _alpm_access(handle, sigdir, "trustdb.gpg", R_OK)) {
		handle->pm_errno = ALPM_ERR_NOT_A_FILE;
		_alpm_log(handle, ALPM_LOG_DEBUG, "Signature verification will fail!\n");
	}

	/* calling gpgme_check_version() returns the current version and runs
	 * some internal library setup code */
	version = gpgme_check_version(NULL);
	_alpm_log(handle, ALPM_LOG_DEBUG, "GPGME version: %s\n", version);
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
	err = gpgme_set_engine_info(GPGME_PROTOCOL_OpenPGP, NULL, sigdir);
	CHECK_ERR();
	err = gpgme_get_engine_info(&enginfo);
	CHECK_ERR();
	_alpm_log(handle, ALPM_LOG_DEBUG, "GPGME engine info: file=%s, home=%s\n",
			enginfo->file_name, enginfo->home_dir);

	init = 1;
	return 0;

error:
	_alpm_log(handle, ALPM_LOG_ERROR, _("GPGME error: %s\n"), gpgme_strerror(err));
	RET_ERR(handle, ALPM_ERR_GPGME, 1);
}

/**
 * Decode a loaded signature in base64 form.
 * @param base64_data the signature to attempt to decode
 * @param data the decoded data; must be freed by the caller
 * @param data_len the length of the returned data
 * @return 0 on success, 1 on failure to properly decode
 */
static int decode_signature(const char *base64_data,
		unsigned char **data, size_t *data_len) {
	size_t len = strlen(base64_data);
	unsigned char *usline = (unsigned char *)base64_data;
	/* reasonable allocation of expected length is 3/4 of encoded length */
	size_t destlen = len * 3 / 4;
	MALLOC(*data, destlen, goto error);
	if(base64_decode(*data, &destlen, usline, len)) {
		free(*data);
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
 * Check the PGP signature for the given file path.
 * If base64_sig is provided, it will be used as the signature data after
 * decoding. If base64_sig is NULL, expect a signature file next to path
 * (e.g. "%s.sig").
 *
 * The return value will be 0 if nothing abnormal happened during the signature
 * check, and -1 if an error occurred while checking signatures or if a
 * signature could not be found; pm_errno will be set. Note that "abnormal"
 * does not include a failed signature; the value in #siglist should be checked
 * to determine if the signature(s) are good.
 * @param handle the context handle
 * @param path the full path to a file
 * @param base64_sig optional PGP signature data in base64 encoding
 * @param siglist a pointer to storage for signature results
 * @return 0 in normal cases, -1 if the something failed in the check process
 */
int _alpm_gpgme_checksig(alpm_handle_t *handle, const char *path,
		const char *base64_sig, alpm_siglist_t *siglist)
{
	int ret = -1, sigcount;
	gpgme_error_t err;
	gpgme_ctx_t ctx;
	gpgme_data_t filedata, sigdata;
	gpgme_verify_result_t verify_result;
	gpgme_signature_t gpgsig;
	char *sigpath = NULL;
	unsigned char *decoded_sigdata = NULL;
	FILE *file = NULL, *sigfile = NULL;

	if(!path || _alpm_access(handle, NULL, path, R_OK) != 0) {
		RET_ERR(handle, ALPM_ERR_NOT_A_FILE, -1);
	}

	if(!siglist) {
		RET_ERR(handle, ALPM_ERR_WRONG_ARGS, -1);
	}
	siglist->count = 0;

	if(!base64_sig) {
		sigpath = _alpm_sigpath(handle, path);
		/* this will just help debugging */
		_alpm_access(handle, NULL, sigpath, R_OK);
	}

	if(init_gpgme(handle)) {
		/* pm_errno was set in gpgme_init() */
		return -1;
	}

	_alpm_log(handle, ALPM_LOG_DEBUG, "checking signature for %s\n", path);

	memset(&ctx, 0, sizeof(ctx));
	memset(&sigdata, 0, sizeof(sigdata));
	memset(&filedata, 0, sizeof(filedata));

	err = gpgme_new(&ctx);
	CHECK_ERR();

	/* create our necessary data objects to verify the signature */
	file = fopen(path, "rb");
	if(file == NULL) {
		handle->pm_errno = ALPM_ERR_NOT_A_FILE;
		goto error;
	}
	err = gpgme_data_new_from_stream(&filedata, file);
	CHECK_ERR();

	/* next create data object for the signature */
	if(base64_sig) {
		/* memory-based, we loaded it from a sync DB */
		size_t data_len;
		int decode_ret = decode_signature(base64_sig,
				&decoded_sigdata, &data_len);
		if(decode_ret) {
			handle->pm_errno = ALPM_ERR_SIG_INVALID;
			goto error;
		}
		err = gpgme_data_new_from_mem(&sigdata,
				(char *)decoded_sigdata, data_len, 0);
	} else {
		/* file-based, it is on disk */
		sigfile = fopen(sigpath, "rb");
		if(sigfile == NULL) {
			_alpm_log(handle, ALPM_LOG_DEBUG, "sig path %s could not be opened\n",
					sigpath);
			handle->pm_errno = ALPM_ERR_SIG_MISSING;
			goto error;
		}
		err = gpgme_data_new_from_stream(&sigdata, sigfile);
	}
	CHECK_ERR();

	/* here's where the magic happens */
	err = gpgme_op_verify(ctx, sigdata, filedata, NULL);
	CHECK_ERR();
	verify_result = gpgme_op_verify_result(ctx);
	CHECK_ERR();
	if(!verify_result || !verify_result->signatures) {
		_alpm_log(handle, ALPM_LOG_DEBUG, "no signatures returned\n");
		handle->pm_errno = ALPM_ERR_SIG_MISSING;
		goto error;
	}
	for(gpgsig = verify_result->signatures, sigcount = 0;
			gpgsig; gpgsig = gpgsig->next, sigcount++);
	_alpm_log(handle, ALPM_LOG_DEBUG, "%d signatures returned\n", sigcount);

	CALLOC(siglist->results, sigcount, sizeof(alpm_sigresult_t),
			handle->pm_errno = ALPM_ERR_MEMORY; goto error);
	siglist->count = sigcount;

	for(gpgsig = verify_result->signatures, sigcount = 0; gpgsig;
			gpgsig = gpgsig->next, sigcount++) {
		alpm_list_t *summary_list, *summary;
		alpm_sigstatus_t status;
		alpm_sigvalidity_t validity;
		gpgme_key_t key;
		alpm_sigresult_t *result;

		_alpm_log(handle, ALPM_LOG_DEBUG, "fingerprint: %s\n", gpgsig->fpr);
		summary_list = list_sigsum(gpgsig->summary);
		for(summary = summary_list; summary; summary = summary->next) {
			_alpm_log(handle, ALPM_LOG_DEBUG, "summary: %s\n", (const char *)summary->data);
		}
		alpm_list_free(summary_list);
		_alpm_log(handle, ALPM_LOG_DEBUG, "status: %s\n", gpgme_strerror(gpgsig->status));
		_alpm_log(handle, ALPM_LOG_DEBUG, "timestamp: %lu\n", gpgsig->timestamp);
		_alpm_log(handle, ALPM_LOG_DEBUG, "exp_timestamp: %lu\n", gpgsig->exp_timestamp);
		_alpm_log(handle, ALPM_LOG_DEBUG, "validity: %s; reason: %s\n",
				string_validity(gpgsig->validity),
				gpgme_strerror(gpgsig->validity_reason));

		result = siglist->results + sigcount;
		err = gpgme_get_key(ctx, gpgsig->fpr, &key, 0);
		if(gpg_err_code(err) == GPG_ERR_EOF) {
			_alpm_log(handle, ALPM_LOG_DEBUG, "key lookup failed, unknown key\n");
			err = GPG_ERR_NO_ERROR;
			/* we dupe the fpr in this case since we have no key to point at */
			STRDUP(result->key.fingerprint, gpgsig->fpr,
					handle->pm_errno = ALPM_ERR_MEMORY; goto error);
		} else {
			CHECK_ERR();
			if(key->uids) {
				result->key.data = key;
				result->key.fingerprint = key->subkeys->fpr;
				result->key.uid = key->uids->uid;
				result->key.name = key->uids->name;
				result->key.email = key->uids->email;
				result->key.created = key->subkeys->timestamp;
				result->key.expires = key->subkeys->expires;
				_alpm_log(handle, ALPM_LOG_DEBUG, "key: %s, %s, owner_trust %s\n",
						key->subkeys->fpr, key->uids->uid, string_validity(key->owner_trust));
			}
		}

		switch(gpg_err_code(gpgsig->status)) {
			/* good cases */
			case GPG_ERR_NO_ERROR:
				status = ALPM_SIGSTATUS_VALID;
				break;
			case GPG_ERR_KEY_EXPIRED:
				status = ALPM_SIGSTATUS_KEY_EXPIRED;
				break;
			/* bad cases */
			case GPG_ERR_SIG_EXPIRED:
				status = ALPM_SIGSTATUS_SIG_EXPIRED;
				break;
			case GPG_ERR_NO_PUBKEY:
				status = ALPM_SIGSTATUS_KEY_UNKNOWN;
				break;
			case GPG_ERR_BAD_SIGNATURE:
			default:
				status = ALPM_SIGSTATUS_INVALID;
				break;
		}

		switch(gpgsig->validity) {
			case GPGME_VALIDITY_ULTIMATE:
			case GPGME_VALIDITY_FULL:
				validity = ALPM_SIGVALIDITY_FULL;
				break;
			case GPGME_VALIDITY_MARGINAL:
				validity = ALPM_SIGVALIDITY_MARGINAL;
				break;
			case GPGME_VALIDITY_NEVER:
				validity = ALPM_SIGVALIDITY_NEVER;
				break;
			case GPGME_VALIDITY_UNKNOWN:
			case GPGME_VALIDITY_UNDEFINED:
			default:
				validity = ALPM_SIGVALIDITY_UNKNOWN;
				break;
		}

		result->status = status;
		result->validity = validity;
	}

	ret = 0;

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
	if(gpg_err_code(err) != GPG_ERR_NO_ERROR) {
		_alpm_log(handle, ALPM_LOG_ERROR, _("GPGME error: %s\n"), gpgme_strerror(err));
		RET_ERR(handle, ALPM_ERR_GPGME, -1);
	}
	return ret;
}
#else
int _alpm_gpgme_checksig(alpm_handle_t UNUSED *handle, const char UNUSED *path,
		const char UNUSED *base64_sig, alpm_siglist_t UNUSED *siglist)
{
	return -1;
}
#endif

/**
 * Form a signature path given a file path.
 * Caller must free the result.
 * @param handle the context handle
 * @param path the full path to a file
 * @return the path with '.sig' appended, NULL on errors
 */
char *_alpm_sigpath(alpm_handle_t *handle, const char *path)
{
	char *sigpath;
	size_t len;

	if(!path) {
		return NULL;
	}
	len = strlen(path) + 5;
	CALLOC(sigpath, len, sizeof(char), RET_ERR(handle, ALPM_ERR_MEMORY, NULL));
	sprintf(sigpath, "%s.sig", path);
	return sigpath;
}

int _alpm_check_pgp_helper(alpm_handle_t *handle, const char *path,
		const char *base64_sig, int optional, int marginal, int unknown,
		alpm_siglist_t **sigdata)
{
	alpm_siglist_t *siglist;
	int ret;

	CALLOC(siglist, 1, sizeof(alpm_siglist_t),
			RET_ERR(handle, ALPM_ERR_MEMORY, -1));

	_alpm_log(handle, ALPM_LOG_DEBUG, "checking signatures for %s\n", path);
	ret = _alpm_gpgme_checksig(handle, path, base64_sig, siglist);
	if(ret && handle->pm_errno == ALPM_ERR_SIG_MISSING) {
		if(optional) {
			_alpm_log(handle, ALPM_LOG_DEBUG, "missing optional signature\n");
			handle->pm_errno = 0;
			ret = 0;
		} else {
			_alpm_log(handle, ALPM_LOG_DEBUG, "missing required signature\n");
			/* ret will already be -1 */
		}
	} else if(ret) {
		_alpm_log(handle, ALPM_LOG_DEBUG, "signature check failed\n");
		/* ret will already be -1 */
	} else {
		size_t num;
		for(num = 0; !ret && num < siglist->count; num++) {
			switch(siglist->results[num].status) {
				case ALPM_SIGSTATUS_VALID:
				case ALPM_SIGSTATUS_KEY_EXPIRED:
					_alpm_log(handle, ALPM_LOG_DEBUG, "signature is valid\n");
					switch(siglist->results[num].validity) {
						case ALPM_SIGVALIDITY_FULL:
							_alpm_log(handle, ALPM_LOG_DEBUG, "signature is fully trusted\n");
							break;
						case ALPM_SIGVALIDITY_MARGINAL:
							_alpm_log(handle, ALPM_LOG_DEBUG, "signature is marginal trust\n");
							if(!marginal) {
								ret = -1;
							}
							break;
						case ALPM_SIGVALIDITY_UNKNOWN:
							_alpm_log(handle, ALPM_LOG_DEBUG, "signature is unknown trust\n");
							if(!unknown) {
								ret = -1;
							}
							break;
						case ALPM_SIGVALIDITY_NEVER:
							_alpm_log(handle, ALPM_LOG_DEBUG, "signature should never be trusted\n");
							ret = -1;
							break;
					}
					break;
				case ALPM_SIGSTATUS_SIG_EXPIRED:
				case ALPM_SIGSTATUS_KEY_UNKNOWN:
				case ALPM_SIGSTATUS_INVALID:
					_alpm_log(handle, ALPM_LOG_DEBUG, "signature is not valid\n");
					ret = -1;
					break;
			}
		}
	}

	if(sigdata) {
		*sigdata = siglist;
	} else {
		alpm_siglist_cleanup(siglist);
		free(siglist);
	}

	return ret;
}

int _alpm_process_siglist(alpm_handle_t *handle, const char *identifier,
		alpm_siglist_t *siglist, int optional, int marginal, int unknown)
{
	size_t i;
	int retry = 0;

	if(!optional && siglist->count == 0) {
		_alpm_log(handle, ALPM_LOG_ERROR,
				_("%s: missing required signature\n"), identifier);
	}

	for(i = 0; i < siglist->count; i++) {
		alpm_sigresult_t *result = siglist->results + i;
		const char *name = result->key.uid ? result->key.uid : result->key.fingerprint;
		switch(result->status) {
			case ALPM_SIGSTATUS_VALID:
			case ALPM_SIGSTATUS_KEY_EXPIRED:
				switch(result->validity) {
					case ALPM_SIGVALIDITY_FULL:
						break;
					case ALPM_SIGVALIDITY_MARGINAL:
						if(!marginal) {
							_alpm_log(handle, ALPM_LOG_ERROR,
									_("%s: signature from \"%s\" is marginal trust\n"),
									identifier, name);
						}
						break;
					case ALPM_SIGVALIDITY_UNKNOWN:
						if(!unknown) {
							_alpm_log(handle, ALPM_LOG_ERROR,
									_("%s: signature from \"%s\" is unknown trust\n"),
									identifier, name);
						}
						break;
					case ALPM_SIGVALIDITY_NEVER:
						_alpm_log(handle, ALPM_LOG_ERROR,
								_("%s: signature from \"%s\" should never be trusted\n"),
								identifier, name);
						break;
				}
				break;
			case ALPM_SIGSTATUS_KEY_UNKNOWN:
				/* TODO import key here */
				_alpm_log(handle, ALPM_LOG_ERROR,
						_("%s: key \"%s\" is unknown\n"),
						identifier, name);
				break;
			case ALPM_SIGSTATUS_SIG_EXPIRED:
				_alpm_log(handle, ALPM_LOG_ERROR,
						_("%s: signature from \"%s\" is expired\n"),
						identifier, name);
				break;
			case ALPM_SIGSTATUS_INVALID:
				_alpm_log(handle, ALPM_LOG_ERROR,
						_("%s: signature from \"%s\" is invalid\n"),
						identifier, name);
				break;
		}
	}

	return retry;
}

/**
 * Check the PGP signature for the given package file.
 * @param pkg the package to check
 * @return a int value : 0 (valid), 1 (invalid), -1 (an error occurred)
 */
int SYMEXPORT alpm_pkg_check_pgp_signature(alpm_pkg_t *pkg,
		alpm_siglist_t *siglist)
{
	ASSERT(pkg != NULL, return -1);
	ASSERT(siglist != NULL, RET_ERR(pkg->handle, ALPM_ERR_WRONG_ARGS, -1));
	pkg->handle->pm_errno = 0;

	return _alpm_gpgme_checksig(pkg->handle, pkg->filename,
			pkg->base64_sig, siglist);
}

/**
 * Check the PGP signature for the given database.
 * @param db the database to check
 * @return a int value : 0 (valid), 1 (invalid), -1 (an error occurred)
 */
int SYMEXPORT alpm_db_check_pgp_signature(alpm_db_t *db,
		alpm_siglist_t *siglist)
{
	ASSERT(db != NULL, return -1);
	ASSERT(siglist != NULL, RET_ERR(db->handle, ALPM_ERR_WRONG_ARGS, -1));
	db->handle->pm_errno = 0;

	return _alpm_gpgme_checksig(db->handle, _alpm_db_path(db), NULL, siglist);
}

int SYMEXPORT alpm_siglist_cleanup(alpm_siglist_t *siglist)
{
	ASSERT(siglist != NULL, return -1);
	size_t num;
	for(num = 0; num < siglist->count; num++) {
		alpm_sigresult_t *result = siglist->results + num;
		if(result->key.data) {
#if HAVE_LIBGPGME
			gpgme_key_unref(result->key.data);
#endif
		} else {
			free(result->key.fingerprint);
		}
	}
	if(siglist->count) {
		free(siglist->results);
	}
	siglist->results = NULL;
	siglist->count = 0;
	return 0;
}

/* vim: set ts=2 sw=2 noet: */
