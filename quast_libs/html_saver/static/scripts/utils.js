
/**********/
/* COLORS */

// var colors = ["#FF5900", "#008FFF", "#168A16", "#7C00FF", "#00B7FF", "#FF0080", "#7AE01B", "#782400", "#E01B6A"];
var standard_colors = [
    '#FF0000', //red
    '#0000FF', //blue
    '#008000', //green
    '#FFA500', //orange
    '#FF00FF', //fushua
    '#CCCC00', //yellow
    '#800000', //maroon
    '#00CCCC', //aqua
    '#808080', //gray
    '#800080', //purple
    '#808000', //olive
    '#000080', //navy
    '#008080', //team
    '#00FF00', //lime
];

function distinctColors(count) {
    var colors = [];
    for(var hue = 0; hue < 360; hue += 360 / count) {
        var color = hsvToRgb(hue, 100, 100);
        var colorStr = '#' + color[0].toString(16) + color[1].toString(16) + color[2].toString(16);
        colors.push();
    }
    return colors;
}

/**************/
/* FORMATTING */
function isIntegral(num) {
    return num % 1 === 0;
}

function isFractional(num) {
    return !isIntegral(num);
}

function getIntervalToPrettyString(interval) {
    return function(num, unit) {
        return intervalToPrettyString(interval, num, unit);
    }
}

function intervalToPrettyString(interval, num, unit) {
    if (typeof num === 'number') {
        var str = toPrettyString(num);
        str += '-' + toPrettyString(num + interval);
        str += (unit ? '<span class="rhs">&nbsp;</span>' + unit : '');
        return str;
    } else {
        return num;
    }
}

function toPrettyString(num, unit) {
    if (typeof num === 'number') {
        var str;
        if (num <= 9999) {
            if (isFractional(num)) {
                if (isIntegral(num * 10)) {
                    str = num.toFixed(1);
                } else if (isIntegral(num * 100) || num >= 100) {
                    str = num.toFixed(2);
                } else {
                    str = num.toFixed(3);
                    if (str.slice(-1) == '0')
                        str = str.slice(0, -1);
                }
            } else {
                str = num.toFixed(0);
            }
        } else {
            str = num.toFixed(0).replace(/(\d)(?=(\d\d\d)+(?!\d))/g,'$1<span class="hs"></span>');
        }
        str += (unit ? '<span class="rhs">&nbsp;</span>' + unit : '');
        return str;
    } else {
        return num;
    }
}

        //elif isinstance(value, float):
        //    if value == 0.0:
        //        return '0'
        //    if human_readable:
        //        if unit == '%':
        //            value *= 100
        //        precision = 2
        //        for i in range(10, 1, -1):
        //            if abs(value) < 1./(10**i):
        //                precision = i + 1
        //                break
        //        return '{value:.{precision}f}{unit_str}'.format(**locals())
        //    else:
        //        return str(value)


function refToPrettyString(num, refs) {
    return refs[Math.round(num)-1];
}

function ordinalNumberToPrettyString(num, unit, tickX) {
    num = num * tickX;
    var numStr = num.toString();
    var lastDigit = numStr[numStr.length-1];
    var beforeLastDigit = numStr[numStr.length-2];

    var res = toPrettyString(num);

    if (lastDigit == '1' && beforeLastDigit != '1') {
        res += "st";
    } else if (lastDigit == '2' && beforeLastDigit != '1') {
        res += "nd";
    } else if (lastDigit == '3' && beforeLastDigit != '1') {
        res += "rd";
    } else {
        res += 'th';
    }

    res += (unit ? '<span class="rhs">&nbsp;</span>' + unit : '');

    return res;
}

function frcNumberToPrettyString(num, unit, tickX, index) {
    if (index % 2 == 0 && num > 0) num--;
    return toPrettyString(num) + ' ' + unit;
}

function getMaxDecimalTick(maxY) {
    var maxYTick = maxY;
    if (maxY <= 100000000000) {
        maxYTick = Math.ceil((maxY+1)/10000000000)*10000000000;
    } if (maxY <= 10000000000) {
        maxYTick = Math.ceil((maxY+1)/1000000000)*1000000000;
    } if (maxY <= 1000000000) {
        maxYTick = Math.ceil((maxY+1)/100000000)*100000000;
    } if (maxY <= 100000000) {
        maxYTick = Math.ceil((maxY+1)/10000000)*10000000;
    } if (maxY <= 10000000) {
        maxYTick = Math.ceil((maxY+1)/1000000)*1000000;
    } if (maxY <= 1000000) {
        maxYTick = Math.ceil((maxY+1)/100000)*100000;
    } if (maxY <= 100000) {
        maxYTick = Math.ceil((maxY+1)/10000)*10000;
    } if (maxY <= 10000) {
        maxYTick = Math.ceil((maxY+1)/1000)*1000;
    } if (maxY <= 1000) {
        maxYTick = Math.ceil((maxY+1)/100)*100.
    } if (maxY <= 100) {
        maxYTick = Math.ceil((maxY+1)/10)*10.
    }
    return maxYTick;
}

function getBpTickFormatter(maxY, additionalText) {
    additionalText = additionalText || '';

    return function(val, axis) {
        var res;
        if (val == 0) {
            res = 0;

        } else if (axis.max >= 1000000) {
            res = val / 1000000;

            if (val > axis.max - 1 || val + axis.tickSize >= 1000000000) {
                res = additionalText + toPrettyString(res, 'Mbp');
            } else {
                res = toPrettyString(res);
            }
        } else if (axis.max >= 1000) {
            res = val / 1000;

            if (val > axis.max - 1 || val + axis.tickSize >= 1000000) {
                res = additionalText + toPrettyString(res, 'kbp');
            } else {
                res = toPrettyString(res);
            }
        } else if (axis.max >= 1) {
            res = val;

            if (val > axis.max - 1 || val + axis.tickSize >= 1000) {
                res = additionalText + toPrettyString(res, 'bp');
            } else {
                res = toPrettyString(res);
            }
        }
        return res;
    }
}

function windowsTickFormatter(v, axis) {
    return toPrettyString(v);
//    var val = v.toFixed(0);
//    if (!gc.yAxisLabeled && val > gc.maxY) {
//        gc.yAxisLabeled = true;
//        var res = val + ' window';
//        if (val > 1) {
//            res += 's'
//        }
//        return res;
//    } else {
//        return val;
//    }
}

function getBpLogTickFormatter(maxY) {
    return getBpTickFormatter(maxY);
}

function getContigNumberTickFormatter(maxX, tickX) {
    return function (val, axis) {
        if (typeof axis.tickSize == 'number' && val > maxX - axis.tickSize) {
            return "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" + ordinalNumberToPrettyString(val, 'contig', tickX);
        } else {
            return val * tickX;
        }
    }
}

function getJustNumberTickFormatter(maxY, additionalText) {
    return function (val, axis) {
        additionalText = additionalText || '';
        if (val > axis.max - axis.tickSize) {
            res = toPrettyString(val) + additionalText;
        } else {
            res = toPrettyString(val);
        }
        return res;
    }
}

function getPercentTickFormatter(maxY, additionalText) {
    return function (val, axis) {
        additionalText = additionalText || '';
        if (val > maxY + 1 || val == 100) {
            res = additionalText + toPrettyString(val, '%');
        } else {
            res = toPrettyString(val);
        }
        return res;
    }
}

function trim(str) {
    return str.replace(/^\s+/g, '');
}

function initial_spaces_to_nbsp(str, metricName) {
    if (metricName.length > 0 && metricName[0] == ' ') {
        str = '&nbsp;&nbsp;&nbsp;' + str;
    }
    return str;
}

function containsObject(obj, list) {
    var i;
    for (i = 0; i < list.length; i++) {
        if (list[i] === obj) {
            return true;
        }
    }

    return false;
}

function addLegendClickEvents(info, numLegendItems, showPlotWithInfo, showReference, index) {
    if (showReference) numLegendItems++;
    for (var i = 0; i < numLegendItems; i++) {
        $('#legend-placeholder input[name=' + i + ']').click(function() {
            showPlotWithInfo(info, index);
        });
    }
}

/*********************/
/* GLOSSARY TOOLTIPS */
function addTooltipIfDefinitionExists(glossary, metricName) {
    metricName = trim(metricName);

    if (containsObject(metricName, Object.keys(glossary))) {
        return '<a class="tooltip-link" rel="tooltip" title="' +
            metricName + ' ' + glossary[metricName] + '">' + metricName + '</a>';
    } else {
        return metricName;
    }
}

/*************/
/* PLOT TIPS */
function bindTip(placeholder, series, plot, xToPrettyStringFunction, tickX, xUnit, position, summaryPlots, unitY) {
    var prevPoint = null;
    var prevIndex = null;

    $(placeholder).bind("plothover", function(event, pos, item) {
        if (dragTable && dragTable.isDragging)
            return;

        if (item) {
            if (prevPoint != item.dataIndex || (summaryPlots && item.seriesIndex != prevIndex)) {
                prevPoint = item.dataIndex;
                prevIndex = item.seriesIndex;
                var x = item.datapoint[0];

                showTip(item.pageX, item.pageY, plot.offset(),
                    plot.width(), plot.height(),
                    series, item.seriesIndex, x, item.dataIndex,
                    xToPrettyStringFunction(x, xUnit, tickX, item.dataIndex) + ':',
                    position, summaryPlots, unitY);
            }
        } else {
            $('#plot_tip').hide();
            $('#plot_tip_vertical_rule').hide();
            $('#plot_tip_horizontal_rule').hide();
            prevPoint = null;
        }
    });
}

function unBindTips(placeholder) {
    $(placeholder).unbind("plothover");
}

var tipElementExists = false;
function showTip(pageX, pageY, offset, plotWidth, plotHeight,
                 series, centralSeriesIndex, xPos, xIndex, xStr, position, summaryPlots, unitY) {
    const LINE_HEIGHT = 16; // pixels

    position = ((position != null) ? position : 'bottom right');
//    pageY -= LINE_HEIGHT * (centralSeriesIndex + 1.5);

    var directions = position.split(' ');

    if (!tipElementExists) {
        $('<div id="plot_tip" class="white_stroked"></div>').appendTo('body');

        $('<div id="plot_tip_vertical_rule"></div>').css({
            height: plotHeight,
        }).appendTo('body');

        $('<div id="plot_tip_horizontal_rule"></div>').css({
            width: plotWidth,
        }).appendTo('body');

        tipElementExists = true;
    }

    $('#plot_tip').html('').css({
        top: pageY + 5 - ((directions[0] == 'top') ? LINE_HEIGHT * (series.length + 2) : 0),
        left: pageX + 10,
    }).show();

    $('#plot_tip_vertical_rule').html('').css({
        top: offset.top,
        left: pageX,
    }).show();

    $('#plot_tip_horizontal_rule').html('').css({
        top: pageY,
        left: offset.left,
    }).show();

    $('<div>' + xStr + '</div>').css({
        height: LINE_HEIGHT,
    }).appendTo('#plot_tip');

    var sortedYsAndColors = [];
    for (var i = 0; i < series.length; i++) {
        if (!summaryPlots || (summaryPlots && series[i].data[xIndex] != undefined && series[i].data[xIndex][1] != null)) {
            sortedYsAndColors.push({
                y: summaryPlots ? series[i].data[xIndex][1] : (i == centralSeriesIndex ? (series[i].data[xIndex] || series[i].data[series[i].data.length - 1])[1] :
                    findNearestPoint(series[i].data, xPos)),
                color: series[i].color,
                label: (series[i].isReference ? 'Reference' : series[i].label),
                isCurrent: i == centralSeriesIndex,
            });
        }
    }
    sortedYsAndColors.sort(function(a, b) { return a.y < b.y;});

    for (i = 0; i < sortedYsAndColors.length; i++) {
        var item = sortedYsAndColors[i];

        $('<div id="tip_line' + i + '">' + toPrettyString(item.y) + (unitY ? unitY : '') +
            ', <span style="color: ' + item.color + ';">' + item.label + '</span></div>').css({
            height: LINE_HEIGHT,
            "font-weight": item.isCurrent ? "bold" : "normal",
        }).appendTo('#plot_tip');
    }
}

function findNearestPoint(points, x) {
    for (var i = 0; i < points.length; i++) {
        if (points[i][0] >= x) return points[i][1];
    }
    return points[points.length-1][1]
}

function addLabelToLegend(idx, label, selectedLabels, colors, link) {
    var isChecked = (selectedLabels.length > 0 && selectedLabels.indexOf(idx.toString())) != -1 ? 'checked="checked"' : "";
    $('#legend-placeholder').append('<div>' +
        '<label for="' + label + '" style="color: ' + colors[idx] + '">' +
        '<input type="checkbox" name="' + idx + '"' + isChecked + ' id="' + label + '">&nbsp;' + label + '</label>' +
        (link ? '<br>' + link : '') + '</div>');
}

function getSelectedAssemblies(labels) {
    var selectedAssemblies = [];
    var labelsMatch = false;
    var legendLabels = [];
    $('#legend-placeholder input[type="checkbox"]').each(function() {
        legendLabels.push($(this).attr('id'));
    });
    if (labels.every(function(label, i) { return ($.inArray(label, legendLabels) != -1 || label == 'reference')}) ) {
        labelsMatch = true;
    }
    if (labelsMatch) {
        $('#legend-placeholder input:checked[type="checkbox"]').each(function() {
            selectedAssemblies.push($(this).attr('name'));
        });
    }
    else {
        selectedAssemblies = Array.apply(null, {length: labels}).map(Number.call, Number);
    }
    return selectedAssemblies;
}

// Cookie functions based on http://www.quirksmode.org/js/cookies.html
// Cookies won't work for local files.

var createCookie = function(name, value, days) {
    var expires = '';
    if (days) {
        var date = new Date();
        date.setTime(date.getTime() + (days * 24 * 60 * 60 * 1000));
        expires = '; expires=' + date.toGMTString();
    }
    var path = document.location.pathname;
    document.cookie = name + '=' + value + expires + '; path=' + path;
};

var readCookie = function(name) {
    var nameEQ = name + '=';
    var ca = document.cookie.split(';');
    for(var i = 0; i < ca.length; i++) {
        var c = ca[i];
        while (c.charAt(0) == ' ') {
            c = c.substring(1, c.length);
        }
        if (c.indexOf(nameEQ) == 0) {
            return c.substring(nameEQ.length, c.length);
        }
    }
    return null;
};

var eraseCookie = function(name) {
    createCookie(name, '', -1);
};


// Dean's forEach: http://dean.edwards.name/base/forEach.js
/*forEach, version 1.0
 Copyright 2006, Dean Edwards
 License: http://www.opensource.org/licenses/mit-license.php */

// array-like enumeration
if (!Array.forEach) { // mozilla already supports this
    Array.forEach = function(array, block, context) {
        for (var i = 0; i < array.length; i++) {
            block.call(context, array[i], i, array);
        }
    };
}

// generic enumeration
Function.prototype.forEach = function(object, block, context) {
    for (var key in object) {
        if (typeof this.prototype[key] == "undefined") {
            block.call(context, object[key], key, object);
        }
    }
};

// character enumeration
String.forEach = function(string, block, context) {
    Array.forEach(string.split(""), function(chr, index) {
        block.call(context, chr, index, string);
    });
};

// globally resolve forEach enumeration
var forEach = function(object, block, context) {
    if (object) {
        if (object instanceof Function) {                 // functions have a "length" property
            Function.forEach(object, block, context);

        } else if (object.each instanceof Function) {     // jQuery
            object.each(function(i, elt) {
                block(elt);
            }, context);

        } else if (object.forEach instanceof Function) {  // the object implements a custom forEach method
            object.forEach(block, context);

        } else if (typeof object == "string") {           // a string
            String.forEach(object, block, context);

        } else if (typeof object.length == "number") {    // array-like object
            Array.forEach(object, block, context);

        } else {
            Object.forEach(object, block, context);
        }
    }
};


jQuery.fn.exists = function(){
    return jQuery(this).length > 0;
};


function Range(from, to) {
    var r  = [];
    for (var i = from; i < to; i++) {
        r.push(i);
    }
    return r;
}


