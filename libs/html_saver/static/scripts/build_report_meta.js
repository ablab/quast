
function showPlotWithInfo(info) {
    var newSeries = [];
    var newColors = [];

    $('#legend-placeholder').find('input:checked').each(function() {
        var number = $(this).attr('name');
        if (number && info.series && info.series.length > 0) {
            var i = 0;
            do {
                var series = info.series[i];
                i++;
            } while (series.number != number && i <= info.series.length);
            //
            if (i <= info.series.length) {
                newSeries.push(series);
                newColors.push(series.color);
            } else {
                console.log('no series with number ' + number);
            }
        }
    });

    if (newSeries.length === 0) {
        newSeries.push({
            data: [],
        });
        newColors.push('#FFF');
    }

    info.showWithData(newSeries, newColors);
}


function Range(from, to) {
    var r  = [];
    for (var i = from; i < to; i++) {
        r.push(i);
    }
    return r;
}


function recoverOrderFromCookies() {
    if (!navigator.cookieEnabled)
        return null;

    var order_string = readCookie("order");
    if (!order_string)
        return null;

    var order = [];
    var fail = false;
    forEach(order_string.split(' '), function(val) {
        val = parseInt(val);
        if (isNaN(val))
            fail = true;
        else
            order.push(val);
    });

    if (fail)
        return null;

    return order;
}

function buildReport() {
    var assembliesNames;
    var order;

    var totalReport = null;
    var qualities = null;
    var mainMetrics = null;
    var refLen = 0;
    var contigs = null;

    var glossary = JSON.parse($('#glossary-json').html());

    function readJson(what) {
        var result;
        try {
            result = JSON.parse($('#' + what + '-json').html());
        } catch (e) {
            result = null;
        }
        return result;
    }

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




