
var normal_scale_span =
    "<span class='selected-switch'>" +
        'Normal' +
        "</span>";
var normal_scale_a =
    "<a class='dotted-link' onClick='setNormalScale()'>" +
        'Normal' +
        "</a>";
var log_scale_span =
    "<span class='selected-switch'>" +
        'logarithmic' +
        "</span>";
var log_scale_a =
    "<a class='dotted-link' onClick='setLogScale()'>" +
        'logarithmic' +
        "</a>";

var show_all_span =
    "<span class='selected-switch gc'>" +
        'Back to overview' +
        "</span>";
var show_all_a =
    "&nbsp;&nbsp;&nbsp;<a class='dotted-link' onClick='showAll()'>" +
        'Back to overview' +
        "</a>";

var gc = {
    isInitialized: false,

    maxY: 0,
    plot: null,
    series: null,
    showWithData: null,
    minPow: 0,
    ticks: null,
    placeholder: null,
    legendPlaceholder: null,
    colors: null,
    yAxisLabeled: false,

    show_all_el: show_all_span,
    reference: false,

    normal_scale_el: null,
    log_scale_el: null,

    draw: function(name, title, colors, filenames, gcInfos, reflen, tickX,
                   placeholder, legendPlaceholder, glossary, order, scalePlaceholder) {
        gc.normal_scale_el = normal_scale_span;
        gc.log_scale_el = log_scale_a;
        $(scalePlaceholder).html(
            "<div id='change-scale' style='margin-right: 3px; visibility: hidden;'>" +
                "<span id='normal_scale_label'>" +
                gc.normal_scale_el +
                "</span>&nbsp;/&nbsp;" +
                "<span id='log_scale_label'>" +
                gc.log_scale_el +
                "</span> scale" +
                "</div>"
        );

        var refIndex = gcInfos.reference_index;
        if (!gc.isInitialized) {
            gc.legendPlaceholder = legendPlaceholder;
            gc.placeholder = placeholder;
            gc.colors = colors;
            gc.filenames = filenames;

            var bin_size = 1.0;
            var plotsN = filenames.length;
            gc.series = new Array(plotsN + 1);
            gc.series[0] = new Array(plotsN);

            gc.maxY = 0;
            var minY = Number.MAX_VALUE;

            function updateMinY(y) {
                if (y < minY && y != 0) {
                    minY = y;
                }
            }
            function updateMaxY(y) {
                if (y > gc.maxY) {
                    gc.maxY = y;
                }
            }

            for (var i = 0; i < plotsN; i++) {
                gc.series[0][i] = {
                    data: [],
                    label: filenames[order[i]],
                    number: order[i],
                    color: colors[order[i]]
                };
            }

            function makeSeriesFromDistributions(distributionsXandY, series_i, plot_i) {
                var distributionsX = distributionsXandY[0];
                var distributionsY = distributionsXandY[1];

                for (var j = 0; j < distributionsX.length; j++) {
                    var x = distributionsX[j];
                    var y = distributionsY[j];
                    gc.series[series_i][plot_i].data.push([x, y]);
                    updateMinY(y);
                    updateMaxY(y);
                }
            }

            function makeSeries(listsOfGCInfo, listOfGcDistributions, seriesIdx) {
                for (var i = 0; i < plotsN; i++) {
                    if (listsOfGCInfo) {
                        makeSeriesFromInfo(listsOfGCInfo[order[i]], seriesIdx, i);
                    } else {
                        makeSeriesFromDistributions(listOfGcDistributions[order[i]], seriesIdx, i);
                    }
                }
            }

            var listsOfGCInfo = gcInfos.lists_of_gc_info;
            var listOfGcDistributions = gcInfos.list_of_GC_distributions;
            makeSeries(listsOfGCInfo, listOfGcDistributions, 0);

            function makeSeriesFromInfo(GC_info, series_i, i) {
                var cur_bin = 0.0;

                var x = cur_bin;
                var y = filterAndSumGcInfo(GC_info, function(GC_percent) {
                    return GC_percent == cur_bin;
                });
                gc.series[series_i][i].data.push([x, y]);

                updateMinY(y);
                updateMaxY(y);

                while (cur_bin < 100.0 - bin_size) {
                    cur_bin += bin_size;

                    x = cur_bin;
                    y = filterAndSumGcInfo(GC_info, function(GC_percent) {
                        return GC_percent > (cur_bin - bin_size) && GC_percent <= cur_bin;
                    });
                    gc.series[series_i][i].data.push([x, y]);

                    updateMinY(y);
                    updateMaxY(y);
                }

                x = 100.0;
                y = filterAndSumGcInfo(GC_info, function(GC_percent) {
                    return GC_percent > cur_bin && GC_percent <= 100.0;
                });

                gc.series[series_i][i].data.push([x, y]);

                updateMinY(y);
                updateMaxY(y);
            }

            for (i = 0; i < plotsN; i++) {
                if (typeof broken_scaffolds_labels !== undefined && $.inArray(filenames[order[i]], broken_scaffolds_labels) != -1) {
                    gc.series[0][i].dashes = {
                        show: true,
                        lineWidth: 1
                    };
                }
                else {
                    gc.series[0][i].lines = {
                        show: true,
                        lineWidth: 1
                    };
                }
            }

            if (refIndex) {
                gc.reference = true;
                gc.series[0].push({
                    data: [],
                    label: 'reference',
                    isReference: true,
                    number: filenames.length,
                    lines: {},
                    dashes: {
                        show: true,
                        lineWidth: 1
                    },
                    color: '#000000'
                });
                if (listsOfGCInfo) {
                    makeSeriesFromInfo(listsOfGCInfo[refIndex], 0, refIndex);
                } else {
                    makeSeriesFromDistributions(listOfGcDistributions[refIndex], 0, refIndex);
                }
                gc.colors.push('#000000')
            }

            if (gcInfos.list_of_GC_contigs_distributions) {
                listOfGcDistributions = gcInfos.list_of_GC_contigs_distributions;
                var maxY = 0;
                for (var file_n = 0; file_n < filenames.length; file_n++) {
                    gc.series[file_n + 1] = new Array(1);
                    gc.series[file_n + 1][0] = {
                        data: [],
                        label: filenames[order[file_n]],
                        number: order[file_n],
                        color: colors[order[file_n]],
                        bars: {
                            show: true,
                            lineWidth: 0.6,
                            fill: 0.6,
                            barWidth: 5
                        }
                    };

                    var distributionsX = listOfGcDistributions[file_n][0];
                    var distributionsY = listOfGcDistributions[file_n][1];

                    for (var j = 0; j < distributionsX.length; j++) {
                        var x = distributionsX[j];
                        var y = distributionsY[j];
                        gc.series[file_n + 1][0].data.push([x, y]);
                        maxY = Math.max(maxY, y);
                    }
                }
                for (var file_n = 0; file_n < filenames.length; file_n++) {
                    gc.series[file_n + 1][0].maxY = maxY;
                }
            }
            // Calculate the minimum possible non-zero Y to clip useless bottoms
            // of logarithmic plots.
            var maxYTick = getMaxDecimalTick(gc.maxY);
            gc.minPow = Math.round(Math.log(minY) / Math.log(10));
            gc.ticks = [];
            for (var pow = gc.minPow; Math.pow(10, pow) < maxYTick; pow++) {
                gc.ticks.push(Math.pow(10, pow));
            }
            gc.ticks.push(Math.pow(10, pow));

            gc.isInitialized = true;
        }

        gc.showWithData = showInNormalScaleWithData;
        if (gcInfos.list_of_GC_contigs_distributions) {
            createLegend(gc.filenames, gc.colors, 0, gc.reference);
        }
        addLegendClickEvents(gc, filenames.length, showPlotWithInfo, false, 0);

        showPlotWithInfo(gc, 0);

        $('#change-scale').css('visibility', 'visible');
        $('#gc_info').show();
    }
};

function showAll() {
    $('#change-scale').show();
    $('#gc_info').show();
    $('#gc_contigs_info').hide();
    createLegend(gc.filenames, gc.colors, 0, gc.reference);

    gc.show_all_el = show_all_span;
    gc.showWithData = gc.log_scale_el == log_scale_a ? showInNormalScaleWithData : showInLogarithmicScaleWithData;

    $('#show_all_label').html(gc.show_all_el);
    showPlotWithInfo(gc, 0);
}

function showPlot(index) {
    $('#change-scale').hide();
    $('#gc_info').hide();
    $('#gc_contigs_info').show();
    createLegend([gc.filenames[index - 1]], [gc.colors[index - 1]], index);

    gc.show_all_el = show_all_a;
    gc.showWithData = showOneAssembly;

    $('#show_all_label').html(gc.show_all_el);
    showPlotWithInfo(gc, index);
}

function showOneAssembly(series, colors) {
    if (series == null) {
        return;
    }

    gc.plot = $.plot(gc.placeholder, series, {
            shadowSize: 0,
            colors: colors,
            legend: {
                container: $('useless-invisible-element-that-does-not-even-exist')
            },
            grid: {
                hoverable: true,
                borderWidth: 1,
                autoHighlight: false,
                mouseActiveRadius: 1000
            },
            yaxis: {
                min: 0,
                max: series[0].maxY * 1.1,
                labelWidth: 120,
                reserveSpace: true,
                lineWidth: 0.5,
                color: '#000',
                tickFormatter: getJustNumberTickFormatter(gc.maxY),
                minTickSize: 1
            },
            xaxis: {
                min: 0,
                max: 100,
                lineWidth: 0.5,
                color: '#000',
                tickFormatter: function (val, axis) {
                    if (val == 100) {
                        return '&nbsp;100% GC'
                    } else {
                        return val;
                    }
                }
            },
            minTickSize: 1
        }
    );

    var firstLabel = $('.yAxis .tickLabel').last();
    firstLabel.append(' contigs');
    unBindTips(gc.placeholder);
    bindTip(gc.placeholder, series, gc.plot, getIntervalToPrettyString(5), 1, '%<span class="rhs">&nbsp;</span>GC', 'top right');
}

function showInNormalScaleWithData(series, colors) {
    if (series == null || gc.maxY == null)
        return;

    gc.yAxisLabeled = false;

    gc.plot = $.plot(gc.placeholder, series, {
            shadowSize: 0,
            colors: colors,
            legend: {
                container: $('useless-invisible-element-that-does-not-even-exist'),
            },
            grid: {
                borderWidth: 1,
                hoverable: true,
                autoHighlight: false,
                mouseActiveRadius: 1000,
            },
            yaxis: {
                min: 0,
//                max: gc.maxY + 0.1 * gc.maxY,
                labelWidth: 120,
                reserveSpace: true,
                lineWidth: 0.5,
                color: '#000',
                tickFormatter: windowsTickFormatter,
                minTickSize: 1
            },
            xaxis: {
                min: 0,
                max: 100,
                lineWidth: 0.5,
                color: '#000',
                tickFormatter: function (val, axis) {
                    if (val == 100) {
                        return '&nbsp;100% GC'
                    } else {
                        return val;
                    }
                }
            },
            minTickSize: 1
        }
    );

    var firstLabel = $('.yAxis .tickLabel').last();
    firstLabel.append(' windows');
    bindTip(gc.placeholder, series, gc.plot, toPrettyString, 1, '%<span class="rhs">&nbsp;</span>GC', 'top right');
}


function showInLogarithmicScaleWithData(series, colors) {
    if (series == null || gc.maxY == null || gc.minPow == null) {
        return;
    }

    gc.yAxisLabeled = false;

    gc.plot = $.plot(gc.placeholder, series, {
            shadowSize: 0,
            colors: colors,
            legend: {
                container: $('useless-invisible-element-that-does-not-even-exist'),
            },
            grid: {
                hoverable: true,
                borderWidth: 1,
                autoHighlight: false,
                mouseActiveRadius: 1000
            },
            yaxis: {
                min: Math.pow(10, gc.minPow),
//                max: gc.maxY,
                labelWidth: 120,
                reserveSpace: true,
                lineWidth: 0.5,
                color: '#000',
                tickFormatter: windowsTickFormatter,
                minTickSize: 1,
                ticks: gc.ticks,

                transform:  function(v) {
                    return Math.log(v + 0.0001)/*move away from zero*/ / Math.log(10);
                },
                inverseTransform: function(v) {
                    return Math.pow(v, 10);
                },
                tickDecimals: 3
            },
            xaxis: {
                min: 0,
                max: 100,
                lineWidth: 0.5,
                color: '#000',
                tickFormatter: function (val, axis) {
                    if (val == 100) {
                        return '&nbsp;100%<span class="rhs">&nbsp;</span>GC'
                    } else {
                        return val;
                    }
                }
            },
            minTickSize: 1
        }
    );

    var firstLabel = $('.yAxis .tickLabel').last();
    firstLabel.append(' windows');
    bindTip(gc.placeholder, series, gc.plot, toPrettyString, 1, '% GC', 'top right');
}


function setLogScale() {
    gc.normal_scale_el = normal_scale_a;
    gc.log_scale_el = log_scale_span;
    gc.showWithData = showInLogarithmicScaleWithData;

    $('#normal_scale_label').html(gc.normal_scale_el);
    $('#log_scale_label').html(gc.log_scale_el);
    showPlotWithInfo(gc, 0);
}


function setNormalScale() {
    gc.normal_scale_el = normal_scale_span;
    gc.log_scale_el = log_scale_a;
    gc.showWithData = showInNormalScaleWithData;

    $('#normal_scale_label').html(gc.normal_scale_el);
    $('#log_scale_label').html(gc.log_scale_el);
    showPlotWithInfo(gc, 0);
}



function filterAndSumGcInfo(GC_info, condition) {
    var contigs_lengths_cur_bin = [];
    for (var j = 0; j < GC_info.length; j++) {
        var GC = GC_info[j];
        var contig_length = GC[0];
        var GC_percent = GC[1];

        if (condition(GC_percent) == true) {
            contigs_lengths_cur_bin.push(contig_length);
        }
    }
    var val_bp = 0;
    for (var j = 0; j < contigs_lengths_cur_bin.length; j++) {
        val_bp += contigs_lengths_cur_bin[j];
    }
    return val_bp;
}

function createLegend(labels, colors, index, reference) {
    var selectedAssemblies = getSelectedAssemblies(labels);
    $('#legend-placeholder').empty();
    var selectors = "";

    labels.forEach(function(label, i) {
        var link = index ? '' : '<span id="' + labels[i] + '-switch"' + "class='plot-gc-type-switch dotted-link'>by contigs<br></span><br>";
        var assemblyIdx = gc.filenames.indexOf(label);
        addLabelToLegend(assemblyIdx, label, selectedAssemblies, colors, link);
    });
    if (reference) {
        isChecked = (selectedAssemblies.length > 0 && selectedAssemblies.indexOf(gc.filenames.length.toString())) != -1 ? 'checked="checked"' : "";
        $('#legend-placeholder').append(
            '<div id="reference-label">' +
                '<label for="reference" style="color: #000000;">' +
                '<input type="checkbox" name="' + gc.filenames.length +
                '" checked="' + isChecked + '" id="reference">&nbsp;' + 'reference' +
                '</label>' +
                '</div>'
        );
    }
    if (index > 0) {
        for (var filenames_n = 0; filenames_n < gc.filenames.length; filenames_n++){
            selectors += '<br><span id="' + gc.filenames[filenames_n] + '-switch" ' +
                "class='plot-switch dotted-link'>" +
                gc.filenames[filenames_n] + "</span>";
        }
        $('#legend-placeholder').append(
            "<br><br><div id='change-assembly' style='margin-right: 3px;'>" +
                "<span id='show_all_label'>" +
                gc.show_all_el +
                "</span><br>" + selectors +
                "</div>"
        );
    }
    addLinksToSwitches(index - 1);
    addLegendClickEvents(gc, gc.series.length, showPlotWithInfo, false, index);
}

function addLinksToSwitches(index) {
    var filenames = gc.filenames;
    for (filenames_n = 0; filenames_n < filenames.length; filenames_n++){
        var switchSpan = document.getElementById(filenames[filenames_n] + "-switch");
        $(switchSpan).click(getToggleSwitchFunction(filenames_n + 1));
        if (filenames_n == index) {
            switchSpan.className = 'plot-switch selected-switch gc';
        }
    }
}

function getToggleSwitchFunction(index) {
    return function() {
        if (index > 0) {
            showPlot(index);
        }
        else {
            showAll();
        }
    };
}