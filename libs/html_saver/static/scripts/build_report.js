
function buildReport() {
    var report = null;
    var contigsLengths = null;
    var alignedContigsLengths = null;
    var referenceLength = null;
    var genesInContigs = null;
    var operonsInContigs = null;
    var gcInfos = null;

    var glossary = JSON.parse($('#glossary-json').html());

    try {
        referenceLength = JSON.parse($('#reference-length-json').html()).reflen;
    } catch (e) {
        referenceLength = null;
    }

    try {
        report = JSON.parse($('#report-json').html());
    } catch (e) {
        report = null;
    }

    if (report) {
        document.title += (report.date);
        $('#subheader').html(report.date + '.');
        $('#mincontig').append('Contigs of length ≥ ' + report.min_contig + ' bp are used.');
        buildTotalReport(report, glossary);
    }

    report = null;

    try {
        contigsLengths = JSON.parse($('#contigs-lengths-json').html());
    } catch (e) {
        contigsLengths = null;
    }

    if (contigsLengths) {
        drawCumulativePlot(contigsLengths.filenames, contigsLengths.lists_of_lengths, referenceLength, $('#cumulative-plot-div'), null,  glossary);
        drawNxPlot(contigsLengths.filenames, contigsLengths.lists_of_lengths, 'Nx', null, $('#nx-plot-div'), null,  glossary);
    }

    try {
        alignedContigsLengths = JSON.parse($('#aligned-contigs-lengths-json').html());
    } catch (e) {
        alignedContigsLengths = null;
    }

    if (alignedContigsLengths) {
        drawNxPlot(alignedContigsLengths.filenames, alignedContigsLengths.lists_of_lengths, 'NAx', null, $('#nax-plot-div'), null,  glossary);
    }

    if (contigsLengths && referenceLength) {
        drawNxPlot(contigsLengths.filenames, contigsLengths.lists_of_lengths, 'NGx',referenceLength, $('#ngx-plot-div'), null,  glossary);
    }
    if (alignedContigsLengths && referenceLength) {
        drawNxPlot(alignedContigsLengths.filenames, alignedContigsLengths.lists_of_lengths, 'NGAx', referenceLength, $('#ngax-plot-div'), null,  glossary);
    }

    contigsLengths = null;
    alignedContigsLengths = null;
    referenceLength = null;

    try {
        genesInContigs = JSON.parse($('#genes-in-contigs-json').html());
    } catch (e) {
        genesInContigs = null;
    }

    try {
        operonsInContigs = JSON.parse($('#operons-in-contigs-json').html());
    } catch (e) {
        operonsInContigs = null;
    }

    if (genesInContigs) {
        drawGenesPlot(genesInContigs.filenames, genesInContigs.genes_in_contigs, 'gene', $('#genes-plot-div'), null,  glossary);
    }
    if (operonsInContigs) {
        drawGenesPlot(operonsInContigs.filenames, operonsInContigs.operons_in_contigs, 'operon', $('#operons-plot-div'), null,  glossary);
    }

    genesInContigs = null;
    operonsInContigs = null;

    try {
        gcInfos = JSON.parse($('#gc-json').html());
    } catch (e) {
        gcInfos = null;
    }

    if (gcInfos) {
        drawGCPlot(gcInfos.filenames, gcInfos.lists_of_gc_info, $('#gc-plot-div'), null, glossary);
    }

    gcInfos = null;
}