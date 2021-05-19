# QUAST Coding Guidelines

## Project Structure
Below are the key project directories and their content.

* __root directory__ (`.`)
	* main executable files such as `quast.py` and `metaquast.py`
	* documentation files such as `LICENSE.txt` and `manual.html`
	* installation scripts such as `install.sh` and `setup.py`

* __quast\_libs__ - all essential QUAST modules for assembly preprocessing, metrics computation and reporting. The largest modules have their own subdirectories inside, e.g., `ca_utils` for contig analysis.
	* `qconfig.py` - storage of all constants, global variables, and option default values. This file also defines QUAST help message
	* `qutils.py` - common functions used across various modules
	* `log.py` - logging functions
	* `reporting.py` - defines the content and order of metrics in all reports (text, PDF and HTML)

* __external\_tools__ - third-party tools used in the QUAST pipeline such as Red (repeat detector), KMC (k-mer counter). Note that the most essential external tools, such as minimap2 (contig aligner) or GeneMark (gene predictor) are stored under `quast_libs`.
	
* __test\_data__ - sample test files such as a reference file, assemblies, genes annotation, etc


## Coding Style/Conventions
* The code mainly follows the standard [PEP8 Style Guide for Python Code](https://www.python.org/dev/peps/pep-0008/). In particular, this includes using only 4 spaces for indentation, and never tabs.
* QUAST code is currently compatible with Python 2.5-2.7 and Python 3.5+. The support for Python 2.5-2.6 will be dropped in the next major release. Python 2.7 support will also be deprecated in the near future.

## Documentation
* Please use inline comments to describe non-trivial code. An even better strategy would be designing your code to comment itself.
* New functionality and quality metrics should be described in the [manual](manual.html).
* It is also essential to add a brief description of new metrics to the QUAST glossary in `quast_libs/html_saver/glossary.json`. The glossary content is appended to all HTML reports and is used for tooltips.

## Tests and Test Data
* Key test data is located in `test_data` and is used with standard QUAST test commands `./quast.py --test`, `./metaquast.py --test` and more advanced `./quast.py --test-sv`, `./metaquast.py --test-no-ref`. Please check that your modifications to the code do not break these essential tests.
* More test data and test scripts are located in `tc_tests/data/`. To comprehensively test your changes, please run `./tc_tests/test_everything.sh`. Ideally, it should be several runs with different Python versions, e.g. `./tc_tests/test_everything.sh python2` and `./tc_tests/test_everything.sh python3` if `python2` and `python3` are valid Python interpreters in your system. This notice is valid until we completely abandon Python2 support.

## Logging
Please avoid using `print()` for printing messages, warnings and errors. Instead use the QUAST logger like the following.

```
from quast_libs.log import get_logger

logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
logger.info('Sample info message')
logger.warning('Sample warning')
logger.error('Non-critical error')
logger.error('Critical error', exit_with_code=1)
```

## Recommendations
* We recommend using [PyCharm](https://www.jetbrains.com/pycharm/) for the QUAST development. In particular, this IDE supports code reformating to matching the PEP8 standard and also helps to maintain simultaneous Python2 and Python3 compatibility.

## Questions and Suggestions
Please post all your questions regarding these Guidelines and suggestions to its improvement to [our public discussion forum](https://github.com/ablab/quast/discussions). You may also write a private email to <quast.support@cab.spbu.ru>. 



