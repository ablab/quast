#!/usr/bin/env python
# coding: utf-8
"""
.. module:: PipeLogger
   :synopsis: base logger customization for the analysis pipeline
.. versionadded:: 3.0.0
.. versionchanged:: 3.0.0

This is a logger for the pipeline that extends the default Python logger class

Copyright (c) 2016-2017, Evgeny Zdobnov (ez@ezlab.org)
Licensed under the MIT license. See LICENSE.md file.

"""

import logging
import sys
# from overrides import overrides  # useful fro dev, but don't want all user to install this
import os

try:
    from configparser import NoOptionError
    from configparser import NoSectionError
except ImportError:
    from ConfigParser import NoOptionError  # Python 2.7
    from ConfigParser import NoSectionError  # Python 2.7

run_dirpath = None

class PipeLogger(logging.getLoggerClass()):
    """
    This class customizes the _logger class
    """

    _level = logging.INFO
    _has_warning = False

    @staticmethod
    def get_logger(name, config=None):
        """
        :param name: the name of the logger to be returned
        :type name: str
        :param config: the parameters of the analysis
        :type config: PipeConfig
        :return: a PipeLogger, new or existing, corresponding to the provided name
        :rtype: PipeLogger
        """
        try:
            if config and config.getboolean('busco', 'quiet'):
                PipeLogger._level = logging.ERROR
            elif config and config.getboolean('busco', 'debug'):
                PipeLogger._level = logging.DEBUG
        except NoOptionError:
            pass
        except NoSectionError:
            pass

        logging.setLoggerClass(PipeLogger)
        return logging.getLogger(name)

    def __init__(self, name):
        """
        :param name: the name of the PipeLogger instance to be created
        :type name: str
        """
        super(PipeLogger, self).__init__(name)
        self.setLevel(PipeLogger._level)
        self._formatter = logging.Formatter('%(levelname)s\t%(message)s')
        self._thread_formatter = logging.Formatter('%(levelname)s:'
                                                   '%(threadName)'
                                                   's\t%(message)s')
        if run_dirpath:
            self.reload_log()
        else:
            self._out_hdlr = logging.StreamHandler(sys.stdout)
            self._out_hdlr.setFormatter(self._formatter)
            self.addHandler(self._out_hdlr)

    def reload_log(self):
        _log_fpath = run_dirpath + '.log'
        for handler in self.handlers:
            self.removeHandler(handler)
        file_handler = logging.FileHandler(_log_fpath, mode='w')
        file_handler.setLevel(logging.DEBUG)
        self.addHandler(file_handler)

    def add_thread_info(self):
        """
        This function appends the thread name to the logs output,
        e.g. INFO:Analysis.py:thread_name
        """
        self._out_hdlr.setFormatter(self._thread_formatter)

    def remove_thread_info(self):
        """
        This function disables the thread name in the logs output,
        e.g. INFO:Analysis.py
        """
        self._out_hdlr.setFormatter(self._formatter)

    # @override
    def warn(self, msg, *args, **kwargs):
        """
        This function overrides the _logger class warn
        :param msg: the message to log
        :type msg: str
        """
        self.warning(msg, *args, **kwargs)

    # @override
    def warning(self, msg, *args, **kwargs):
        """
        This function overrides the _logger class warning
        :param msg: the message to log
        :type msg: str
        """
        PipeLogger._has_warning = True
        super(PipeLogger, self).warning(msg, *args, **kwargs)

    @staticmethod
    def has_warning():
        """
        :return: whether any _logger did log warnings
        :rtype: boolean
        """
        return PipeLogger._has_warning

    @staticmethod
    def reset_warning():
        """
        Reset the has warning flag to False for all _logger
        """
        PipeLogger._has_warning = False

    def info_external_tool(self, tool, msg, *args, **kwargs):
        """
        This function logs an info line mentioning this is an external tool
        :param tool: the name of the tool
        :type tool: str
        :param msg: the message
        :type msg: str
        :return:
        """
        if msg != '':  # do not log blank lines
            self.info('[%s]\t%s' % (tool, msg), *args, **kwargs)
