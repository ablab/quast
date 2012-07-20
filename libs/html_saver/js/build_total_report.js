
function buildTotalReport(report, glossary) {
    var table = '';
    table += '<table class="report-table">';

    for (var i = 0; i < report.header.length; i++) {
        var keyCell;

        if (i == 0) {
            keyCell = '<span class="report-table-header">Total report</span>';
        } else {
            var key = report.header[i];
            if (glossary.hasOwnProperty(key)) {
                keyCell = '<a class="tooltip-link" href="#" rel="tooltip" title="' +
                    key + ' ' + glossary[key] + '">' + key + '</a>';
            } else {
                keyCell = key;
            }
        }

        table += '<tr><td><span style="">' + keyCell + '</span></td>';

        for (var j = 0; j < report.results.length; j++) {
            var value = report.results[j][i];
            var valueCell = value;

            if (i == 0) {
                valueCell = '<span class="report-table-header">' + value + '</span>';
            } else {
                if (typeof value == 'number') {
                    valueCell = toPrettyString(value);
                }
                if (value == null && report.header[i].substr(0,2) == 'NG') {
                    valueCell = '-';
                }
            }

            table += '<td><span style="">' + valueCell + '</span></td>'
        }
        table += '</tr>\n';
    }
    table += '</table>';

    $('#report').append(table);
}
