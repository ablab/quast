String.prototype.trunc =
    function(n){
        return this.substr(0, n-1) + (this.length > n ? '&hellip;' : '');
    };

function fillOneRow(metric, mainMetrics, group_n, order, glossary, primary, rowName, report_n, assembliesNames, notAlignedContigs)
{
    (function(group_n) {
        var id_group = '#group_' + group_n;
        $(function() {
            $(id_group).removeClass('group_empty');
        });
    })(group_n);

    var table = '';
    var metricName = metric.metricName;
    var quality = metric.quality;
    var values = metric.values;

    var trClass = 'content-row';
    if (metric.isMain || $.inArray(metricName, mainMetrics) > -1) {
        (function(group_n) {
            var id_group = '#group_' + group_n;
            $(function() {
                $(id_group).removeClass('row_hidden');
            });
        })(group_n);
    } else {
        trClass = 'content-row row_hidden row_to_hide';
    }
	var tdClass = '';
    if (!primary)
	{
        trClass += ' secondary_hidden';
		tdClass = ' secondary_td';
	}
    else
        trClass += ' primary';

    table +=
        '<tr class="' + trClass + '" quality="' + quality + '" onclick="toggleSecondary($(this))">' +
            '<td class="left_column_td' + tdClass + '"><span class="metric-name">' + (primary ? '+ ' : '') +
                nbsp(addTooltipIfDefinitionExists(glossary, rowName), metricName) +
            '</span>' +
        '</td>';

    if (report_n > -1) {
        for (var not_aligned_n = 0; not_aligned_n < notAlignedContigs[report_n].length; not_aligned_n++) {
            values.splice(assembliesNames.indexOf(notAlignedContigs[report_n][not_aligned_n]), 0, '');
        }
    }

    for (var val_n = 0; val_n < values.length; val_n++) {
        value = values[order[val_n]];

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

                if (num !== null) {
                    table += '<td number="' + num + '"><span>' + toPrettyString(num) + rest + '</span></td>';
                } else {
                    table += '<td><span>' + value + '</span></td>';
                }
            }
        }
    }

    return table;

}

function buildTotalReport(assembliesNames, report, order, date, minContig, glossary, qualities, mainMetrics, reports) {
    $('#report_date').html('<p>' + date + '</p>');
    $('#mincontig').html('<p>All statistics are based on contigs of size >= ' + minContig +
        '<span class="rhs">&nbsp;</span>bp, unless otherwise noted (e.g., "# contigs (>= 0 bp)" and "Total length (>= 0 bp)" include all contigs.)</p>');

//    $('#extended_link').css('width', '183');

    $('#extended_link').append('' +
        '<div id="extended_report_link_div" style="float: left;"><a class="dotted-link" id="extended_report_link">Extended report</a>' +
        '</div>' +
        '<div style="float: left;"><span id="report_legend" style="display: none;"></span>' +
        '</div>' +
        '<div style="clear: both;">' +
        '</div>');

    $('#extended_report_link').click(function() {
        $('.row_to_hide').toggleClass('row_hidden');

        var link = $('#extended_report_link');
        if (link.html() == 'Extended report') {
            link.html('Short report');
        } else {
            link.html('Extended report');
        }
    });

    var table = '';
    table += '<table cellspacing="0" class="report_table draggable" id="main_report_table">';
    var refNames = []
    for (var report_n = 0; report_n < reports.length; report_n++) {
                var refName = reports[report_n].referenceName;
                refNames.push(refName);
            }
    reports = refNames.map(function (name, report_n) {
    return {
        name: name,
        report: this[report_n].report,
        asmNames: this[report_n].assembliesNames
        };
    }, reports);
    reports.sort( function( a, b ) { return a.name > b.name; });
    notAlignedContigs = {};
    for(report_n = 0; report_n < reports.length; report_n++ ) {
        notAlignedContigs[report_n] = [];
        for (var assembly_n = 0; assembly_n < assembliesNames.length; assembly_n++) {
            var assemblyName = assembliesNames[assembly_n];
            if (reports[report_n].asmNames.indexOf(assemblyName) == -1) {
                notAlignedContigs[report_n].push(assemblyName);
            }
        }
    }

    for (var group_n = 0; group_n < report.length; group_n++) {
        var group = report[group_n];
        var groupName = group[0];
        var metrics = group[1];

        var width = assembliesNames.length + 1;

        if (groupName == 'Reference statistics') {
            var referenceValues = {};
            for (var metric_n = 0; metric_n < metrics.length; metric_n++) {
                var metric = metrics[metric_n];
                var metricName = metric.metricName;
                var value = metric.values[0];
                referenceValues[metricName] = value;
            }
            var refName = referenceValues['Reference name'];
            var refLen = referenceValues['Reference length'];
            var refGC = referenceValues['Reference GC (%)'];
            var refGenes = referenceValues['Reference genes'];
            var refOperons = referenceValues['Reference operons'];

            if (refName) {
                $('#reference_name').find('.val').html(refName);
            }
            $('#reference_name').show();

            if (refLen)
                $('#reference_length').show().find('.val').html(toPrettyString(refLen));
            if (refGC)
                $('#reference_gc').show().find('.val').html(toPrettyString(refGC));
            if (refGenes)
                $('#reference_genes').show().find('.val').html(toPrettyString(refGenes));
            if (refOperons)
                $('#reference_operons').show().find('.val').html(toPrettyString(refOperons));

            continue;
        }

        if (group_n == 0) {
            table += '<tr class="top_row_tr"><td id="top_left_td" class="left_column_td"><span>' + groupName + '</span></td>';

            for (var assembly_n = 0; assembly_n < assembliesNames.length; assembly_n++) {
                var assemblyName = assembliesNames[order[assembly_n]];
                if (assemblyName.length > 30) {
                    assemblyName =
                        '<span class="tooltip-link" rel="tooltip" title="' + assemblyName + '">' +
                            assemblyName.trunc(30) +
                            '</span>'
                }

                table += '<td class="second_through_last_col_headers_td" position="' + order[assembly_n] + '">' +
                    '<span class="drag_handle"><span class="drag_image"></span></span>' +
                    '<span class="assembly_name">' + assemblyName + '</span>' +
                    '</td>';
            }

        } else {
            table +=
                '<tr class="group_header row_hidden group_empty" id="group_' + group_n + '">' +
                    '<td class="left_column_td"><span>' + groupName + '</span></td>'; //colspan="' + width + '"
            for (var i = 1; i < width; i++) {
                table += '<td></td>';
            }
            table += '</tr>';
        }
        for (metric_n = 0; metric_n < metrics.length; metric_n++) {
            var metric = metrics[metric_n];
            table += fillOneRow(metric, mainMetrics, group_n, order, glossary, true, metric.metricName, -1, assembliesNames, notAlignedContigs);
            for(report_n = 0; report_n < reports.length; report_n++ ) {  //  add information for each reference
                var metrics_ref = reports[report_n].report[group_n][1];
                for (var metric_ext_n = 0; metric_ext_n < metrics_ref.length; metric_ext_n++){
                    if (metrics_ref[metric_ext_n].metricName == metrics[metric_n].metricName) {
                        table += fillOneRow(metrics_ref[metric_ext_n], mainMetrics, group_n, order, glossary, false, reports[report_n].name, report_n, assembliesNames, notAlignedContigs);
                        break;
                    }
                }
            }
        }
        table += '</tr>';
    }
    table += '</table>';

    (function() {
        $(function() {
            $('tr.group_empty').removeClass('row_hidden');
        });
    })();

    $('#report').append(table);

    var RED_HUE = 0;
    var GREEN_HUE = 120;
    var GREEN_HSL = 'hsl(' + GREEN_HUE + ', 80%, 40%)';

    var legend = '<span>';
    var step = 6;
    for (var hue = RED_HUE; hue < GREEN_HUE + step; hue += step) {
        var lightness = (Math.pow(hue-75, 2))/350 + 35;
        legend += '<span style="color: hsl(' + hue + ', 80%, ' + lightness + '%);">';

        switch (hue) {
            case RED_HUE:
                legend += 'w'; break;
            case RED_HUE + step:
                legend += 'o'; break;
            case RED_HUE + 2 * step:
                legend += 'r'; break;
            case RED_HUE + 3 * step:
                legend += 's'; break;
            case RED_HUE + 4 * step:
                legend += 't'; break;

            case GREEN_HUE - 3 * step:
                legend += 'b'; break;
            case GREEN_HUE - 2 * step:
                legend += 'e'; break;
            case GREEN_HUE - step:
                legend += 's'; break;
            case GREEN_HUE:
                legend += 't'; break;

            default:
                legend += '.';
        }
        legend += '</span>';
    }
    legend += '</span>';
    $('#extended_report_link_div').width($('#top_left_td').outerWidth());

    $('#report_legend').append(legend);

    $(".report_table td[number]").mouseenter(function() {
        if (dragTable && dragTable.isDragging)
            return;

        var cells = $(this).parent().find('td[number]');
        var numbers = $.map(cells, function(cell) { return $(cell).attr('number'); });
        var quality = $(this).parent().attr('quality');

        var min = Math.min.apply(null, numbers);
        var max = Math.max.apply(null, numbers);

        var maxHue = GREEN_HUE;
        var minHue = RED_HUE;

        if (quality == 'Less is better') {
            maxHue = RED_HUE;
            minHue = GREEN_HUE;
        }

        if (max == min) {
            $(cells).css('color', GREEN_HSL);
        } else {
            var k = (maxHue - minHue) / (max - min);
            var hue = 0;
            var lightness = 0;
            cells.each(function(i) {
                var number = numbers[i];
                hue = Math.round(minHue + (number - min)*k);
                lightness = Math.round((Math.pow(hue-75, 2))/350 + 35);
//                $(this).css('color', 'hsl(' + hue + ', 80%, 35%)');
                $(this).css('color', 'hsl(' + hue + ', 80%, ' + lightness + '%)');
            });
        }

        if (numbers.length > 1)
            $('#report_legend').show('fast');

    }).mouseleave(function() {
        $(this).parent().find('td[number]').css('color', 'black');
    });
}

function toggleSecondary(caller)
{
	if(!caller.hasClass('primary'))
		return;
	var nextRow = caller.next('.content-row');
    var sign = nextRow.css('display') == 'none' ? '-' : '+';
    var left_column = caller.context.firstChild;
    left_column.textContent = sign + left_column.textContent.slice(1);
    while (!nextRow.hasClass('primary') && (nextRow.length > 0)) {
        nextRow.toggleClass('secondary_hidden');
        nextRow.css('background-color', '#F5F5DC');
        nextRow = nextRow.next('.content-row');
    }
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