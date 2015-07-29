function buildTotalReport(assembliesNames, report, order, date, minContig,
                          glossary, qualities, mainMetrics) {
    $('#report_date').html('<p>' + date + '</p>');
    $('#mincontig').html('<p>All statistics are based on contigs of size >= ' + minContig +
        '<span class="rhs">&nbsp;</span>bp, unless otherwise noted (e.g., "# contigs (>= 0 bp)" and "Total length (>= 0 bp)" include all contigs.)</p>');

    var table = '';
    table += '<table cellspacing="0" class="report_table draggable" id="main_report_table">';

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
            var refChr = referenceValues['Reference chromosomes'];
            var refTotalreads = referenceValues['Reference reads'];
            var refMappedreads = referenceValues['Reference mapped reads'];
            var refPairedreads = referenceValues['Reference properly paired reads'];

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
            if (refChr)
                $('#reference_chr').show().find('.val').html(toPrettyString(refChr));
            if (refTotalreads)
                $('#reference_reads').show().find('.val').html(toPrettyString(refTotalreads));
            if (refMappedreads)
                $('#reference_mappedreads').show().find('.val').html(toPrettyString(refMappedreads));
            if (refPairedreads)
                $('#reference_pairedreads').show().find('.val').html(toPrettyString(refPairedreads));
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
            (function(group_n) {
                var id_group = '#group_' + group_n;
                $(function() {
                    $(id_group).removeClass('group_empty');
                });
            })(group_n);

            metric = metrics[metric_n];
            metricName = metric.metricName;
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

            table +=
                '<tr class="' + trClass + '" quality="' + quality + '">' +
                    '<td class="left_column_td"><span class="metric-name">' +
                        initial_spaces_to_nbsp(addTooltipIfDefinitionExists(glossary, metricName), metricName) +
                    '</span>' +
                '</td>';

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

    table += biuldExtendedLinkClick();

    setUpHeatMap(table);
}
