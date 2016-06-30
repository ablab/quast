#!/usr/bin/env python
"""
QUAST evaluates genome assemblies.
It works both with and without references genome.
The tool accepts multiple assemblies, thus is suitable for comparison.
"""
import os

from setuptools import setup, find_packages

version = open('VERSION.txt').read().strip()

print("""-----------------------------------
 Installing QUAST version {}
-----------------------------------
""".format(version))

setup(
    name='quast',
    version=version,
    author='Alexei Gurevich',
    author_email='alexeigurevich@gmail.com',
    description="Genome assembly evaluation toolkit",
    long_description=__doc__,
    keywords=['bioinformatics', 'genome assembly', 'metagenome assembly'],
    url='quast.sf.net',
    download_url='quast.sf.net',
    license='GPLv2',

    packages=['libs'],
    package_dir={'libs': 'libs'},
    package_data={
        'libs': ['libs/*']
    },
    include_package_data=True,

    zip_safe=False,
    scripts=['quast.py', 'metaquast.py', 'icarus.py'],
    data_files={
        '': [
            '*.txt',
            '*.md',
            '*.html',
            '*.bib',
            'test_data',
            '*.js',
            '*.css',
            '*.png',
            '*.pxm',
            '*.json',
        ],
        # 'libs/html_saver/static': [
        #     'libs/html_saver/static',
        # ],
    },
    install_requires=[
        'matplotlib',
        'joblib',
        'simplejson',
    ],
    classifiers=[
        'Environment :: Console',
        'Environment :: Web Environment',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: GNU General Public License v2 (GPLv2)',
        'Natural Language :: English',
        'Operating System :: MacOS :: MacOS X',
        'Operating System :: POSIX',
        'Operating System :: Unix',
        'Programming Language :: Python',
        'Programming Language :: JavaScript',
        'Topic :: Scientific/Engineering',
        'Topic :: Scientific/Engineering :: Bio-Informatics',
        'Topic :: Scientific/Engineering :: Visualization',
    ],
)

print("""
--------------------------------
 QUAST installation complete!
--------------------------------
For help in running QUAST, please see the documentation available
at quast.bioinf.spbau.ru/manual.html or run: quast --help
""")
