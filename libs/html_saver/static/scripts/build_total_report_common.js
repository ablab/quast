String.prototype.trunc =
    function(n){
        return this.substr(0, n-1) + (this.length > n ? '&hellip;' : '');
    };


function setUpHeatMap(table) {
    (function () {
        $(function () {
            $('tr.group_empty').removeClass('row_hidden');
        });
    })();

    $('#main_report').append(table);

    var RED_HUE = 0;
    var GREEN_HUE = 120;
    var GREEN_HSL = 'hsl(' + GREEN_HUE + ', 80%, 40%)';

    //$('#extended_report_link_div').width($('#top_left_td').outerWidth());

    $(".report_table td[number]").mouseenter(function () {
        if (dragTable && dragTable.isDragging)
            return;

        var cells = $(this).parent().find('td[number]');
        var numbers = $.map(cells, function (cell) {
            return $(cell).attr('number');
        });
        var quality = $(this).parent().attr('quality');

        var min = Math.min.apply(null, numbers);
        var max = Math.max.apply(null, numbers);

        var maxHue = GREEN_HUE;
        var minHue = RED_HUE;

        if (quality == 'Less is better') {
            maxHue = RED_HUE;
            minHue = GREEN_HUE;
        }

        if (max == min) {
            $(cells).css('color', GREEN_HSL);
        } else {
            var k = (maxHue - minHue) / (max - min);
            var hue = 0;
            var lightness = 0;
            cells.each(function (i) {
                var number = numbers[i];
                hue = Math.round(minHue + (number - min) * k);
                lightness = Math.round((Math.pow(hue - 75, 2)) / 350 + 35);
//                $(this).css('color', 'hsl(' + hue + ', 80%, 35%)');
                $(this).css('color', 'hsl(' + hue + ', 80%, ' + lightness + '%)');
            });
        }

        if (numbers.length > 1)
            $('#report_legend').show('fast');

    }).mouseleave(function () {
        $(this).parent().find('td[number]').css('color', 'black');
    });
}


function extendedClick() {
    $('.row_to_hide').toggleClass('row_hidden');

    var link = $('#extended_report_link');
    if (link.html() == 'Extended report') {
        link.html('Short report');
    } else {
        link.html('Extended report');
    }
}

function biuldExtendedLinkClick() {
    return '<p id="extended_link"><a class="dotted-link" id="extended_report_link" onclick="extendedClick($(this))">Extended report</a></p>';
}


