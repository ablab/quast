/**
 * Created with PyCharm.
 * User: vladsaveliev
 * Date: 10.07.12
 * Time: 13:11
 * To change this template use File | Settings | File Templates.
 */

function build_total_report() {
    var report = JSON.parse($('#report-json').html());

    $('#header').append('<span class="date">' + report.date + '. Contigs file: ' + report.results[0][0] + '</span>');

    document.title = 'Report of ' + report.date;

    var table = '';
    table += '<table class=".report-table">';

    for (var i = 1; i < report.header.length; i++) {

        table += '\t<tr><td>' + report.header[i] + '</td>';

        table = table + '<td>' + report.results[0][i] + '</td>';

        table += '</tr>\n';
    }
    table += '</table>';

    $('#report').append(table);
}