
var show_all_span =
    "<span class='selected-switch misassemblies'>" +
        'Back to overview' +
        "</span>";
var show_all_a =
    "&nbsp;&nbsp;&nbsp;<a class='dotted-link' onClick='showAll()'>" +
        'Back to overview' +
        "</a>";


var misassemblies = {
    isInitialized: false,

    maxY: 0,
    plot: null,
    series: null,
    showWithData: null,
    minPow: 0,
    ticks: null,
    placeholder: null,
    legendPlaceholder: null,
    colors: null,
    yAxisLabeled: false,

    show_all_el: show_all_span,

    draw: function(name, title, colors, filenames, data, reflen, tickX,
                   placeholder, legendPlaceholder, glossary, order, scalePlaceholder) {

        var cur_filenames = data.filenames;
        var main_coordX = data.main_coord_x;
        var main_coordY = data.main_coord_y;
        var refs = data.refnames;
        var main_refnames = data.main_refnames;

        misassemblies.refs = main_refnames;
        misassemblies.colors = colors;
        misassemblies.filenames = filenames;
        misassemblies.placeholder = placeholder;

        if (!misassemblies.isInitialized) {
            var plotsN = cur_filenames.length;
            misassemblies.series = new Array(plotsN + 1);
            misassemblies.series[0] = new Array(plotsN);
            for (var i = 0; i < plotsN; i++) {
                var index = $.inArray(cur_filenames[order[i]], filenames);
                var plot_coordX = main_coordX[order[i]];
                var plot_coordY = main_coordY[order[i]];
                var size = plot_coordX.length;
                misassemblies.series[0][i] = {
                    data: [],
                    label: filenames[index],
                    number: index,
                    color: colors[index],
                    points: {
                        show: true,
                        fillColor: colors[index]
                    },
                    lines: {
                        show: true,
                        lineWidth: 0.1
                    }
                };
                for (var k = 0; k < size; k++) {
                    misassemblies.series[0][i].data.push([plot_coordX[k], plot_coordY[k]]);
                }
                if (misassemblies.series[0][i].data[0][1] > misassemblies.maxY) {
                    misassemblies.maxY = misassemblies.series[0][i].data[0][1];
                }
            }
            var misassembl_colors = ['#E31A1C', '#1F78B4', '#33A02C'];
            var misassembl_labels = ['relocations', 'translocations', 'inversions'];

            refTicks = [];
            for (var ref_n = 0; ref_n < main_refnames.length; ref_n++) {
                    refTicks.push([ref_n + 1, main_refnames[ref_n]]);
            }
            misassemblies.references = main_refnames;
            refs = refs.filter( function(el) {
              return main_refnames.indexOf(el) > -1;
            } );
            for (var file_n = 0; file_n < filenames.length; file_n++) {
                var coordX = data.coord_x[file_n];
                var coordY = data.coord_y[file_n];
                plotsN = 3;
                var sortedRefs = [];
                for (var refs_n = 0; refs_n < refs.length; refs_n++){
                    var curRefN = $.inArray(main_refnames[refs_n], refs);
                    for (i = 0; i < plotsN; i++) {
                        sortedRefs.push([refs_n+1, coordY[plotsN*curRefN+i]]);
                    }
                }

                misassemblies.series[file_n + 1] = new Array(plotsN);
                for (i = 0; i < plotsN; i++) {

                    size = coordX.length/plotsN;
                    misassemblies.series[file_n + 1][i] = {
                        data: [],
                        label: misassembl_labels[i],
                        number: i,
                        color: misassembl_colors[i],
                        stack: 0,
                        lines: {
                            show: false,
                            steps: false
                        },
                        bars: {
                            show: true,
                            lineWidth: 0.6,
                            fill: 0.6,
                            barWidth: 0.9,
                            align: 'center'
                        }
                    };

                    for (k = 0; k < size; k++) {
                        misassemblies.series[file_n + 1][i].data.push(sortedRefs[k*plotsN+i]);
                    }
                    if (misassemblies.series[file_n + 1][i].data[0][1] > misassemblies.maxY) {
                        misassemblies.maxY = misassemblies.series[file_n + 1][i].data[0][1];
                    }
                }
            }

            misassemblies.showWithData = showAllAssemblies;
            misassemblies.isInitialized = true;
        }
        createLegend(misassemblies.filenames, misassemblies.colors, 0);
        showPlotWithInfo(misassemblies, 0);

        $('#change-assembly').css('visibility', 'visible');

        $('#misassemblies_info').hide();
    }
};


function showAllAssemblies(series, colors) {
    if (series == null)
        return;

    misassemblies.plot = $.plot(misassemblies.placeholder, series, {
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
                labelWidth: 125,
                reserveSpace: true,
                lineWidth: 0.5,
                color: '#000',
                tickFormatter: getJustNumberTickFormatter(misassemblies.maxY),
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
    var firstLabel = $('.yAxis .tickLabel').last();
    firstLabel.prepend('Misassemblies<span class="rhs">&nbsp;</span>=<span class="rhs">&nbsp;</span>');
    unBindTips(misassemblies.placeholder);
    bindTip(misassemblies.placeholder, series, misassemblies.plot, refToPrettyString, 1, refNames, 'top right', true);
}


function showOneAssembly(series, colors) {
    if (series == null) {
        return;
    }

    misassemblies.plot = $.plot(misassemblies.placeholder, series, {
            shadowSize: 0,
            colors: colors,
            legend: {
                container: $('useless-invisible-element-that-does-not-even-exist')
            },
            grid: {
                hoverable: true,
                borderWidth: 1,
                autoHighlight: false,
                mouseActiveRadius: 1000
            },
            yaxis: {
                min: 0,
                labelWidth: 125,
                reserveSpace: true,
                lineWidth: 0.5,
                color: '#000',
                tickFormatter: getJustNumberTickFormatter(misassemblies.maxY),
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

    var firstLabel = $('.yAxis .tickLabel').last();
    firstLabel.append(' misassemblies');
    unBindTips(misassemblies.placeholder);
    bindTip(misassemblies.placeholder, series, misassemblies.plot, refToPrettyString, 1, refNames,  'top right');
}

function showAll() {
    createLegend(misassemblies.filenames, misassemblies.colors, 0);
    misassemblies.show_all_el = show_all_span;
    misassemblies.showWithData = showAllAssemblies;

    $('#show_all_label').html(misassemblies.show_all_el);
    showPlotWithInfo(misassemblies, 0);
}


function showPlot(index) {
    var misassembl_labels = ['relocations', 'translocations', 'inversions'];
    var misassembl_colors = ['#E31A1C', '#1F78B4', '#33A02C'];
    createLegend(misassembl_labels, misassembl_colors, index);

    misassemblies.show_all_el = show_all_a;
    misassemblies.showWithData = showOneAssembly;

    $('#show_all_label').html(misassemblies.show_all_el);
    showPlotWithInfo(misassemblies, index);
}

function createLegend(labels, colors, index){
    var sortBtnClass = getSortRefsRule();

    var filenames = misassemblies.filenames;
    var selectedLabels = getSelectedAssemblies(labels);
    $('#legend-placeholder').empty();
    var selectors = "";

    labels.forEach(function(label, i) {
        var link = index == 0 ? '<span id="' + labels[i] + '-switch"' + " class='plot-mis-type-switch dotted-link'>by type<br></span><br>" : '';
        addLabelToLegend(i, label, selectedLabels, colors, link);
    });
    if (index > 0) {
        for (var filenames_n = 0; filenames_n < filenames.length; filenames_n++){
            selectors += '<br><span id="' + filenames[filenames_n] + '-switch" ' +
                "class='plot-switch dotted-link'>" +
                filenames[filenames_n] + "</span>";
        }
        $('#legend-placeholder').append(
            "<br><br><div id='change-assembly' style='margin-right: 3px;'>" +
                "<span id='show_all_label'>" +
                misassemblies.show_all_el +
                "</span><br>" + selectors +
                "</div>"
        );
        addSortRefsBtn(sortBtnClass);
    }
    else addSortRefsBtn(sortBtnClass);
    addLinksToSwitches(index - 1);
    addLegendClickEvents(misassemblies, misassemblies.series.length, showPlotWithInfo, false, index);
    moveSortRefsBtns();
    setSortRefsBtns(misassemblies, index);
}

function addLinksToSwitches(index) {
    var filenames = misassemblies.filenames;
    for (filenames_n = 0; filenames_n < filenames.length; filenames_n++){
        var switchSpan = document.getElementById(filenames[filenames_n] + "-switch");
        $(switchSpan).click(getToggleSwitchFunction(filenames_n+1));
        if (filenames_n == index) {
            switchSpan.className = 'plot-switch selected-switch misassemblies';
        }
    }
}

function getToggleSwitchFunction(index) {
    return function() {
        if (index > 0) showPlot(index);
        else showAll();
    };
}