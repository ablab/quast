
function buildReport() {
    var assembliesNames;
    var order;

    var totalReport = null;
    var qualities = null;
    var mainMetrics = null;
    var refLen = 0;
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

    buildTotalReport(assembliesNames, totalReport.report, order, totalReport.date,
        totalReport.minContig, glossary, qualities, mainMetrics, totalReports.slice(1));

    $('.plots').hide();

    return 0;
}
