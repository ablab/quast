
function buildTotalReport(report) {
    var table = '';
    table += '<table class=".report-table">';

    for (var i = 0; i < report.header.length; i++) {
        if (i == 0) {
            table += '\t<tr><td>Total report</td>';
        } else {
            table += '\t<tr><td>' + report.header[i] + '</td>';
        }

        for (var j = 0; j < report.results.length; j++) {
            val = report.results[j][i];
            if (typeof val == 'number') {
                val = toPrettyString(val);
            }
            if (val == null && report.header[i].substr(0,2) == 'NG') {
                val = '-';
            }
            table = table + '<td style="text-align: right">' + val + '</td>';
        }

        table += '</tr>\n';
    }
    table += '</table>';

    $('#report').append(table);
}
