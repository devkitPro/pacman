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
 * Check the PGP package signature for the given package file.
 * @param pkgpath the full path to a package file
 * @param sig PGP signature data in raw form (already decoded)
 * @return a int value : 0 (valid), 1 (invalid), -1 (an error occured)
 */
int _alpm_gpgme_checksig(const char *pkgpath, const pmpgpsig_t *sig)
{
	int ret = 0;
	gpgme_error_t err;
	gpgme_ctx_t ctx;
	gpgme_data_t pkgdata, sigdata;
	gpgme_verify_result_t result;
	gpgme_signature_t gpgsig;
	FILE *pkgfile = NULL, *sigfile = NULL;

	ALPM_LOG_FUNC;

	if(!sig || !sig->rawdata) {
		 RET_ERR(PM_ERR_SIG_UNKNOWN, -1);
	}
	if(!pkgpath || access(pkgpath, R_OK) != 0) {
		RET_ERR(PM_ERR_PKG_NOT_FOUND, -1);
	}
	if(gpgme_init()) {
		/* pm_errno was set in gpgme_init() */
		return -1;
	}

	_alpm_log(PM_LOG_DEBUG, "checking package signature for %s\n", pkgpath);

	memset(&ctx, 0, sizeof(ctx));
	memset(&sigdata, 0, sizeof(sigdata));
	memset(&pkgdata, 0, sizeof(pkgdata));

	err = gpgme_new(&ctx);
	CHECK_ERR();

	/* create our necessary data objects to verify the signature */
	/* first the package itself */
	pkgfile = fopen(pkgpath, "rb");
	if(pkgfile == NULL) {
		pm_errno = PM_ERR_PKG_OPEN;
		ret = -1;
		goto error;
	}
	err = gpgme_data_new_from_stream(&pkgdata, pkgfile);
	CHECK_ERR();

	/* next create data object for the signature */
	err = gpgme_data_new_from_mem(&sigdata, (char*)sig->rawdata, sig->rawlen, 0);
	CHECK_ERR();

	/* here's where the magic happens */
	err = gpgme_op_verify(ctx, sigdata, pkgdata, NULL);
	CHECK_ERR();
	result = gpgme_op_verify_result(ctx);
		gpgsig = result->signatures;
	if (!gpgsig || gpgsig->next) {
		_alpm_log(PM_LOG_ERROR, _("Unexpected number of signatures\n"));
		ret = -1;
		goto error;
	}
	fprintf(stdout, "\nsummary=%x\n", gpgsig->summary);
	fprintf(stdout, "fpr=%s\n", gpgsig->fpr);
	fprintf(stdout, "status=%d\n", gpgsig->status);
	fprintf(stdout, "timestamp=%lu\n", gpgsig->timestamp);
	fprintf(stdout, "wrong_key_usage=%u\n", gpgsig->wrong_key_usage);
	fprintf(stdout, "pka_trust=%u\n", gpgsig->pka_trust);
	fprintf(stdout, "chain_model=%u\n", gpgsig->chain_model);
	fprintf(stdout, "validity=%d\n", gpgsig->validity);
	fprintf(stdout, "validity_reason=%d\n", gpgsig->validity_reason);
	fprintf(stdout, "key=%d\n", gpgsig->pubkey_algo);
	fprintf(stdout, "hash=%d\n", gpgsig->hash_algo);

	if(gpgsig->summary & GPGME_SIGSUM_VALID) {
		/* good signature, continue */
	} else if(gpgsig->summary & GPGME_SIGSUM_GREEN) {
		/* 'green' signature, not sure what to do here */
		_alpm_log(PM_LOG_WARNING, _("Package %s has a green signature.\n"),
				pkgpath);
	} else if(gpgsig->summary & GPGME_SIGSUM_KEY_MISSING) {
		pm_errno = PM_ERR_SIG_UNKNOWN;
		_alpm_log(PM_LOG_WARNING, _("Package %s has a signature from an unknown key.\n"),
				pkgpath);
		ret = -1;
	} else {
		/* we'll capture everything else here */
		pm_errno = PM_ERR_SIG_INVALID;
		_alpm_log(PM_LOG_ERROR, _("Package %s has an invalid signature.\n"),
				pkgpath);
		ret = 1;
	}

error:
	gpgme_data_release(sigdata);
	gpgme_data_release(pkgdata);
	gpgme_release(ctx);
	if(sigfile) {
		fclose(sigfile);
	}
	if(pkgfile) {
		fclose(pkgfile);
	}
	if(err != GPG_ERR_NO_ERROR) {
		_alpm_log(PM_LOG_ERROR, _("GPGME error: %s\n"), gpgme_strerror(err));
		RET_ERR(PM_ERR_GPGME, -1);
	}
	return ret;
}

/**
 * Check the PGP package signature for the given package file.
 * @param pkg the package to check
 * @return a int value : 0 (valid), 1 (invalid), -1 (an error occured)
 */
int SYMEXPORT alpm_pkg_check_pgp_signature(pmpkg_t *pkg)
{
	ALPM_LOG_FUNC;
	ASSERT(pkg != NULL, return 0);

	return _alpm_gpgme_checksig(alpm_pkg_get_filename(pkg),
			alpm_pkg_get_pgpsig(pkg));
}

/* vim: set ts=2 sw=2 noet: */
