human_to_size() {
  awk -v human="$1" '
  function trim(s) {
    gsub(/^[[:space:]]+|[[:space:]]+$/, "", s)
    return s
  }

  function parse_units(units) {
    if (!units || units == "B")
      return 1
    if (match(units, /^.iB$/))
      return 1024
    if (match(units, /^.B$/))
      return 1000
    if (length(units) == 1)
      return 1024

    # parse failure: invalid base
    return -1
  }

  function parse_scale(s) {
    return index("BKMGTPE", s) - 1
  }

  function isnumeric(string) {
    return match(string, /^[-+]?[[:digit:]]*(\.[[:digit:]]*)?/)
  }

  BEGIN {
    # peel off the leading number as the size, fail on invalid number
    human = trim(human)
    if (isnumeric(human))
      size = substr(human, RSTART, RLENGTH)
    else
      exit 1

    # the trimmed remainder is assumed to be the units
    units = trim(substr(human, RLENGTH + 1))

    base = parse_units(units)
    if (base < 0)
      exit 1

    scale = parse_scale(substr(units, 1, 1))
    if (scale < 0)
      exit 1

    printf "%d\n", size * base^scale + (size + 0 > 0 ? 0.5 : -0.5)
  }'
}
