
var nx = {
    nx: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null
    },

    nax: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null
    },

    ngx: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null
    },

    ngax: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null
    },

    draw: function (name, title, colors, filenames, data, refPlotValue, tickX,
                    placeholder, legendPlaceholder, glossary, order, scalePlaceholder) {

        $(scalePlaceholder).empty();

        var coordX = data.coord_x;
        var coordY = data.coord_y;

        var cur_filenames = data.filenames;
        var info = nx[name];

        if (!info.isInitialized) {
            var plotsN = cur_filenames.length;
            info.series = new Array(plotsN);

            for (var i = 0; i < plotsN; i++) {
                var index = $.inArray(cur_filenames[order[i]], filenames);
                var plot_coordX = coordX[order[i]];
                var plot_coordY = coordY[order[i]];
                var size = plot_coordX.length;

                info.series[i] = {
                    data: [],
                    label: filenames[index],
                    number: index,
                    color: colors[index]
                };
                info.series[i].data.push([0.0, plot_coordY[0]]);
                var currentLen = 0;
                var x = 0.0;

                for (var k = 0; k < size; k++) {
                    info.series[i].data.push([plot_coordX[k], plot_coordY[k]]);
                }

                if (info.series[i].data[0][1] > info.maxY) {
                    info.maxY = info.series[i].data[0][1];
                }

                var lastPt = info.series[i].data[info.series[i].data.length-1];
                info.series[i].data.push([lastPt[0], 0]);
            }

            for (i = 0; i < plotsN; i++) {
                if (typeof broken_scaffolds_labels !== undefined && $.inArray(filenames[order[i]], broken_scaffolds_labels) != -1) {
                    info.series[i].dashes = {
                        show: true,
                        lineWidth: 1
                    };
                }
                else {
                    info.series[i].lines = {
                        show: true,
                        lineWidth: 1
                    };
                }
            }

            // for (i = 0; i < plotsN; i++) {
            //     plotsData[i].points = {
            //         show: true,
            //         radius: 1,
            //         fill: 1,
            //         fillColor: false,
            //     }
            // }

            info.showWithData = function(series, colors) {
                var plot = $.plot(placeholder, series, {
                        shadowSize: 0,
                        colors: colors,
                        legend: {
                            container: $('useless-invisible-element-that-does-not-even-exist'),
                        },
                        grid: {
                            borderWidth: 1,
                            hoverable: true,
                            autoHighlight: false,
                            mouseActiveRadius: 1000
                        },
                        yaxis: {
                            min: 0,
//                        max: info.maxY,
                            labelWidth: 120,
                            reserveSpace: true,
                            lineWidth: 0.5,
                            color: '#000',
                            tickFormatter: getBpTickFormatter(info.maxY),
                            minTickSize: 1
                        },
                        xaxis: {
                            min: 0,
                            max: 100,
                            lineWidth: 0.5,
                            color: '#000',
                            tickFormatter: function (val, axis) {
                                if (val == 100) {
                                    return '&nbsp;x<span class="rhs">&nbsp;</span>=<span class="rhs">&nbsp;</span>100%'
                                } else {
                                    return val;
                                }
                            }
                        },
                        minTickSize: tickX
                    }
                );

                var firstLabel = $('.yAxis .tickLabel').last();
                firstLabel.prepend(title + '<span class="rhs">&nbsp;</span>=<span class="rhs">&nbsp;</span>');

                bindTip(placeholder, series, plot, toPrettyString, 1, '%', 'top right');

            };

            info.isInitialized = true;
        }

        addLegendClickEvents(info, filenames.length, showPlotWithInfo);
        showPlotWithInfo(info);
    }
};


