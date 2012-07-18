
function buildReport() {
    var report = null;
    var contigsLengths = null;
    var alignedContigsLengths = null;
    var referenceLength = null;
    var contigs = null;
    var genes = null;
    var operons = null;

    try {
        report = JSON.parse($('#report-json').html());
    } catch (e) {
        report = null;
    }

    if (report) {
        document.title += ('of ' + report.date);
        $('#subheader').append('of ' + report.date);
        buildTotalReport(report);
    }

    report = null;

    try {
        contigsLengths = JSON.parse($('#contigs-lengths-json').html());
    } catch (e) {
        contigsLengths = null;
    }

    if (contigsLengths) {
        drawCommulativePlot(contigsLengths.filenames, contigsLengths.lists_of_lengths, $('#commulative-plot-div'), null);
        drawNxPlot(contigsLengths.filenames, contigsLengths.lists_of_lengths, 'Nx', null, $('#nx-plot-div'), null);
    }

    try {
        alignedContigsLengths = JSON.parse($('#aligned-contigs-lengths-json').html());
    } catch (e) {
        alignedContigsLengths = null;
    }

    if (alignedContigsLengths) {
        drawNxPlot(alignedContigsLengths.filenames, alignedContigsLengths.lists_of_lengths, 'NAx', null, $('#nax-plot-div'), null);
    }

    try {
        referenceLength = JSON.parse($('#reference-length-json').html()).reflen;
    } catch (e) {
        referenceLength = null;
    }

    if (contigsLengths && referenceLength) {
        drawNxPlot(contigsLengths.filenames, contigsLengths.lists_of_lengths, 'NGx',referenceLength, $('#ngx-plot-div'), null);
    }
    if (alignedContigsLengths && referenceLength) {
        drawNxPlot(alignedContigsLengths.filenames, alignedContigsLengths.lists_of_lengths, 'NGAx', referenceLength, $('#ngax-plot-div'), null);
    }

    contigsLengths = null;
    alignedContigsLengths = null;
    referenceLength = null;

    try {
        genes = JSON.parse($('#genes-json').html());
    } catch (e) {
        genes = null;
    }
    try {
        operons = JSON.parse($('#operons-json').html());
    } catch (e) {
        operons = null;
    }
    if (genes || operons) {
        try {
            contigs = JSON.parse($('#contigs-json').html());
        } catch (e) {
            contigs = null;
        }
    }

    if (contigs) {
        if (genes) {
            drawGenesPlot(contigs.filenames, contigs.contigs, genes.genes, genes.found, 'gene', $('#genes-plot-div'), null);
        }
        if (operons) {
            drawGenesPlot(contigs.filenames, contigs.contigs, operons.operons, operons.found, 'operon', $('#operons-plot-div'), null);
        }
    }

    contigs = null;
    genes = null;
    operons = null;
}