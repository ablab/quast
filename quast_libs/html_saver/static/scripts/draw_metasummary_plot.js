var refNames;
var refTicks;

var plotHeight = 0;
var sortRefsBtnHeight = 85;

var summary = {
    largest: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null
    },
    contigs: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null
    },nga50: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null
    },
    misassemblies: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null
    },
    totallen: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null
    },
    mismatches: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null
    },
    ns: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null
    },
    indels: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null
    },
    misassembled: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null
    },
    genome: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null
    },duplication: {
        isInitialized: false,
        maxY: 0,
        maxYTick: 0,
        series: null,
        showWithData: null
    },

    draw: function (name, title, colors, filenames, data, refPlotValue, tickX,
                    placeholder, legendPlaceholder, glossary, order, scalePlaceholder) {
        var sortBtnClass = getSortRefsRule();

        var selectedAssemblies = getSelectedAssemblies(filenames);
        $('#legend-placeholder').empty();

        filenames.forEach(function(filename, i) {
            addLabelToLegend(i, filename, selectedAssemblies, colors)
        });

        $(scalePlaceholder).empty();
        addSortRefsBtn(sortBtnClass);

        var coordX = data.coord_x;
        var coordY = data.coord_y;

        refNames = data.refnames;
        var cur_filenames = data.filenames;
        var info = summary[name];

        if (!info.isInitialized) {
            var plotsN = cur_filenames.length;
            info.series = new Array(plotsN);
            info.references = refNames;

            for (var i = 0; i < plotsN; i++) {
                var index = $.inArray(cur_filenames[order[i]], filenames);
                var plot_coordX = coordX[order[i]];
                var plot_coordY = coordY[order[i]];
                var size = plot_coordX.length;

                info.series[i] = {
                    data: [],
                    label: filenames[index],
                    number: index,
                    color: colors[index],
                    points: {
                        show: true,
                        fillColor: colors[index]
                    }
                };

                var x = 0.0;

                for (var k = 0; k < size; k++) {
                    info.series[i].data.push([plot_coordX[k], plot_coordY[k]]);
                    info.maxY = Math.max(info.maxY, plot_coordY[k]);
                }
            }

            for (i = 0; i < plotsN; i++) {
                info.series[i].lines = {show: true, lineWidth: 0.1}
            }
            var yFormatter = getBpTickFormatter;
            if ($.inArray(name, ['contigs', 'misassemblies', 'mismatches', 'indels', 'ns', 'duplication']) > -1)
                yFormatter = getJustNumberTickFormatter;
            else if (name == 'genome')
                yFormatter = getPercentTickFormatter;
            refTicks = [];
            for (var i = 0; i < refNames.length; i++)
            {
                refTicks.push([i + 1, refNames[i]]);
            }
            info.showWithData = function(series, colors) {
                var plot = $.plot(placeholder, series, {
                        shadowSize: 0,
                        colors: colors,
                        legend: {
                            container: $('useless-invisible-element-that-does-not-even-exist')
                        },
                        grid: {
                            borderWidth: 1,
                            hoverable: true,
                            autoHighlight: false,
                            mouseActiveRadius: 1000
                        },
                        yaxis: {
                            min: 0,
                            max: name == 'genome' ? Math.min(100, info.maxY + 10) : null,
                            labelWidth: 125,
                            reserveSpace: true,
                            lineWidth: 0.5,
                            color: '#000',
                            tickFormatter: yFormatter(info.maxY),
                            minTickSize: 1
                        },
                        xaxis: {
                            min: 0,
                            max: refNames.length + 1,
                            lineWidth: 1,
                            rotateTicks: 90,
                            color: '#000',
                            ticks: refTicks
                        },
                        minTickSize: 1
                    }
                );
                plotHeight = plot.height();

                var firstLabel = $('.yAxis .tickLabel').last();
                firstLabel.prepend(title + '<span class="rhs">&nbsp;</span>=<span class="rhs">&nbsp;</span>');

                bindTip(placeholder, series, plot, refToPrettyString, 1, refNames, 'top right', true);
            };

            info.isInitialized = true;
        }

        addLegendClickEvents(info, filenames.length, showPlotWithInfo);

        showPlotWithInfo(info);
        moveSortRefsBtns();
        setSortRefsBtns(info);
    }
};

function getSortRefsRule() {
    if ($("input[name=sortRefs]")[0])
        return $("input[name=sortRefs]:checked").val();
}

function moveSortRefsBtns() {
    var spaceAfterHeight = Math.max(50, plotHeight - $('#legend-placeholder').height() + 5);
    $('#sortRefsBtnSpace').height(spaceAfterHeight);
}

function addSortRefsBtn(sortBtnClass) {
    $('#legend-placeholder').append('<div id="sortRefsBtnSpace"></div>');
    $('#legend-placeholder').append('<span>Sort references by: <br></span>');
    $('#legend-placeholder').append(
        '<label><input type="radio" id="sortByName" name="sortRefs" value="name">&nbspname</label><br>'
    );
    $('#legend-placeholder').append(
        '<label><input type="radio" id="sortByValue" name="sortRefs" value="value" checked="checked">' +
        '&nbspthe best average value<br>among all assemblies</label>'
    );
    if (sortBtnClass)
       $("input[name=sortRefs][value=" + sortBtnClass + "]").prop('checked', true);
}

function setSortRefsBtns(info, index) {
    $("#legend-placeholder input[name='sortRefs']").click(function(){
        showPlotWithInfo(info, index);
    });
}

function getSortOrder() {
    var sortOrder = getSortRefsRule();
    if (!sortOrder)
        return;
    if (sortOrder == 'name') {
        sortOrder = 'alphabet';
    }
    else if (sortOrder) {
        sortOrder = 'value';
    }
    return sortOrder;
}

function sortReferences(sortOrder, info) {
    if (sortOrder == 'alphabet') {
        refNames = sortedRefs;
    }
    else if (sortOrder == 'value') {
        refNames = info.references;
    }
    refTicks = [];
    for (var i = 0; i < refNames.length; i++)
    {
        refTicks.push([i + 1, refNames[i]]);
    }
}
