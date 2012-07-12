/**
 * Created with PyCharm.
 * User: vladsaveliev
 * Date: 10.07.12
 * Time: 13:11
 * To change this template use File | Settings | File Templates.
 */

function draw_commulative_plot(filenames, lists_of_lengths, placeholder) {

    var plotsN = lists_of_lengths.length;
    var plots_data = new Array(plotsN);

    var maxContigNumber = 0;

    for (var i = 0; i < plotsN; i++) {
        lengths = lists_of_lengths[i];
        var size = lengths.length;

        plots_data[i] = { data: new Array(size), label: filenames[i] };

        var y = 0;
        for (var j = 0; j < size; j++) {
            y += lengths[j];
            plots_data[i].data[j] = [j+1, y];
        }

        if (size > maxContigNumber) {
            maxContigNumber = size;
        }
    }

    $.plot(placeholder, plots_data, {
            shadowSize: 0,
            grid: {
                borderWidth: 1,
                color: 'CCC',
            },
            yaxis: {
                labelWidth: 80,
                reserveSpace: true,
                tickFormatter: function (val, axis) {
                    if (val > 1000000) {
                        return (val / 1000000).toFixed(1) + " Mbp";
                    } else if (val > 1000) {
                        return (val / 1000).toFixed(0) + " Kbp";
                    } else {
                        return val.toFixed(0) + " bp";
                    }
                },
            },
            xaxis: {
                tickFormatter: function (val, axis) {
                    if (typeof axis.tickSize == 'number' && val > maxContigNumber - axis.tickSize) {
                        return "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" + val + "'th&nbsp;contig";
                    }
                    return val;
                }
            }
        }
    );
}

