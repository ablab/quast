/*
The MIT License (MIT)

Copyright (c) 2013 bill@bunkat.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
    var ContigData = function(chromosome) {

        var parseData = function (data) {
            chart = { assemblies: {} };

            for (var i = 0; i < data.length; i++) {
                if (!chart.assemblies[data[i].assembly])
                    chart.assemblies[data[i].assembly] = [];

                var sublane = 0;

                while (isOverlapping(data[i], chart.assemblies[data[i].assembly][sublane]))
                    sublane++;

                if (!chart.assemblies[data[i].assembly][sublane])
                    chart.assemblies[data[i].assembly][sublane] = [];

                chart.assemblies[data[i].assembly][sublane].push(data[i]);
            }

            return collapseLanes(chart);
        };

        var isOverlapping = function(item, lane) {
            if (lane)
                for (var i = 0; i < lane.length; i++)
                    if (item.corr_start <= lane[i].corr_end && lane[i].corr_start <= item.corr_end)
                        return true;

            return false;
        };


        var collapseLanes = function (chart) {
            var lanes = [], items = [], laneId = 0, itemId = 0;

            for (var assemblyName in chart.assemblies) {
                var lane = chart.assemblies[assemblyName];

                for (var i = 0; i < lane.length; i++) {
                    var subLane = lane[i];
                    var numItems = 0;
                    for (var j = 0; j < subLane.length; j++) {
                        var item = subLane[j];
                        if (item.name != 'FICTIVE') {
                            item.lane = laneId;
                            item.id = itemId;
                            items.push(item);
                            itemId++;
                            numItems++;
                        }
                    }

                    if (numItems > 0){
                        lanes.push({
                        id: laneId,
                        label: i === 0 ? assemblyName : ''
                        });
                        laneId++;
                    }

                }
            }

            return {lanes: lanes, items: items};
        };

        // return parseData(generateRandomWorkItems());
        return parseData(contig_data[chromosome]);
    };

    /**
     * Allow library to be used within both the browser and node.js
     */
    var root = typeof exports !== "undefined" && exports !== null ? exports : window;
    root.contigData = ContigData;

    var data = contigData(CHROMOSOME),
            lanes = data.lanes,
            items = data.items;
    var w = 0.9 * (window.innerWidth || document.documentElement.clientWidth || document.body.clientWidth) - 300;
    var margin = {
                top: 20, right: 15, bottom: 15, left: /*Math.max(d3.max(lanes, function (d) {
                 return getTextSize(d.label);
                 }), 120)*/ 145
            },
            mainLanesHeight = 40,
            miniLanesHeight = 18,
            lanesInterval = 20,
            miniScale = 50,
            mainScale = 50,
            width = w,
            chartWidth = w,
            miniHeight = lanes.length * miniLanesHeight,
            mainHeight = lanes.length * (mainLanesHeight + lanesInterval),
            coverageHeight = typeof coverage_data != 'undefined' ? 125 : 0;
            coverageSpace = typeof coverage_data != 'undefined' ? 50 : 0;
    height = Math.max(mainHeight + 4 * coverageHeight + miniHeight + miniScale + mainScale - margin.bottom - margin.top, 500);
    var total_len = 0;
    for (var chr in chromosomes_len) {
        total_len += chromosomes_len[chr];
    };
    var x_mini = d3.scale.linear()
            .domain([0, total_len])
            .range([0, chartWidth]);
    var x_main = d3.scale.linear()
            .range([0, chartWidth]);

    var ext = d3.extent(lanes, function (d) {
        return d.id;
    });
    var y_main = d3.scale.linear().domain([ext[0], ext[1] + 1]).range([0, mainHeight]);
    var y_mini = d3.scale.linear().domain([ext[0], ext[1] + 1]).range([0, miniHeight]);

    var letterSize = getSize('w') - 1;
    var numberSize = getSize('0') - 1;

    var chart = d3.select('body').append('div').attr('id', 'chart')
            .append('svg:svg')
            .attr('width', width + margin.right + margin.left)
            .attr('height', height + margin.top + margin.bottom)
            .attr('class', 'chart');

    chart.append('defs').append('clipPath')
            .attr('id', 'clip')
            .append('rect')
            .attr('width', width)
            .attr('height', mainHeight);

    var main = chart.append('g')
            .attr('transform', 'translate(' + margin.left + ',' + margin.top + ')')
            .attr('width', chartWidth)
            .attr('height', mainHeight + mainScale)
            .attr('class', 'main');

    var mini = chart.append('g')
            .attr('transform', 'translate(' + margin.left + ',' + (mainHeight + mainScale + coverageHeight + 50) + ')')
            .attr('width', chartWidth)
            .attr('height', miniHeight + miniScale)
            .attr('class', 'mini');

    // draw the lanes for the main chart
    main.append('g').selectAll('.laneLines')
            .data(lanes)
            .enter().append('line')
            .attr('x1', 0)
            .attr('y1', function (d) {
                return d3.round(y_main(d.id)) + .5;
            })
            .attr('x2', chartWidth)
            .attr('y2', function (d) {
                return d3.round(y_main(d.id)) + .5;
            })
            .attr('stroke', function (d) {
                return d.label === '' ? 'white' : 'lightgray'
            });

    main.append('g').selectAll('.laneText')
            .data(lanes)
            .enter().append('text')
            .text(function (d) {
                return getVisibleText(d.label, 180);
            })
            .attr('x', -10)
            .attr('y', function (d) {
                return y_main(d.id + .5);
            })
            .attr('dy', '.5ex')
            .attr('text-anchor', 'end')
            .attr('class', 'laneText');

    // draw the lanes for the mini chart
    mini.append('g').selectAll('.laneLines')
            .data(lanes)
            .enter().append('line')
            .attr('x1', 0)
            .attr('y1', function (d) {
                return d3.round(y_mini(d.id)) + .5;
            })
            .attr('x2', chartWidth)
            .attr('y2', function (d) {
                return d3.round(y_mini(d.id)) + .5;
            })
            .attr('stroke', function (d) {
                return d.label === '' ? 'white' : 'lightgray'
            });

    mini.append('g').selectAll('.laneText')
            .data(lanes)
            .enter().append('text')
            .text(function (d) {
                return d.label;
            })
            .attr('x', -10)
            .attr('y', function (d) {
                return y_mini(d.id + .5);
            })
            .attr('dy', '.5ex')
            .attr('text-anchor', 'end')
            .attr('class', 'laneText');

    // draw the x axis
    var miniTickValue;

    if (x_mini.domain()[1] > 1000000000)
        miniTickValue = 'Gbp';
    else if (x_mini.domain()[1] > 1000000)
        miniTickValue = 'Mbp';
    else if (x_mini.domain()[1] > 1000)
        miniTickValue = 'kbp';
    else
        miniTickValue = 'bp';

    var miniTicksValues = x_mini.ticks(5);
    miniTicksValues = [x_mini.domain()[0]].concat(miniTicksValues);
    miniTicksValues.push(x_mini.domain()[1]);

    var format = function (d) {
                if (miniTickValue == 'Gbp')
                    return d3.round(d / 1000000000, 2);
                else if (miniTickValue == 'Mbp')
                    return d3.round(d / 1000000, 2);
                else if (miniTickValue == 'kbp')
                    return d3.round(d / 1000, 2);
                else
                    return d;
            };

    var min_ticks_delta = Math.max(getTextSize(format(miniTicksValues.slice(-1)[0]).toString(), numberSize),
                getTextSize(format(miniTicksValues.slice(-2)[0]).toString(), numberSize));
    if (x_mini(miniTicksValues.slice(-1)[0]) - x_mini(miniTicksValues.slice(-2)[0]) < min_ticks_delta) {
        miniTicksValues.splice(-2, 1)
    }

    var xMiniAxis = d3.svg.axis()
            .scale(x_mini)
            .orient('bottom')
            .tickSize(6, 0, 0)
            .tickValues(miniTicksValues)
            .tickFormat(format);

    var mainTickValue;

    var xMainAxis = d3.svg.axis()
            .scale(x_main)
            .orient('bottom')
            .tickSize(6, 0, 0);
    var mainTicksValues = [];

    xMainAxis.tickValues(mainTicksValues)
                .tickFormat(format);
    main.append('g')
            .attr('transform', 'translate(0,' + mainHeight + ')')
            .attr('class', 'main axis')
            .call(xMainAxis);

    mini.append('g')
            .attr('transform', 'translate(0,' + miniHeight + ')')
            .attr('class', 'axis')
            .call(xMiniAxis).append('text')
            .attr('transform', 'translate(' + x_mini(x_mini.domain()[1]) + ',' + (miniScale / 2 + 2) + ')');
    var lastTickValue = miniTicksValues.pop();
    var lastTick = mini.select(".axis").selectAll("g")[0].pop();
    d3.select(lastTick).select('text').text(format(lastTickValue) + ' ' + miniTickValue)
            .attr('transform', 'translate(-10, 0)');
    // draw a line representing today's date
    main.append('line')
            .attr('y1', 0)
            .attr('y2', mainHeight)
            .attr('class', 'main curSegment')
            .attr('clip-path', 'url(#clip)');

    var current = (x_mini.domain()[1] + x_mini.domain()[0]) / 2;

    mini.append('line')
            .attr('x1', x_mini(current) + .5)
            .attr('y1', 0)
            .attr('x2', x_mini(current) + .5)
            .attr('y2', miniHeight)
            .attr('class', 'curSegment');

    var visItems = null;
    main.append('rect')
            .attr('pointer-events', 'painted')
            .attr('width', chartWidth)
            .attr('height', mainHeight)
            .attr('visibility', 'hidden')
            .on('click', function (d) {
                coordinates = d3.mouse(this);
                var x = coordinates[0];
                var y = coordinates[1];
                var laneHeight = mainHeight / lanes.length;
                var lane = parseInt(y / laneHeight);
                var laneCoords1 = laneHeight*lane;
                var laneCoords2 = laneHeight*(lane+1);
                var itemToSelect = null;
                var minX = 10;
                var e = itemRects.selectAll(".mainItem").filter(function () {
                    var width = this.getBoundingClientRect().width;
                    var curCoords = d3.transform(d3.select(this).attr("transform")).translate;
                    var curY = curCoords[1];
                    if (curY > laneCoords1 && curY < laneCoords2) {
                        var currentx = curCoords[0];
                        if (Math.abs(currentx - x) < 10 || Math.abs(currentx + width - x) < 10 ) {
                            if (Math.abs(currentx - x) < minX) {
                                minX = Math.abs(currentx - x);
                                itemToSelect = d3.select(this);
                                return d3.select(this)
                            }
                        }
                    }
                }); // each
                if (e.length > 0) {
                    e = itemToSelect[0].pop();
                    e.__onclick();
                }
    });
    // draw the items
    var itemRects = main.append('g')
            .attr('clip-path', 'url(#clip)');

    mini.append('g').selectAll('miniItems')
            .data(getPaths(items))
            .enter().append('path')
            .attr('class', function (d) {
                return 'miniItem ' + d.class;
            })
            .attr('d', function (d) {
                return d.path;
            });

    // invisible hit area to move around the selection window
    mini.append('rect')
            .attr('pointer-events', 'painted')
            .attr('width', chartWidth)
            .attr('height', miniHeight)
            .attr('visibility', 'hidden')
            .on('mouseup', moveBrush);

    // draw the selection area
    var delta = (x_mini.domain()[1] - x_mini.domain()[0]) / 8;

    var brush = d3.svg.brush()
            .x(x_mini)
            .extent([current - delta, current + delta])
            .on("brush", display);
    d3.select('body').on("keypress", keyPressAnswer);
    d3.select('body').on("keydown", keyDownAnswer);

    mini.append('g')
            .attr('class', 'x brush')
            .call(brush)
            .selectAll('rect')
            .attr('y', 1)
            .attr('height', miniHeight - 1);

    mini.selectAll('rect.background').remove();

    // draw contig info menu
    var menu = d3.select('body').append('div')
            .attr('id', 'menu');
    menu.append('div')
            .attr('class', ' block title')
            .text('Contig info');
    info = menu.append('div')
            .attr('class', 'block');
    info.append('p')
            .style({'text-align': 'center'})
            .text('<CLICK ON CONTIG>');

    var selected_id;
    var prev = undefined;

    if (typeof coverage_data != 'undefined') {
// draw mini coverage
        var x_cov_mini_S = x_mini,      // x coverage scale
                y_cov_mini_S = d3.scale.log()
                        .domain([Math.max(d3.max(coverage_data[CHROMOSOME], function (d) {
                            return d;
                        }), 100) + 100, .1])
                        .range([0, coverageHeight]),
                x_cov_mini_A = xMiniAxis,
                y_cov_mini_A = d3.svg.axis()
                        .scale(y_cov_mini_S)
                        .orient('left')
                        .tickValues(y_cov_mini_S.ticks().filter(function (d) {
                            var i = 0;
                            for (; Math.pow(10, i) < d; ++i);
                            if (d == Math.pow(10, i) && d <= y_cov_mini_S.domain()[0]) return d;
                        }))
                        .tickSize(2, 0),
                mini_cov = chart.append('g')
                        .attr('class', 'coverage')
                        .attr('transform', 'translate(' + margin.left + ', ' + (mainHeight +
                        miniHeight +
                        lanesInterval +
                        coverageHeight +
                        miniScale +
                        mainScale + 50) + ')');

        mini_cov.append('g')
                .attr('class', 'y')
                .call(y_cov_mini_A);

        mini_cov.append('g')
                .attr('transform', 'translate(0, ' + (coverageHeight) + ')')
                .attr('class', 'x')
                .call(x_cov_mini_A);

        var line = '',
                l = coverage_data[CHROMOSOME].length,
                cov_mini_dots_amount = Math.min(500, l),
                pos = 0,
                step = l / cov_mini_dots_amount;

        for (var s, i = step; i < l; i += step) {
            s = d3.sum(coverage_data[CHROMOSOME].slice(pos, i), function (d) {
                        return d
                    }) / step;
            if (s >= 1)
                line += ['M', x_cov_mini_S(pos * 100), y_cov_mini_S(s), 'H', x_cov_mini_S(i * 100)].join(' ');
            pos = i;
        }

        var not_covered_line = '',
                not_covered_width = Math.max(chartWidth / x_cov_mini_S.domain()[1], 1);

        for (var i = 0; i < not_covered[CHROMOSOME].length; ++i) {
            not_covered_line += ['M', x_cov_mini_S(not_covered[CHROMOSOME][i]), y_cov_mini_S(.1), 'H', x_cov_mini_S(not_covered[CHROMOSOME][i]) + not_covered_width].join(' ');
        }

        mini_cov.append('g')
                .append('path')
                .attr('class', 'covered')
                .attr('d', line);
        mini_cov.append('g')
                .append('path')
                .attr('class', 'notCovered')
                .attr('d', not_covered_line);
        mini_cov.append('text')
                .attr('transform', 'translate(' + (chartWidth / 2) + ',' + (coverageHeight + 50) + ')')
                .text('Genome, ' + miniTickValue);
        mini_cov.append('text')
                .text('Coverage')
                .attr('transform', 'rotate(-90 20, 80)');

//invisible area for events
        mini_cov.append('rect')
                .attr('pointer-events', 'painted')
                .attr('width', chartWidth)
                .attr('height', coverageHeight)
                .attr('visibility', 'hidden')
                .on('mouseup', moveCovBrush);

// draw the selection area
        var delta = (x_cov_mini_S.domain()[1] - x_cov_mini_S.domain()[0]) / 8;

        var brush_cov = d3.svg.brush()
                .x(x_cov_mini_S)
                .extent([current - delta, current + delta])
                .on("brush", sync);

        mini_cov.append('g')
                .attr('class', 'x brush')
                .call(brush_cov)
                .selectAll('rect')
                .attr('y', 1)
                .attr('height', coverageHeight - 1);

        mini_cov.selectAll('rect.background').remove();

// draw main coverage
        var y_cov_main_S = y_cov_mini_S;

        var y_cov_main_vals = function (d) {
            var i = 0;
            for (; Math.pow(10, i) < d; ++i);
            if (d == Math.pow(10, i) && d <= y_cov_main_S.domain()[0]) return d;
        };

        var y_cov_main_A = y_cov_mini_A = d3.svg.axis()
                .scale(y_cov_main_S)
                .orient('left')
                .tickValues(y_cov_main_S.ticks().filter(y_cov_main_vals))
                .tickSize(2, 0);

        var x_cov_main_A = xMainAxis;

        var main_cov = chart.append('g')
                .attr('class', 'COV')
                .attr('transform', 'translate(' + margin.left + ', ' + (mainHeight +
                lanesInterval +
                mainScale) + ')');
        main_cov.append('g')
                .attr('class', 'x')
                .attr('transform', 'translate(0, ' + coverageHeight + ')')
                .call(x_cov_main_A);
        main_cov.append('g')
                .attr('class', 'y')
                .attr('transform', 'translate(0, 0)')
                .call(y_cov_main_A);
    }

    var arrows = [];
    var arrow = "M0,101.08h404.308L202.151,303.229L0,101.08z";
    var lines = [];
    var len = 0;
    var commonChrName = CHROMOSOME.length + 1;
    if (chrContigs.length > 1) {
        for (var chr in chromosomes_len) {
            var shortName = chr.slice(commonChrName, chr.length);
            lines.push({name: shortName, corr_start: len, corr_end: len + chromosomes_len[chr], y1: 0, y2: mainHeight, len: chromosomes_len[chr]});
            len += chromosomes_len[chr];
        }
    }
    var itemLines = main.append('g')
            .attr('clip-path', 'url(#clip)');
    var refNames = main.append('g');

    display();

    document.getElementById('left').onclick=function() {
            keyPress('left', 2) };
    document.getElementById('left_shift').onclick=function() {
            keyPress('left', 10) };
    document.getElementById('right').onclick=function() {
            keyPress('right', 2) };
    document.getElementById('right_shift').onclick=function() {
            keyPress('right', 10) };
    document.getElementById('zoom_in').onclick=function() {
            keyPress('zoom_in', 25) };
    document.getElementById('zoom_in_5').onclick=function() {
            keyPress('zoom_in', 40) };
    document.getElementById('zoom_out').onclick=function() {
            keyPress('zoom_out', 50) };
    document.getElementById('zoom_out_5').onclick=function() {
            keyPress('zoom_out', 200) };

    function display() {
        x_main = d3.scale.linear()
            .range([0, chartWidth]);
        var rects
                , minExtent = Math.max(brush.extent()[0], x_mini.domain()[0])
                , maxExtent = Math.min(brush.extent()[1], x_mini.domain()[1])
                , visibleText = function (d) {
                    var drawLimit = letterSize * 3;
                    var visibleLength = x_main(Math.min(maxExtent, d.corr_end)) - x_main(Math.max(minExtent, d.corr_start)) - 20;
                    if (visibleLength > drawLimit)
                        return getVisibleText(d.name, visibleLength, d.len);
                },
                visibleArrows = arrows.filter(function (d) {
                    if (d.corr_start < maxExtent && d.corr_end > minExtent) return d;
                }),
                visibleLines = lines.filter(function (d) {
                    if (d.corr_end < maxExtent) return d;
                }),
                visibleRefNames = lines.filter(function (d) {
                    if (d.corr_start < maxExtent && d.corr_end > minExtent) return d;
                });
        visItems = items.filter(function (d) {
                if (d.corr_start < maxExtent && d.corr_end > minExtent) {
                    var drawLimit = 1;
                    var visibleLength = x_main(Math.min(maxExtent, d.corr_end)) - x_main(Math.max(minExtent, d.corr_start));
                    if (visibleLength > drawLimit)
                        return d;
                }
            }),
        mini.select('.brush').call(brush.extent([minExtent, maxExtent]));
        if (typeof coverage_data != "undefined")
            mini_cov.select('.brush').call(brush_cov.extent([minExtent, maxExtent]));

        x_main.domain([minExtent, maxExtent]);

        // shift the today line
        main.select('.main.curLine')
                .attr('x1', x_main(current) + .5)
                .attr('x2', x_main(current) + .5);

        //upd arrows
        var shift = 4.03;

        //lines between reference contigs
        main.selectAll('.lines').remove();
        var lineContigs = itemLines.selectAll('.g')
                .data(visibleLines, function (d) {
                    return d.id;
                });

        var others = lineContigs.enter().append('g')
                .attr('class', 'lines')
                .attr('transform', function (d) {
                    var x = x_main(d.corr_end);
                    var y = 0;

                    return 'translate(' + x + ', ' + y + ')';
                });
        others.append('rect')
                .attr('width', function (d) {
                    return 1;
                })
                .attr('height', function (d) {
                    return d.y2;
                })
                .attr('fill', '#300000');

        var chartArrows = main.selectAll('.arrow')
                .data(visibleArrows, function (d) {
                    return d.id;
                })
                .attr('transform', function (d) {
                    var x = x_main((Math.max(minExtent, d.corr_start) + Math.min(maxExtent, d.corr_end)) / 2) - shift;
                    var y = y_main(d.lane);

                    return 'translate(' + x + ', ' + y + ')';
                });
        chartArrows.select('path')
                .attr('transform', 'scale(0.02)');
        chartArrows.enter().append('g')
                .attr('transform', function (d) {
                    var x = x_main((Math.max(minExtent, d.corr_start) + Math.min(maxExtent, d.corr_end)) / 2) - shift;
                    var y = y_main(d.lane);

                    return 'translate(' + x + ', ' + y + ')';
                })
                .attr('class', 'arrow')
                .append('path')
                .attr('d', arrow)
                .attr('transform', 'scale(0.02)');

        chartArrows.exit().remove();
        // update the item rects

        rects = itemRects.selectAll('g')
                .data(visItems, function (d) {
                    return d.id;
                })
                .attr('transform', function (d) {
                    var x = x_main(Math.max(minExtent, d.corr_start));
                    var y = y_main(d.lane) + .25 * lanesInterval;
                    d.class.search("misassembled") != -1 ? y += .25 * lanesInterval : 0;
                    d.class.search("odd") != -1 ? y += .25 * lanesInterval : 0;

                    return 'translate(' + x + ', ' + y + ')';
                })
                .attr('width', function (d) {
                    return x_main(Math.min(maxExtent, d.corr_end)) - x_main(Math.max(minExtent, d.corr_start));
                });

        rects.select('.R')
                .attr('transform',  function (d) {
                    if (d.id == selected_id){return 'translate(1,1)';}
                })
                .attr('width', function (d) {
                    var w = x_main(Math.min(maxExtent, d.corr_end)) - x_main(Math.max(minExtent, d.corr_start));
                    return (d.id == selected_id ? Math.max(w - 2, .5) : w);
                })
                .attr('height', function (d) {
                    return (d.id == selected_id ? mainLanesHeight - 2 : mainLanesHeight);
                })
                .attr('stroke', 'black')
                .attr('stroke-width', function (d) {
                    return (d.id == selected_id ? 2 : 0);
                })
                .attr('stroke-opacity', function (d) {
                    return (d.id == selected_id ? 1 : 0);
                });

        rects.select('text')
                .text(visibleText);
        rects.exit().remove();

        var other = rects.enter().append('g')
                .attr('class', function (d) {
                    return 'mainItem ' + d.class;
                })
                .attr('transform', function (d) {
                    var x = x_main(Math.max(minExtent, d.corr_start));
                    var y = y_main(d.lane) + .25 * lanesInterval;
                    d.class.search("misassembled") != -1 ? y += .25 * lanesInterval : 0;
                    d.class.search("odd") != -1 ? y += .25 * lanesInterval : 0;

                    return 'translate(' + x + ', ' + y + ')';
                })
                .attr('width', function (d) {
                    return x_main(Math.min(maxExtent, d.corr_end)) - x_main(Math.max(minExtent, d.corr_start));
                });

        other.append('rect')
                .attr('class', 'R')
                .attr('transform',  function (d) {
                    if (d.id == selected_id){return 'translate(1,1)';}
                })
                .attr('width', function (d) {
                    var w = x_main(Math.min(maxExtent, d.corr_end)) - x_main(Math.max(minExtent, d.corr_start));
                    return (d.id == selected_id ? Math.max(w - 2, .5) : w);
                })
                .attr('height', function (d) {
                    return (d.id == selected_id ? mainLanesHeight - 4 : mainLanesHeight);
                })
                .attr('stroke', 'black')
                .attr('stroke-width', function (d) {
                    return (d.id == selected_id ? 2 : 0);
                })
                .attr('stroke-opacity', function (d) {
                    return (d.id == selected_id ? 1 : 0);
                });

        other.on('click', function A(d, i) {
            selected_id = d.id;
            changeInfo(d);
        })
                .on('mouseenter', glow)
                .on('mouseleave', disglow);

        other.append('text')
                .text(visibleText)
                .attr('text-anchor', 'start')
                .attr('class', 'itemLabel')
                .attr('transform', 'translate(5, 20)');


        // upd coverage
        if (typeof coverage_data != 'undefined') {
            main_cov.select('.covered').remove();
            main_cov.select('.notCovered').remove();

            x_cov_main_A = xMainAxis;
            main_cov.select('.x').call(x_cov_main_A);

            var line = '',
            l = (maxExtent - minExtent) / 100,
            cov_main_dots_amount = Math.min(500, l),
            step = Math.ceil(l / cov_main_dots_amount);

            var y_max = 1;
            for (var s, i = (minExtent / 100); i + step <= (maxExtent / 100); i += step) {
                s = d3.sum(coverage_data[CHROMOSOME].slice(i, i + step), function (d) {
                            return d
                        }) / step;
                y_max = Math.max(y_max, s);
                if (s >= 1)
                    line += ['M', x_main(i * 100), y_cov_main_S(s), 'H', x_main((i + step) * 100)].join(' ');
            }

        y_cov_main_S.domain([y_max + 100, .1]);
            y_cov_main_A.scale(y_cov_main_S);
            y_cov_main_A.tickValues(y_cov_main_S.ticks().filter(y_cov_main_vals));
            main_cov.select('.y').call(y_cov_main_A);

            var not_covered_line = '',
                    not_covered_width = Math.max(chartWidth / (maxExtent - minExtent + 1), 1);

            var vnc = not_covered[CHROMOSOME].filter(function (d) {
                if (minExtent <= d && d <= maxExtent) return d;
            });
            for (var i = 0; i < vnc.length; ++i) {
                not_covered_line += ['M', x_main(vnc[i]), y_cov_main_S(.1), 'H', x_main(vnc[i]) + not_covered_width].join(' ');
            }

            main_cov.append('g')
                    .attr('class', 'covered')
                    .append('path')
                    .attr('d', line);
        if (not_covered_line != '')
                main_cov.append('g')
                    .attr('class', 'notCovered')
                    .append('path')
                    .attr('d', not_covered_line);
        }
        main.selectAll('.refs').remove();

        var visibleRefs = refNames.selectAll('.g')
                .data(visibleRefNames, function (d) {
                    return d.id;
                });

        var otherRefs = visibleRefs.enter().append('g')
                .attr('class', 'refs')
                .attr('transform', function (d) {
                    var x = x_main(Math.max(minExtent, d.corr_start));
                    var y = d.y2 + 10;

                    return 'translate(' + x + ', ' + y + ')';
                })
                .attr('width', function (d) {
                    return x_main(Math.min(maxExtent, d.corr_end)) - x_main(Math.max(minExtent, d.corr_start));
                });
        otherRefs.append('text')
                .text(visibleText)
                .attr('text-anchor', 'start')
                .attr('class', 'itemLabel')
                .attr('transform', 'translate(10, 0)');
    }

    function keyPress (cmd, deltaCoeff) {
        var ext = brush.extent();
        var delta = .01 * (ext[1] - ext[0]);
        if (deltaCoeff) delta *= deltaCoeff;
        switch (cmd) {
            case 'zoom_in':
                brush.extent([ext[0] + delta, ext[1] - delta]);
                break;
            case 'zoom_out':
                brush.extent([ext[0] - delta, ext[1] + delta]);
                break;
            case 'left':
                brush.extent([ext[0] - delta, ext[1] - delta]);
                break;
            case 'right':
                brush.extent([ext[0] + delta, ext[1] + delta]);
                break;
            case 'esc': {
                info.selectAll('p')
                    .remove();
                info.selectAll('span')
                    .remove();
                info.append('p')
                    .text('<CLICK ON CONTIG>');
                arrows = [];
                mini.selectAll('.arrow').remove();
                selected_id = null;
                break
            }
        }
        display();
    }

    function keyPressAnswer() {
        var key = d3.event.keyCode;
        var charCode = d3.event.which || d3.event.keyCode;
        var charStr = String.fromCharCode(charCode);
        if (d3.event.shiftKey) deltaCoeff = 5;
        else deltaCoeff = 1;
        if (charStr == '-' || charStr == '_') // -
            keyPress('zoom_out', deltaCoeff);
        else if ((charStr == '+' || charStr == '=') && ext[1] - ext[0] > 0) // +
            keyPress('zoom_in', deltaCoeff);
    }

    function keyDownAnswer() {
        var key = d3.event.keyCode;
        if (d3.event.shiftKey) deltaCoeff = 5;
        else deltaCoeff = 1;
        if (key == 39) // >
            keyPress('right', deltaCoeff);
        else if (key == 37) // <
            keyPress('left', deltaCoeff);
        else if (key == 27)
            keyPress('esc');
    }

    function sync() {
        var minExtent = Math.max(brush_cov.extent()[0], x_cov_mini_S.domain()[0]),
                maxExtent = Math.min(brush_cov.extent()[1], x_cov_mini_S.domain()[1])
        brush.extent([minExtent, maxExtent]);
        display();
    }


    function moveCovBrush() {
        var origin = d3.mouse(this)
                , point = x_cov_mini_S.invert(origin[0])
                , halfExtent = (brush_cov.extent()[1] - brush_cov.extent()[0]) / 2
                , begin = point - halfExtent
                , end = point + halfExtent;

        brush_cov.extent([begin, end]);
        brush.extent([begin, end]);
        display();
    }


    function moveBrush() {
        var origin = d3.mouse(this)
                , point = x_mini.invert(origin[0])
                , halfExtent = (brush.extent()[1] - brush.extent()[0]) / 2
                , begin = point - halfExtent
                , end = point + halfExtent;

        brush.extent([begin, end]);
        if (typeof coverage_data != "undefined") {
            brush_cov.extent([begin, end]);
        }
        display();
    }


    function getSize(text) {
        var tmp = document.createElement("span");
        tmp.innerHTML = text;
        tmp.style.visibility = "hidden";
        tmp.className = "itemLabel";
        tmp.style.whiteSpace = "nowrap";
        document.body.appendChild(tmp);
        size = tmp.offsetWidth;
        document.body.removeChild(tmp);
        return size;
    }


    // generates a single path for each item class in the mini display
    // ugly - but draws mini 2x faster than append lines or line generator
    // is there a better way to do a bunch of lines as a single path with d3?
    function getPaths(items) {
        var paths = {}, d, result = [];
        var curLane = 0;
        var isSimilarNow = "False";
        var numItem = 0;
        for (var c, i = 0; i < items.length; i++) {
            d = items[i];
            if (d.lane != curLane) numItem = 0;
            c = (d.misassembled == "False" ? "correct" : "misassembled");
            c += (d.similar == "True" ? " similar" : "");
            if (d.similar != isSimilarNow) numItem = 0;
            c += (numItem % 2 == 0 ? " odd" : "");

            items[i].class = c;

            if (!paths[c]) paths[c] = '';

            var y = y_mini(d.lane);
            y += .5 * miniLanesHeight;
            if (d.class.search("misassembled") != -1)
                y += .08 * miniLanesHeight;
            if (d.class.search("odd") != -1)
                y += .04 * miniLanesHeight;

            paths[c] += ['M', x_mini(d.corr_start), (y), 'H', x_mini(d.corr_end)].join(' ');
            isSimilarNow = d.similar;
            curLane = d.lane;
            numItem++;
        }

        for (var className in paths) {
            result.push({class: className, path: paths[className]});
        }

        return result;
    }


    function getTextSize(text, size) {

        return text.length * size;
    }


    function glow() {
        d3.select(this)
                .transition()
                .style({'opacity': .5})
                .select('rect');
    }


    function disglow() {
        d3.select(this)
                .transition()
                .style({'opacity': 1})
                .select('rect');
    }


    function getVisibleText(fullText, l, lenChromosome) {
        var t = '';
        if (getSize(fullText) > l) {
            t = fullText.slice(0, fullText.length - 1);
            while (getSize(t) > l && t.length > 3) {
                t = fullText.slice(0, t.length - 1);
            }
        }
        else t = fullText;
        if (lenChromosome && t.length == fullText.length) {
            var t_plus_len = fullText + ' (' + lenChromosome + ' bp)';
            if (getSize(t_plus_len) <= l) return t_plus_len;
        }
        return (t.length < 3 ? '' : t + (t.length >= fullText.length ? '' : '...'));
    }


    function changeInfo(d) {
        info.selectAll('p')
                .remove();

        info.selectAll('span')
                .remove();

        info.append('p')
                .style({'display': 'block', 'word-break': 'break-all', 'word-wrap': 'break-word'})
                .text('Name: ' + d.name, 280);

        info.append('p')
                .text('Type: ' + (d.misassembled == "True" ? 'misassembled' : 'correct'));

        var appendPositionElement = function(data, start, end, whereAppend, start_in_contig, end_in_contig, is_expanded) {
            var posVal = function (d) {
                if (mainTickValue == 'Gbp')
                    return d3.round(d / 1000000000, 2);
                else if (mainTickValue == 'Mbp')
                    return d3.round(d / 1000000, 2);
                else if (mainTickValue == 'kbp')
                    return d3.round(d / 1000, 2);
                else
                    return d;
            };
            var format = function (d) {
                d = d.toString();
                for (var i = 3; i < d.length; i += 4 )
                    d = d.slice(0 , d.length - i) + ' ' + d.slice(length - i, d.length);
                return d;
            };

            var e = data.filter(function (d) {
                if (d.type == 'A') {
                    if (start_in_contig && d.start_in_contig == start_in_contig && d.end_in_contig == end_in_contig) return d;
                    else if (!start_in_contig && d.corr_start <= start && end <= d.corr_end) return d;
                }
            })[0];
            var ndash = String.fromCharCode(8211);
            if (is_expanded)
                var whereAppendBlock = whereAppend.append('p')
                        .attr('class', 'head_plus collapsed')
                        .on('click', openClose);
            else var whereAppendBlock = whereAppend;
            var d = whereAppendBlock.append('span')
                    .attr('class', is_expanded ? 'head' : 'head main')
                    .on('click', openClose)
                    .text(['Position:', posVal(e.start), ndash, posVal(e.end), mainTickValue, ' '].join(' '));
            if (chrContigs.indexOf(e.chr) == -1) {
                d.append('a')
                        .attr('href', '_' + (links_to_chromosomes ? links_to_chromosomes[e.chr] : e.chr) + '.html')
                        .attr('target', '_blank')
                        .text('(' + e.chr + ')');
            }
            else if (chrContigs.length > 1) {
                d.append('span')
                        .text('(' + e.chr + ')');
            }
            d = d.append('p')
                    .attr('class', is_expanded ? 'close' : 'open');
            d.append('p')
                    .text(['reference:',
                        format(e.start), ndash, format(e.end),
                        '(' + format(Math.abs(e.end - e.start) + 1) + ')', 'bp'].join(' '));
            d.append('p')
                    .text(['contig:',
                        format(e.start_in_contig), ndash,  format(e.end_in_contig),
                        '(' + format(Math.abs(e.end_in_contig - e.start_in_contig) + 1) + ')', 'bp'].join(' '));
            d.append('p')
                    .text(['IDY:', e.IDY, '%'].join(' '));
        };
        appendPositionElement(d.structure, d.corr_start, d.corr_end, info);

        showArrows(d);
        if (d.misassembled == "True") {
            var blocks = info.append('p')
                    .attr('class', 'head main')
                    .text('Blocks: ' + d.structure.filter(function(d) { if (d.type == "A") return d;}).length);


            for (var i = 0; i < d.structure.length; ++i) {
                var e = d.structure[i];
                if (e.type == "A") {
                    appendPositionElement(d.structure, e.corr_start, e.corr_end, blocks, e.start_in_contig, e.end_in_contig, true);
                } else {
                    blocks.append('p')
                            .text(e.mstype);
                }
            }
        }
    }


    function showArrows(d) {
        var shift = 4.03;
        arrows = [];
        mini.selectAll('.arrow').remove();
        var y = y_mini(d.lane) - 1;

        if (d.misassembled == "True") {
            for (var i = 0; i < d.structure.length; ++i) {
                var e = d.structure[i];
                if (e.type == "A") {
                    if (!(e.corr_start <= d.corr_start && d.corr_end <= e.corr_end) && chrContigs.indexOf(e.chr) != -1) {
                        arrows.push({start: e.corr_start, end: e.corr_end, lane: d.lane, selected: false});
                        mini.append('g')
                                .attr('transform', 'translate(' + (x_mini((e.corr_end + e.corr_start) / 2) - shift) + ',' + y + ')')
                                .attr('class', 'arrow')
                                .append('path')
                                .attr('d', arrow)
                    }
                }
            }
        }

        arrows.push({start: d.corr_start, end: d.corr_end, lane: d.lane, selected: true});
        mini.append('g')
                .attr('transform', 'translate(' + (x_mini((d.corr_end + d.corr_start) / 2) - 4.03) + ',' + y + ')')
                .attr('class', 'arrow selected')
                .append('path')
                .attr('d', arrow);
        display();
    }

    function openClose() {
        var c = d3.select(this);
        if (c.attr('class') == 'head_plus expanded' || c.attr('class') == 'head_plus collapsed' ){
            c.attr('class', c.attr('class') == 'head_plus expanded' ? 'head_plus collapsed' : 'head_plus expanded');
            p = c.select('span').select('p');
            p.attr('class', p.attr('class') == 'open' ? 'close' : 'open');
        }
        d3.event.stopPropagation();
    }