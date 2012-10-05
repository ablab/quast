
function buildTotalReport(report, glossary) {
    var table = '';
    table += '<table cellspacing="0" class="report-table">';

    for (var col = 0; col < report.header.length; col++) {
        var keyCell;

        if (report.header[col] == '# misassemblies') {
            table += '<tr class="subheader-tr"><td colspan="' + (report.results.length+1) + '"><b>Structural variations</b></td></tr>'
        }
        if (report.header[col] == 'Genome fraction (%)') {
            table += '<tr class="subheader-tr"><td colspan="' + (report.results.length+1) + '"><b>Genes and operons</b></td></tr>'
        }
        if (report.header[col] == 'NA50') {
            table += '<tr class="subheader-tr"><td colspan="' + (report.results.length+1) + '"><b>Aligned</b></td></tr>'
        }

        if (col == 0) {
            keyCell = '<span class="report-table-header">Basic stats</span>';
            table += '<tr><td><span style="">' + keyCell + '</span></td>';
        } else {
            keyCell = addTooltipIfDefenitionExists(glossary, report.header[col]);
            table += '<tr class="content-row"><td><span style="">' + keyCell + '</span></td>';
        }

        for (var row = 0; row < report.results.length; row++) {
            var value = report.results[row][col];
            var valueCell = value;

            if (col == 0) {
                valueCell = '<span class="report-table-header">' + value + '</span>';
                table += '<td><span>' + valueCell + '</span></td>';

            } else {
                if (value == 'None' /* && report.header[i].substr(0,2) == 'NG' */) {
                    valueCell = '-';
                    table += '<td><span>' + valueCell + '</span></td>';

                } else {
                    if (typeof value == 'number') {
                        valueCell = toPrettyString(value);
                        table += '<td number="' + value + '"><span>' + valueCell + '</span></td>';
                    } else {
                        valueCell = toPrettyString(value);
                        table += '<td><span>' + valueCell + '</span></td>';
                    }
                }
            }
        }
        table += '</tr>\n';
    }
    table += '</table>';

    $(document).ready(function() {
        $(".report-table td:[number]").mouseenter(function() {
            var cells = $(this).parent().find('td:[number]');
            var numbers = $.map(cells, function(cell) { return $(cell).attr('number'); });

            var min = Math.min.apply(null, numbers);
            var max = Math.max.apply(null, numbers);

            var RED_HUE = 0;
            var GREEN_HUE = 130;

            if (max == min) {
                $(cells).css('color', 'hsl(' + GREEN_HUE + ', 80%, 50%)');
            } else {
                var k = (GREEN_HUE - RED_HUE) / (max - min);

                cells.each(function(i) {
                    var number = numbers[i];
                    var hue = (number - min)*k;
                    $(this).css('color', 'hsl(' + hue + ', 80%, 50%)');
                });
            }
        }).mouseleave(function() {
            $(this).parent().find('td:[number]').css('color', 'black');
        });
    });

    $('#report').append(table);
}


