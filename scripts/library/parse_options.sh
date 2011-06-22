# getopt like parser
parse_options() {
	local short_options=$1; shift;
	local long_options=$1; shift;
	local ret=0;
	local unused_options=""
	local i

	while [[ -n $1 ]]; do
		if [[ ${1:0:2} = '--' ]]; then
			if [[ -n ${1:2} ]]; then
				local match=""
				for i in ${long_options//,/ }; do
					if [[ ${1:2} = ${i//:} ]]; then
						match=$i
						break
					fi
				done
				if [[ -n $match ]]; then
					if [[ ${1:2} = $match ]]; then
						printf ' %s' "$1"
					else
						if [[ -n $2 ]]; then
							printf ' %s' "$1"
							shift
							printf " '%s'" "$1"
						else
							printf "@SCRIPTNAME@: $(gettext "option %s requires an argument")" "'$1'" >&2
							ret=1
						fi
					fi
				else
					echo "@SCRIPTNAME@: $(gettext "unrecognized option") '$1'" >&2
					ret=1
				fi
			else
				shift
				break
			fi
		elif [[ ${1:0:1} = '-' ]]; then
			for ((i=1; i<${#1}; i++)); do
				if [[ $short_options =~ ${1:i:1} ]]; then
					if [[ $short_options =~ ${1:i:1}: ]]; then
						if [[ -n ${1:$i+1} ]]; then
							printf ' -%s' "${1:i:1}"
							printf " '%s'" "${1:$i+1}"
						else
							if [[ -n $2 ]]; then
								printf ' -%s' "${1:i:1}"
								shift
								printf " '%s'" "${1}"
							else
								printf "@SCRIPTNAME@: $(gettext "option %s requires an argument")" "'-${1:i:1}'" >&2
								ret=1
							fi
						fi
						break
					else
						printf ' -%s' "${1:i:1}"
					fi
				else
					echo "@SCRIPTNAME@: $(gettext "unrecognized option") '-${1:i:1}'" >&2
					ret=1
				fi
			done
		else
			unused_options="${unused_options} '$1'"
		fi
		shift
	done

	printf " --"
	[[ $unused_options ]] && printf ' %s' "${unused_options[@]}"
	[[ $1 ]] && printf " '%s'" "$@"
	printf "\n"

	return $ret
}