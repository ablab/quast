
function drawGenesPlot(filenames, filesContigs, genes, found, kind, div, legendPlaceholder,  glossary) {

    div.html(
        "<div class='plot'>" +
            "<span class='plot-header'>" + kind[0].toUpperCase() + kind.slice(1) + "s covered</span>" +
            "<div style='width: 800px; height: 600px;' id='" + kind + "s-plot-placeholder'></div>" +
        "</div>"
    );

    var plotsN = filenames.length;
    var plotsData = new Array(plotsN);

    var maxY = 0;
    var maxX = 0;

    for (var fi = 0; fi < plotsN; fi++) {
        var filename = filenames[fi];
        var contigs = filesContigs[filename];
        for (var i = 0; i < genes.length; i++) {
            found[i] = 0
        }

        plotsData[fi] = {
            data: [[0, 0]],
            label: filenames[fi],
        };

        var contigNo = 0;
        var totalFull = 0;

        for (var k = 0; k < contigs.length; k++) {
            var alignedBlocks = contigs[k];
            contigNo += 1;

            for (i = 0; i < genes.length; i++) {
                var g = genes[i];

                if (found[i] == 0) {
                    for (var bi = 0; bi < alignedBlocks.length; bi++) {
                        var block = alignedBlocks[bi];

                        if (block[0] <= g[0] && g[1] <= block[1]) {
                            found[i] = 1;
                            totalFull += 1;
                            break;
                        }
                    }
                }
            }

            plotsData[fi].data.push([contigNo, totalFull]);

            if (plotsData[fi].data[k][1] > maxY) {
                maxY = plotsData[fi].data[k][1];
            }
        }

        if (contigs.length > maxX) {
            maxX = contigs.length;
        }
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



    $.plot($('#' + kind + 's-plot-placeholder'), plotsData, {
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
                    if (val > maxY + 1) {
                        return val + ' ' + kind + 's';
                    } else {
                        return val;
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

