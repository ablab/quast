
var cumulative = {
    isInitialized: false,

    maxX: 0,
    maxY: 0,
    maxYTick: 0,
    series: null,
    showWithData: null,
    colors: [],

    draw: function(name, title, colors, filenames, listsOfLengths, refLengths, tickX,
                   placeholder, legendPlaceholder, glossary, order, scalePlaceholder) {

        $(scalePlaceholder).empty();

        if (!this.isInitialized) {
            //    div.html(
            //        "<span class='plot-header'>" + addTooltipIfDefinitionExists(glossary, title) + "</span>" +
            //        "<div class='plot-placeholder' id='cumulative-plot-placeholder'></div>"
            //    );
            cumulative.series = [];
            var plotsN = filenames.length;
            var refLength = 0;
            if (refLengths) {
                for (var i = 0, size = refLengths.length; i < size; i++)
                    refLength += refLengths[i];
            }
            if (refLength) {
                cumulative.maxY = refLength;
            }

            cumulative.colors = colors;

            function addCumulativeLenData(label, index, color, lengths, isRef) {
                if (!(lengths instanceof Array))
                    lengths = [lengths];
                var size = lengths.length;
                var points = {
                    data: new Array(size + 1),
                    label: label,
                    number: index,
                    color: color
                };

                points.data[0] = [0, 0];

                var y = 0;
                for (var j = 0; j < size; j++) {
                    y += lengths[j];
                    points.data[j+1] = [j+1, y];
                    if (y > cumulative.maxY) {
                        cumulative.maxY = y;
                    }
                }

                if (size > cumulative.maxX) {
                    cumulative.maxX = size;
                }
                if (isRef){
                    points.isReference = true;
                    points.dashes = {show: true, lineWidth: 1};
                    if (size < cumulative.maxX) {
                        points.data[size + 1] = [cumulative.maxX, y];
                    }
                }
                return points;
            }

            for (var i = 0; i < plotsN; i++) {
                var lengths = listsOfLengths[order[i]];
                var asm_name = filenames[order[i]];
                var color = colors[order[i]];
                cumulative.series[i] = addCumulativeLenData(asm_name, i, color, lengths);
            }

//            var lineColors = [];
//
//            for (i = 0; i < colors.length; i++) {
//                lineColors.push(changeColor(colors[i], 0.9, false));
//            }

            for (i = 0; i < plotsN; i++) {
                if (typeof broken_scaffolds_labels !== undefined && $.inArray(filenames[order[i]], broken_scaffolds_labels) != -1) {
                    cumulative.series[i].dashes = {
                        show: true,
                        lineWidth: 1
                    };
                }
                else {
                    cumulative.series[i].lines = {
                        show: true,
                        lineWidth: 1
                    };
                }
            }

            for (i = 0; i < plotsN; i++) {
                cumulative.colors.push(cumulative.series[i].color);
            }

            //cumulative.maxYTick = getMaxDecimalTick(cumulative.maxY);

            if (refLengths) {
                size = refLengths.length;
                var ref_label = 'reference,&nbsp;' + toPrettyString(refLength, 'bp');
                cumulative.series.push(addCumulativeLenData(ref_label, i, '#000000', refLengths, true));
                cumulative.colors.push('#000000');
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
                //max: cumulative.maxYTick,
                labelWidth: 120,
                reserveSpace: true,
                lineWidth: 0.5,
                color: '#000000',
                tickFormatter: getBpTickFormatter(cumulative.maxY),
                minTickSize: 1
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
                        mouseActiveRadius: 1000
                    },
                    yaxes: yaxes,
                    xaxis: {
                        min: 0,
                        max: cumulative.maxX,
                        lineWidth: 0.5,
                        color: '#000',
                        tickFormatter: getContigNumberTickFormatter(cumulative.maxX, tickX),
                        minTickSize: tickX
                    }
                });

                bindTip(placeholder, series, plot, ordinalNumberToPrettyString, tickX, 'contig', 'bottom right');
            };

            cumulative.isInitialized = true;
        }

        addLegendClickEvents(cumulative, filenames.length, showPlotWithInfo, refLengths);

        showPlotWithInfo(cumulative);

        $('#contigs_are_ordered').show();
    },
};
