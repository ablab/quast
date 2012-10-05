
function filterAndSumGCinfo(GC_info, condition) {
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

// Initial array of colors. If there are more assemblies, no more colors
// are generated for some reason.
// This problem appeared in the cumulative plot drawer, where I add
// two additional black colors for the reference line.
var colors = ["#FF5900", "#008FFF", "#168A16", "#7C00FF", "#00B7FF", "#FF0080", "#7AE01B", "#782400", "#E01B6A"];

var plotPlaceholderName = '#gc-plot-placeholder';

var plot;
var plotsData;
var maxY;
var minPow;
var ticks;
var legendPlaceholder;

function drawInNormalScale() {
    if (plotsData == null || maxY == null) {
        return;
    }
    plot = $.plot($(plotPlaceholderName), plotsData, {
            shadowSize: 0,
            colors: colors,
            legend: {
                container: legendPlaceholder,
                position: 'ne',
                labelBoxBorderColor: '#FFF',
            },
            grid: {
                borderWidth: 1,
            },
            yaxis: {
                min: 0,
                labelWidth: 120,
                reserveSpace: true,
                lineWidth: 0.5,
                color: '#000',
                tickFormatter: getBpTickFormatter(maxY),
                minTickSize: 1,
            },
            xaxis: {
                min: 0,
                max: 100,
                lineWidth: 0.5,
                color: '#000',
                tickFormatter: function (val, axis) {
                    if (val == 100) {
                        return '&nbsp;100%'
                    } else {
                        return val;
                    }
                }
            },
            minTickSize: 1,
        }
    );
}

function drawInLogarighmicScale() {
    if (plotsData == null || maxY == null || minPow == null) {
        return;
    }
    plot = $.plot($(plotPlaceholderName), plotsData, {
            shadowSize: 0,
            colors: colors,
            legend: {
                container: legendPlaceholder,
                position: 'ne',
                labelBoxBorderColor: '#FFF',
            },
            grid: {
                borderWidth: 1,
            },
            yaxis: {
                min: Math.pow(10, minPow),
                labelWidth: 120,
                reserveSpace: true,
                lineWidth: 0.5,
                color: '#000',
                tickFormatter: getBpLogTickFormatter(maxY),
                minTickSize: 1,
                ticks: ticks,

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
                        return '&nbsp;100%'
                    } else {
                        return val;
                    }
                }
            },
            minTickSize: 1,
        }
    );
}

function setLogScale() {
    $('#normal_scale_label').html(normal_scale_a);
    $('#log_scale_label').html(log_scale_span);
    drawInLogarighmicScale();
}

function setNormalScale() {
    $('#normal_scale_label').html(normal_scale_span);
    $('#log_scale_label').html(log_scale_a);
    drawInNormalScale();
}

var normal_scale_span =
    "<span>" +
        'Normal scale' +
        "</span>";
var normal_scale_a =
    "<a class='dotted-link' onClick='setNormalScale()'>" +
        'Normal scale' +
        "</a>";
var log_scale_span =
    "<span>" +
        'Logarithmic scale' +
        "</span>";
var log_scale_a =
    "<a class='dotted-link' onClick='setLogScale()'>" +
        'Logarithmic scale' +
        "</a>";


function drawGCPlot(filenames, listsOfGCInfo, div, legendPh, glossary) {
    var title = 'GC content';
    legendPlaceholder = legendPh;
    div.html(
        "<div style='width: 675px;'>" +
        "<div class='plot-header' style='float: left'>" + addTooltipIfDefenitionExists(glossary, title) + "</div>" +
        "<div id='change-scale' style='float: right; margin-right: 3px; visibility: hidden;'>" +
        "<span id='normal_scale_label'>" +
        normal_scale_span +
        "</span>&nbsp;/&nbsp;<span id='log_scale_label'>" +
        log_scale_a +
        "</span>" +
        "</div>" +
        "<div style='clear: both'>" +
        "</div>" +
        "<div class='plot-placeholder' id='gc-plot-placeholder'></div>" +
        "</div>"
    );

    var bin_size = 1.0;
    var plotsN = filenames.length;
    plotsData = new Array(plotsN);

    maxY = 0;
    var minY = Number.MAX_VALUE;

    function updateMinY(y) {
        if (y < minY && y != 0) {
            minY = y;
        }
    }
    function updateMaxY(y) {
        if (y > maxY) {
            maxY = y;
        }
    }

    for (var i = 0; i < plotsN; i++) {
        plotsData[i] = {
            data: [],
            label: filenames[i],
        };

        var GC_info = listsOfGCInfo[i];
        var cur_bin = 0.0;

        var x = cur_bin;
        var y = filterAndSumGCinfo(GC_info, function(GC_percent) {
            return GC_percent == cur_bin;
        });
        plotsData[i].data.push([x, y]);

        updateMinY(y);
        updateMaxY(y);

        while (cur_bin < 100.0 - bin_size) {
            cur_bin += bin_size;

            x = cur_bin;
            y = filterAndSumGCinfo(GC_info, function(GC_percent) {
                return GC_percent > (cur_bin - bin_size) && GC_percent <= cur_bin;
            });
            plotsData[i].data.push([x, y]);

            updateMinY(y);
            updateMaxY(y);
        }

        x = 100.0;
        y = filterAndSumGCinfo(GC_info, function(GC_percent) {
            return GC_percent > cur_bin && GC_percent <= 100.0;
        });
        plotsData[i].data.push([x, y]);

        updateMinY(y);
        updateMaxY(y);
    }

    for (i = 0; i < plotsN; i++) {
        plotsData[i].lines = {
            show: true,
            lineWidth: 1,
        }
    }

    // Calculate the minimum possible non-zero Y to clip useless bottoms
    // of logarithmic plots.
    var maxYTick = getMaxDecimalTick(maxY);
    minPow = Math.round(Math.log(minY) / Math.log(10));
    ticks = [];
    for (var pow = minPow; Math.pow(10, pow) < maxYTick; pow++) {
        ticks.push(Math.pow(10, pow));
    }
    ticks.push(Math.pow(10, pow));

    drawInNormalScale();
    $('#change-scale').css('visibility', 'visible');
}
