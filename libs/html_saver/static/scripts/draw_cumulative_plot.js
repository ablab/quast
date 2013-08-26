
var cumulative = {
    isInitialized: false,

    maxX: 0,
    maxY: 0,
    maxYTick: 0,
    series: null,
    showWithData: null,
    colors: [],

    draw: function(name, title, colors, filenames, listsOfLengths, refLenght,
                   placeholder, legendPlaceholder, glossary, order) {

        if (!this.isInitialized) {
            //    div.html(
            //        "<span class='plot-header'>" + addTooltipIfDefinitionExists(glossary, title) + "</span>" +
            //        "<div class='plot-placeholder' id='cumulative-plot-placeholder'></div>"
            //    );
            cumulative.series = [];
            var plotsN = filenames.length;

            if (refLenght) {
                cumulative.maxY = refLenght;
            }

            cumulative.colors = colors;

            for (var i = 0; i < plotsN; i++) {
                var lengths = listsOfLengths[order[i]];
                var asm_name = filenames[order[i]];
                var color = colors[order[i]];
                
                var size = lengths.length;

                cumulative.series[i] = {
                    data: new Array(size+1),
                    label: asm_name,
                    number: i,
                    color: color,
                };

                cumulative.series[i].data[0] = [0, 0];

                var y = 0;
                for (var j = 0; j < size; j++) {
                    y += lengths[j];
                    cumulative.series[i].data[j+1] = [j+1, y];
                    if (y > cumulative.maxY) {
                        cumulative.maxY = y;
                    }
                }

                if (size > cumulative.maxX) {
                    cumulative.maxX = size;
                }
            }

//            var lineColors = [];
//
//            for (i = 0; i < colors.length; i++) {
//                lineColors.push(changeColor(colors[i], 0.9, false));
//            }

            for (i = 0; i < plotsN; i++) {
                cumulative.series[i].lines = {
                    show: true,
                    lineWidth: 1,
                    //                color: lineColors[i],
                };
                //    In order to draw dots instead of lines
                cumulative.series[i].points = {
                    show: false,
                    radius: 1,
                    fill: 1,
                    fillColor: false,
                };
            }

            for (i = 0; i < plotsN; i++) {
                cumulative.colors.push(cumulative.series[i].color);
            }

            cumulative.maxYTick = getMaxDecimalTick(cumulative.maxY);

            if (refLenght) {
                cumulative.series.push({
                    data: [[0, refLenght], [cumulative.maxX, refLenght]],
                    label: 'reference,&nbsp;' + toPrettyString(refLenght, 'bp'),
                    isReference: true,
                    dashes: {
                        show: true,
                        lineWidth: 1,
                    },
                    yaxis: 1,
                    number: cumulative.series.length,
                    color: '#000000',
                });

                cumulative.colors.push('#000000');

                //        plotsData = [({
                //            data: [[0, referenceLength], [maxX, referenceLength]],
                //            dashes: {
                //                show: true,
                //                lineWidth: 1,
                //            },
                //            number: plotsData.length,
                //        })].concat(plotsData);
            }


            //    if (referenceLength) {
            //        yaxes.push({
            //            ticks: [referenceLength],
            //            min: 0,
            //            max: maxYTick,
            //            position: 'right',
            ////            labelWidth: 50,
            //            reserveSpace: true,
            //            tickFormatter: function (val, axis) {
            //                return '<div style="">' + toPrettyStringWithDimension(referenceLength, 'bp') +
            //                    ' <span style="margin-left: -0.2em;">(reference)</span></div>';
            //            },
            //            minTickSize: 1,
            //        });
            //    }
            var yaxis = {
                min: 0,
                max: cumulative.maxYTick,
                labelWidth: 120,
                reserveSpace: true,
                lineWidth: 0.5,
                color: '#000000',
                tickFormatter: getBpTickFormatter(cumulative.maxY),
                minTickSize: 1,
            };
            var yaxes = [yaxis];

            cumulative.showWithData = function(series, colors) {
                var plot = $.plot(placeholder, series, {
                    shadowSize: 0,
                    colors: cumulative.colors,
                    legend: {
                        container: $('useless-invisible-element-that-does-not-even-exist'),
                    },
                    //            legend: {
                    //                container: legendPlaceholder,
                    //                position: 'se',
                    //                labelBoxBorderColor: '#FFF',
                    //                labelFormatter: labelFormatter,
                    //            },
                    grid: {
                        borderWidth: 1,
                        hoverable: true,
                        autoHighlight: false,
                        mouseActiveRadius: 1000,
                    },
                    yaxes: yaxes,
                    xaxis: {
                        min: 0,
                        max: cumulative.maxX,
                        lineWidth: 0.5,
                        color: '#000',
                        tickFormatter: getContigNumberTickFormatter(cumulative.maxX),
                        minTickSize: 1,
                    },
                });

                bindTip(placeholder, series, plot, ordinalNumberToPrettyString, 'contig', 'bottom right');
            };

            cumulative.isInitialized = true;
        }

        $.each(cumulative.series, function(i, series) {
            $('#legend-placeholder').find('#label_' + series.number + '_id').click(function() {
                showPlotWithInfo(cumulative);
            });
        });

        showPlotWithInfo(cumulative);

        $('#contigs_are_ordered').show();
        $('#gc_info').hide();

        //    placeholder.resize(function () {
        //        alert("Placeholder is now "
        //            + $(this).width() + "x" + $(this).height()
        //            + " pixels");
        //    });

        // var o = plot.pointOffset({ x: 0, y: 0});
        // $('#cumulative-plot-placeholder').append(
        //     '<div style="position:absolute;left:' + (o.left + 400) + 'px;top:' + (o.top - 400) + 'px;color:#666;font-size:smaller">Actual measurements</div>'
        // );
    },
};
