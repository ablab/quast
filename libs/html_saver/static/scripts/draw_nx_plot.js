
var nx = {
    nx: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null,
    },

    nax: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null,
    },

    ngx: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null,
    },

    ngax: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null,
    },

    draw: function (name, title, colors, filenames, data, refPlotValue,
                    placeholder, legendPlaceholder, glossary, order) {

        var listsOfLengths = data.listsOfLengths;
        var refLength = data.refLen;

        var info = nx[name];

        if (!info.isInitialized) {
            var plotsN = filenames.length;
            info.series = new Array(plotsN);

            for (var i = 0; i < plotsN; i++) {
                var lengths = listsOfLengths[order[i]];

                var size = lengths.length;

                if (name == 'ngx' || name == 'ngax') {
                    sumLen = refLength;
                } else {
                    var sumLen = 0;
                    for (var j = 0; j < lengths.length; j++) {
                        sumLen += lengths[j];
                    }
                }

                info.series[i] = {
                    data: [],
                    label: filenames[order[i]],
                    number: i,
                    color: colors[order[i]],
                };
                info.series[i].data.push([0.0, lengths[0]]);
                var currentLen = 0;
                var x = 0.0;

                for (var k = 0; k < size; k++) {
                    currentLen += lengths[k];
                    info.series[i].data.push([x, lengths[k]]);
                    x = currentLen * 100.0 / sumLen;
                    info.series[i].data.push([x, lengths[k]]);
                }

                if (info.series[i].data[0][1] > info.maxY) {
                    info.maxY = info.series[i].data[0][1];
                }

                var lastPt = info.series[i].data[info.series[i].data.length-1];
                info.series[i].data.push([lastPt[0], 0]);
            }

            for (i = 0; i < plotsN; i++) {
                info.series[i].lines = {
                    show: true,
                    lineWidth: 1,
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
                            mouseActiveRadius: 1000,
                        },
                        yaxis: {
                            min: 0,
//                        max: info.maxY,
                            labelWidth: 120,
                            reserveSpace: true,
                            lineWidth: 0.5,
                            color: '#000',
                            tickFormatter: getBpTickFormatter(info.maxY),
                            minTickSize: 1,
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
                        minTickSize: 1,
                    }
                );

                var firstLabel = $('.yAxis .tickLabel').last();
                firstLabel.prepend(title + '<span class="rhs">&nbsp;</span>=<span class="rhs">&nbsp;</span>');

                bindTip(placeholder, series, plot, toPrettyString, '%', 'top right');

            };

            info.isInitialized = true;
        }

        $.each(info.series, function(i, series) {
            $('#legend-placeholder').find('#label_' + series.number + '_id').click(function() {
                showPlotWithInfo(info);
            });
        });

        showPlotWithInfo(info);

        $('#contigs_are_ordered').hide();
        $('#gc_info').hide();
    }
};


