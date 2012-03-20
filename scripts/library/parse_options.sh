# getopt like parser
parse_options() {
	local short_options=$1; shift;
	local long_options=$1; shift;
	local ret=0;
	local unused_options=()
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
					local needsargument=0

					[[ ${match} = ${1:2}: ]] && needsargument=1
					[[ ${match} = ${1:2}:: && -n $2 && ${2:0:1} != "-" ]] && needsargument=1

					if (( ! needsargument )); then
						OPTRET+=("$1")
					else
						if [[ -n $2 ]]; then
							OPTRET+=("$1" "$2")
							shift
							while [[ -n $2 && ${2:0:1} != "-" ]]; do
								shift
								OPTRET+=("$1")
							done
						else
							printf "@SCRIPTNAME@: $(gettext "option %s requires an argument\n")" "'$1'" >&2
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
					local needsargument=0

					[[ $short_options =~ ${1:i:1}: && ! $short_options =~ ${1:i:1}:: ]] && needsargument=1
					[[ $short_options =~ ${1:i:1}:: && \
						( -n ${1:$i+1} || ( -n $2 && ${2:0:1} != "-" ) ) ]] && needsargument=1

					if (( ! needsargument )); then
						OPTRET+=("-${1:i:1}")
					else
						if [[ -n ${1:$i+1} ]]; then
							OPTRET+=("-${1:i:1}" "${1:i+1}")
							while [[ -n $2 && ${2:0:1} != "-" ]]; do
								shift
								OPTRET+=("$1")
							done
						else
							if [[ -n $2 ]]; then
								OPTRET+=("-${1:i:1}" "$2")
								shift
								while [[ -n $2 && ${2:0:1} != "-" ]]; do
									shift
									OPTRET+=("$1")
								done

							else
								printf "@SCRIPTNAME@: $(gettext "option %s requires an argument\n")" "'-${1:i:1}'" >&2
								ret=1
							fi
						fi
						break
					fi
				else
					echo "@SCRIPTNAME@: $(gettext "unrecognized option") '-${1:i:1}'" >&2
					ret=1
				fi
			done
		else
			unused_options+=("$1")
		fi
		shift
	done

	OPTRET+=('--' "${unused_options[@]}")
	return $ret
}
