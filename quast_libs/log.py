############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################
from __future__ import with_statement
import os
import sys
from datetime import datetime
from quast_libs import qconfig

import logging

_loggers = {}


def get_main_logger():
    if qconfig.LOGGER_META_NAME in _loggers.keys() and _loggers[qconfig.LOGGER_META_NAME]._logger.handlers:
        return _loggers[qconfig.LOGGER_META_NAME]
    return _loggers[qconfig.LOGGER_DEFAULT_NAME]


def get_logger(name):
    if name in _loggers.keys():
        return _loggers[name]
    else:
        _loggers[name] = QLogger(name)
        return _loggers[name]


class MetaQErrorFormatter(logging.Formatter):
    def __init__(self, indent_val=None, ref_name=None, log_fpath=None):
        self._indent_val = indent_val
        self._ref_name = ref_name
        self._log_fpath = log_fpath

        logging.Formatter.__init__(self)

    def format(self, record):
        if record.msg:
            record.msg = self._indent_val * '  ' + self._ref_name + ': ' + record.msg + '(details are in ' + self._log_fpath + ')'
        return record.msg


class QLogger(object):
    _logger = None  # logging.getLogger('quast')
    _name = ''
    _log_fpath = ''
    _start_time = None
    _indent_val = 0
    _num_notices = 0
    _num_warnings = 0
    _num_nf_errors = 0
    _is_metaquast = False
    _is_parallel_run = False

    def __init__(self, name):
        self._name = name
        self._logger = logging.getLogger(name)
        self._logger.setLevel(logging.DEBUG)

    def set_up_metaquast(self, is_parallel_run=False, ref_name=None):
        self._is_metaquast = True
        self._is_parallel_run = is_parallel_run
        self._ref_name = ref_name

    def set_up_console_handler(self, indent_val=0, debug=False):
        self._indent_val = indent_val

        for handler in self._logger.handlers:
            if isinstance(handler, logging.StreamHandler):
                self._logger.removeHandler(handler)

        console_handler = logging.StreamHandler(sys.stdout, )
        console_handler.setFormatter(logging.Formatter(indent_val * '  ' + '%(message)s'))
        console_handler.setLevel(logging.DEBUG if debug else logging.INFO)
        if self._is_parallel_run:
            console_handler.setFormatter(MetaQErrorFormatter(indent_val, self._ref_name, self._log_fpath))
            console_handler.setLevel(logging.ERROR)
        self._logger.addHandler(console_handler)

    def set_up_debug_level(self):
        for handler in self._logger.handlers:
            handler.setLevel(logging.DEBUG)

    def set_up_file_handler(self, output_dirpath, err_fpath=None):
        for handler in self._logger.handlers:
            if isinstance(handler, logging.FileHandler):
                self._logger.removeHandler(handler)

        self._log_fpath = os.path.join(output_dirpath, self._name + '.log')
        file_handler = logging.FileHandler(self._log_fpath, mode='w')
        file_handler.setLevel(logging.DEBUG)
        self._logger.addHandler(file_handler)

    def start(self):
        if self._indent_val == 0 and not self._is_metaquast:
            self._logger.info('')
            self.print_version()
            self._logger.info('')
            self.print_system_info()

        self._start_time = self.print_timestamp('Started: ')
        self._logger.info('')
        self._logger.info('Logging to ' + self._log_fpath)

    def finish_up(self, numbers=None, check_test=False):
        test_result = 0
        if not self._is_metaquast:
            self._logger.info('  Log is saved to ' + self._log_fpath)
            if qconfig.save_error:
                self._logger.info('  Errors are saved to ' + qconfig.error_log_fpath)

            finish_time = self.print_timestamp('Finished: ')
            self._logger.info('Elapsed time: ' + str(finish_time - self._start_time))
            if numbers:
                self.print_numbers_of_notifications(prefix="Total ", numbers=numbers)
            else:
                self.print_numbers_of_notifications()
            self._logger.info('\nThank you for using QUAST!')
            if check_test:
                if (numbers is not None and numbers[2] > 0) or self._num_nf_errors > 0:
                    self._logger.info('\nTEST FAILED! Please find non-fatal errors in the log and try to fix them.')
                    test_result = 1
                elif (numbers is not None and numbers[1] > 0) or self._num_warnings > 0:
                    self._logger.info('\nTEST PASSED with WARNINGS!')
                else:
                    self._logger.info('\nTEST PASSED!')

        for handler in self._logger.handlers:
            self._logger.removeHandler(handler)

        global _loggers
        del _loggers[self._name]
        return test_result

    def debug(self, message='', indent=''):
        self._logger.debug(indent + message)

    def info(self, message='', indent=''):
        if qconfig.silent:
            self._logger.debug(indent + message)
        else:
            self._logger.info(indent + message)

    # main_info always print in stdout
    def main_info(self, message='', indent=''):
        self._logger.info(indent + message)

    def info_to_file(self, message='', indent=''):
        for handler in list(self._logger.handlers):
            if isinstance(handler, logging.FileHandler):
                self._logger.removeHandler(handler)

        with open(self._log_fpath, 'a') as f:
            f.write(indent + message + '\n')

        file_handler = logging.FileHandler(self._log_fpath, mode='a')
        file_handler.setLevel(logging.DEBUG)

        self._logger.addHandler(file_handler)

    def notice(self, message='', indent=''):
        self._num_notices += 1
        self._logger.info(indent + ('NOTICE: ' + str(message) if message else ''))

    def warning(self, message='', indent=''):
        self._num_warnings += 1
        self._logger.warning(indent + ('WARNING: ' + str(message) if message else ''))

    def error(self, message='', exit_with_code=0, to_stderr=False, indent='', fake_if_nested_run=False):
        if fake_if_nested_run and self._indent_val > 0:
            self.info('')
            self.notice(message)
            return

        if message:
            msg = indent + 'ERROR! ' + str(message)
            if exit_with_code:
                msg += "\n\nIn case you have troubles running QUAST, you can write to quast.support@cab.spbu.ru\n" \
                       "or report an issue on our GitHub repository https://github.com/ablab/quast/issues\n" \
                       "Please provide us with quast.log file from the output directory."
        else:
            msg = ''

        if to_stderr or not self._logger.handlers:
            sys.stderr.write(msg + '\n')
        else:
            self._logger.error('')
            self._logger.error(msg)

        if qconfig.error_log_fpath:
            with open(qconfig.error_log_fpath, 'a') as f:
                f.write(msg)

        if exit_with_code:
            exit(exit_with_code)
        else:
            self._num_nf_errors += 1

    def exception(self, e, exit_code=0):
        # FIXME: special case: handling the known bug of macOS & Python3.8+ & joblib
        # see: https://github.com/ablab/quast/issues/175
        extra_message = ''
        if type(e) == TypeError and 'NoneType' in str(e) and 'int' in str(e):
            if qconfig.max_threads and qconfig.max_threads > 1 and qconfig.platform_name == 'macosx' and \
                    sys.version_info.major == 3 and sys.version_info.minor >= 8:
                extra_message = '\n\nThis seems to be a known bug when using multi-threading in Python 3.8+ on macOS!\n' \
                                'The current workarounds are\n' \
                                '  to switch to single-thread execution (-t 1)\n' \
                                'or\n' \
                                '  to downgrade your Python to 3.7 or below.\n' \
                                'Sorry for the inconvenience!\n' \
                                'Please find more details in https://github.com/ablab/quast/issues/175\n'

        if self._logger.handlers:
            self._logger.error('')
            self._logger.exception(e)
            if extra_message:
                self._logger.info(extra_message)
        else:
            sys.stderr.write(str(e) + '\n')
            if extra_message:
                sys.stderr.write(extra_message + '\n')

        if exit_code:
            exit(exit_code)

    def print_command_line(self, args, indent='',
                           wrap_after=80, only_if_debug=False, is_main=False):
        if only_if_debug:
            out = self.debug
        elif is_main:
            out = self.main_info
        else:
            out = self.info

        text = ''
        line = indent

        for i, arg in enumerate(args):
            if ' ' in arg or '\t' in arg:
                args[i] = "'" + arg + "'"

            line += arg

            if i == len(args) - 1:
                text += line

            elif wrap_after is not None and len(line) > wrap_after:
                text += line + ' \\\n'
                line = ' ' * len(indent)

            else:
                line += ' '

        out(text)

    def print_params(self, indent='',
                           wrap_after=80, only_if_debug=False):
        self._logger.info("CWD: " + os.getcwd())
        self._logger.info("Main parameters: ")
        text = '  '
        line = indent
        options = [('MODE', qconfig.get_mode()),
                   ('threads', qconfig.max_threads), ('eukaryotic', not qconfig.prokaryote),
                   ('split scaffolds', qconfig.split_scaffolds), ('min contig length', qconfig.min_contig),
                   ('min alignment length', qconfig.min_alignment), ('min alignment IDY', qconfig.min_IDY),
                   ('ambiguity', qconfig.ambiguity_usage), ('use all alignments', qconfig.use_all_alignments),
                   ('min local misassembly length', qconfig.local_misassembly_min_length),
                   ('min extensive misassembly length', qconfig.extensive_misassembly_threshold)]
        for i, (option, value) in enumerate(options):
            if value is not False:
                line += option + ': ' + str(value).lower()

                if i == len(options) - 1:
                    text += line

                elif wrap_after is not None and len(line) > wrap_after:
                    text += line + ', \\\n'
                    line = ' ' * len(indent) + '  '

                else:
                    line += ', '

        self._logger.info(text)

    def print_timestamp(self, message=''):
        now = datetime.now()
        current_time = now.strftime("%Y-%m-%d %H:%M:%S")
        self.main_info('')
        self.main_info(message + current_time)
        return now

    def print_version(self, to_stderr=False):
        if to_stderr:
            sys.stderr.write("Version: " + qconfig.quast_version() + '\n')
        else:
            self.info("Version: " + qconfig.quast_version())

    def print_system_info(self):
        self._logger.info("System information:")
        import platform
        self._logger.info("  OS: " + platform.platform() + " (%s)" % qconfig.platform_name)
        self._logger.info("  Python version: " + str(sys.version_info[0]) + "." + str(sys.version_info[1]) + '.'\
                  + str(sys.version_info[2]))
        try:
            import multiprocessing
            self._logger.info("  CPUs number: " + str(multiprocessing.cpu_count()))
        except ImportError:
            self._logger.info("  Problem occurred when getting CPUs number information")

    def print_numbers_of_notifications(self, prefix="", numbers=None):
        if not numbers:
            numbers = (self._num_notices, self._num_warnings, self._num_nf_errors)
        self._logger.info(prefix + "NOTICEs: %d; WARNINGs: %d; non-fatal ERRORs: %d" %
                          numbers)

    def get_numbers_of_notifications(self):
        return (self._num_notices, self._num_warnings, self._num_nf_errors)

