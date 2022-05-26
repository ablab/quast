#!/bin/bash

############################################################################
# Copyright (c) 2015-2022 Saint Petersburg State University
# Copyright (c) 2011-2015 Saint Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

# installs general QUAST pipeline, general MetaQUAST pipeline,
# and MetaQUAST for de novo datasets (without references)

quast_home=$(dirname "$0")

stdout_log_fname=$quast_home/install_log.stdout
echo "Starting QUAST test... (stdout redirected to $stdout_log_fname)"
echo "" > $stdout_log_fname
echo "Starting QUAST test" >> $stdout_log_fname
$quast_home/quast.py --test >> $stdout_log_fname
return_code=$?
if [ $return_code -ne 0 ]; then
   echo 'ERROR! QUAST TEST FAILED!'
   exit 1
fi
echo "Starting MetaQUAST test... (stdout redirected to $stdout_log_fname)"
echo "" >> $stdout_log_fname
echo "Starting MetaQUAST test" >> $stdout_log_fname
$quast_home/metaquast.py --test >> $stdout_log_fname
return_code=$?
if [ $return_code -ne 0 ]; then
   echo 'ERROR! METAQUAST TEST FAILED!'
   exit 1
fi
echo "Starting QUAST test with structural variants detection... (stdout redirected to $stdout_log_fname)"
echo "" >> $stdout_log_fname
echo "Starting QUAST test with structural variants detection" >> $stdout_log_fname
$quast_home/quast.py --test-sv >> $stdout_log_fname
return_code=$?
if [ $return_code -ne 0 ]; then
   echo 'ERROR! QUAST TEST WITH STRUCTURAL VARIANTS DETECTION FAILED!'
   echo 'However, the lightweight version of QUAST was installed successfully!'
   exit 1
fi
echo "Starting MetaQUAST test without references... (stdout redirected to $stdout_log_fname)"
echo "" >> $stdout_log_fname
echo "Starting MetaQUAST test without references" >> $stdout_log_fname
$quast_home/metaquast.py --test-no-ref --fast >> $stdout_log_fname
return_code=$?
if [ $return_code -ne 0 ]; then
   echo 'ERROR! METAQUAST TEST WITHOUT REFERENCES FAILED!'
   echo 'However, the lightweight version of QUAST was installed successfully!'
   exit 1
fi
echo 'QUAST INSTALLED SUCCESSFULLY!'
exit 0
