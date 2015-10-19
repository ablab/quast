
function buildReport() {
    var assembliesNames;
    var order;

    var totalReport = null;
    var qualities = null;
    var mainMetrics = null;
    var contigsLens = null;
    var coordNx = null;
    var contigsLensNx = null;
    var alignedContigsLens = null;
    var refLen = 0;
    var contigs = null;
    var genesInContigs = null;
    var operonsInContigs = null;
    var gcInfos = null;

    var glossary = JSON.parse($('#glossary-json').html());

    var plotsSwitchesDiv = document.getElementById('plots-switches');
    var toRemoveRefLabel = true;
    var firstPlot = true;

    /****************/
    /* Total report */

    if (!(totalReport = readJson('total-report'))) {
        console.log("Error: cannot read #total-report-json");
        return 1;
    }

    assembliesNames = totalReport.assembliesNames;

    order = recoverOrderFromCookies() || totalReport.order || Range(0, assembliesNames.length);

    buildTotalReport(assembliesNames, totalReport.report, order, totalReport.date,
        totalReport.minContig, glossary, qualities, mainMetrics);

    if (refLen = readJson('reference-length'))
        refLen = refLen.reflen;

    /****************/
    /* Plots        */

    while (assembliesNames.length > colors.length) {  // colors is defined in utils.js
        colors = colors.concat(colors);
    }

    $(plotsSwitchesDiv).html('<b>Plots:</b>');

    assembliesNames.forEach(function(filename, i) {
        var id = 'label_' + i + '_id';
        $('#legend-placeholder').append('<div>' +
            '<label for="' + id + '" style="color: ' + colors[i] + '">' +
            '<input type="checkbox" name="' + i + '" checked="checked" id="' + id + '">&nbsp;' + filename + '</label>' +
            '</div>');
    });

    var tickX = 1;
    if (tickX = readJson('tick-x'))
        tickX = tickX.tickX;

    if (contigsLens = readJson('contigs-lengths')) {
        makePlot(firstPlot, assembliesNames, order, 'cumulative', 'Cumulative length', cumulative.draw, contigsLens.lists_of_lengths, refLen, tickX);
        firstPlot = false;
    }

    if (coordNx = readJson('coord-nx')) {
        makePlot(firstPlot, assembliesNames, order, 'nx', 'Nx', nx.draw, {
                coord_x: coordNx.coord_x,
                coord_y: coordNx.coord_y,
                filenames: coordNx.filenames
            },
            null, null
        );
        firstPlot = false;
    }

    if (coordNx = readJson('coord-nax')) {
        makePlot(firstPlot, assembliesNames, order, 'nax', 'NAx', nx.draw, {
                coord_x: coordNx.coord_x,
                coord_y: coordNx.coord_y,
                filenames: coordNx.filenames
            },
            null, null
        );
        firstPlot = false;
    }

    if (coordNx = readJson('coord-ngx')) {
        makePlot(firstPlot, assembliesNames, order, 'ngx', 'NGx', nx.draw, {
                coord_x: coordNx.coord_x,
                coord_y: coordNx.coord_y,
                filenames: coordNx.filenames
            },
            null, null
        );
        firstPlot = false;
    }

    if (coordNx = readJson('coord-ngax')) {
        makePlot(firstPlot, assembliesNames, order, 'ngax', 'NGAx', nx.draw, {
                coord_x: coordNx.coord_x,
                coord_y: coordNx.coord_y,
                filenames: coordNx.filenames
            },
            null, null
        );
        firstPlot = false;
    }


    genesInContigs = readJson('genes-in-contigs');
    operonsInContigs = readJson('operons-in-contigs');
//    if (genesInContigs || operonsInContigs)
//        contigs = readJson('contigs');

    if (genesInContigs) {
        makePlot(firstPlot, assembliesNames, order, 'genes', 'Genes', gns.draw,  {
                filesFeatureInContigs: genesInContigs.genes_in_contigs,
                kind: 'gene',
                filenames: genesInContigs.filenames
            },
            genesInContigs.ref_genes_number, tickX
        );
        firstPlot = false;
    }
    if (operonsInContigs) {
        makePlot(firstPlot, assembliesNames, order, 'operons', 'Operons', gns.draw, {
                filesFeatureInContigs: operonsInContigs.operons_in_contigs,
                kind: 'operon',
                filenames: operonsInContigs.filenames
            },
            operonsInContigs.ref_operons_number, tickX
        );
        firstPlot = false;
    }

    if (gcInfos = readJson('gc')) {
        makePlot(firstPlot, assembliesNames, order, 'gc', 'GC content', gc.draw, gcInfos, null);
    }

    var noReference = true;
    var report = totalReport.report;
    for (var group_n = 0; group_n < report.length; group_n++) {
        var group = report[group_n];
        var groupName = group[0];
        if (groupName == 'Reference statistics' && group[1].length > 0) {
            noReference = false;
        }
    }
    if (noReference) extendAll();
    return 0;
}

function extendAll() {
    $('.row_to_hide').toggleClass('row_hidden');

    var link = $('#extended_report_link');
    link.hide();
}