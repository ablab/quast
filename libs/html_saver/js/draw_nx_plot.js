/**
 * Created with PyCharm.
 * User: vladsaveliev
 * Date: 11.07.12
 * Time: 19:59
 * To change this template use File | Settings | File Templates.
 */

function drawNxPlot(filenames, listsOfLengths, title,
                    refLength, plotPlaceholder, legendPlaceholder) {

    var plotsN = listsOfLengths.length;
    var plotsData = new Array(plotsN);

    var maxY = 0;

    for (var i = 0; i < plotsN; i++) {
        var lengths = listsOfLengths[i];

        var size = lengths.length;

        var sumLen = 0;
        for (var j = 0; j < lengths.length; j++) {
            sumLen += lengths[j];
        }
        if (refLength) {
            sumLen = refLength;
        }

        plotsData[i] = {
            data: new Array(),
            label: filenames[i],
        };
        //plotsData[i].data.push([0.0, lengths[0]]);
        var currentLen = 0;
        var x = 0.0;

        for (var k = 0; k < size; k++) {
            currentLen += lengths[k];
            //plotsData[i].data.push([x, lengths[k]]);
            x = currentLen * 100.0 / sumLen;
            plotsData[i].data.push([x, lengths[k]]);
        }

        if (plotsData[i].data[0][1] > maxY) {
            maxY = plotsData[i].data[0][1];
        }
    }

    for (var i = 0; i < plotsN; i++) {
        plotsData[i].points = {
            show: true,
                radius: 1,
                fill: 1,
                fillColor: false,
        }
    }

    $.plot(plotPlaceholder, plotsData, {
            shadowSize: 0,
            colors: ["#FF5900", "#008FFF", "#168A16", "#7C00FF", "#FF0080"],
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
                labelWidth: 80,
                reserveSpace: true,
                lineWidth: 0.5,
                color: '#000',
                tickFormatter: function (val, axis) {
                    if (val == 0) {
                        return 0;
                    } else if (val > 1000000) {
                        return (val / 1000000).toFixed(1) + ' Mbp';
                    } else if (val > 1000) {
                        if (val + axis.tickSize >= 1000000 || val >= maxY) {
                            return (val / 1000).toFixed(0) + ' Kbp';
                        } else {
                            return (val / 1000).toFixed(0);
                        }
                    } else {
                        return val.toFixed(0) + ' bp';
                    }
                },
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
        }
    );
}

