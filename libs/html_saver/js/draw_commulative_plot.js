
function drawCommulativePlot(filenames, lists_of_lengths, div, legendPlaceholder, glossary) {

    div.html(
        "<div class='plot'>" +
            "<span class='plot-header'>Commulative length</span>" +
            "<div style='width: 580px; height: 400px;' id='commulative-plot-placeholder'></div>" +
        "</div>"
    );

    var plotsN = lists_of_lengths.length;
    var plotsData = new Array(plotsN);

    var maxX = 0;

    for (var i = 0; i < plotsN; i++) {
        var lengths = lists_of_lengths[i];
        var size = lengths.length;

        plotsData[i] = {
            data: new Array(size),
            label: filenames[i],
        };

        var y = 0;
        for (var j = 0; j < size; j++) {
            y += lengths[j];
            plotsData[i].data[j] = [j+1, y];
        }

        if (size > maxX) {
            maxX = size;
        }
    }

    for (i = 0; i < plotsN; i++) {
        plotsData[i].points = {
            show: true,
            radius: 1,
            fill: 1,
            fillColor: false,
        }
    }

    $.plot($('#commulative-plot-placeholder'), plotsData, {
            shadowSize: 0,
            colors: ["#FF5900", "#008FFF", "#168A16", "#7C00FF", "#FF0080"],
            legend: {
                container: legendPlaceholder,
                position: 'se',
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
                tickFormatter: function (val, axis) {
                    if (val == 0) {
                        return 0;
                    } else if (val > 1000000) {
                        return (val / 1000000).toFixed(1) + " Mbp";
                    } else if (val > 1000) {
                        return (val / 1000).toFixed(0) + " Kbp";
                    } else {
                        return val.toFixed(0) + " bp";
                    }
                },
            },
            xaxis: {
                min: 0,
                lineWidth: 0.5,
                color: '#000',
                tickFormatter: function (val, axis) {
                    if (typeof axis.tickSize == 'number' && val > maxX - axis.tickSize) {
                        return "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" + val + "'th&nbsp;contig";
                    }
                    return val;
                },
            },
        }
    );
}

