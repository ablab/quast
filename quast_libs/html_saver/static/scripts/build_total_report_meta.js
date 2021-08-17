function fillOneRow(metric, mainMetrics, group_n, order, glossary, is_primary, rowName,
                    report_n, assembliesNames, notAlignedContigs, notExtendedMetrics, isEmptyRows, metricsNotForCombinedReference) {
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
    var iconPlots;
    if (metric.isMain || $.inArray(metricName, mainMetrics) != -1) {
        if ($.inArray(metricName, mainMetrics) != -1) {
            var numPlot = $.inArray(metricName, mainMetrics);
            iconPlots = '<img id="' + numPlot + '" class="icon_plot" style="vertical-align: bottom" onclick="setPlot($(this))"/>';
        }
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
    if (!is_primary) {
        trClass += ' secondary_hidden';
        tdClass = 'secondary_td';
    }
    else {
        trClass += ' primary';
    }

    var not_extend = false;
    if ($.inArray(metricName, notExtendedMetrics) > -1 || isEmptyRows == true){
        not_extend = true;
        trClass += ' not_extend';
    }

    table +=
        '<tr class="' + trClass + '" quality="' + quality + '" onclick="toggleSecondary(event, $(this))">' +
        '<td class="left_column_td ' + tdClass + '">' +
        '<span class="metric-name' +
          (is_primary ? ' primary' : ' secondary') + (not_extend || !is_primary ? '' : ' expandable collapsed') + '">' +
           initial_spaces_to_nbsp(addTooltipIfDefinitionExists(glossary, rowName.trunc(55)), metricName) +
          (iconPlots && is_primary ? ("&nbsp" + iconPlots) : '') +
        '</span></td>';

    tooltipForGenomeStatistics = 'Metrics that depend on the reference length are not calculated for the combined reference.';
    tooltipForGCStatistics = 'GC content is not calculated for the combined reference.';
    if (is_primary && $.inArray(metricName, metricsNotForCombinedReference) != -1) {
        for (var val_n = 0; val_n < assembliesNames.length; val_n++) {
            table += '<td><a class="tooltip-link" rel="tooltip" title="' +
                (metricName.indexOf('GC') == -1 ? tooltipForGenomeStatistics : tooltipForGCStatistics)  + '"> ... </a></td>';
        }
        return table;
    }

    if (report_n > -1 && notAlignedContigs[report_n] != null) {
        for (var not_aligned_n = 0; not_aligned_n < notAlignedContigs[report_n].length; not_aligned_n++) {
            values.splice(assembliesNames.indexOf(notAlignedContigs[report_n][not_aligned_n]), 0, '');
        }
    }

    for (var val_n = 0; val_n < values.length; val_n++) {
        var value = values[order[val_n]];

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


function buildGenomeTable(reports, group_n, numColumns) {
    var tableGenome = '';
    tableGenome += '<div class="report" id="ref_report">';
    tableGenome += '<table cellspacing="0" id="refgenome">';
    tableGenome += '<tr class="top_row_tr"><td class="left_column_td"><span>' + 'Reference' + '</span></td>';
    var colNames = ['Size, bp', 'Fragments', 'GC, %', 'Genomic features', 'Operons'];
    for (var col_n = 0; col_n < numColumns; col_n++) {
        var columnName = colNames[col_n];
        tableGenome += '<td class="second_through_last_col_headers_td">' +
            '<span class="assembly_name">' + columnName + '</span>' +
        '</td>';
    }

    var combined_reference_size = 0;
    var notAlignedReport;
    for (var report_n = 0; report_n < reports.length; report_n++ ) {
        var trClass = 'content-row';
        var refName = reports[report_n].name;
        if (refName == 'not_aligned') {
            notAlignedReport = reports[report_n];
            continue;
        }
        tableGenome +=
            '<tr class="' + trClass + '">' +
            '<td class="left_column_td">' +
                '<span class="metric-name">' +
                    '<a href="runs_per_reference/' + refName + '/report.html">' + refName + '</a>' +
                '</span>' +
            '</td>';
        var metrics = reports[report_n].report[group_n][1];
        var referenceMetrics = ['Reference length', 'Reference fragments', 'Reference GC (%)',
                                'Reference genomic features', 'Reference operons'];
        for (var metric_n = 0; metric_n < metrics.length; metric_n++) {
            var metric = metrics[metric_n];
            if (referenceMetrics.indexOf(metric.metricName) === -1) continue;

            var value = metric.values[0];

            if (metric.metricName == 'Reference length')
                combined_reference_size += value;

            if (value === null || value === '') {
                tableGenome += '<td><span>-</span></td>';
            } else {
                if (typeof value === 'number') {
                    tableGenome +=
                        '<td number="' + value + '" class="number"><span>'
                        + toPrettyString(value) + '</span></td>';
                } else {
                    var result = /([0-9\.]+)(.*)/.exec(value);
                    var num = parseFloat(result[1]);
                    var rest = result[2];
//                        alert('value = ' + value + ' result = ' + result);

//                        var num = parseFloat(value);

                    if (num !== null) {
                        tableGenome += '<td number="' + num + '" class="number"><span>' + toPrettyString(num) + rest + '</span></td>';
                    } else {
                        tableGenome += '<td><span>' + value + '</span></td>';
                    }
                }
                //if (metric.metricName == 'Reference size'
            }
        }
        tableGenome += '</tr>';

    }

    tableGenome += '<tr><td></td><td class="second_through_last_col_headers_td last_row">' + toPrettyString(combined_reference_size) + '</td>';
    for (metric_n = 1; metric_n < metrics.length; metric_n++) {
        tableGenome += '<td class="second_through_last_col_headers_td"></td>';
    }
    tableGenome += '</tr>';

    //tableGenome += '<hr>';

    tableGenome += '</table>';
    tableGenome += '<br>';
    tableGenome += '<br>';
    tableGenome +=
        '<span class="metric-name">' +
            '<a href="combined_reference/report.html">' + 'Combined reference' + '</a>' +
        '</span><br>';
    if (notAlignedReport) {
        tableGenome +=
            '<span class="metric-name">' +
                '<a href="not_aligned/report.html">' + 'Not aligned contigs' + '</a>' +
            '</span>';
    }

    tableGenome += '</div>';
    return tableGenome;
}


function buildTotalReport(assembliesNames, report, order, date, minContig, glossary,
                          qualities, mainMetrics, reports) {
    $('#report_date').html('<p>' + date + '</p>');
    var extraInfo = '<p>All statistics are based on contigs of size &ge; ' + minContig +
        '<span class="rhs">&nbsp;</span>bp, unless otherwise noted (e.g., "# contigs (>= 0 bp)" and "Total length (>= 0 bp)" include all contigs).</p>';
    $('#extrainfo').html(extraInfo);
    $('#plot-caption').show();
    $('#per_ref_msg').html('<p>Rows show values for the whole assembly (column name) vs. the combined reference (concatenation of all provided references).<br>' +
        'Clicking on a row with <span style="color: #CCC">+</span> sign will expand values for contigs aligned to each of the references separately.<br>' +
        'Note that some metrics (e.g. # contigs) may not sum up, because one contig may be aligned to several references and thus, counted several times.<br>' +
        'All metrics that depend on the reference length (such as NG50, LG50, etc), plus the GC % are not calculated for the combined reference.<br>' +
        'The combined reference is just a concatenation of all available reference genomes of the species, presumably represented in the metagenomic dataset, ' +
        'but not necessarily the real content.<br>So it might miss many correctly assembled species, ' +
        'and therefore it does not make sense to apply the size and the GC content of the combined reference for assembly evaluation.</p>');
    $('#quast_name').html('MetaQUAST');
    $('#report_name').html('summary report');
    if (kronaPaths = readJson('krona')) {
        if (kronaPaths.paths != undefined) {
            $('#krona').html('Krona charts: ');
            for (var assembly_n = 0; assembly_n < assembliesNames.length; assembly_n++ ) {
                var assemblyName = assembliesNames[assembly_n];
                $('#krona').append(
                    '&nbsp&nbsp<span class="metric-name">' +
                    '<a href="' + kronaPaths.paths[assembly_n] + '">' + assemblyName + '</a>' +
                    '</span>&nbsp&nbsp');
            }
            if (assembliesNames.length > 1)  $('#krona').append(
                    '&nbsp&nbsp&nbsp&nbsp<span class="metric-name">' +
                    '<a href="' + kronaPaths.paths[assembliesNames.length] + '">Summary</a>' +
                    '</span>&nbsp');
        }
    }

    var table = '';
    table += '<table cellspacing="0" class="report_table draggable" id="main_report_table">';
    var refNames = [];
    var missedRefs = [];
    var nonEmptyReports = [];
    for (var report_n = 0; report_n < reports.length; report_n++) {
        var _refName = reports[report_n].referenceName;
        if (!_refName) _refName = 'not_aligned';
        if (!reports[report_n].report) missedRefs.push(_refName);
        else {
            nonEmptyReports.push(reports[report_n]);
            refNames.push(_refName);
        }
    }
    reports = refNames.map(function (name, report_n) {
    return {
        name: name,
        report: this[report_n].report,
        asmNames: this[report_n].assembliesNames
        };
    }, nonEmptyReports);
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
    var notExtendedMetrics = [];
    if (minContig > 0)
        notExtendedMetrics = [ '# contigs (&gt;= 0 bp)', 'Total length (&gt;= 0 bp)', 'Fully unaligned length', '# fully unaligned contigs'];

    function getGenomeBasedContiguityMetrics() {  // retrieving NG50, LG50, NGA50, NGx, LGx, etc with exact values of 'x'
        var gbcMetricNames = [];
        for (var group_n = 0; group_n < report.length; group_n++) { // the metrics are in per_ref reports but not in combined_ref
            var groupName = report[group_n][0];
            if (groupName.startsWith('Genome')) {  // only 'Genome statistics' section is needed
                for (var report_n = 0; report_n < reports.length; report_n++) {
                    var metrics_by_refs = reports[report_n].report[group_n][1];
                    if (metrics_by_refs.length > 0) { // check for special case -- not_aligned report: length == 0 there
                        for (var metric_n = 0; metric_n < metrics_by_refs.length; metric_n++) {
                            var metricName = metrics_by_refs[metric_n].metricName;
                            if (metricName.startsWith('NG') || metricName.startsWith('LG')) {
                                gbcMetricNames.push(metricName);
                            }
                        }
                        break;
                    }
                }
                break;
            }
        }
        return gbcMetricNames;
    }

    var metricsNotForCombinedReference = ['GC (%)', 'Avg contig read support'].concat(
                                          getGenomeBasedContiguityMetrics(metricsNames));

    for (var group_n = 0; group_n < report.length; group_n++) {
        var group = report[group_n];
        var groupName = group[0];
        var metrics = group[1];
        var metricsNames = [];
        for (var metric_n = 0; metric_n < metrics.length; metric_n++)
            metricsNames.push(metrics[metric_n].metricName);
        for (var report_n = 0; report_n < reports.length; report_n++) {
            var metrics_by_refs = reports[report_n].report[group_n][1];
            for (var metric_n = 0; metric_n < metrics_by_refs.length; metric_n++) {
                var metric_by_refs = metrics_by_refs[metric_n].metricName;
                if ($.inArray(metric_by_refs, metricsNotForCombinedReference) != -1) {
                    if ($.inArray(metric_by_refs, metricsNames) == -1) {
                        metrics.push(metrics_by_refs[metric_n]);
                        metricsNames.push(metric_by_refs);
                    }
                }
            }
        }

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
            var totalReads = referenceValues['# total reads'];
            var refMappedReads = referenceValues['Reference mapped (%)'];
            var refPairedReads = referenceValues['Reference properly paired (%)'];

            var numColumns = 1; // no GC in combined reference

            $('#combined_reference_name').show();
            if (refLen) {
                $('#reference_length').show().find('.val').html(toPrettyString(refLen));
                numColumns++;
            }
            if (refFragments) {
                $('#reference_fragments').show().find('.val').html(toPrettyString(refFragments));
                if (refFragments > 1)
                    $('#reference_fragments').find('.plural_ending').show();
                numColumns++;
            }
            var refFiles = 0;
            for (var report_n = 0; report_n < reports.length; report_n++ ) {
                var refName = reports[report_n].name;
                if (refName != 'not_aligned') {
                    refFiles += 1;
                }
            }
            if (refFiles) {
                $('#combined_reference_files').show().find('.val').html(toPrettyString(refFiles));
                if (refFiles > 1)
                    $('#combined_reference_files').find('.plural_ending').show();
            }
            if (refGC) {
                $('#reference_gc').show().find('.val').html(toPrettyString(refGC));
            }
            if (refFeatures) {
                $('#reference_features').show().find('.val').html(toPrettyString(refFeatures));
                numColumns++;
            }
            if (refOperons) {
                $('#reference_operons').show().find('.val').html(toPrettyString(refOperons));
                numColumns++;
            }

            if (totalReads)
                $('#total_reads').show().find('.val').html(toPrettyString(totalReads));
            if (refMappedReads !== undefined)
                $('#reference_mapped_reads').show().find('.val').html(toPrettyString(refMappedReads));
            if (refPairedReads !== undefined)
                $('#reference_paired_reads').show().find('.val').html(toPrettyString(refPairedReads));
            $('#main_ref_genome').html(buildGenomeTable(reports, group_n, numColumns));
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
            var metric = metrics[metric_n];
            var isEmptyRows = true;
            for(report_n = 0; report_n < reports.length; report_n++ ) {  //  add information for each reference
                var metrics_ref = reports[report_n].report[group_n][1];
                for (var metric_ext_n = 0; metric_ext_n < metrics_ref.length; metric_ext_n++){
                    if (metrics_ref[metric_ext_n].metricName == metrics[metric_n].metricName) {
                        isEmptyRows = false;
                        break;
                    }
                }
            }
            table += fillOneRow(metric, mainMetrics, group_n, order, glossary, true, metric.metricName, -1, assembliesNames,
                notAlignedContigs, notExtendedMetrics, isEmptyRows, metricsNotForCombinedReference);
            for(report_n = 0; report_n < reports.length; report_n++ ) {  //  add information for each reference
                var metrics_ref = reports[report_n].report[group_n][1];
                for (var metric_ext_n = 0; metric_ext_n < metrics_ref.length; metric_ext_n++){
                    if (metrics_ref[metric_ext_n].metricName == metrics[metric_n].metricName) {
                        table += fillOneRow(metrics_ref[metric_ext_n], mainMetrics, group_n, order, glossary, false,
                            reports[report_n].name, report_n, assembliesNames, notAlignedContigs, notExtendedMetrics);
                        break;
                    }
                }
            }
            var emptyMetric = metric;
            emptyMetric.values=[];
            for(var n=0; n < assembliesNames.length; n++) {
                emptyMetric.values.push(null);
            }
            for(report_n = 0; report_n < missedRefs.length; report_n++ ) {  //  add information for each reference
                table += fillOneRow(emptyMetric, mainMetrics, group_n, order, glossary, false,
                            missedRefs[report_n], report_n+reports.length, assembliesNames, notAlignedContigs, notExtendedMetrics);
            }
        }
        table += '</tr>';
    }
    table += '</table>';

    //table += '<p id="extended_link"><a class="dotted-link" id="extended_report_link" onclick="extendedLinkClick($(this))">Extended report</a></p>';
    table += buildExtendedLinkClick();

    setUpHeatMap(table);
}

function setPlot(icon) {
    num = icon.attr('id');
    names = ['contigs', 'largest', 'totallen', 'misassemblies', 'misassembled', 'mismatches', 'indels',
            'ns', 'genome', 'duplication', 'nga50'];
    switchSpan = names[num] + '-switch';
    document.getElementById(switchSpan).click();
}