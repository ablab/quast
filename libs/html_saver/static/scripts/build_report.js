
function showPlotWithInfo(info) {
    var newSeries = [];
    var newColors = [];

    $('#legend-placeholder').find('input:checked').each(function() {
        var number = $(this).attr('name');
        if (number && info.series && info.series.length > 0) {
            i = 0;
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


function buildReport() {
    var assembliesNames;

    var totalReport = null;
    var qualities = null;
    var mainMetrics = null;
    var contigsLens = null;
    var alignedContigsLens = null;
    var refLen = 0;
    var contigs = null;
    var genesInContigs = null;
    var operonsInContigs = null;
    var gcInfos = null;

    var glossary = JSON.parse($('#glossary-json').html());

    var plotsSwitchesDiv = document.getElementById('plots-switches');
    var plotPlaceholder = document.getElementById('plot-placeholder');
    var legendPlaceholder = document.getElementById('legend-placeholder');
    var scalePlaceholder = document.getElementById('scale-placeholder');

    function getToggleFunction(name, drawPlot, data, refLen) {
        return function() {
            this.parentNode.getElementsByClassName('selected-switch')[0].className = 'plot-switch dotted-link';
            this.className = 'plot-switch selected-switch';
            togglePlots(name, drawPlot, data, refLen)
        };
    }

    var toRemoveRefLabel = true;
    function togglePlots(name, drawPlot, data, refLen) {
        if (name === 'cumulative') {
            $(plotPlaceholder).addClass('cumulative-plot-placeholder');
            if (refLen) {
                $('#legend-placeholder').append(
                    '<div id="reference-label">' +
                        '<label for="label_' + assembliesNames.length + '_id" style="color: #000000;">' +
                        '<input type="checkbox" name="' + assembliesNames.length +
                        '" checked="checked" id="label_' + assembliesNames.length +
                        '_id">&nbsp;' + 'Reference,&nbsp;' +
                        toPrettyString(refLen, 'bp') +
                        '</label>' +
                        '</div>');
            }
            toRemoveRefLabel = true;
        } else {
            $(plotPlaceholder).removeClass('cumulative-plot-placeholder');
            if (toRemoveRefLabel) {
                var el = $('#reference-label');
                el.remove();
                toRemoveRefLabel = false;
            }
        }

        $(scalePlaceholder).html('');
        drawPlot(name, colors, assembliesNames, data, refLen, plotPlaceholder, legendPlaceholder, glossary, scalePlaceholder);
    }

    var firstPlot = true;
    function makePlot(name, title, drawPlot, data, refLen) {
        var switchSpan = document.createElement('span');
        switchSpan.id = name + '-switch';
        switchSpan.innerHTML = title;
        plotsSwitchesDiv.appendChild(switchSpan);

        if (firstPlot) {
            switchSpan.className = 'plot-switch selected-switch';
            togglePlots(name, drawPlot, data, refLen);
            firstPlot = false;

        } else {
            switchSpan.className = 'plot-switch dotted-link';
        }

        $(switchSpan).click(getToggleFunction(name, drawPlot, data, refLen));
    }

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

    if (!(totalReport = readJson('total-report'))) {
        console.log("Error: cannot read #total-report-json");
        return 1;
    }

    assembliesNames = totalReport.assembliesNames;
//    if (assembliesNames.length === 0)
//        console.log("Error: no assemblies");
//        return 1;

//    qualities = readJson('qualities');
//    mainMetrics = readJson('main-metrics');
    buildTotalReport(assembliesNames, totalReport.report, totalReport.date, totalReport.minContig, glossary, qualities, mainMetrics);

    if (refLen = readJson('reference-length'))
        refLen = refLen.reflen;

    /****************/
    /* Plots        */

    $(plotsSwitchesDiv).html('<b>Plots:</b>');

    assembliesNames.forEach(function(filename, i) {
        var id = 'label_' + i + '_id';
        $('#legend-placeholder').append('<div>' +
            '<label for="' + id + '" style="color: ' + colors[i] + '">' +
            '<input type="checkbox" name="' + i + '" checked="checked" id="' + id + '">&nbsp;' + filename + '</label>' +
            '</div>');
    });

    if (contigsLens = readJson('contigs-lengths')) {
        makePlot('cumulative', 'Cumulative length', cumulative.draw, contigsLens.lists_of_lengths, refLen);
        makePlot('nx', 'Nx', nx.draw, contigsLens.lists_of_lengths, null);
    }

    if (alignedContigsLens = readJson('aligned-contigs-lengths'))
        makePlot('nax', 'NAx', nx.draw, alignedContigsLens.lists_of_lengths, null);

    if (contigsLens && refLen)
        makePlot('ngx', 'NGx', nx.draw, contigsLens.lists_of_lengths, refLen);

    if (alignedContigsLens && refLen)
        makePlot('ngax', 'NGAx', nx.draw, alignedContigsLens.lists_of_lengths, refLen);

    genesInContigs = readJson('genes-in-contigs');
    operonsInContigs = readJson('operons-in-contigs');

//    if (genesInContigs || operonsInContigs)
//        contigs = readJson('contigs');

    if (genesInContigs) {
        makePlot('genes', 'Genes', gns.draw,  {
                filesFeatureInContigs: genesInContigs.genes_in_contigs,
                kind: 'gene',
            },
            refLen
        );
    }
    if (operonsInContigs) {
        makePlot('operons', 'Operons', gns.draw, {
                filesFeatureInContigs: operonsInContigs.operons_in_contigs,
                kind: 'operon',
            },
            refLen
        );
    }

    if (gcInfos = readJson('gc'))
        makePlot('gc', 'GC content', gc.draw, gcInfos.lists_of_gc_info, refLen);

    return 0;
}




