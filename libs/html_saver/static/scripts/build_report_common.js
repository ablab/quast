function showPlotWithInfo(info) {
    var newSeries = [];
    var newColors = [];

    $('#legend-placeholder').find('input:checked').each(function() {
        var number = $(this).attr('name');
        if (number && info.series && info.series.length > 0) {
            var i = 0;
            do {
                var series = info.series[i];
                i++;
            } while (i <= info.series.length && (series == null || series.number != number));
            //
            if (i <= info.series.length) {
                newSeries.push(series);
                newColors.push(series.color);
            } else {
                console.log('no series with number ' + number);
            }
        }
    });

    if (newSeries.length === 0) {
        newSeries.push({
            data: []
        });
        newColors.push('#FFF');
    }

    info.showWithData(newSeries, newColors);
}

function recoverOrderFromCookies() {
    if (!navigator.cookieEnabled)
        return null;

    var order_string = readCookie("order");
    if (!order_string)
        return null;

    var order = [];
    var fail = false;
    forEach(order_string.split(' '), function(val) {
        val = parseInt(val);
        if (isNaN(val))
            fail = true;
        else
            order.push(val);
    });

    if (fail)
        return null;

    return order;
}


function readJson(what) {
    var result;
    try {
        result = JSON.parse($('#' + what + '-json').html());
    } catch (e) {
        result = null;
    }
    return result;
}


function getToggleFunction(assembliesNames, order, name, title, drawPlot, data, refPlotValue, tickX) {
    return function() {
        this.parentNode.getElementsByClassName('selected-switch')[0].className = 'plot-switch dotted-link';
        this.className = 'plot-switch selected-switch';
        togglePlots(assembliesNames, order, name, title, drawPlot, data, refPlotValue, tickX)
    };
}

function togglePlots(assembliesNames, order, name, title, drawPlot, data, refPlotValue, tickX) {
    var plotPlaceholder = document.getElementById('plot-placeholder');
    var legendPlaceholder = document.getElementById('legend-placeholder');
    var scalePlaceholder = document.getElementById('scale-placeholder');

    var glossary = JSON.parse($('#glossary-json').html());

    if (name === 'cumulative') {
        $(plotPlaceholder).addClass('cumulative-plot-placeholder');
    } else {
        $(plotPlaceholder).removeClass('cumulative-plot-placeholder');
    }

    var el = $('#reference-label');
    el.remove();

    if (refPlotValue) {
        $('#legend-placeholder').append(
            '<div id="reference-label">' +
                '<label for="label_' + assembliesNames.length + '_id" style="color: #000000;">' +
                '<input type="checkbox" name="' + assembliesNames.length +
                '" checked="checked" id="label_' + assembliesNames.length +
                '_id">&nbsp;' + 'reference,&nbsp;' +
                toPrettyString(refPlotValue) +
                '</label>' +
                '</div>'
        );
    }

    drawPlot(name, title, colors, assembliesNames, data, refPlotValue, tickX,
        plotPlaceholder, legendPlaceholder, glossary, order, scalePlaceholder);
}

function makePlot(firstPlot, assembliesNames, order, name, title, drawPlot, data, refPlotValue, tickX) {
    var switchSpan = document.createElement('span');
    switchSpan.id = name + '-switch';
    switchSpan.innerHTML = title;
    var plotsSwitchesDiv = document.getElementById('plots-switches');
    plotsSwitchesDiv.appendChild(switchSpan);

    if (firstPlot) {
        switchSpan.className = 'plot-switch selected-switch';
        togglePlots(assembliesNames, order, name, title, drawPlot, data, refPlotValue, tickX);
    } else {
        switchSpan.className = 'plot-switch dotted-link';
    }

    $(switchSpan).click(getToggleFunction(assembliesNames, order, name, title, drawPlot, data, refPlotValue, tickX));
}