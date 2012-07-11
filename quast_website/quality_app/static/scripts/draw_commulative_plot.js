/**
 * Created with PyCharm.
 * User: vladsaveliev
 * Date: 10.07.12
 * Time: 13:11
 * To change this template use File | Settings | File Templates.
 */

function draw_commulative_plot() {
    var code = JSON.parse($('#lengths-json').html());
    var lengths = code.lists_of_lengths[0];
    var size = lengths.length;

    var data = new Array(size);
    var y = 0;
    for (var i = 0; i < size; i++) {
        y += lengths[i];
        data[i] = [i+1, y];
    }

    $.plot($('#commulative-plot-placeholder'),
        [ { data: data, } ],
        {
            shadowSize: 0,
            grid: {
                borderWidth: 1,
                color: 'CCC',
            },
            yaxis: {
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
                    if (typeof axis.tickSize == 'number' && val > data[size-1][0] - axis.tickSize) {
                        return "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" + val + "'th&nbsp;contig";
                    }
                    return val;
                }
            }
        }
    );
}

