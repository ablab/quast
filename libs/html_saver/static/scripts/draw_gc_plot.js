
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

    normal_scale_el: normal_scale_span,
    log_scale_el: log_scale_a,

    draw: function(name, title, colors, filenames, gcInfos, reflen, tickX,
                   placeholder, legendPlaceholder, glossary, order, scalePlaceholder) {
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

        if (!gc.isInitialized) {
            gc.legendPlaceholder = legendPlaceholder;
            gc.placeholder = placeholder;
            gc.colors = colors;

            var bin_size = 1.0;
            var plotsN = filenames.length;
            gc.series = new Array(plotsN);

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
                gc.series[i] = {
                    data: [],
                    label: filenames[order[i]],
                    number: i,
                    color: colors[order[i]],
                };
            }

            function makeSeriesFromDistributions(listOfGcDistributions) {
                for (var i = 0; i < plotsN; i++) {
                    var distributionsXandY = listOfGcDistributions[order[i]];
                    var distributionsX = distributionsXandY[0];
                    var distributionsY = distributionsXandY[1];

                    for (var j = 0; j < distributionsX.length; j++) {
                        var x = distributionsX[j];
                        var y = distributionsY[j];
                        gc.series[i].data.push([x, y]);
                        updateMinY(y);
                        updateMaxY(y);
                    }
                }
            }

            var listsOfGCInfo = gcInfos.lists_of_gc_info;
            if (listsOfGCInfo) {
                makeSeriesFromInfo(listsOfGCInfo);
            } else {
                var listOfGcDistributions = gcInfos.list_of_GC_distributions;
                makeSeriesFromDistributions(listOfGcDistributions);
            }

            function makeSeriesFromInfo(listsOfGCInfo) {
                for (var i = 0; i < plotsN; i++) {
                    var GC_info = listsOfGCInfo[order[i]];

                    var cur_bin = 0.0;

                    var x = cur_bin;
                    var y = filterAndSumGcInfo(GC_info, function(GC_percent) {
                        return GC_percent == cur_bin;
                    });
                    gc.series[i].data.push([x, y]);

                    updateMinY(y);
                    updateMaxY(y);

                    while (cur_bin < 100.0 - bin_size) {
                        cur_bin += bin_size;

                        x = cur_bin;
                        y = filterAndSumGcInfo(GC_info, function(GC_percent) {
                            return GC_percent > (cur_bin - bin_size) && GC_percent <= cur_bin;
                        });
                        gc.series[i].data.push([x, y]);

                        updateMinY(y);
                        updateMaxY(y);
                    }

                    x = 100.0;
                    y = filterAndSumGcInfo(GC_info, function(GC_percent) {
                        return GC_percent > cur_bin && GC_percent <= 100.0;
                    });

                    gc.series[i].data.push([x, y]);

                    updateMinY(y);
                    updateMaxY(y);
                }
            }

            for (i = 0; i < plotsN; i++) {
                gc.series[i].lines = {
                    show: true,
                    lineWidth: 1,
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

            gc.showWithData = showInNormalScaleWithData;

            gc.isInitialized = true;
        }

        $.each(gc.series, function(i, series) {
            $('#legend-placeholder').find('#label_' + series.number + '_id').click(function() {
                showPlotWithInfo(gc);
            });
        });

        showPlotWithInfo(gc);

        $('#change-scale').css('visibility', 'visible');

        $('#contigs_are_ordered').hide();
        $('#gc_info').show();
    }
};


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
                minTickSize: 1,
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
            minTickSize: 1,
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
                mouseActiveRadius: 1000,
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
                tickDecimals: 3,
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
            minTickSize: 1,
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
    showPlotWithInfo(gc);
}


function setNormalScale() {
    gc.normal_scale_el = normal_scale_span;
    gc.log_scale_el = log_scale_a;
    gc.showWithData = showInNormalScaleWithData;

    $('#normal_scale_label').html(gc.normal_scale_el);
    $('#log_scale_label').html(gc.log_scale_el);
    showPlotWithInfo(gc);
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
