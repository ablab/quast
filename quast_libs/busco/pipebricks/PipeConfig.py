#!/usr/bin/env python
# coding: utf-8
"""
.. module:: ParseConfig
   :synopsis: parses config files 
.. versionadded:: 3.0.0
.. versionchanged:: 3.0.0

Copyright (c) 2016-2017, Evgeny Zdobnov (ez@ezlab.org)
Licensed under the MIT license. See LICENSE.md file.

"""

try:
    from configparser import ConfigParser
except ImportError:
    from ConfigParser import RawConfigParser as ConfigParser  # Python 2.7


class PipeConfig(ConfigParser):
    """
    This class parses config files
    """

    def __init__(self, conf_file):
        try:
            super(PipeConfig, self).__init__()
        except TypeError:
            ConfigParser.__init__(self)  # Python 2.7

        self.conf_file = conf_file

        try:
            self.readfp(open(self.conf_file), 'r')  # deprecated but kept for 2.7
        except IOError as Err:
            if Err.errno == 2:
                pass
            else:
                raise Err
