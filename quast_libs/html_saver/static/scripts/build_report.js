
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
    var refLengths = null;
    var contigs = null;
    var featuresInContigs = null;
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

    buildTotalReport(assembliesNames, totalReport, order, glossary, qualities, mainMetrics);

    if (refLengths = readJson('reference-length'))
        refLengths = refLengths.reflen;

    /****************/
    /* Plots        */

    while (assembliesNames.length > colors.length) {  // colors is defined in utils.js
        colors = colors.concat(colors);
    }

    $(plotsSwitchesDiv).html('<b>Plots:</b>');

    var selectedAssemblies = Array.apply(null, {length: assembliesNames}).map(Number.call, Number);
    assembliesNames.forEach(function(filename, i) {
        addLabelToLegend(i, filename, selectedAssemblies, colors)
    });

    var tickX = 1;
    if (tickX = readJson('tick-x'))
        tickX = tickX.tickX;

    if (contigsLens = readJson('contigs-lengths')) {
        makePlot(firstPlot, assembliesNames, order, 'cumulative', 'Cumulative length', cumulative.draw, contigsLens.lists_of_lengths, refLengths, tickX);
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

    if (coordMisassemblies = readJson('coord-misassemblies')) {
        makePlot(firstPlot, assembliesNames, order, 'misassemblies', 'Misassemblies', frc.draw,  {
                coord_x: coordMisassemblies.coord_x,
                coord_y: coordMisassemblies.coord_y,
                filenames: coordMisassemblies.filenames
            },
            null, 1
        );
        firstPlot = false;
    }

    featuresInContigs = readJson('features-in-contigs');
    operonsInContigs = readJson('operons-in-contigs');
//    if (genesInContigs || operonsInContigs)
//        contigs = readJson('contigs');

    if (featuresInContigs) {
        makePlot(firstPlot, assembliesNames, order, 'features', 'Genomic features', gns.draw,  {
                filesFeatureInContigs: featuresInContigs.features_in_contigs,
                kind: 'gene',
                filenames: featuresInContigs.filenames
            },
            featuresInContigs.ref_features_number, tickX
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
    gcInfos = readJson('gc');
    if (gcInfos && (gcInfos.lists_of_gc_info || gcInfos.list_of_GC_distributions)) {
        makePlot(firstPlot, assembliesNames, order, 'gc', 'GC content', gc.draw, gcInfos, gcInfos.reference_index);
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
    appendIcarusLinks();
    return 0;
}

function extendAll() {
    $('.row_to_hide').toggleClass('row_hidden');

    var link = $('#extended_report_link');
    link.hide();
}