
var frc = {
    genes: {
        isInitialized: false,

        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null,

        yAxisLabeled: false
    },

    operons: {
        isInitialized: false,

        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null,

        yAxisLabeled: false
    },

    misassemblies: {
        isInitialized: false,

        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null,

        yAxisLabeled: false
    },

    draw: function (name, title, colors, filenames, data, refGenesNumber, tickX,
                    placeholder, legendPlaceholder, glossary, order, scalePlaceholder) {
//    div.html(
//        "<span class='plot-header'>" + kind[0].toUpperCase() + kind.slice(1) + "s covered</span>" +
//        "<div class='plot-placeholder' id='" + kind + "s-plot-placeholder'></div>"
//    );
        $(scalePlaceholder).empty();

        var info = frc[name];
        var coordX = data.coord_x;
        var coordY = data.coord_y;

        info.yAxisLabeled = false;
        var cur_filenames = data.filenames;
        if (!info.isInitialized) {
            var plotsN = cur_filenames.length;
            info.series = new Array(plotsN);

            info.maxY = 0;
            info.maxX = 0;

            for (var i = 0; i < plotsN; i++) {
                var index = $.inArray(cur_filenames[order[i]], filenames);
                var plot_coordX = coordX[order[i]];
                var plot_coordY = coordY[order[i]];
                var featureSpace = plot_coordX[plot_coordX.length - 1];
                var maxY = plot_coordY[plot_coordY.length - 1];

                info.series[i] = {
                    data: [],
                    label: filenames[index],
                    number: index,
                    color: colors[index]
                };

                info.series[i].data.push([0.0, plot_coordY[0]]);

                if (featureSpace > info.maxX) {
                    info.maxX = featureSpace;
                }
                for (var k = 0; k < plot_coordX.length; k++) {
                    info.series[i].data.push([plot_coordX[k], plot_coordY[k]]);
                }

                if (maxY > info.maxY) {
                    info.maxY = maxY;
                }
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

            info.showWithData = function(series, colors) {
                var plot = $.plot(placeholder, series, {
                    shadowSize: 0,
                    colors: colors,
                    legend: {
                        container: $('useless-invisible-element-that-does-not-even-exist')
                    },
                    grid: {
                        borderWidth: 1,
                        hoverable: true,
                        autoHighlight: false,
                        mouseActiveRadius: 1000,
                    },
                    yaxis: {
                        min: 0,
                        max: Math.max(100, info.maxY),
                        labelWidth: 145,
                        reserveSpace: true,
                        lineWidth: 0.5,
                        color: '#000',
                        tickFormatter: getPercentTickFormatter(Math.max(100, info.maxY)),
                        minTickSize: 1
                    },
                    xaxis: {
                        min: 0,
                        max: info.maxX,
                        lineWidth: 0.5,
                        color: '#000',
                        tickFormatter: getJustNumberTickFormatter(info.maxX, ' ' + name),
                        minTickSize: tickX
                    }
                });

                var firstLabel = $('.yAxis .tickLabel').last();
                firstLabel.prepend('Genome coverage<span class="rhs">&nbsp;</span>=<span class="rhs">&nbsp;</span>');

                bindTip(placeholder, series, plot, frcNumberToPrettyString, tickX, name, 'bottom right', false, '%');
            };

            info.isInitialized = true;
        }

        addLegendClickEvents(info, filenames.length, showPlotWithInfo, refGenesNumber);
        showPlotWithInfo(info);
        $('#frc_info').show();
        $('.frc_plot_name').html(name);
    }
};


