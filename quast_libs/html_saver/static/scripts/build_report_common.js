function showPlotWithInfo(info, index) {
    var newSeries = [];
    var newColors = [];
    var oldSeries = info.series;
    var usingSeries;
    var sortOrder;
    if ($("input[name=sortRefs]")[0])
        sortOrder = getSortOrder();
    if (index != undefined) {
        oldSeries = info.series[index];
    }
    if (sortOrder == 'alphabet') {
        usingSeries = [];
        sortedRefs = info.references.slice(0).sort();
        for(var i = 0; i < oldSeries.length; i++) {
            usingSeries.push($.extend(true, {}, oldSeries[i]));
            for(var j = 0; j < info.references.length; j++) {
                usingSeries[i].data[j][1] = oldSeries[i].data[info.references.indexOf(sortedRefs[j])][1];
            }
        }
    }
    else usingSeries = oldSeries;
    $('#legend-placeholder').find('input[type="checkbox"]:checked').each(function() {
        var number = $(this).attr('name');
        if (number && usingSeries && usingSeries.length > 0) {
            var i = 0;
            do {
                var series = usingSeries[i];
                i++;
            } while (i <= usingSeries.length && (series == null || series.number != number));
            //
            if (i <= usingSeries.length) {
                newSeries.push(series);
                newColors.push(series.color);
            } else {
                console.log('no series with number ' + number);
            }
        }
    });
    if (sortOrder) sortReferences(sortOrder, info);

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

    $('#contigs_are_ordered').hide();
    $('#gc_info').hide();
    $('#gc_contigs_info').hide();
    $('#frc_info').hide();

    var selectedAssemblies = getSelectedAssemblies(assembliesNames);
    var sortBtnClass;
    if ($("input[name=sortRefs]")[0]) {
        sortBtnClass = getSortRefsRule();
    }
    $('#legend-placeholder').empty();
    assembliesNames.forEach(function(filename, i) {
        addLabelToLegend(i, filename, selectedAssemblies, colors);
    });
    if (refPlotValue) {
        $('#legend-placeholder').append(
            '<div id="reference-label">' +
                '<label for="reference" style="color: #000000;">' +
                '<input type="checkbox" name="' + assembliesNames.length +
                '" checked="checked" id="reference">&nbsp;' + 'reference' +
                '</label>' +
                '</div>'
        );
    }
    if (sortBtnClass) {
        addSortRefsBtn(sortBtnClass);
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