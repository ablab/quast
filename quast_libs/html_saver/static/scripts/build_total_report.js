function addRow(metric, mainMetrics, rowName, group_n, order, glossary, isExpandable, isPrimary) {
    (function(group_n) {
        var id_group = '#group_' + group_n;
        $(function() {
            $(id_group).removeClass('group_empty');
        });
    })(group_n);

    var table = '';
    metricName = metric.metricName;
    var quality = metric.quality;
    var values = metric.values;

    var trClass = 'content-row';
    if (metric.isMain || $.inArray(metricName, mainMetrics) > -1) {
        (function(group_n) {
            var id_group = '#group_' + group_n;
            $(function() {
                $(id_group).removeClass('row_hidden');
                $(id_group).removeClass('row_to_hide');
            });
        })(group_n);
    } else {
        trClass = 'content-row row_hidden row_to_hide';
    }
    var tdClass = '';
    if (!isPrimary) {
        trClass += ' secondary_hidden';
        tdClass = 'secondary_td';
    }
    else {
        trClass += ' primary';
    }
    if (isExpandable) {
        table +=
            '<tr class="' + trClass + '" quality="' + quality + '" onclick="toggleSecondary(event, $(this))">' +
            '<td class="left_column_td ' + tdClass + '">' +
            '<span class="metric-name expandable collapsed">' +
               initial_spaces_to_nbsp(addTooltipIfDefinitionExists(glossary, rowName.trunc(55)), metricName) +
            '</span></td>';
    }
    else {
        table +=
            '<tr class="' + trClass + '" quality="' + quality + '">' +
            '<td class="left_column_td"><span class="metric-name">' +
            initial_spaces_to_nbsp(addTooltipIfDefinitionExists(glossary, rowName.trunc(55)), metricName) +
            '</span>' +
            '</td>';
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
    return table;
}

function getSubRows(subReports, groupName, metricName) {
    rows = [];
    if (subReports) {
        for (var report_n = 0; report_n < subReports.length; report_n++) {
            subReport = subReports[report_n];
            for (var group_n = 0; group_n < subReport.length; group_n++) {
                if (subReport[group_n][0] != groupName)
                    continue;
                metrics = subReport[group_n][1];
                for (var metric_n = 0; metric_n < metrics.length; metric_n++) {
                    if (metrics[metric_n].metricName == metricName)
                        rows.push(metrics[metric_n])
                }
            }
        }
    }
    return rows;
}

function buildTotalReport(assembliesNames, totalReport, order, glossary, qualities, mainMetrics) {
    var report = totalReport.report,
        date = totalReport.date,
        minContig = totalReport.minContig,
        referenceName = totalReport.referenceName,
        subReports = totalReport.subreports,
        subReferences = totalReport.subreferences;
    $('#report_date').html('<p>' + date + '</p>');
    var extraInfo = '<p>All statistics are based on contigs of size >= ' + minContig +
        '<span class="rhs">&nbsp;</span>bp, unless otherwise noted (e.g., "# contigs (>= 0 bp)" and "Total length (>= 0 bp)" include all contigs).</p>';
    $('#extrainfo').html(extraInfo);
    $('#plot-caption').show();

    var table = '';
    table += '<table cellspacing="0" class="report_table draggable" id="main_report_table">';

    if (referenceName) {
        $('#reference_name').show().find('.val').html(referenceName);
    }

    if (report[0][0] == 'Genome statistics') {  // if first section is empty (no reference), swap it and w/o reference statistics
        var genomeMetrics = report[0][1];
        var isSectionEmpty = true;
        for (var index = 0; index < genomeMetrics.length; index++) {
            if (genomeMetrics[index].isMain || $.inArray(genomeMetrics[index].metric_name, mainMetrics) > -1)
                isSectionEmpty = false;
        }
        if (isSectionEmpty) {
            for (var group_n = 0; group_n < report.length; group_n++) {
                if (report[group_n][0] == 'Statistics without reference') {
                    report[0] = report.splice(group_n, 1, report[0])[0];
                }
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
            var refLen = referenceValues['Reference length'];
            var refFragments = referenceValues['Reference fragments'];
            var refGC = referenceValues['Reference GC (%)'];
            var refFeatures = referenceValues['Reference genomic features'];
            var refOperons = referenceValues['Reference operons'];
            var refChr = referenceValues['Reference chromosomes'];
            var totalReads = referenceValues['# total reads'];
            var refMappedReads = referenceValues['Reference mapped (%)'];
            var refPairedReads = referenceValues['Reference properly paired (%)'];
            var estRefLen = referenceValues['Estimated reference length'];

            if (refLen)
                $('#reference_length').show().find('.val').html(toPrettyString(refLen));
            else if (estRefLen)
                $('#est_reference_length').show().find('.val').html(toPrettyString(estRefLen));
            if (refFragments) {
                $('#reference_fragments').show().find('.val').html(refFragments);
                if (refFragments > 1)
                    $('#reference_fragments').find('.plural_ending').show();
            }
            if (refGC)
                $('#reference_gc').show().find('.val').html(toPrettyString(refGC));
            if (refFeatures)
                $('#reference_features').show().find('.val').html(toPrettyString(refFeatures));
            if (refOperons)
                $('#reference_operons').show().find('.val').html(toPrettyString(refOperons));
            if (refChr) {
                $('#reference_chr').show().find('.val').html(refChr);
                if (refChr > 1)
                    $('#reference_chr').find('.plural_ending').show();
            }
            if (totalReads)
                $('#total_reads').show().find('.val').html(toPrettyString(totalReads));
            if (refMappedReads !== undefined)
                $('#reference_mapped_reads').show().find('.val').html(toPrettyString(refMappedReads));
            if (refPairedReads !== undefined)
                $('#reference_paired_reads').show().find('.val').html(toPrettyString(refPairedReads));
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
                '<tr class="group_header row_to_hide row_hidden group_empty" id="group_' + group_n + '">' +
                    '<td class="left_column_td"><span>' + groupName + '</span></td>'; //colspan="' + width + '"
            for (var i = 1; i < width; i++) {
                table += '<td></td>';
            }
            table += '</tr>';
        }

        for (metric_n = 0; metric_n < metrics.length; metric_n++) {
            isExpandable = false;
            isPrimary = true;
            metricName = metrics[metric_n].metricName;
            subRows = getSubRows(subReports, groupName, metricName);
            if (subRows && subRows.length > 0) {
                isExpandable = true;
                table += addRow(metrics[metric_n], mainMetrics, metricName, group_n, order, glossary, isExpandable, isPrimary);
                for (var rows_n = 0; rows_n < subRows.length; rows_n++) {
                    isExpandable = false;
                    isPrimary = false;
                    table += addRow(subRows[rows_n], mainMetrics, subReferences[rows_n], group_n, order, glossary, isExpandable, isPrimary);
                }
            }
            else table += addRow(metrics[metric_n], mainMetrics, metricName, group_n, order, glossary, isExpandable, isPrimary);
        }
        table += '</tr>';
    }
    table += '</table>';

    table += buildExtendedLinkClick();

    setUpHeatMap(table);
}
