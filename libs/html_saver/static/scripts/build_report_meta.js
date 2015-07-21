
function buildReport() {
    var assembliesNames;
    var order;

    var totalReport = null;
    var qualities = null;
    var mainMetrics = null;
    var contigs = null;

    var glossary = JSON.parse($('#glossary-json').html());

    /****************/
    /* Total report */

    if (!(totalReports = readJson('total-report'))) {
        console.log("Error: cannot read #total-report-json");
        return 1;
    }

    totalReport = totalReports[0];

    assembliesNames = totalReport.assembliesNames;

    order = recoverOrderFromCookies() || totalReport.order || Range(0, assembliesNames.length);

    mainMetrics = ['# contigs', 'Largest contig', 'Total length', 'N50', '# misassemblies', 'Misassembled contigs length',
        '# mismatches per 100 kbp', '# indels per 100 kbp', "# N's per 100 kbp", 'Genome fraction (%)', 'Duplication ratio', 'NGA50']
    buildTotalReport(assembliesNames, totalReport.report, order, totalReport.date,
        totalReport.minContig, glossary, qualities, mainMetrics, totalReports.slice(1));

    var plotsSwitchesDiv = document.getElementById('plots-switches');
    $(plotsSwitchesDiv).html('<b>Plots:</b>');

    assembliesNames.forEach(function(filename, i) {
        var id = 'label_' + i + '_id';
        $('#legend-placeholder').append('<div>' +
            '<label for="' + id + '" style="color: ' + colors[i] + '">' +
            '<input type="checkbox" name="' + i + '" checked="checked" id="' + id + '">&nbsp;' + filename + '</label>' +
            '</div>');
    });

    if (summaryReports = readJson('summary')){
        name_reports = ['contigs', 'largest', 'totallen', 'n50', 'misassemblies', 'misassembled', 'mismatches', 'indels',
            'ns', 'genome', 'duplication', 'nga50'];
        title_reports = ['Contigs', 'Largest contig', 'Total length', 'N50', 'Misassemblies', 'Misassembled length', 'Mismatches',
        'Indels', "N's", 'Genome fraction', 'Duplication ratio', 'NGA50'];
        for (var i = 0; i < summaryReports.length; i++) {
            if (summaryReports[i].refnames != undefined) {
                makePlot(i == 0, assembliesNames, order, name_reports[i], title_reports[i], summary.draw, {
                        coord_x: summaryReports[i].coord_x,
                        coord_y: summaryReports[i].coord_y,
                        filenames: summaryReports[i].filenames,
                        refnames: summaryReports[i].refnames
                    },
                    null, null
                );
            }
        }
    }


    return 0;
}
