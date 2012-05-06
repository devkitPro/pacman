size_to_human() {
	awk -v size="$1" '
	BEGIN {
		suffix[1] = "B"
		suffix[2] = "KiB"
		suffix[3] = "MiB"
		suffix[4] = "GiB"
		suffix[5] = "TiB"
		count = 1

		while (size > 1024) {
			size /= 1024
			count++
		}

		sizestr = sprintf("%.2f", size)
		sub(/\.?0+$/, "", sizestr)
		printf("%s %s", sizestr, suffix[count])
	}'
}
