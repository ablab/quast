#!/usr/bin/env python
# coding: utf-8
"""
.. module:: Toolset
   :synopsis: the interface to OS enables to run executables / scripts
   in external processes
.. versionadded:: 3.0.0
.. versionchanged:: 3.0.0

Copyright (c) 2016-2017, Evgeny Zdobnov (ez@ezlab.org)
Licensed under the MIT license. See LICENSE.md file.

"""
import os
import subprocess
import threading
import time
from quast_libs.busco.pipebricks.PipeLogger import PipeLogger
# from overrides import overrides  # useful fro dev, but don't want all user to install this


class Job(threading.Thread):
    """
    Build and executes one work item in an external process
    """
    _logger = PipeLogger.get_logger(__name__)

    def __init__(self, tool_name, name, thread_id, logger):
        """
        :param name: a name of an executable / script ("a tool") to be run
        :type name: list
        :param thread_id: an int id for the thread
        :type thread_id: int
        """
        # initialize parent
        super(Job, self).__init__()

        self.tool_name = tool_name
        self.cmd_line = name
        self.thread_id = thread_id
        self.stdout_file = None
        self.stderr_file = None
        self.stdout = subprocess.PIPE
        self.stderr = subprocess.PIPE
        self._logger = logger

    def add_parameter(self, parameter):
        """
        Append parameter to the command line
        :parameter: a parameter
        :type parameter: str
        """
        self.cmd_line.append(parameter)

    # @override
    def run(self):
        """
        Start external process and block the current thread's execution
        till the process' run is over
        """
        if self.stdout_file:
            self.stdout = open(self.stdout_file[0], self.stdout_file[1])
        if self.stderr_file:
            self.stderr = open(self.stderr_file[0], self.stderr_file[1])
        process = subprocess.Popen(self.cmd_line, shell=False, stderr=self.stderr, stdout=self.stdout)
        logger = self._logger if self._logger else Job._logger
        logger.debug('%s thread nb %i has started' % (self.tool_name, self.thread_id))
        process.wait()
        process_out = []
        if process.stdout:
            process_out += process.stdout.readlines()
        if process.stderr:
            process_out += process.stderr.readlines()
        for line in process_out:
            logger.info_external_tool(self.tool_name, line.decode("utf-8").strip())
        if self.stdout_file:
            self.stdout.close()
        if self.stderr_file:
            self.stderr.close()


class ToolException(Exception):
    """
    Module-specific exception
    """
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return self.value


class Tool:
    """
    Collection of utility methods used by all tools
    """

    _logger = PipeLogger.get_logger(__name__)

    @staticmethod
    def check_tool_available(name, config, without_path=False):
        """
        Check tool's availability.
        1. The section ['name'] is available in the config
        2. This section contains keys 'path' and 'command'
        3. The string resulted from contatination of values of these two keys
        represents the full path to the command
        :param name: the name of the tool to execute
        :type name: str
        :param config: initialized instance of ConfigParser
        :type config: configparser.ConfigParser
        :param without_path: tells whether it has to be also available without the full path included
        :type without_path: boolean
        :return: True if the tool can be run, False if it is not the case
        :rtype: bool
        """
        if not config.has_section(name):
            raise ToolException('Section for the tool [\'%s\'] is not '
                                'present in the config file' % name)

        if not config.has_option(name, 'path'):
            raise ToolException('Key \'path\' in the section [\'%s\'] is not '
                                'present in the config file' % name)

        if without_path:
            cmd = name
            without_path_check = subprocess.call(
                'type %s' % cmd, shell=True, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE) == 0
        else:
            without_path_check = True

        cmd = os.path.join(config.get(name, 'path'), name)

        return without_path_check and subprocess.call(
            'type %s' % cmd, shell=True, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE) == 0

    def __init__(self, name, config):
        """
        Initialize job list for a tool
        :param name: the name of the tool to execute
        :type name: str
        :param config: initialized instance of ConfigParser
        :type config: configparser.ConfigParser
        """
        if not config.has_section(name):
            raise ToolException('Section for the tool [\'%s\'] is not '
                                'configured in the config.ini.default file' % name)

        if not config.has_option(name, 'path'):
            raise ToolException('Key \'path\' in the section [\'%s\'] is not '
                                'configured in the config.ini.default file' % name)

        self.name = name

        self.cmd = [os.path.join(config.get(name, 'path'), name)]

        keys = sorted(item[0] for item in config.items(name))

        for key in keys:
            if not key == 'path' and not config.has_option('DEFAULT', key):
                self.cmd.append(config.get(name, key))

        self.jobs_to_run = []
        self.jobs_running = []
        self._logger = None

    def create_job(self, logger=None):
        """
        Create one work item
        """
        job_id = 1 + len(self.jobs_to_run) + len(self.jobs_running)
        job = Job(self.name, self.cmd[:], job_id, logger)
        self.jobs_to_run.append(job)
        return job

    def remove_job(self, job):
        """
        Remove one work item
        :param job: the Job to remove
        :type job: Job
        """
        self.jobs_to_run.remove(job)

    def run_jobs(self, max_threads, logger=None, log_it=True):
        """
        This method run all jobs created for the Tool and redirect
        the standard output and error to the current logger
        :param max_threads: the number or threads to run simultaneously
        :type max_threads: int
        :param log_it: whether to log the progress for the tasks. Default True
        :type log_it: boolean
        """
        # Wait for all threads to finish and log progress
        total = len(self.jobs_to_run)
        already_logged = 0
        self._logger = logger or Tool._logger
        while len(self.jobs_to_run) > 0 or len(self.jobs_running) > 0:
            time.sleep(0.001)
            for j in self.jobs_to_run:
                if len(self.jobs_running) < max_threads:
                    self.jobs_running.append(j)
                    self.jobs_to_run.remove(j)
                    j.start()
                    self._logger.debug(j.cmd_line)
            for j in self.jobs_running:
                if not j.is_alive():
                    self.jobs_running.remove(j)

            nb_done = total - (len(self.jobs_to_run) + len(self.jobs_running))

            if (nb_done == total or int(nb_done % (float(total)/10)) == 0) and nb_done != already_logged:
                if log_it:
                    self._logger.info('[%s]\t%i of %i task(s) completed at %s' % (self.name, nb_done, total,
                                                                            time.strftime("%m/%d/%Y %H:%M:%S")))
                else:
                    self._logger.debug('[%s]\t%i of %i task(s) completed at %s' % (self.name, nb_done, total,
                                                                             time.strftime("%m/%d/%Y %H:%M:%S")))
                already_logged = nb_done
