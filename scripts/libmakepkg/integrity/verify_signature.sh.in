#!/bin/bash
#
#   verify_signature.sh - functions for checking PGP signatures
#
#   Copyright (c) 2011-2016 Pacman Development Team <pacman-dev@archlinux.org>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

[[ -n "$LIBMAKEPKG_INTEGRITY_VERIFY_SIGNATURE_SH" ]] && return
LIBMAKEPKG_INTEGRITY_VERIFY_SIGNATURE_SH=1

LIBRARY=${LIBRARY:-'@libmakepkgdir@'}

source "$LIBRARY/util/message.sh"
source "$LIBRARY/util/pkgbuild.sh"

check_pgpsigs() {
	(( SKIPPGPCHECK )) && return 0
	! source_has_signatures && return 0

	msg "$(gettext "Verifying source file signatures with %s...")" "gpg"

	local file ext decompress found pubkey success status fingerprint trusted
	local warning=0
	local errors=0
	local statusfile=$(mktemp)
	local all_sources

	case $1 in
		all)
			get_all_sources 'all_sources'
			;;
		*)
			get_all_sources_for_arch 'all_sources'
			;;
	esac
	for file in "${all_sources[@]}"; do
		file="$(get_filename "$file")"
		if [[ $file != *.@(sig?(n)|asc) ]]; then
			continue
		fi

		printf "    %s ... " "${file%.*}" >&2

		if ! file="$(get_filepath "$file")"; then
			printf '%s\n' "$(gettext "SIGNATURE NOT FOUND")" >&2
			errors=1
			continue
		fi

		found=0
		for ext in "" gz bz2 xz lrz lzo Z; do
			if sourcefile="$(get_filepath "${file%.*}${ext:+.$ext}")"; then
				found=1
				break;
			fi
		done
		if (( ! found )); then
			printf '%s\n' "$(gettext "SOURCE FILE NOT FOUND")" >&2
			errors=1
			continue
		fi

		case "$ext" in
			gz)  decompress="gzip -c -d -f" ;;
			bz2) decompress="bzip2 -c -d -f" ;;
			xz)  decompress="xz -c -d" ;;
			lrz) decompress="lrzip -q -d" ;;
			lzo) decompress="lzop -c -d -q" ;;
			Z)   decompress="uncompress -c -f" ;;
			"")  decompress="cat" ;;
		esac

		$decompress < "$sourcefile" | gpg --quiet --batch --status-file "$statusfile" --verify "$file" - 2> /dev/null
		# these variables are assigned values in parse_gpg_statusfile
		success=0
		status=
		pubkey=
		fingerprint=
		trusted=
		parse_gpg_statusfile "$statusfile"
		if (( ! $success )); then
			printf '%s' "$(gettext "FAILED")" >&2
			case "$status" in
				"missingkey")
					printf ' (%s)' "$(gettext "unknown public key") $pubkey" >&2
					;;
				"revokedkey")
					printf " ($(gettext "public key %s has been revoked"))" "$pubkey" >&2
					;;
				"bad")
					printf ' (%s)' "$(gettext "bad signature from public key") $pubkey" >&2
					;;
				"error")
					printf ' (%s)' "$(gettext "error during signature verification")" >&2
					;;
			esac
			errors=1
		else
			if (( ${#validpgpkeys[@]} == 0 && !trusted )); then
				printf "%s ($(gettext "the public key %s is not trusted"))" $(gettext "FAILED") "$fingerprint" >&2
				errors=1
			elif (( ${#validpgpkeys[@]} > 0 )) && ! in_array "$fingerprint" "${validpgpkeys[@]}"; then
				printf "%s (%s %s)" "$(gettext "FAILED")" "$(gettext "invalid public key")" "$fingerprint"
				errors=1
			else
				printf '%s' "$(gettext "Passed")" >&2
				case "$status" in
					"expired")
						printf ' (%s)' "$(gettext "WARNING:") $(gettext "the signature has expired.")" >&2
						warnings=1
						;;
					"expiredkey")
						printf ' (%s)' "$(gettext "WARNING:") $(gettext "the key has expired.")" >&2
						warnings=1
						;;
				esac
			fi
		fi
		printf '\n' >&2
	done

	rm -f "$statusfile"

	if (( errors )); then
		error "$(gettext "One or more PGP signatures could not be verified!")"
		exit 1
	fi

	if (( warnings )); then
		warning "$(gettext "Warnings have occurred while verifying the signatures.")"
		plain "$(gettext "Please make sure you really trust them.")"
	fi
}

parse_gpg_statusfile() {
	local type arg1 arg6 arg10

	while read -r _ type arg1 _ _ _ _ arg6 _ _ _ arg10 _; do
		case "$type" in
			GOODSIG)
				pubkey=$arg1
				success=1
				status="good"
				;;
			EXPSIG)
				pubkey=$arg1
				success=1
				status="expired"
				;;
			EXPKEYSIG)
				pubkey=$arg1
				success=1
				status="expiredkey"
				;;
			REVKEYSIG)
				pubkey=$arg1
				success=0
				status="revokedkey"
				;;
			BADSIG)
				pubkey=$arg1
				success=0
				status="bad"
				;;
			ERRSIG)
				pubkey=$arg1
				success=0
				if [[ $arg6 == 9 ]]; then
					status="missingkey"
				else
					status="error"
				fi
				;;
			VALIDSIG)
				if [[ $arg10 ]]; then
					# If the file was signed with a subkey, arg10 contains
					# the fingerprint of the primary key
					fingerprint=$arg10
				else
					fingerprint=$arg1
				fi
				;;
			TRUST_UNDEFINED|TRUST_NEVER)
				trusted=0
				;;
			TRUST_MARGINAL|TRUST_FULLY|TRUST_ULTIMATE)
				trusted=1
				;;
		esac
	done < "$1"
}

source_has_signatures() {
	local file all_sources

	get_all_sources_for_arch 'all_sources'
	for file in "${all_sources[@]}"; do
		if [[ ${file%%::*} = *.@(sig?(n)|asc) ]]; then
			return 0
		fi
	done
	return 1
}
