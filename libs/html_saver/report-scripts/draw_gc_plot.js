
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


function drawGCPlot(filenames, listsOfGCInfo, div, legendPlaceholder, glossary) {
    var title = 'GC content';
    div.html(
        "<div class='plot'>" +
            "<span class='plot-header'>" + addTooltipIfDefenitionExists(glossary, title) + "</span>" +
            "<div style='width: 800px; height: 600px;' id='gc-plot-placeholder'></div>" +
            "</div>"
    );

    var bin_size = 1.0;
    var plotsN = filenames.length;
    var plotsData = new Array(plotsN);

    var maxY = 0;

    for (var i = 0; i < plotsN; i++) {
        plotsData[i] = {
            data: new Array(),
            label: filenames[i],
        };

        var GC_info = listsOfGCInfo[i];
        var cur_bin = 0.0;

        var x = cur_bin;
        var y = filterAndSumGCinfo(GC_info, function(GC_percent) {
            return GC_percent == cur_bin;
        });
        plotsData[i].data.push([x, y]);

        if (maxY < y) {
            maxY = y;
        }

        while (cur_bin < 100.0 - bin_size) {
            cur_bin += bin_size;

            x = cur_bin;
            y = filterAndSumGCinfo(GC_info, function(GC_percent) {
                return GC_percent > (cur_bin - bin_size) && GC_percent <= cur_bin;
            });
            plotsData[i].data.push([x, y]);

            if (maxY < y) {
                maxY = y;
            }
        }

        x = 100.0;
        y = filterAndSumGCinfo(GC_info, function(GC_percent) {
            return GC_percent > cur_bin && GC_percent <= 100.0;
        });
        plotsData[i].data.push([x, y]);

        if (maxY < y) {
            maxY = y;
        }

//        if (plotsData[i].data[0][1] > maxY) {
//            maxY = plotsData[i].data[0][1];
//        }

    }

    for (i = 0; i < plotsN; i++) {
        plotsData[i].lines = {
            show: true,
            lineWidth: 1,
        }
    }

//    for (i = 0; i < plotsN; i++) {
//        plotsData[i].points = {
//            show: true,
//            radius: 1,
//            fill: 1,
//            fillColor: false,
//        }
//    }
    var colors = ["#FF5900", "#008FFF", "#168A16", "#7C00FF", "#00B7FF", "#FF0080", "#7AE01B", "#782400", "#E01B6A"];

    var plot = $.plot($('#gc-plot-placeholder'), plotsData, {
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