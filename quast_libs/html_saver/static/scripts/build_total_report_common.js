String.prototype.trunc =
    function(n){
        return this.substr(0, n-1) + (this.length > n ? '&hellip;' : '');
    };


function getColor (hue, lightness) {
    lightness = lightness ? lightness : 92;
    var rgb = hslToRgb(hue / 360, 0.8, lightness / 100);
    return '#' + rgb[0].toString(16) + rgb[1].toString(16) + rgb[2].toString(16);
}

function getMedian (x) {
    if (x.length == 0) return null;
    if (x.length % 2 == 1) return x[(x.length - 1) / 2];
    else return (x[(x.length / 2) - 1] + x[(x.length / 2)]) / 2;
}

function toggleSecondary(event, caller) {
    event = event || window.event;
    if(event.target.nodeName == "IMG") return;
    if (!caller.hasClass('primary') || caller.hasClass('not_extend')) {
        return;
    }
    var nextRow = caller.next('.content-row');
    $(caller).find('.metric-name').toggleClass('collapsed').toggleClass('expanded');

    while (!nextRow.hasClass('primary') && (nextRow.length > 0)) {
        nextRow.toggleClass('secondary_hidden');
        nextRow.find('.left_column_td').css('background-color', '#E8E8E8');
        nextRow = nextRow.next('.content-row');
    }
}

function setUpHeatMap(table) {
    
    (function () {
        $(function () {
            $('tr.group_empty').removeClass('row_hidden');
        });
    })();

    $('#main_report').append(table);
    var rows = $('#main_report_table').find('.content-row');
    var showHeatmap = false;
    for (var rows_n = 0; rows_n < rows.length; rows_n++) {
        if ($(rows[rows_n]).find('td[number]').length > 1) {
            showHeatmap = true;
            break
        }
    }

    if (showHeatmap) {
        var canvas = document.getElementById('gradientHeatmap');
          var context = canvas.getContext('2d');
          context.rect(0, 0, canvas.width, canvas.height);

          var gradient = context.createLinearGradient(0, 0, canvas.width, canvas.height);
          gradient.addColorStop(0, getColor(0, 65));
          gradient.addColorStop(0.5, 'white');
          gradient.addColorStop(1, getColor(240, 65));
          canvas.style.border = "0px solid rgba(0, 0, 0, .1)";
          context.fillStyle = gradient;
          context.fill();
          $('#heatmaps_chbox').change(function(){
               if($(this).is(':checked')) toggleHeatMap('on');
               else toggleHeatMap('off');
            });
          toggleHeatMap('on');
          $('#heatmap_header').show();
    }
}

function toggleHeatMap(state){
    var rows = $('#main_report_table').find('.content-row');
    for (var rows_n = 0; rows_n < rows.length; rows_n++) {
        var cells = $(rows[rows_n]).find('td[number]');
        if (state == 'on') {
            var quality = $(rows[rows_n]).attr('quality');
            heatMapOneRow(cells, quality);
        }
        else cells.each(function (i) {
            $(this).css('background', 'white');
            $(this).css('color', 'black');
        });
    }
}

function heatMapOneRow (cells, quality) {
    if (quality == 'Equal')
        return;
    var BLUE_HUE = 240;
    var BLUE_OUTER_BRT = 55;
    var BLUE_INNER_BRT = 65;

    var RED_HUE = 0;
    var RED_OUTER_BRT = 50;
    var RED_INNER_BRT = 60;

    var MIN_NORMAL_BRT = 80;
    var MEDIAN_BRT = 100;

    var numbers = $.map(cells, function (cell) {
        return parseFloat($(cell).attr('number'));
    });
    
    var min = Math.min.apply(null, numbers);
    var max = Math.max.apply(null, numbers);

    var topHue = BLUE_HUE;
    var lowHue = RED_HUE;
    
    var innerTopBrt = BLUE_INNER_BRT;
    var outerTopBrt = BLUE_OUTER_BRT;
    var innerLowBrt = RED_INNER_BRT;
    var outerLowBrt = RED_OUTER_BRT;

    if (quality == 'Less is better') {
        topHue = RED_HUE;
        lowHue = BLUE_HUE;

        innerTopBrt = RED_INNER_BRT;
        outerTopBrt = RED_OUTER_BRT;
        innerLowBrt = BLUE_INNER_BRT;
        outerLowBrt = BLUE_OUTER_BRT;
    }

    var twoCols = cells.length == 2;

    if (max == min) {
        $(cells).css('color', MEDIAN_BRT);
    } else {
        var sortedValues = numbers.slice().sort(function(a, b) {
          return a - b;
        });
        var median = getMedian(sortedValues);
        var l = numbers.length;
        var q1 = sortedValues[Math.floor((l - 1) / 4)];
        var q3 = sortedValues[Math.floor((l - 1) * 3 / 4)];

        var d = q3 - q1;
        var low_outer_fence = q1 - 3 * d;
        var low_inner_fence = q1 - 1.5 * d;
        var top_inner_fence = q3 + 1.5 * d;
        var top_outer_fence = q3 + 3 * d;
        cells.each(function (i) {
            var number = numbers[i];
            if (number < low_outer_fence) {
                $(this).css('background', getColor(lowHue, twoCols ? null : outerLowBrt));
                if (twoCols != true) $(this).css('color', 'white');
            }
            else if (number < low_inner_fence) {
                $(this).css('background', getColor(lowHue, twoCols ? null : innerLowBrt));
            }
            else if (number < median) {
                var k = (MEDIAN_BRT - MIN_NORMAL_BRT) / (median - low_inner_fence);
                var brt = Math.round(MEDIAN_BRT - (median - number) * k);
                $(this).css('background', getColor(lowHue, twoCols ? null : brt));
            }
            else if (number > top_inner_fence) {
                $(this).css('background', getColor(topHue, twoCols ? null : innerTopBrt));
            }
            else if (number > top_outer_fence) {
                $(this).css('background', getColor(topHue, twoCols ? null : outerTopBrt));
                if (twoCols != true) $(this).css('color', 'white');
            }
            else if (number > median) {
                var k = (MEDIAN_BRT - MIN_NORMAL_BRT) / (top_inner_fence - median);
                var brt = Math.round(MEDIAN_BRT - (number - median) * k);
                $(this).css('background', getColor(topHue, twoCols ? null : brt));
            }
        });
    }
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

function buildExtendedLinkClick() {
    return '<p id="extended_link"><a class="dotted-link" id="extended_report_link" onclick="extendedClick($(this))">Extended report</a></p>';
}

function appendIcarusLinks() {
    if (icarusLinks = readJson('icarus')) {
        if (icarusLinks.links != undefined) {
            var links = '';
            for (var link_n = 0; link_n < icarusLinks.links.length; link_n++) {
                //links += '<a href="' + icarusLinks.links[link_n] + '">' + icarusLinks.links_names[link_n] + '</a><br>'
                links += '<a class="btn btn-default btn-xs" role="button" href="' + icarusLinks.links[link_n] + '">' + icarusLinks.links_names[link_n] + '</a><br>'
            }
            $('#icarus').html(links);
        }
    }
}
