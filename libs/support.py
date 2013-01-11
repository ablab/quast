#!/usr/bin/env python

import sys

# Based on http://stackoverflow.com/a/616686/92396
class Tee(object):
    def __init__(self, name, mode, console=True):
        self.file = open(name, mode)
        self.stdout = sys.stdout
        self.stderr = sys.stderr
        self.console = console
        sys.stdout = self
        sys.stderr = self

    def free(self):
        sys.stdout = self.stdout
        sys.stderr = self.stderr
        self.file.close()

    def write(self, data):
        self.file.write(data)
        if self.console:
            self.stdout.write(data)
        self.flush()

    def flush(self):
        self.file.flush()
        self.stdout.flush()