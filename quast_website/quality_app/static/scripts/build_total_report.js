/**
 * Created with PyCharm.
 * User: vladsaveliev
 * Date: 10.07.12
 * Time: 13:11
 * To change this template use File | Settings | File Templates.
 */

function build_total_report() {
    var report = JSON.parse($('#report-json').html());

    $('#header').append('<p class="date">' + report.date + '<br>Contigs file: ' + report.results[0][0] + '</p>');

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