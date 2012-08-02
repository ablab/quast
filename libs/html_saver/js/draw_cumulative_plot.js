

function drawCumulativePlot(filenames, lists_of_lengths, reference_length, div, legendPlaceholder, glossary) {
    var title = 'Cumulative length';
    div.html(
        "<div class='plot'>" +
            "<span class='plot-header'>" + addTooltipIfDefenitionExists(glossary, title) + "</span>" +
            "<div style='width: 850px; height: 600px;' id='cumulative-plot-placeholder'></div>" +
            "</div>"
    );

    var plotsN = lists_of_lengths.length;
    var plotsData = new Array(plotsN);

    var maxX = 0;
    var maxY = 0;

    if (reference_length) {
        maxY = reference_length;
    }

    for (var i = 0; i < plotsN; i++) {
        var lengths = lists_of_lengths[i];
        var size = lengths.length;

        plotsData[i] = {
            data: new Array(size+1),
            label: filenames[i],
        };

        plotsData[i].data[0] = [0, 0];

        var y = 0;
        for (var j = 0; j < size; j++) {
            y += lengths[j];
            plotsData[i].data[j+1] = [j+1, y];
            if (y > maxY) {
                maxY = y;
            }
        }

        if (size > maxX) {
            maxX = size;
        }
    }

    for (i = 0; i < plotsN; i++) {
        plotsData[i].lines = {
            show: true,
            lineWidth: 1,
        }
    }

    var maxYTick = maxY;
    if (maxY <= 100000000000) {
        maxYTick = Math.ceil((maxY+1)/10000000000)*10000000000;
    } if (maxY <= 10000000000) {
        maxYTick = Math.ceil((maxY+1)/1000000000)*1000000000;
    } if (maxY <= 1000000000) {
        maxYTick = Math.ceil((maxY+1)/100000000)*100000000;
    } if (maxY <= 100000000) {
        maxYTick = Math.ceil((maxY+1)/10000000)*10000000;
    } if (maxY <= 10000000) {
        maxYTick = Math.ceil((maxY+1)/1000000)*1000000;
    } if (maxY <= 1000000) {
        maxYTick = Math.ceil((maxY+1)/100000)*100000;
    } if (maxY <= 100000) {
        maxYTick = Math.ceil((maxY+1)/10000)*10000;
    } if (maxY <= 10000) {
        maxYTick = Math.ceil((maxY+1)/1000)*1000;
    } if (maxY <= 1000) {
        maxYTick = Math.ceil((maxY+1)/100)*100.
    } if (maxY <= 100) {
        maxYTick = Math.ceil((maxY+1)/10)*10.
    }

//    In order to draw dots instead of lines
//
//    for (i = 0; i < plotsN; i++) {
//        plotsData[i].points = {
//            show: true,
//            radius: 1,
//            fill: 1,
//            fillColor: false,
//        }
//    }

    var colors = ["#FF5900", "#008FFF", "#168A16", "#7C00FF", "#FF0080"];

    if (reference_length) {
        plotsData = [({
            data: [[0, reference_length], [maxX, reference_length]],
            label: 'Reference, ' + toPrettyStringWithDimencion(reference_length, 'bp'),
            dashes: {
                show: true,
                lineWidth: 1,
            },
            yaxis: 2,
        })].concat(plotsData);

        plotsData = [({
            data: [[0, reference_length], [maxX, reference_length]],
            dashes: {
                show: true,
                lineWidth: 1,
            },
        })].concat(plotsData);

        colors = ["#000000"].concat(colors);
        colors = ["#000000"].concat(colors);
    }

    var yaxis = {
        min: 0,
        max: maxYTick,
        labelWidth: 120,
        reserveSpace: true,
        lineWidth: 0.5,
        color: '#000',
        tickFormatter: getBpTickFormatter(maxY),
        minTickSize: 1,
    };

    var yaxes = [yaxis];

    if (reference_length) {
        yaxes.push({
            ticks: [reference_length],
            min: 0,
            max: maxYTick,
            position: 'rigth',
            labelWidth: 50,
            reserveSpace: true,
            tickFormatter: function (val, axis) {
                return '<div style="">' + toPrettyStringWithDimencion(reference_length, 'bp') +
                    ' <span style="margin-left: -0.2em;">(reference)</span></div>';
            },
            minTickSize: 1,
        });
    }

    var plot = $.plot($('#cumulative-plot-placeholder'), plotsData, {
            shadowSize: 0,
            colors: colors,
            legend: {
                container: legendPlaceholder,
                position: 'se',
                labelBoxBorderColor: '#FFF',
            },
            grid: {
                borderWidth: 1,
            },
            yaxes: yaxes,
            xaxis: {
                min: 0,
                lineWidth: 0.5,
                color: '#000',
                tickFormatter: getContigNumberTickFormatter(maxX),
                minTickSize: 1,
            },
        }
    );

    // var o = plot.pointOffset({ x: 0, y: 0});
    // $('#cumulative-plot-placeholder').append(
    //     '<div style="position:absolute;left:' + (o.left + 400) + 'px;top:' + (o.top - 400) + 'px;color:#666;font-size:smaller">Actual measurements</div>'
    // );
}