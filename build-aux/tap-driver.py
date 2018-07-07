#!/usr/bin/env python3
# Adapted from tappy copyright (c) 2016, Matt Layman
# MIT license
# https://github.com/python-tap/tappy

import io
import re
import subprocess
import sys


class Directive(object):
    """A representation of a result line directive."""

    skip_pattern = re.compile(
        r"""^SKIP\S*
            (?P<whitespace>\s*) # Optional whitespace.
            (?P<reason>.*)      # Slurp up the rest.""",
        re.IGNORECASE | re.VERBOSE)
    todo_pattern = re.compile(
        r"""^TODO\b             # The directive name
            (?P<whitespace>\s*) # Immediately following must be whitespace.
            (?P<reason>.*)      # Slurp up the rest.""",
        re.IGNORECASE | re.VERBOSE)

    def __init__(self, text):
        """Initialize the directive by parsing the text.
        The text is assumed to be everything after a '#\s*' on a result line.
        """
        self._text = text
        self._skip = False
        self._todo = False
        self._reason = None

        match = self.skip_pattern.match(text)
        if match:
            self._skip = True
            self._reason = match.group('reason')

        match = self.todo_pattern.match(text)
        if match:
            if match.group('whitespace'):
                self._todo = True
            else:
                # Catch the case where the directive has no descriptive text.
                if match.group('reason') == '':
                    self._todo = True
            self._reason = match.group('reason')

    @property
    def text(self):
        """Get the entire text."""
        return self._text

    @property
    def skip(self):
        """Check if the directive is a SKIP type."""
        return self._skip

    @property
    def todo(self):
        """Check if the directive is a TODO type."""
        return self._todo

    @property
    def reason(self):
        """Get the reason for the directive."""
        return self._reason


class Parser(object):
    """A parser for TAP files and lines."""

    # ok and not ok share most of the same characteristics.
    result_base = r"""
        \s*                    # Optional whitespace.
        (?P<number>\d*)        # Optional test number.
        \s*                    # Optional whitespace.
        (?P<description>[^#]*) # Optional description before #.
        \#?                    # Optional directive marker.
        \s*                    # Optional whitespace.
        (?P<directive>.*)      # Optional directive text.
    """
    ok = re.compile(r'^ok' + result_base, re.VERBOSE)
    not_ok = re.compile(r'^not\ ok' + result_base, re.VERBOSE)
    plan = re.compile(r"""
        ^1..(?P<expected>\d+) # Match the plan details.
        [^#]*                 # Consume any non-hash character to confirm only
                              # directives appear with the plan details.
        \#?                   # Optional directive marker.
        \s*                   # Optional whitespace.
        (?P<directive>.*)     # Optional directive text.
    """, re.VERBOSE)
    diagnostic = re.compile(r'^#')
    bail = re.compile(r"""
        ^Bail\ out!
        \s*            # Optional whitespace.
        (?P<reason>.*) # Optional reason.
    """, re.VERBOSE)
    version = re.compile(r'^TAP version (?P<version>\d+)$')

    TAP_MINIMUM_DECLARED_VERSION = 13

    def parse(self, fh):
        """Generate tap.line.Line objects, given a file-like object `fh`.
        `fh` may be any object that implements both the iterator and
        context management protocol (i.e. it can be used in both a
        "with" statement and a "for...in" statement.)
        Trailing whitespace and newline characters will be automatically
        stripped from the input lines.
        """
        with fh:
            for line in fh:
                yield self.parse_line(line.rstrip())

    def parse_line(self, text):
        """Parse a line into whatever TAP category it belongs."""
        match = self.ok.match(text)
        if match:
            return self._parse_result(True, match)

        match = self.not_ok.match(text)
        if match:
            return self._parse_result(False, match)

        if self.diagnostic.match(text):
            return ('diagnostic', text)

        match = self.plan.match(text)
        if match:
            return self._parse_plan(match)

        match = self.bail.match(text)
        if match:
            return ('bail', match.group('reason'))

        match = self.version.match(text)
        if match:
            return self._parse_version(match)

        return ('unknown',)

    def _parse_plan(self, match):
        """Parse a matching plan line."""
        expected_tests = int(match.group('expected'))
        directive = Directive(match.group('directive'))

        # Only SKIP directives are allowed in the plan.
        if directive.text and not directive.skip:
            return ('unknown',)

        return ('plan', expected_tests, directive)

    def _parse_result(self, ok, match):
        """Parse a matching result line into a result instance."""
        return ('result', ok, match.group('number'),
            match.group('description').strip(),
            Directive(match.group('directive')))

    def _parse_version(self, match):
        version = int(match.group('version'))
        if version < self.TAP_MINIMUM_DECLARED_VERSION:
            raise ValueError('It is an error to explicitly specify '
                             'any version lower than 13.')
        return ('version', version)


class Rules(object):

    def __init__(self):
        self._lines_seen = {'plan': [], 'test': 0, 'failed': 0, 'version': []}
        self._errors = []

    def check(self, final_line_count):
        """Check the status of all provided data and update the suite."""
        if self._lines_seen['version']:
            self._process_version_lines()
        self._process_plan_lines(final_line_count)

    def check_errors(self):
        if self._lines_seen['failed'] > 0:
            self._add_error('Tests failed.')
        if self._errors:
            for error in self._errors:
                print(error)
            return 1
        return 0

    def _process_version_lines(self):
        """Process version line rules."""
        if len(self._lines_seen['version']) > 1:
            self._add_error('Multiple version lines appeared.')
        elif self._lines_seen['version'][0] != 1:
            self._add_error('The version must be on the first line.')

    def _process_plan_lines(self, final_line_count):
        """Process plan line rules."""
        if not self._lines_seen['plan']:
            self._add_error('Missing a plan.')
            return

        if len(self._lines_seen['plan']) > 1:
            self._add_error('Only one plan line is permitted per file.')
            return

        expected_tests, at_line = self._lines_seen['plan'][0]
        if not self._plan_on_valid_line(at_line, final_line_count):
            self._add_error(
                'A plan must appear at the beginning or end of the file.')
            return

        if expected_tests != self._lines_seen['test']:
            self._add_error(
                'Expected {expected_count} tests '
                'but only {seen_count} ran.'.format(
                    expected_count=expected_tests,
                    seen_count=self._lines_seen['test']))

    def _plan_on_valid_line(self, at_line, final_line_count):
        """Check if a plan is on a valid line."""
        # Put the common cases first.
        if at_line == 1 or at_line == final_line_count:
            return True

        # The plan may only appear on line 2 if the version is at line 1.
        after_version = (
            self._lines_seen['version'] and
            self._lines_seen['version'][0] == 1 and
            at_line == 2)
        if after_version:
            return True

        return False

    def handle_bail(self, reason):
        """Handle a bail line."""
        self._add_error('Bailed: {reason}').format(reason=reason)

    def handle_skipping_plan(self):
        """Handle a plan that contains a SKIP directive."""
        sys.exit(77)

    def saw_plan(self, expected_tests, at_line):
        """Record when a plan line was seen."""
        self._lines_seen['plan'].append((expected_tests, at_line))

    def saw_test(self, ok):
        """Record when a test line was seen."""
        self._lines_seen['test'] += 1
        if not ok:
            self._lines_seen['failed'] += 1

    def saw_version_at(self, line_counter):
        """Record when a version line was seen."""
        self._lines_seen['version'].append(line_counter)

    def _add_error(self, message):
        self._errors += [message]


if __name__ == '__main__':
    parser = Parser()
    rules = Rules()

    try:
        out = subprocess.check_output(sys.argv[1:], universal_newlines=True)
    except subprocess.CalledProcessError as e:
        sys.stdout.write(e.output)
        raise e

    line_generator = parser.parse(io.StringIO(out))
    line_counter = 0
    for line in line_generator:
        line_counter += 1

        if line[0] == 'unknown':
            continue

        if line[0] == 'result':
            rules.saw_test(line[1])
            print('{okay} {num} {description} {directive}'.format(
                okay=('' if line[1] else 'not ') + 'ok', num=line[2],
                description=line[3], directive=line[4].text))
        elif line[0] == 'plan':
            if line[2].skip:
                rules.handle_skipping_plan()
            rules.saw_plan(line[1], line_counter)
        elif line[0] == 'bail':
            rules.handle_bail(line[1])
        elif line[0] == 'version':
            rules.saw_version_at(line_counter)
        elif line[0] == 'diagnostic':
            print(line[1])

    rules.check(line_counter)
    sys.exit(rules.check_errors())
