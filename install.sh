#!/bin/bash
echo 'Starting QUAST...'
$(dirname '$0')/quast.py --test
return_code=$?
if [ $return_code -ne 0 ]; then
   echo 'ERROR! QUAST TEST FAILED!'
fi
echo 'Starting MetaQUAST...'
$(dirname "$0")/metaquast.py --test-no-ref
return_code=$?
if [ $return_code -ne 0 ]; then
   echo 'ERROR! METAQUAST TEST FAILED!'
fi
echo 'QUAST INSTALLED SUCCESSFULLY!'
