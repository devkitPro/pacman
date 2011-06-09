msg() {
	(( QUIET )) && return
	local mesg=$1; shift
	printf "==> ${mesg}\n" "$@" >&1
}

msg2() {
	(( QUIET )) && return
	local mesg=$1; shift
	printf "  -> ${mesg}\n" "$@" >&1
}

warning() {
	local mesg=$1; shift
	printf "==> $(gettext "WARNING:") ${mesg}\n" "$@" >&2
}

error() {
	local mesg=$1; shift
	printf "==> $(gettext "ERROR:") ${mesg}\n" "$@" >&2
}