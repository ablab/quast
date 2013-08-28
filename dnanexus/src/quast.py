#!/usr/bin/env python
# quast 2.2.0
#
# Basic execution pattern: this app will run on a single machine from
# beginning to end.
#
# DNAnexus Python Bindings (dxpy) documentation:
#   http://autodoc.dnanexus.com/bindings/python/current/

import dxpy
import subprocess
import os


@dxpy.entry_point('main')
def main(contigs, reference=None, genes=None, operons=None, min_contig=None, contig_thresholds=None, 
    gene_finding=None, gene_thresholds=None, eukaryote=None, est_ref_length=None, scaffolds=None):

    # The following line(s) initialize your data object inputs on the platform
    # into dxpy.DXDataObject instances that you can start using immediately.

    if reference:
        reference = dxpy.DXFile(reference)
    if genes:
        genes = dxpy.DXFile(genes)
    if operons:
        operons = dxpy.DXFile(operons)

    contigs = [dxpy.DXFile(item) for item in contigs]

    if contig_thresholds:
        contig_thresholds = ','.join(str(t) for t in contig_thresholds)
    if gene_thresholds:
        gene_thresholds = ','.join(str(t) for t in gene_thresholds)

    # The following line(s) download your file inputs to the local file system
    # using variable names for the filenames.

    if reference:
        dxpy.download_dxfile(reference.get_id(), reference.describe()["name"])
    if genes:
        dxpy.download_dxfile(genes.get_id(), genes.describe()["name"])
    if operons:
        dxpy.download_dxfile(reference.get_id(), operons.describe()["name"])

    contig_names = dict()
    for i in range(len(contigs)):
        base_name, base_ext = os.path.splitext(contigs[i].describe()["name"])
        name = base_name
        suffix_id = 1
        while name in contig_names.values():
            suffix_id += 1
            name = base_name + "__" + str(suffix_id)
        name += base_ext
        contig_names[contigs[i].get_id()] = name
        dxpy.download_dxfile(contigs[i].get_id(), contig_names[contigs[i].get_id()])

    command_line = "quast.py -o output"
    if reference:
        command_line += " -R " + reference.describe()["name"]
    if genes:
        command_line += " -G " + genes.describe()["name"]
    if operons:
        command_line += " -O " + operons.describe()["name"]
    for i in range(len(contigs)):
        command_line +=  " " + contig_names[contigs[i].get_id()]
    if min_contig:
        command_line += " --min-contig " + str(min_contig)
    if contig_thresholds:
        command_line += " --contig-thresholds " + contig_thresholds
    else:
        command_line += " --contig-thresholds None"
    if gene_finding:
        command_line += " --gene-finding"
    if gene_thresholds:
        command_line += " --gene-thresholds " + gene_thresholds
    else:
        command_line += " --gene-thresholds None"
    if eukaryote:
        command_line += " --eukaryote"
    if est_ref_length != None:
        command_line += " --est-ref-size " + str(est_ref_length)
    if scaffolds:
        command_line += " --scaffolds"

    subprocess.call(command_line, shell=True)
        
    # output HTML report
    sub_output = json.loads(subprocess.check_output("dx-build-report-html -r /report output/report.html", shell=True))
    output = {}
    output["report"] = dxpy.dxlink(sub_output["recordId"])
        
    return output

dxpy.run()
