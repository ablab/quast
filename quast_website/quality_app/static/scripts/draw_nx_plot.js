/**
 * Created with PyCharm.
 * User: vladsaveliev
 * Date: 11.07.12
 * Time: 19:59
 * To change this template use File | Settings | File Templates.
 */

function draw_nx_plot(filenames, lists_of_lengths, title, ref_lengths, placeholder) {

    var plotsN = lists_of_lengths.length;
    var plots_data = new Array(plotsN);

    var maxX = 0;

    for (var i = 0; i < plotsN; i++) {
        lengths = lists_of_lengths[i];

        var size = lengths.length;

        var sum_len = 0;
        for (var j = 0; j < lengths.length; j++) {
            sum_len += lengths[j];
        }
        if (ref_lengths) {
            sum_len = ref_lengths[i];
        }

        plots_data[i] = { data: new Array(), label: filenames[i] };
        plots_data[i].data.push([0.0, lengths[0]]);
        var current_len = 0;
        var x = 0.0;

        for (var k = 0; k < size; k++) {
            current_len += lengths[k];
            plots_data[i].data.push([x + 1e-10, lengths[k]]);
            x = current_len * 100.0 / sum_len;
            plots_data[i].data.push([x, lengths[k]]);
        }

        if (size > maxX) {
            maxX = size;
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
//                tickFormatter: function (val, axis) {
//                    if (typeof axis.tickSize == 'number' && val > maxX - axis.tickSize) {
//                        return "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" + val + "'th&nbsp;contig";
//                    }
//                    return val;
//                }
            }
        }
    );
}

