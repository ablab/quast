import os
import shutil
import sys

from quast_libs import qconfig


class ReferenceFinder:
    def __init__(self, logger, ref_path):
        self.ref_path = ref_path
        self.logger = logger
        if qconfig.output_dirpath is None:
            self.output_dir = 'viralquast_results'
        else:
            self.output_dir = qconfig.output_dirpath
        try:
            if not os.path.exists(self.output_dir):
                os.mkdir(self.output_dir)
            else:
                shutil.rmtree(self.output_dir)
                os.mkdir(self.output_dir)
        except Exception as ex:
            self.logger.error('No access to output directory, exiting...')
            sys.exit(1)

    def find_reference(self, *args):
        pass