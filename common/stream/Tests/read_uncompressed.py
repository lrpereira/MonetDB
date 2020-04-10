#!/usr/bin/env python3

import read_tests
import sys

filter = lambda f: f.endswith('.txt')

if read_tests.all_tests(filter) != 0:
	sys.exit(1)
