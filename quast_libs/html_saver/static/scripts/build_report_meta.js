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

    mainMetrics = ['# contigs', 'Largest alignment', 'Total aligned length', '# misassemblies', 'Misassembled contigs length',
        '# mismatches per 100 kbp', '# indels per 100 kbp', "# N's per 100 kbp", 'Genome fraction (%)', 'Duplication ratio', 'NGA50'];
    buildTotalReport(assembliesNames, totalReport.report, order, totalReport.date,
        totalReport.minContig, glossary, qualities, mainMetrics, totalReports.slice(1));

    var plotsSwitchesDiv = document.getElementById('plots-switches');
    $(plotsSwitchesDiv).html('<b>Plots:</b>');
    
    var misassembl_coordX = null;
    var misassembl_coordY = null;
    var misassembl_refNames = null;
    var summaryReports = null;
    var firstPlot = true;

    if (summaryReports = readJson('summary')){
        name_reports = ['contigs', 'largest', 'totallen', 'misassemblies', 'misassembled', 'mismatches',
            'indels', 'ns', 'genome', 'duplication', 'nga50'];
        title_reports = ['Contigs', 'Largest alignment', 'Total aligned len', 'Misassemblies', 'Mis. len', 'Mismatches',
            'Indels', "N's per 100 kbp", 'Genome frac.', 'Dup. ratio', 'NGA50'];
        for (var i = 0; i < summaryReports.length; i++) {
            if (summaryReports[i].refnames != undefined && summaryReports[i].refnames.length > 0) {
                if (name_reports[i] == 'misassemblies') {
                    misassembl_coordX = summaryReports[i].coord_x;
                    misassembl_coordY = summaryReports[i].coord_y;
                    misassembl_refNames = summaryReports[i].refnames;
                    var misassembliesReports = null;
                    if (misassembliesReports = readJson('misassemblies')) {
                        if (misassembliesReports.refnames  != undefined) {
                            makePlot(firstPlot, assembliesNames, order, 'misassemblies', 'Misassemblies', misassemblies.draw, {
                                    main_coord_x: misassembl_coordX,
                                    main_coord_y: misassembl_coordY,
                                    main_refnames: misassembl_refNames,
                                    coord_x: misassembliesReports.coord_x,
                                    coord_y: misassembliesReports.coord_y,
                                    filenames: misassembliesReports.filenames,
                                    refnames: misassembliesReports.refnames
                                },
                                null, null
                            );
                        }
                    }
                }
                else {
                    makePlot(firstPlot, assembliesNames, order, name_reports[i], title_reports[i], summary.draw, {
                            coord_x: summaryReports[i].coord_x,
                            coord_y: summaryReports[i].coord_y,
                            filenames: summaryReports[i].filenames,
                            refnames: summaryReports[i].refnames
                        },
                        null, null
                    );
                }
                firstPlot = false;
            }
        }
    }

    if (firstPlot) $('.plots').hide();
    $('#contigs_are_ordered').hide();
    appendIcarusLinks();
    return 0;
}
