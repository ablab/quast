
var gns = {
    genes: {
        isInitialized: false,

        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null,

        yAxisLabeled: false,
    },

    operons: {
        isInitialized: false,

        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null,

        yAxisLabeled: false,
    },

    draw: function (name, title, colors, filenames, data, refGenesNumber,
                    placeholder, legendPlaceholder, glossary, order, scalePlaceholder) {
//    div.html(
//        "<span class='plot-header'>" + kind[0].toUpperCase() + kind.slice(1) + "s covered</span>" +
//        "<div class='plot-placeholder' id='" + kind + "s-plot-placeholder'></div>"
//    );
        $(scalePlaceholder).empty()

        var info = gns[name];

        info.yAxisLabeled = false;

        if (!info.isInitialized) {
            var filesFeatureInContigs = data.filesFeatureInContigs;
            var kind = data.kind;

            var plotsN = filenames.length;
            info.series = new Array(plotsN);

            info.maxY = 0;
            info.maxX = 0;

            if (refGenesNumber) {
                info.maxY = refGenesNumber;
            }

            for (var fi = 0; fi < plotsN; fi++) {
                var filename = filenames[order[fi]];
                var featureInContigs = filesFeatureInContigs[filename];

                info.series[fi] = {
                    data: [[0, 0]],
                    label: filenames[order[fi]],
                    number: fi,
                    color: colors[order[fi]],
                };

                var contigNo = 0;
                var totalFull = 0;

                for (var k = 0; k < featureInContigs.length; k++) {
                    contigNo += 1;
                    totalFull += featureInContigs[k];

                    info.series[fi].data.push([contigNo, totalFull]);

                    if (info.series[fi].data[k][1] > info.maxY) {
                        info.maxY = info.series[fi].data[k][1];
                    }
                }

                if (featureInContigs.length > info.maxX) {
                    info.maxX = featureInContigs.length;
                }
            }

            for (i = 0; i < plotsN; i++) {
                info.series[i].lines = {
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

            if (refGenesNumber) {
                info.series.push({
                    data: [[0, refGenesNumber], [info.maxX, refGenesNumber]],
                    label: 'reference,&nbsp;' + toPrettyString(refGenesNumber, 'genes'),
                    isReference: true,
                    dashes: {
                        show: true,
                        lineWidth: 1,
                    },
                    yaxis: 1,
                    number: info.series.length,
                    color: '#000000',
                });

                colors.push('#000000');
            }

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
                        tickFormatter: function (val, axis) {
//                            if (!info.yAxisLabeled && val > info.maxY) {
//                                info.yAxisLabeled = true;
//                                var res = val + ' ' + kind;
//                                if (val > 1) {
//                                    res += 's'
//                                }
//                                return res;
//                            } else {
                            return val;
//                            }
                        },
                        minTickSize: 1,
                    },
                    xaxis: {
                        min: 0,
                        max: info.maxX,
                        lineWidth: 0.5,
                        color: '#000',
                        tickFormatter: getContigNumberTickFormatter(info.maxX),
                        minTickSize: 1,
                    },
                });

                var firstLabel = $('.yAxis .tickLabel').last();
                firstLabel.append(' ' + name);

                bindTip(placeholder, series, plot, ordinalNumberToPrettyString, 'contig', 'bottom right');
            };

            info.isInitialized = true;
        }

        $.each(info.series, function(i, series) {
            $('#legend-placeholder').find('#label_' + i + '_id').click(function() {
                showPlotWithInfo(info);
            });
        });

        showPlotWithInfo(info);

        $('#contigs_are_ordered').show();
        $('#gc_info').hide();
    }
};


