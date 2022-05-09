import os
import shutil

from quast_libs.qutils import check_dirpath
from quast_libs import qutils, qconfig, reporting
from quast_libs import contigs_analyzer
from quast import parse_options


from quast_libs.log import get_logger
logger = get_logger(qconfig.LOGGER_DEFAULT_NAME)
logger.set_up_console_handler()


def run_quast_funcs(args):
    check_dirpath(qconfig.QUAST_HOME, 'You are trying to run it from ' + str(qconfig.QUAST_HOME) + '\n.' +
                  'Please, put QUAST in a different directory, then try again.\n', exit_code=3)

    if not args:
        qconfig.usage(stream=sys.stderr)
        sys.exit(1)

    try:
        import importlib
        importlib.reload(qconfig)
        importlib.reload(qutils)
    except (ImportError, AttributeError):
        reload(qconfig)
        reload(qutils)

    try:
        locale.setlocale(locale.LC_ALL, 'en_US.utf8')
    except Exception:
        try:
            locale.setlocale(locale.LC_ALL, 'en_US.UTF-8')
        except Exception:
            print('Python locale settings can\'t be changed')
    quast_path = [__file__]
    quast_py_args, contigs_fpaths = parse_options(logger, quast_path + args)
    output_dirpath, ref_fpath, labels = qconfig.output_dirpath, qconfig.reference, qconfig.labels
    corrected_dirpath = os.path.join(output_dirpath, qconfig.corrected_dirname)

    ########################################################################
    from quast_libs import reporting
    reports = reporting.reports
    try:
        import importlib
        importlib.reload(reporting)
    except (ImportError, AttributeError):
        reload(reporting)
    reporting.reports = reports
    reporting.assembly_fpaths = []
    from quast_libs import plotter  # Do not remove this line! It would lead to a warning in matplotlib.

    if qconfig.is_combined_ref:
        corrected_dirpath = os.path.join(output_dirpath, '..', qconfig.corrected_dirname)
    else:
        if os.path.isdir(corrected_dirpath):
            shutil.rmtree(corrected_dirpath)
        os.mkdir(corrected_dirpath)

    # PROCESSING REFERENCE
    if ref_fpath:
        original_ref_fpath = ref_fpath
        ref_fpath = qutils.correct_reference(ref_fpath, corrected_dirpath)
        is_cyclic = qconfig.prokaryote and not qconfig.check_for_fragmented_ref
        if qconfig.optimal_assembly:
            if not qconfig.pacbio_reads and not qconfig.nanopore_reads and not qconfig.mate_pairs:
                print(
                    'Upper Bound Assembly cannot be created. It requires mate-pairs or long reads (Pacbio SMRT or Oxford Nanopore).')
            else:
                from quast_libs import optimal_assembly
                optimal_assembly_fpath = optimal_assembly.do(ref_fpath, original_ref_fpath,
                                                             os.path.join(output_dirpath,
                                                                          qconfig.optimal_assembly_basename))
                if optimal_assembly_fpath is not None:
                    contigs_fpaths.insert(0, optimal_assembly_fpath)
                    labels.insert(0, 'UpperBound')
                    labels = qutils.process_labels(contigs_fpaths, labels)
    else:
        ref_fpath = ''

    contigs_fpaths, old_contigs_fpaths = qutils.correct_contigs(contigs_fpaths, corrected_dirpath, labels,
                                                                reporting)

    from quast_libs import basic_stats
    icarus_gc_fpath, circos_gc_fpath = basic_stats.do(ref_fpath, contigs_fpaths,
                                                      os.path.join(output_dirpath, 'basic_stats'), output_dirpath)

    aligner_statuses, aligned_lengths_per_fpath = contigs_analyzer.do(
        ref_fpath, contigs_fpaths, is_cyclic,
        os.path.join(output_dirpath, qconfig.detailed_contigs_reports_dirname),
        old_contigs_fpaths, qconfig.bed)

    reports_fpaths, transposed_reports_fpaths = reporting.save_total(output_dirpath)
