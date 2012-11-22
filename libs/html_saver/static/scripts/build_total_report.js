
function buildTotalReport(assembliesNames, report, date, minContig, glossary, qualities, mainMetrics) {
    $('#subheader').html('<p>' + date + '</p>');
    $('#mincontig').append('<p>Contigs shorter than ' + minContig + "<span class='rhs'>&nbsp;</span>" + 'bp were skipped</p>');
    $('#extended_link').append('<a class="dotted-link" id="extended_report_link">Extended report</a>');
    $('#extended_report_link').click(function() {
        $('.content-row.row_hidden').fadeToggle('fast');

        var link = $('#extended_report_link');
        if (link.html() == 'Extended report') {
            link.html('Short report');
        } else {
            link.html('Extended report')
        }
    });


    var table = '';
    table += '<table cellspacing="0" class="report-table">';

    for (var group_n = 0; group_n < report.length; group_n++) {
        var group = report[group_n];
        var groupName = group[0];
        var metrics = group[1];
        var width = assembliesNames.length + 1;

        if (group_n == 0) {
            table += '<tr class="header-tr"><td>' + groupName + '</td>';

            for (var assembly_n = 0; assembly_n < assembliesNames.length; assembly_n++) {
                var assemblyName = assembliesNames[assembly_n];
                table += '<td>' + assemblyName + '</td>';
            }

        } else {
            table +=
                '<tr class="subheader-tr row_hidden" id="group_' + group_n + '">' +
                    '<td colspan="' + width + '">' + groupName + '</td>' +
                    '</tr>';
        }

        for (var metric_n = 0; metric_n < metrics.length; metric_n++) {
            var metric = metrics[metric_n];
            var metricName = metric.metricName;
            var quality = metric.quality;
            var values = metric.values;

            var trClass = 'content-row';
            if (metric.isMain || $.inArray(metricName, mainMetrics) > -1) {
                (function(group_n) {
                    var id = '#group_' + group_n;
                    $(function() {
                        $(id).removeClass('row_hidden');
                    });
                })(group_n);
            } else {
                trClass = 'content-row row_hidden';
            }

            table +=
                '<tr class="' + trClass + '" quality="' + quality + '">' +
                    '<td><span class="metric-name">' +
                    addTooltipIfDefinitionExists(glossary, metricName) +
                    '</span>' +
                    '</td>';

            for (var value_n = 0; value_n < values.length; value_n++) {
                var value = values[value_n];

                if (value === null || value === '') {
                    table += '<td><span>-</span></td>';
                } else {
                    if (typeof value === 'number') {
                        table +=
                            '<td number="' + value + '"><span>'
                                + toPrettyString(value) + '</span></td>';
                    } else {
                        var result = /([0-9\.]+)(.*)/.exec(value);
                        var num = parseFloat(result[1]);
                        var rest = result[2];
//                        alert('value = ' + value + ' result = ' + result);

//                        var num = parseFloat(value);

                        if (num !== null) {
                            table += '<td number="' + num + '"><span>' + toPrettyString(num) + rest + '</span></td>';
                        } else {
                            table += '<td><span>' + value + '</span></td>';
                        }
                    }
                }
            }
        }
        table += '</tr>';
    }
    table += '</table>';

    $('#report').append(table);

//    $().load(function() {

    $(".report-table td[number]").mouseenter(function() {
        var cells = $(this).parent().find('td[number]');
        var numbers = $.map(cells, function(cell) { return $(cell).attr('number'); });
        var quality = $(this).parent().attr('quality');

        var min = Math.min.apply(null, numbers);
        var max = Math.max.apply(null, numbers);

        var RED_HUE = 0;
        var GREEN_HUE = 130;

        var maxHue = GREEN_HUE;
        var minHue = RED_HUE;

        if (quality == 'Less is better') {
            maxHue = RED_HUE;
            minHue = GREEN_HUE;
        }

        if (max == min) {
            $(cells).css('color', 'hsl(' + GREEN_HUE + ', 80%, 50%)');
        } else {
            var k = (maxHue - minHue) / (max - min);

            cells.each(function(i) {
                var number = numbers[i];
                var hue = minHue + (number - min)*k;

                $(this).css('color', 'hsl(' + hue + ', 80%, 50%)');
            });
        }
    }).mouseleave(function() {
            $(this).parent().find('td[number]').css('color', 'black');
        });
//    });
}
//
//function buildNewTotalReport(report, glossary) {
//    var table = '';
//    table += '<table cellspacing="0" class="report-table">';
//
//    for (var col = 0; col < report.header.length; col++) {
//        var keyCell;
//
//        if (report.header[col] == '# misassemblies') {
//            table += '<tr class="subheader-tr"><td colspan="' + (report.results.length+1) + '"><b>Structural variations</b></td></tr>'
//        }
//        if (report.header[col] == 'Genome fraction (%)') {
//            table += '<tr class="subheader-tr"><td colspan="' + (report.results.length+1) + '"><b>Genes and operons</b></td></tr>'
//        }
//        if (report.header[col] == 'NA50') {
//            table += '<tr class="subheader-tr"><td colspan="' + (report.results.length+1) + '"><b>Aligned</b></td></tr>'
//        }
//
//        if (col == 0) {
//            keyCell = '<span class="report-table-header">Basic stats</span>';
//            table += '<tr><td><span style="">' + keyCell + '</span></td>';
//        } else {
//            keyCell = addTooltipIfDefinitionExists(glossary, report.header[col]);
//            table += '<tr class="content-row"><td><span style="">' + keyCell + '</span></td>';
//        }
//
//        for (var row = 0; row < report.results.length; row++) {
//            var value = report.results[row][col];
//            var valueCell = value;
//
//            if (col == 0) {
//                valueCell = '<span class="report-table-header">' + value + '</span>';
//                table += '<td><span>' + valueCell + '</span></td>';
//
//            } else {
//                if (value == 'None' /* && report.header[i].substr(0,2) == 'NG' */) {
//                    valueCell = '-';
//                    table += '<td><span>' + valueCell + '</span></td>';
//
//                } else {
//                    if (typeof value == 'number') {
//                        valueCell = toPrettyString(value);
//                        table += '<td number="' + value + '"><span>' + valueCell + '</span></td>';
//                    } else {
//                        valueCell = toPrettyString(value);
//                        table += '<td><span>' + valueCell + '</span></td>';
//                    }
//                }
//            }
//        }
//        table += '</tr>\n';
//    }
//    table += '</table>';
//
//    $(document).ready(function() {
//        $(".report-table td:[number]").mouseenter(function() {
//            var cells = $(this).parent().find('td:[number]');
//            var numbers = $.map(cells, function(cell) { return $(cell).attr('number'); });
//
//            var min = Math.min.apply(null, numbers);
//            var max = Math.max.apply(null, numbers);
//
//            var RED_HUE = 0;
//            var GREEN_HUE = 130;
//
//            if (max == min) {
//                $(cells).css('color', 'hsl(' + GREEN_HUE + ', 80%, 50%)');
//            } else {
//                var k = (GREEN_HUE - RED_HUE) / (max - min);
//
//                cells.each(function(i) {
//                    var number = numbers[i];
//                    var hue = (number - min)*k;
//                    $(this).css('color', 'hsl(' + hue + ', 80%, 50%)');
//                });
//            }
//        }).mouseleave(function() {
//            $(this).parent().find('td:[number]').css('color', 'black');
//        });
//    });
//
//    $('#report').append(table);
//}
//
//
