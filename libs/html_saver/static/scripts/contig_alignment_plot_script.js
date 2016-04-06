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

    INTERLACE_BLOCKS_VERT_OFFSET = true;
    INTERLACE_BLOCKS_COLOR = true;
    BLOCKS_SHADOW = false;

    function parseData (data) {
        chart = { assemblies: {} };

        for (var assembly in data) {
            var alignments = data[assembly];
            if (!chart.assemblies[assembly])
                chart.assemblies[assembly] = [];
            for (var numAlign in alignments)
                chart.assemblies[assembly].push(alignments[numAlign]);
        }

        return collapseLanes(chart);
    }

    function isOverlapping (item, lane) {
        if (lane)
            for (var i = 0; i < lane.length; i++)
                if (item.corr_start <= lane[i].corr_end && lane[i].corr_start <= item.corr_end)
                    return true;

        return false;
    }


    function collapseLanes (chart) {
        var lanes = [], items = [], laneId = 0, itemId = 0, groupId = 0;

        for (var assemblyName in chart.assemblies) {
            var lane = chart.assemblies[assemblyName];
            var currentLen = 0;
            var numItems = 0;
            for (var i = 0; i < lane.length; i++) {
                var item = lane[i];
                if (item.mis_ends) var misassembled_ends = item.mis_ends.split(';');
                item.supp = '';
                item.lane = laneId;
                item.id = itemId;
                item.groupId = groupId;
                item.assembly = assemblyName;
                if (isContigSizePlot) {
                    item.corr_start = currentLen;
                    currentLen += item.size;
                    item.corr_end = currentLen;
                }
                items.push(item);
                itemId++;
                numItems++;
                if (item.mis_ends && misassembled_ends) {
                    for (var num = 0; num < misassembled_ends.length; num++) {
                        if (!misassembled_ends[num]) continue;
                        var suppItem = {};
                        suppItem.name = item.name;
                        suppItem.corr_start = item.corr_start;
                        suppItem.corr_end = item.corr_end;
                        suppItem.assembly = item.assembly;
                        suppItem.id = itemId;
                        suppItem.lane = laneId;
                        suppItem.groupId = groupId;
                        suppItem.supp = misassembled_ends[num];
                        suppItem.misassemblies = item.misassemblies.split(';')[num];
                        items.push(suppItem);
                        itemId++;
                        numItems++;
                    }
                }
                groupId++;
            }

            if (numItems > 0){
                lanes.push({
                id: laneId,
                label: assemblyName
                });
                laneId++;
            }
        }

        for (var laneNum = 0; laneNum < lanes.length; laneNum++) {
            if (lanes[laneNum].label) {
                assemblyName = lanes[laneNum].label;
                var description = assemblyName + '\n';
                description += 'length: ' + assemblies_len[assemblyName] + '\n';
                description += 'contigs: ' + assemblies_contigs[assemblyName] + '\n';
                if (!isContigSizePlot)
                    description += 'misassemblies: ' + assemblies_misassemblies[assemblyName];
                else
                    description += 'N50: ' + assemblies_n50[assemblyName];
                lanes[laneNum].description = description;
                if (!isContigSizePlot)
                    lanes[laneNum].link = assemblies_links[assemblyName];
            }
        }

        return {lanes: lanes, items: items};
    };

    var ContigData = function(chromosome) {
        // return parseData(generateRandomWorkItems());
        return parseData(contig_data[chromosome]);
    };

    /**
     * Allow library to be used within both the browser and node.js
     */
    var root = typeof exports !== "undefined" && exports !== null ? exports : window;
    root.contigData = ContigData;

    var isContigSizePlot = !CHROMOSOME;
    if (CHROMOSOME) var data = contigData(CHROMOSOME);
    else var data = parseData(contig_data);
    var lanes = data.lanes, items = data.items;

    var w = 0.9 * (window.innerWidth || document.documentElement.clientWidth || document.body.clientWidth) - 300;
    var margin = {
                top: 20, right: 15, bottom: 15, left: /*Math.max(d3.max(lanes, function (d) {
                 return getTextSize(d.label);
                 }), 120)*/ 145
            },
            mainLanesHeight = 45,
            miniLanesHeight = 18,
            annotationMiniLanesHeight = 18,
            featureMiniHeight = 10,
            annotationLanesHeight = 30,
            featureHeight = 20,
            annotationLanesInterval = 10,
            offsetsY = [0, .3, .15],
            offsetsMiniY = [0, .1, .05],
            lanesInterval = 15,
            miniScale = 50,
            mainScale = 50,
            paleContigsOpacity = .25,
            width = w,
            chartWidth = w,
            miniHeight = lanes.length * miniLanesHeight,
            mainHeight = lanes.length * (mainLanesHeight + lanesInterval),
            coverageHeight = typeof coverage_data != 'undefined' ? 125 : 0;
            coverageSpace = typeof coverage_data != 'undefined' ? 50 : 0;

    var contigsColors = {'N50': '#7437BA', 'N75': '#7437BA', 'NG50': '#B53778', 'NG75': '#B53778'};

    // legend items
    var legendItemWidth = 50;
    var legendItemHeight = 30;
    var legendItemXSpace = 5;
    var legendItemYSpace = 20;
    var legendItemOddOffset = 5;
    var legendTextOffsetX = legendItemWidth * 2 + legendItemXSpace;

    var total_len = 0;
    if (CHROMOSOME) {
      for (var chr in chromosomes_len) {
          total_len += chromosomes_len[chr];
      };
    }
    else total_len = contigs_total_len;
    var x_mini = d3.scale.linear()
            .domain([0, total_len])
            .range([0, chartWidth]);
    var x_main = d3.scale.linear()
            .range([0, chartWidth]);

    var ext = d3.extent(lanes, function (d) {
        return d.id;
    });
    var minBrushExtent = 10;
    var y_main = d3.scale.linear().domain([ext[0], ext[1] + 1]).range([0, mainHeight]);
    var y_mini = d3.scale.linear().domain([ext[0], ext[1] + 1]).range([0, miniHeight]);
    var hideBtnAnnotationsMini, hideBtnAnnotationsMain;

    var letterSize = getSize('w') - 1;
    var numberSize = getSize('0') - 1;

    var annotationsHeight = 0, annotationsMiniHeight = 0;
    if (CHROMOSOME) {
      var featuresData = parseFeaturesData(CHROMOSOME);
      annotationsHeight = annotationLanesHeight * featuresData.lanes.length;
      annotationsMiniHeight = annotationMiniLanesHeight * featuresData.lanes.length;
      var ext = d3.extent(featuresData.lanes, function (d) {
          return d.id;
      });
      var y_anno_mini = d3.scale.linear().domain([ext[0], ext[1] + 1]).range([0, annotationsMiniHeight]);
      var y_anno = d3.scale.linear().domain([ext[0], ext[1] + 1]).range([0, annotationsHeight]);
    }

    var featuresHidden = false, drawCoverage = false, coverageMainHidden = true;
    if (!featuresData || featuresData.features.length == 0)
      featuresHidden = true;
    if (typeof coverage_data != "undefined")
        drawCoverage = true;
    var featuresMainHidden = featuresHidden || lanes.length > 3;
    var brush, brush_cov, brush_anno;

    var spaceAfterMain = 0;
    var spaceAfterTrack = 40;
    var annotationsMainOffsetY = mainHeight + mainScale + (featuresHidden ? 0 : spaceAfterMain);
    var covMainOffsetY = typeof coverage_data != 'undefined' ? (annotationsMainOffsetY +
                            (featuresHidden ? spaceAfterMain : spaceAfterTrack)) : annotationsMainOffsetY;
    if (!featuresMainHidden)
        covMainOffsetY += annotationsHeight;
    var miniOffsetY = covMainOffsetY + spaceAfterTrack;
    var annotationsMiniOffsetY = miniOffsetY + miniHeight + (featuresHidden ? 0 : spaceAfterTrack);
    var covMiniOffsetY = annotationsMiniOffsetY + annotationsMiniHeight + spaceAfterTrack;

    var baseChartHeight = covMiniOffsetY + coverageHeight * 2 + annotationsHeight + margin.top + margin.bottom + 100;
    var curChartHeight = baseChartHeight;

    var chart = d3.select('body').append('div').attr('id', 'chart')
            .append('svg:svg')
            .attr('width', width + margin.right + margin.left)
            .attr('height', curChartHeight)
            .attr('class', 'chart');

    chart.append('defs').append('clipPath')
            .attr('id', 'clip')
            .append('rect')
            .attr('width', width)
            .attr('height', mainHeight);

    var filter = chart.append('defs')
            .append('filter').attr('id', 'shadow');
    filter.append('feOffset').attr('result', 'offOut').attr('in', 'SourceAlpha').attr('dx', '2');
    filter.append('feGaussianBlur').attr('result', 'blurOut').attr('in', 'offOut').attr('stdDeviation', '2');
    filter.append('feBlend').attr('in', 'SourceGraphic').attr('in2', 'blurOut').attr('mode', 'normal');

    var main = chart.append('g')
            .attr('transform', 'translate(' + margin.left + ',' + margin.top + ')')
            .attr('width', chartWidth)
            .attr('height', mainHeight + mainScale)
            .attr('class', 'main');

    var mainOffsetY = 120;

    var hideBtnAnnotationsMiniOffsetY = annotationsMiniOffsetY + mainOffsetY;
    var hideBtnAnnotationsMainOffsetY = annotationsMainOffsetY + mainOffsetY;
    var hideBtnCoverageMiniOffsetY = covMiniOffsetY + mainOffsetY;
    var hideBtnCoverageMainOffsetY = covMainOffsetY + mainOffsetY;

    //annotations track
    if (!featuresHidden) {
        var annotationsMain = chart.append('g')
            .attr('transform', 'translate(' + margin.left + ',' + annotationsMainOffsetY + ')')
            .attr('width', chartWidth)
            .attr('height', annotationLanesHeight)
            .attr('class', 'main')
            .attr('id', 'annotationsMain');
        if (featuresMainHidden)
            annotationsMain.attr('display', 'none')
    }

    var mini = chart.append('g')
            .attr('transform', 'translate(' + margin.left + ',' + miniOffsetY + ')')
            .attr('width', chartWidth)
            .attr('height', miniHeight + miniScale)
            .attr('class', 'main');
    if (!featuresHidden) {
        var annotationsMini = chart.append('g')
            .attr('transform', 'translate(' + margin.left + ',' + annotationsMiniOffsetY + ')')
            .attr('width', chartWidth)
            .attr('height', annotationMiniLanesHeight)
            .attr('class', 'main')
            .attr('id', 'annotationsMini');
    }

    // draw the lanes for the main chart
    main.append('g').selectAll('.laneLines')
            .data(lanes)
            //.enter().append('line')
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
                return y_main(d.id + .1);
            })
            .attr('dy', '.5ex')
            .attr('text-anchor', 'end')
            .attr('class', 'laneText')
            .text(function(d) { return d.description; })
            .call(wrap, 110, true, !isContigSizePlot, -10, /\n/);

    function addTooltipTspan(displayedText, tspan, width) {
        var maxLetters = Math.ceil(width / letterSize);
        if (maxLetters < displayedText.length) {
            var fullName = displayedText;
            tspan.on('mouseover',function(d) {
                addTooltip(d, '<span class="lane_tooltip">' + fullName + '</span>');
            });
            tspan.on('mouseout',function(d) {
                removeTooltip();
            });
            displayedText = fullName.substring(0, maxLetters) + '...';
        }
        return displayedText
    }

    function wrap(text, width, cutText, addStdoutLink, offsetX, separator) {
      text.each(function() {
          var text = d3.select(this),
              words = text.text().split(separator).reverse(),
              word,
              line = [],
              lineNumber = 0,
              lineHeight = 1.1,
              y = text.attr('y'),
              dy = parseFloat(text.attr('dy')),
              tspan = text.text(null).append('tspan').attr('x', addStdoutLink ? -40 : offsetX)
                                    .attr('y', y).attr('dy', dy + 'em')
                                    .style('font-weight', 'bold');
          var firstLine = true;
          while (word = words.pop()) {
            line.push(word);
            var displayedText = line.join(' ');
            tspan.text(displayedText);
            if ((tspan.node().getComputedTextLength() > width || firstLine) && line.length > 1) {
                line.pop();
                displayedText = line.join(' ');
                displayedText = (cutText && firstLine) ? addTooltipTspan(line[0], tspan, width) : displayedText;
                tspan.text(displayedText);
                line = [word];
                if (firstLine && addStdoutLink) {
                    linkAdded = true;
                    tspan = text.append('tspan')
                            .attr('x', offsetX)
                            .attr('y', y)
                            .attr('dy', lineNumber * lineHeight + dy + 'em')
                            .attr('text-decoration', 'underline')
                            .attr('fill', '#0000EE')
                            .style("cursor", "pointer")
                            .text('(text)')
                            .on('click',function(d) {
                                window.open(d.link, '_blank');
                                d3.event.stopPropagation();
                            });
                }
                firstLine = false;
                if (word.search("\\+") != -1) {
                    tspan = text.append('tspan')
                                .attr('x', offsetX)
                                .attr('y', y)
                                .attr('dy', ++lineNumber * lineHeight + dy + 'em')
                                .text(word);
                    var msWords = word.split('+');
                    var misassemblies = msWords[0];
                    var extMisassemblies = misassemblies.split(' ')[1];
                    var localMisassemblies = msWords[1];
                    var msTooltip = extMisassemblies + ' extensive + ' + localMisassemblies + ' local misassemblies';
                    tspan.on('mouseover',function(d) {
                        addTooltip(d, '<span class="lane_tooltip">' + msTooltip + '</span>');
                    });
                    tspan.on('mouseout',function(d) {
                        removeTooltip();
                    });
                }
                else {
                    tspan = text.append('tspan')
                                .attr('x', offsetX)
                                .attr('y', y)
                                .attr('dy', ++lineNumber * lineHeight + dy + 'em')
                                .text(word);
                }
            }
            else if (cutText && firstLine) {
                displayedText = (cutText && firstLine) ? addTooltipTspan(line[0], tspan, width) : displayedText;
                tspan.text(displayedText);
            }
          }
      });
    }

    // draw the lanes for the mini chart
    mini.append('g').selectAll('.laneLines')
            .data(lanes)
            //.enter().append('line')
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
            .attr('x', -10)
            .attr('y', function (d) {
                return y_mini(d.id + .5);
            })
            .attr('dy', '.5ex')
            .attr('text-anchor', 'end')
            .attr('class', 'laneText')
            .text(function(d) { return d.label; })
            .call(wrap, 140, true, false, -10, /\n/);

    // draw the lanes for the annotations chart
    if (!featuresHidden) {
        var featurePaths = getFeaturePaths(featuresData.features);
        addFeatureTrackInfo(annotationsMini, y_anno_mini);
        addFeatureTrackInfo(annotationsMain, y_anno);
    }

    var mini_cov, main_cov, x_cov_mini_S, y_cov_main_S, y_cov_main_A, y_cov_vals;
    if (drawCoverage)
        setupCoverage();
    // draw the x axis
    var xMainAxis, xMiniAxis, scaleTextMain, scaleTextFeatures;
    setupXAxis();

    var current = (x_mini.domain()[1] + x_mini.domain()[0]) / 2;

    // draw a line representing today's date
    main.append('line')
            .attr('y1', 0)
            .attr('y2', mainHeight)
            .attr('class', 'main curSegment')
            .attr('clip-path', 'url(#clip)');

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
                if (e.length > 0 && itemToSelect) {
                    e = itemToSelect[0].pop();
                    e.__onclick();
                }
    });

    // draw the items
    var itemRects = main.append('g')
            .attr('clip-path', 'url(#clip)');
    var itemNonRects = main.append('g')
            .attr('clip-path', 'url(#clip)');

    var miniPaths = getPaths(items);
    mini.append('g').selectAll('miniItems')
            .data(miniPaths)
            .enter().append('path')
            .attr('class', function (d) {
              if (d.text) return '';
              if (!d.supp) return 'miniItem ' + d.class;
              else return 'mainItem end ' + d.class;
            })
            .attr('d', function (d) {
              return d.path;
            })
            .attr('stroke-width', 10)
            .attr('stroke', function (d) {
              if (d.text) return addGradient(d, d.text, true);
            });

    var div = d3.select('body').append('div')
                .attr('class', 'feature_tip')
                .style('opacity', 0);
    if (!featuresHidden) addFeatureTrackItems(annotationsMini, x_mini);

    addSelectionAreas();

    d3.select('body').on("keypress", keyPressAnswer);
    d3.select('body').on("keydown", keyDownAnswer);

    // draw contig info menu
    var menu = d3.select('body').append('div')
            .attr('id', 'menu');
    menu.append('div')
            .attr('class', ' block title')
            .text('Contig info');
    info = menu.append('div')
            .attr('class', 'block');
    addClickContigText(info);

    // draw legend
    appendLegend();

    var selected_id;
    var prev = undefined;

    var arrows = [];
    var markerWidth = 3,
        markerHeight = 3;
    var markerCircleR = 2,
        markerCircleD = 4;

    chart.append("svg:defs").selectAll("marker")
        .data(["arrow", "arrow_selected"])
        .enter().append("svg:marker")
        .attr("id", function (d) {
            return 'start_' + d })
        .attr("refX", markerCircleR)
        .attr("refY", markerCircleR)
        .attr("markerWidth", markerCircleD)
        .attr("markerHeight", markerCircleD)
        .append("circle")
        .attr("cx", markerCircleR)
        .attr("cy", markerCircleR)
        .attr("r", markerCircleR);
    d3.select('#start_arrow').select('circle').style('fill', '#909090');

    chart.append("svg:defs").selectAll("marker")
        .data(["arrow", "arrow_selected"])
        .enter().append("svg:marker")
        .attr("id", String)
        .attr("viewBox", "0 -5 10 10")
        .attr("refX", 0)
        .attr("refY", 0)
        .attr("markerWidth", markerWidth)
        .attr("markerHeight", markerHeight)
        .attr("orient", "auto")
        .append("svg:path")
        .attr("d", "M0,-5L10,0L0,5");
    d3.select('#arrow').select('path').style('fill', '#777777');

    var separatedLines = [];
    var currentLen = 0;
    if (!isContigSizePlot) {
        if (chrContigs.length > 1) {
            for (var i = 0; i < chrContigs.length; i++) {
                chrName = chrContigs[i];
                chrLen = chromosomes_len[chrName];
                separatedLines.push({name: chrName, corr_start: currentLen, corr_end: currentLen + chrLen,
                               y1: 0, y2: mainHeight, len: chrLen});
                currentLen += chrLen;
            }
        }
    }
    else {
        for (var line = 0; line < contigLines.length; line++) {
            for (var lane = 0; lane < lanes.length; lane++)
                if (lanes[lane].label == contigLines[line].assembly)
                    contigLines[line].lane = lanes[lane].id;
        }
        separatedLines = contigLines;
        for (var i = 0; i < items.length; i++) addGradient(items[i], items[i].marks, false);
        mini.append('g').selectAll('miniItems')
            .data(separatedLines)
            .enter().append('text')
            .attr('class', 'miniItems text')
            .text(function (d) {
                return d.label;
            })
            .style('fill', 'white')
            .attr('transform', function (d) {
                var x = Math.max(x_mini(d.corr_end) - x_mini(d.size) + 1, (x_mini(d.corr_end) - x_mini(d.size) / 2) - getSize(d.label) / 2);
                var y = y_mini(d.lane) + miniLanesHeight - 4;
                return 'translate(' + x + ', ' + y + ')';
            });
    }

    var itemLabels = main.append('g');
    var itemLines = main.append('g')
            .attr('clip-path', 'url(#clip)');
    if (!featuresHidden)
      var featurePath = annotationsMain.append('g')
        .attr('clip-path', 'url(#clip)');

    var rectItems = [];
    var nonRectItems = [];

    if (isContigSizePlot) {
        var drag = d3.behavior.drag()
            .on('dragstart', function () {
                d3.event.sourceEvent.stopPropagation();
            })
             .on('drag', function() {
                d3.event.sourceEvent.stopPropagation();
                if (d3.event.x < 5 || d3.event.x > chartWidth) return;
                lineCountContigs.attr('transform', 'translate(' + d3.event.x + ',0)');
                getNumberOfContigs(d3.event.x);
            });
        var startPos = 400;

        var lineCountContigs = itemLines.append('g')
                .attr('id', 'countLine')
                .attr('transform', function (d) {
                    return 'translate(' + startPos + ', 0)';
                })
                .attr('width', function (d) {
                    return 5;
                })
                .call(drag);
        lineCountContigs.append('rect')
                .attr('width', function (d) {
                    return 5;
                })
                .attr('height', function (d) {
                    return mainHeight;
                })
                .attr('fill', '#300000');
    }

    display();

    setupInterface();

    function display() {
        x_main = d3.scale.linear()
            .range([0, chartWidth]);
        var rects
                , minExtent = Math.max(brush.extent()[0], x_mini.domain()[0])
                , maxExtent = Math.min(brush.extent()[1], x_mini.domain()[1])
                , visibleText = function (d) {
                    if (!d.name && !d.label) return;
                    var drawLimit = letterSize * 3;
                    if (d.label) {
                       visibleLength = (x_main(d.corr_end) - x_main(minExtent))  + (x_main(maxExtent) - x_main(d.corr_end));
                       if (visibleLength > drawLimit)
                           return getVisibleText(d.label, visibleLength);
                    }
                    var visibleLength = x_main(Math.min(maxExtent, d.corr_end)) - x_main(Math.max(minExtent, d.corr_start)) - 20;
                    if (visibleLength > drawLimit)
                        return getVisibleText(d.name, visibleLength, d.len);
                },
                visibleArrows = arrows.filter(function (d) {
                    if (d.corr_start < maxExtent && d.corr_end > minExtent) return d;
                }),
                visibleLines = separatedLines.filter(function (d) {
                    if (d.corr_end < maxExtent) return d;
                }),
                visibleLinesLabels = separatedLines.filter(function (d) {
                    if (d.name && d.corr_start < maxExtent && d.corr_end > minExtent) return d;
                    if (d.label) {
                        var textSize = d.label.length * letterSize / 2;
                        if (d.label && d.corr_end - textSize > minExtent && d.corr_end + textSize < maxExtent) return d;
                    }
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
        if (drawCoverage)
            mini_cov.select('.brush').call(brush_cov.extent([minExtent, maxExtent]));
        if (!featuresHidden)
            annotationsMini.select('.brush').call(brush_anno.extent([minExtent, maxExtent]));

        x_main.domain([minExtent, maxExtent]);
        document.getElementById('input_coords').value = Math.round(minExtent) + "-" + Math.round(maxExtent);

        // shift the today line
        main.select('.main.curLine')
                .attr('x1', x_main(current) + .5)
                .attr('x2', x_main(current) + .5);

        mainAxisUpdate();

        //upd arrows
        var shift = 4.03;

        //lines between reference contigs
        main.selectAll('.main_lines').remove();
        var lineContigs = itemLines.selectAll('.g')
                .data(visibleLines, function (d) {
                    return d.id;
                });

        var lines = lineContigs.enter().append('g')
                .attr('class', 'main_lines')
                .attr('transform', function (d) {
                    var x = x_main(d.corr_end);
                    var y = d.assembly ? y_main(d.lane) : 0;

                    return 'translate(' + x + ', ' + y + ')';
                });
        lines.append('rect')
                .attr('width', function (d) {
                    return 1;
                })
                .attr('height', function (d) {
                    return d.assembly ? mainLanesHeight + lanesInterval : d.y2;
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

        //update features
        if (!featuresMainHidden) drawFeaturesMain(minExtent, maxExtent);
        removeTooltip();

        // update the item rects
        rectItems = [];
        nonRectItems = [];
        for (var item = 0; item < visItems.length; item++) {
          if (visItems[item].supp ) {
            var w = x_main(visItems[item].corr_end) - x_main(visItems[item].corr_start);
            var triangle_width = Math.sqrt(0.5) * mainLanesHeight / 2;
            if (w > triangle_width * 1.5) nonRectItems.push(visItems[item]);
        }
          else rectItems.push(visItems[item]);
        }
        rects = itemRects.selectAll('g')
                .data(rectItems, function (d) {
                    return d.id;
                })
                .attr('transform', function (d) {
                    var x = x_main(Math.max(minExtent, d.corr_start));
                    var y = y_main(d.lane) + .25 * lanesInterval;
                    if (INTERLACE_BLOCKS_VERT_OFFSET) y += offsetsY[d.order % 3] * lanesInterval;

                    return 'translate(' + x + ', ' + y + ')';
                })
                .attr('width', function (d) {
                    return x_main(Math.min(maxExtent, d.corr_end)) - x_main(Math.max(minExtent, d.corr_start));
                });
                //.classed('light_color', function (d) {
                //    return x_main(d.corr_end) - x_main(d.corr_start) > mainLanesHeight;
                //});

        rects.select('.R')
                .attr('transform',  function (d) {
                    if (d.groupId == selected_id) {
                        return 'translate(1,1)';
                    }
                })
                .attr('width', function (d) {
                    var w = x_main(Math.min(maxExtent, d.corr_end)) - x_main(Math.max(minExtent, d.corr_start));
                    return (d.groupId == selected_id ? Math.max(w - 2, .5) : w);
                })
                .attr('height', function (d) {
                    return (d.groupId == selected_id ? mainLanesHeight - (d.supp ? 4 : 2) : mainLanesHeight);
                })
                .attr('stroke', 'black')
                .attr('stroke-width', function (d) {
                    return (d.groupId == selected_id ? 2 : 0);
                })
                .attr('stroke-opacity', function (d) {
                    return (d.groupId == selected_id ? 1 : 0);
                })
                .attr('opacity', function (d) {
                  if (!d || !d.size) return 1;
                  return d.size > minContigSize ? 1 : paleContigsOpacity;
                });
        rects.exit().remove();

        var other = rects.enter().append('g')
                .attr('class', function (d) {
                    if (!d.marks) return 'mainItem ' + d.class;
                })// Define the gradient
                .attr('fill', function (d) {
                    if (d.marks) return addGradient(d, d.marks, false, true);
                })
                .attr('transform', function (d) {
                    var x = x_main(Math.max(minExtent, d.corr_start));
                    var y = y_main(d.lane) + .25 * lanesInterval;
                    if (INTERLACE_BLOCKS_VERT_OFFSET) y += offsetsY[d.order % 3] * lanesInterval;
                    return 'translate(' + x + ', ' + y + ')';
                })
                .attr('width', function (d) {
                    return x_main(Math.min(maxExtent, d.corr_end)) - x_main(Math.max(minExtent, d.corr_start));
                });
                //.classed('light_color', function (d) {
                //    return x_main(d.corr_end) - x_main(d.corr_start) > mainLanesHeight;
                //});

        if (BLOCKS_SHADOW) other.attr('filter', 'url(#shadow)');

        other.append('rect')
                .attr('class', 'R')
                .attr('transform',  function (d) {
                    if (d.groupId == selected_id) {
                        return 'translate(1,1)';
                    }
                })
                .attr('width', function (d) {
                    var w = x_main(Math.min(maxExtent, d.corr_end)) - x_main(Math.max(minExtent, d.corr_start));
                    return (d.groupId == selected_id ? Math.max(w - 2, .5) : w);
                })
                .attr('height', function (d) {
                    return (d.groupId == selected_id ? mainLanesHeight - 4 : mainLanesHeight);
                })
                .attr('stroke', 'black')
                .attr('stroke-width', function (d) {
                    return (d.groupId == selected_id ? 2 : 0);
                })
                .attr('stroke-opacity', function (d) {
                    return (d.groupId == selected_id ? 1 : 0);
                })
                .attr('opacity', function (d) {
                  if (!d || !d.size) return 1;
                  return d.size > minContigSize ? 1 : paleContigsOpacity;
                });

        var nonRects = itemNonRects.selectAll('g')
                .data(nonRectItems, function (d) {
                    return d.id;
                })
                .attr('transform', function (d) {
                    var x = d.supp == "L" ? x_main(d.corr_start) : x_main(d.corr_end);
                    var y = y_main(d.lane) + .25 * lanesInterval;
                    if (INTERLACE_BLOCKS_VERT_OFFSET) y += offsetsY[d.order % 3] * lanesInterval;

                    return 'translate(' + x + ', ' + y + ')';
                });

        function make_triangle(d) {
            var startX = 0;
            var startY = d.groupId == selected_id ? 2 : 0;
            if (d.supp == "L")
                path = ['M', startX, startY, 'L', startX + (0.5 * (mainLanesHeight - startY) / 2),
                    (startY + (mainLanesHeight - startY)) / 2, 'L', startX, mainLanesHeight - startY, 'L',  startX, startY].join(' ');
            if (d.supp == "R")
                path = ['M', startX, startY, 'L', startX - (0.5 * (mainLanesHeight - startY) / 2),
                    (startY + (mainLanesHeight - startY)) / 2, 'L', startX, mainLanesHeight - startY, 'L',  startX, startY].join(' ');
            return path;
        }

        nonRects.selectAll('path')
                .attr('transform',  function (d) {
                    if (d.groupId == selected_id) {
                        if (d.supp == "L") return 'translate(2,0)';
                        else return 'translate(-2,0)';
                    }
                })
                .attr('d', make_triangle);

        nonRects.exit().remove();
        itemNonRects.selectAll('text')
                .remove();

        var otherNonRects = nonRects.enter().append('g')
                .attr('class', function (d) {
                    return 'mainItem end ' + d.class;
                })
                .attr('transform', function (d) {
                    var x = d.supp == "L" ? x_main(d.corr_start) : x_main(d.corr_end);
                    var y = y_main(d.lane) + .25 * lanesInterval;
                    if (INTERLACE_BLOCKS_VERT_OFFSET) y += offsetsY[d.order % 3] * lanesInterval;

                    return 'translate(' + x + ', ' + y + ')';                })
                .attr('pointer-events', 'none');

        otherNonRects.append('path')
                .attr('class', 'R')
                .attr('transform',  function (d) {
                    if (d.groupId == selected_id) {
                        if (d.supp == "L") return 'translate(2,2)';
                        else return 'translate(0,2)';
                    }
                })
                .attr('d', make_triangle);

        other.on('click', function A(d, i) {
            selected_id = d.groupId;
            changeInfo(d);
        })
                .on('mouseenter', glow)
                .on('mouseleave', disglow);
        var prevX = 0;
        var prevLane = -1;
        var visTexts = rectItems.filter(function (d) {
            var textStart = x_main(Math.max(minExtent, d.corr_start));
            if (textStart - prevX > 20 || d.lane != prevLane) {
                var visWidth = x_main(Math.min(maxExtent, d.corr_end)) - textStart;
                if (visWidth > 20) {
                    textLen = d.name.length * letterSize;
                    prevX = textStart + Math.min(textLen, visWidth) - 30;
                    prevLane = d.lane;
                    return d;
                }
            }
        });
        itemNonRects.selectAll('text')
            .data(visTexts, function (d) {
                return d.id;
            })
            .attr('class', 'itemLabel')
            .enter().append('text')
            .attr('x', function(d) {
               return x_main(Math.max(minExtent, d.corr_start)) + 5;
            })
            .attr('y', function(d) {
                var y = y_main(d.lane) + .25 * lanesInterval;
                if (INTERLACE_BLOCKS_VERT_OFFSET) y += offsetsY[d.order % 3] * lanesInterval;
                return y + 20;
            })
            .text(function(d) {
                if (!d.size || d.size > minContigSize) return visibleText(d);
            });
        //only for contig size plot
        mini.selectAll('path')
            .attr('opacity', function (d) {
              if (!d || !d.size) return 1;
              return d.size > minContigSize ? 1 : paleContigsOpacity;
            });
        if (isContigSizePlot)
            getNumberOfContigs(d3.transform(d3.select('#countLine').attr("transform")).translate[0]);

        // upd coverage
        if (drawCoverage && !coverageMainHidden) {
            main_cov.select('.covered').remove();
            main_cov.select('.notCovered').remove();

            var line = '',
            l = (maxExtent - minExtent) / 100,
            cov_main_dots_amount = Math.min(500, l),
            step = Math.round(l / cov_main_dots_amount);

            var y_max = 1;
            var cov_lines = [];
            for (var s, i = (minExtent / 100);; i += step) {
                coverage = coverage_data[CHROMOSOME].slice(i, i + step);
                if (coverage.length == 0) break;
                s = d3.sum(coverage, function (d) {
                            return d
                        }) / coverage.length;
                y_max = Math.max(y_max, s);
                if (s >= 1)
                    cov_lines.push([x_main(i * 100), s, x_main((i + step) * 100)]);
                if (i + step > (maxExtent / 100)) break;
            }

            y_max = getNextMaxCovValue(y_max);
            y_cov_main_S.domain([y_max, .1]);
            y_cov_main_A.scale(y_cov_main_S);
            for (i = 0; i < cov_lines.length; i++) {
                cov_line = cov_lines[i];
                line += ['M', cov_line[0], y_cov_main_S(cov_line[1]), 'H', cov_line[2]].join(' ');
            }

            y_cov_main_A.tickValues(y_cov_main_S.ticks().filter(y_cov_vals));
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
        main.selectAll('.main_labels').remove();

        var visibleLabels = itemLabels.selectAll('.g')
                            .data(visibleLinesLabels, function (d) {
                                return d.id;
                            });

        var labels = visibleLabels.enter().append('g')
                        .attr('class', 'main_labels')
                        .attr('transform', function (d) {
                            var x = d.label ? x_main(d.corr_end) - d.label.length * letterSize :
                                                   x_main(Math.max(minExtent, d.corr_start)) + 5 ;
                            var y = d.y2 ? d.y2 - 3 : y_main(d.lane) + 5;

                            return 'translate(' + x + ', ' + y + ')';
                        });
        labels.append('rect')
                .attr('class', 'main_labels')
                .attr('height', 15)
                .attr('transform', 'translate(0, -12)');
        labels.append('text')
                .text(visibleText)
                .attr('text-anchor', 'start')
                .attr('class', 'itemLabel');
    }

    function addSelectionAreas() {
        brush = drawBrush(mini, miniHeight);
        if (!featuresHidden)
            brush_anno = drawBrush(annotationsMini, annotationsMiniHeight, 'features');
        if (drawCoverage)
            brush_cov = drawBrush(mini_cov, coverageHeight, 'coverage');
    }
    function keyPress (cmd, deltaCoeff) {
        var ext = brush.extent();
        var delta = .01 * (ext[1] - ext[0]);
        if (deltaCoeff) delta *= deltaCoeff;
        delta = Math.max(1, delta);
        switch (cmd) {
            case 'zoom_in':
                if (ext[1] - ext[0] - 2 * delta > minBrushExtent)
                    brush.extent([ext[0] + delta, ext[1] - delta]);
                break;
            case 'zoom_out':
                brush.extent([ext[0] - delta, ext[1] + delta]);
                break;
            case 'left':
                if (ext[0] > 0) brush.extent([ext[0] - delta, ext[1] - delta]);
                break;
            case 'right':
                if (ext[1] < x_mini.domain()[1]) brush.extent([ext[0] + delta, ext[1] + delta]);
                break;
            case 'esc': {
                info.selectAll('p')
                    .remove();
                info.selectAll('span')
                    .remove();
                addClickContigText(info);
                setBaseChartHeight();
                arrows = [];
                mini.selectAll('.arrow').remove();
                mini.selectAll('.arrow_selected').remove();
                removeTooltip();
                selected_id = null;
                break
            }
        }
        itemNonRects.select('.glow').remove();
        display();
    }

    function setBaseChartHeight() {
        curChartHeight = baseChartHeight;
        chart.attr('height', curChartHeight);
    }

    function addClickContigText(info) {
        p = info.append('p');
        p.text('<click on a contig to get details>');
        p.attr('class', 'click_a_contig_text');
    }

    function addTooltip(feature, tooltipText) {
        if (!tooltipText)
            tooltipText = feature ? '<strong>' + (feature.name ? feature.name + ',' : '') + '</strong> <span>' +
                          (feature.id ? ' ID=' + feature.id + ',' : '') + ' coordinates: ' + feature.start + '-' + feature.end + '</span>' : '';
        if (div.html() != tooltipText) {
            div.transition()
                .duration(200)
                .style('opacity', 1);
            div.html(tooltipText)
                .style('left', (d3.event.pageX - 50) + 'px')
                .style('top', (d3.event.pageY + 5) + 'px');
        }
        else removeTooltip();
    }

    function removeTooltip() {
        div = d3.select('body').select('.feature_tip');
        div.style('opacity', 0);
        div.html('');
    }

    function setupInterface() {
        document.getElementById('left').onclick=function() {
            keyPress('left', 1) };
        document.getElementById('left_shift').onclick=function() {
            keyPress('left', 5) };
        document.getElementById('right').onclick=function() {
            keyPress('right', 1) };
        document.getElementById('right_shift').onclick=function() {
            keyPress('right', 5) };
        document.getElementById('zoom_in').onclick=function() {
            keyPress('zoom_in', 25) };
        document.getElementById('zoom_in_5').onclick=function() {
            keyPress('zoom_in', 40) };
        document.getElementById('zoom_out').onclick=function() {
            keyPress('zoom_out', 50) };
        document.getElementById('zoom_out_5').onclick=function() {
            keyPress('zoom_out', 200) };

        document.getElementById('input_coords').onkeydown=function() {
            enterCoords(this) };
        if (document.getElementById('input_contig_threshold')) {
            document.getElementById('input_contig_threshold').value = minContigSize;
            document.getElementById('input_contig_threshold').onkeyup = function() {
                setContigSizeThreshold(this) };
        }

        var checkboxes = document.getElementsByName('misassemblies_select');
        for(var i = 0; i < checkboxes.length; i++) {
            checkboxes[i].addEventListener('change', function(){
                showMisassemblies();
            });
        }
        if (!featuresHidden) addAnnotationsTrackButtons();
        if (drawCoverage) addCovTrackButtons();
    }

    function addCovTrackButtons() {
        var hideBtnCoverageMini = document.getElementById('hideBtnCovMini');
        setTrackBtnPos(hideBtnCoverageMini, hideBtnCoverageMiniOffsetY, 'cov', 'mini', true);
        var hideBtnCoverageMain = document.getElementById('hideBtnCovMain');
        setTrackBtnPos(hideBtnCoverageMain, hideBtnCoverageMainOffsetY, 'cov', 'main', false);
    }

    function addAnnotationsTrackButtons() {
        hideBtnAnnotationsMini = document.getElementById('hideBtnAnnoMini');
        setTrackBtnPos(hideBtnAnnotationsMini, hideBtnAnnotationsMiniOffsetY, 'features', 'mini', true);
        hideBtnAnnotationsMain = document.getElementById('hideBtnAnnoMain');
        if (!featuresMainHidden)
            hideBtnAnnotationsMain.innerHTML = "Hide";
        setTrackBtnPos(hideBtnAnnotationsMain, hideBtnAnnotationsMainOffsetY + 6, 'features', 'main', !featuresMainHidden);
    }

    function setTrackBtnPos(hideBtn, offsetY, track, pane, doHide) {
        var hideBtnExpandWidth = 130;
        hideBtn.style.display = "";
        hideBtn.style.left = margin.left - hideBtnExpandWidth;
        hideBtn.style.top = offsetY;
        hideBtn.onclick = function() {
            hideTrack(track, pane, doHide);
        };
    }

    function keyPressAnswer() {
        if (d3.event.target.className == 'textBox') return;
        var charCode = d3.event.which || d3.event.keyCode;
        var charStr = String.fromCharCode(charCode);
        if (d3.event.shiftKey) deltaCoeff = 5;
        else deltaCoeff = 1;
        var ext = brush.extent();
        if (charStr == '-' || charStr == '_') // -
            keyPress('zoom_out', deltaCoeff);
        else if (charStr == '+' || charStr == '=') // +
            keyPress('zoom_in', deltaCoeff);
    }

    function keyDownAnswer() {
        var key = d3.event.keyCode;
        if (d3.event.target.className == 'textBox') return;
        if (d3.event.shiftKey) deltaCoeff = 5;
        else deltaCoeff = 1;
        var ext = brush.extent();
        if (key == 39 && x_mini.domain()[1] - ext[0] > minBrushExtent) // >
            keyPress('right', deltaCoeff);
        else if (key == 37 && ext[1] > minBrushExtent) // <
            keyPress('left', deltaCoeff);
        else if (key == 27)
            keyPress('esc');
    }

    function enterCoords(textBox) {
        var key = this.event.keyCode;
        if (key == 27) {
            document.getElementById('input_coords').blur();
        }
        if (key == 13) {
            var coordText = textBox.value;
            var coords = coordText.split('-');
            setCoords(coords);
        }
    }

    var timerAnimationSetCoords;

    function setCoords(coords, animation) {
        var ext = brush.extent();
        var startCoord = ext[0], endCoord = ext[1];
        if (coords.length >= 2 && parseInt(coords[0]) <= parseInt(coords[1])) {
            startCoord = parseInt(coords[0]);
            endCoord = Math.max(parseInt(coords[1]), startCoord + 5);
        }
        else if (coords.length == 1 && parseInt(coords[0])) {
            startCoord = parseInt(coords[0]);
            var brushSize = ext[1] - ext[0];
            endCoord = startCoord + brushSize;
        }
        startCoord = Math.max(0, startCoord);
        endCoord = Math.min(endCoord, x_mini.domain()[1]);
        startCoord = Math.min(startCoord, endCoord - minBrushExtent);
        clearInterval(timerAnimationSetCoords);
        if (animation) {
            var distance = Math.abs(startCoord - ext[0]);
            var distRange = distance / (ext[1] - ext[0]);
            if (distRange > 50) {
                distRange = distRange * 0.05;
                var zoomDelta = (distRange - 1) * .5 * 100;
                brush.extent([ext[0] - zoomDelta, ext[1] + zoomDelta]);
            }
            var delta = Math.max(5, 0.05 * distance);
            ext = brush.extent();
            var numSteps = Math.max(1, parseInt(distance / delta));
            if (ext[0] > startCoord) delta = -delta;
            delta = (startCoord - ext[0]) / numSteps;
            timerAnimationSetCoords = setInterval(function() {
                ext = [ext[0] + delta, ext[1] + delta];
                if ((delta > 0 && ext[0] >= startCoord) || (delta < 0 && ext[0] <= startCoord)) {
                    clearInterval(timerAnimationSetCoords);
                    brush.extent([startCoord, endCoord]);
                    display();
                    return;
                }
                brush.extent(ext);
                display();
            }, 5)
        }
        else {
            brush.extent([startCoord, endCoord]);
            display();
        }
    }

    function setContigSizeThreshold(textBox) {
        var key = this.event.keyCode;
        if (key == 27) {
            document.getElementById('input_contig_threshold').blur();
        }
        else {
            if (parseInt(textBox.value)) minContigSize = parseInt(textBox.value);
            else if (key == 13) minContigSize = 0;
            display();
        }
    }

    function getNumberOfContigs(x) {
        lineCountContigs.selectAll('g')
                .remove();
        for (var item = 0; item < rectItems.length; item++) {
            if (x_main(rectItems[item].corr_start) <= x && x <= x_main(rectItems[item].corr_end)) {
                d = rectItems[item];
                order = (d.order + 1).toString();
                offsetX = order.length * letterSize + 50;
                offsetY = y_main(d.lane) + mainLanesHeight / 2;
                var suffix = 'th';
                var lastNumber = order.slice(-1);
                if (lastNumber == '1' && order != "11") suffix = 'st';
                else if (lastNumber == '2' && order != "12") suffix = 'nd';
                else if (lastNumber == '3' && order != "13") suffix = 'rd';
                var container = lineCountContigs.append('g')
                        .attr('transform', function (d) {
                            return 'translate(' + (-offsetX) + ', ' + offsetY + ')';
                        })
                        .attr('width', function (d) {
                        });
                container.append('rect')
                        .attr('height', 15)
                        .attr('width', offsetX)
                        .attr('fill', '#fff')
                        .attr('transform', 'translate(-3, -12)');
                container.append('text')
                        .text(order + suffix + ' contig')
                        .attr('text-anchor', 'start')
                        .attr('class', 'itemLabel');
            }
        }
    }

    function sync(syncBrush, track) {
        var minExtent = Math.max(syncBrush.extent()[0], x_mini.domain()[0]),
                maxExtent = Math.min(syncBrush.extent()[1], x_mini.domain()[1]);
        if (minExtent + minBrushExtent >= x_mini.domain()[1]) minExtent = maxExtent - minBrushExtent;
        if (maxExtent - minExtent < minBrushExtent) maxExtent = minExtent + minBrushExtent;
        brush.extent([minExtent, maxExtent]);
        if (brush_cov && track != 'coverage') brush_cov.extent([minExtent, maxExtent]);
        if (brush_anno && track != 'features') brush_anno.extent([minExtent, maxExtent]);
        display();
    }

    function moveBrush() {
        var origin = d3.mouse(this)
                , point = x_mini.invert(origin[0])
                , halfExtent = (brush.extent()[1] - brush.extent()[0]) / 2
                , begin = point - halfExtent
                , end = point + halfExtent;

        brush.extent([begin, end]);
        if (drawCoverage)
            brush_cov.extent([begin, end]);
        if (!featuresHidden)
            brush_anno.extent([begin, end]);

        display();
    }

    function addGradient(d, marks, miniItem, gradientExists) {
      if (!marks) return;
      var gradientId = 'gradient' + d.id + (miniItem ? 'mini' : '');
      marks = marks.split(', ');
      if (marks.length == 1) return contigsColors[marks[0]];
      if (gradientExists) return 'url(#' + gradientId + ')';
      var gradient = chart.append("svg:defs")
          .append("svg:linearGradient")
          .attr("id", gradientId);
      if (miniItem) {
          gradient.attr("x1", d.x)
                  .attr("y1", d.y)
                  .attr("x2", d.x)
                  .attr("y2", d.y + 1)
                  .attr("gradientUnits", "userSpaceOnUse");
      }
      else {
          gradient.attr("x1", "0%")
                  .attr("y1", "0%")
                  .attr("x2", "0%")
                  .attr("y2", "100%");
      }
      gradientSteps = ["50%", "50%"];

      for (var m = 0; m < marks.length; m++)
        gradient.append("svg:stop")
          .attr("offset", gradientSteps[m])
          .attr("stop-color", contigsColors[marks[m]])
          .attr("stop-opacity", 1);

      return 'url(#' + gradientId + ')';
    }


    function setupXAxis() {
        var mainTickValue;
        xMainAxis = d3.svg.axis()
                .scale(x_main)
                .orient('bottom')
                .tickSize(6, 0, 0);
        addMainXAxis(main, mainHeight);
        var miniTickValue = getTickValue(x_mini.domain()[1]);

        xMiniAxis = appendXAxis(mini, x_mini, miniHeight, miniTickValue);

        mini.append('g')
            .attr('transform', 'translate(0,' + miniHeight + ')')
            .attr('class', 'axis')
            .call(xMiniAxis);

        if (!featuresHidden) {
            addMiniXAxis(annotationsMini, x_mini, annotationsMiniHeight, miniTickValue);
            addMainXAxis(annotationsMain, annotationsHeight);
        }
        if (drawCoverage) {
            addMiniXAxis(mini_cov, x_mini, coverageHeight, miniTickValue);
            addMainXAxis(main_cov, coverageHeight);
        }
    }

    function addMiniXAxis(track, scale, height, tickValue) {
        var axis = appendXAxis(track, scale, height, tickValue);
        track.append('g')
            .attr('transform', 'translate(0,' + height + ')')
            .attr('class', 'axis')
            .call(axis);
    }

    function addMainXAxis(track, trackHeight) {
        track.append('g')
                .attr('transform', 'translate(0,' + trackHeight + ')')
                .attr('class', 'main axis')
                .call(xMainAxis);
    }

    function getTickValue(value) {
        if (value > 1000000000)
          return 'Gbp';
        else if (value > 1000000)
          return 'Mbp';
        else if (value > 1000)
          return 'kbp';
        else
          return 'bp';
    }

    function formatValue(d, tickValue) {
          d = Math.round(d);
          if (tickValue == 'Gbp')
              return d3.round(d / 1000000000, 2);
          else if (tickValue == 'Mbp')
              return d3.round(d / 1000000, 2);
          else if (tickValue == 'kbp')
              return d3.round(d / 1000, 2);
          else
              return d;
      }

    function appendXAxis(lane, scale, laneHeight, tickValue) {
      var ticksValues = scale.ticks(5);
      ticksValues = [scale.domain()[0]].concat(ticksValues);
      ticksValues.push(scale.domain()[1]);

      var min_ticks_delta = Math.max(getTextSize(formatValue(ticksValues.slice(-1)[0], tickValue).toString(), numberSize),
                  getTextSize(formatValue(ticksValues.slice(-2)[0], tickValue).toString(), numberSize));
      if (scale(ticksValues.slice(-1)[0]) - scale(ticksValues.slice(-2)[0]) < min_ticks_delta) {
          ticksValues.splice(-2, 1)
      }

      var xAxis = d3.svg.axis()
            .scale(scale)
            .orient('bottom')
            .tickSize(6, 0, 0)
            .tickValues(ticksValues)
            .tickFormat(function(d) {
              return formatValue(d, tickValue);
            });
      if (!tickValue) return xAxis;

      lane.append('g')
              .attr('transform', 'translate(0,' + laneHeight + ')')
              .attr('class', 'axis')
              .call(xAxis).append('text')
              .attr('transform', 'translate(' + scale(scale.domain()[1]) + ',' + (laneHeight / 2 + 2) + ')');
      var lastTick = lane.select(".axis").selectAll("g")[0].pop();
      var lastTickValue = ticksValues.pop();
      d3.select(lastTick).select('text').text(formatValue(lastTickValue, tickValue) + ' ' + tickValue)
              .attr('transform', 'translate(-10, 0)');
      return xAxis;
    }

    function mainAxisUpdate() {
        var domain = x_main.domain()[1] - x_main.domain()[0];
        var start = x_main.domain()[0];
        mainTickValue = getTickValue(domain);

        xMainAxis.tickFormat(function(d) {
                              return formatValue(start + d * domain, mainTickValue);
                            });
        updateTrack(main);
        if (!featuresMainHidden) updateTrack(annotationsMain);
        if (!coverageMainHidden) updateTrack(main_cov);
    }

    function updateTrack(track) {
        track.select('.main.axis').call(xMainAxis);
        var lastTick = track.select(".axis").selectAll("g")[0].pop();
        var textSize = Math.max(0, (formatValue(x_main.domain()[1], mainTickValue).toString().length - 2) * numberSize);
        d3.select(lastTick).select('text').text(formatValue(x_main.domain()[1], mainTickValue) + ' ' + mainTickValue)
                  .attr('transform', 'translate(-' + textSize + ', 0)');
    }

    function drawBrush(track, height, trackName) {
        var offsetY = 7;
        track.append('rect')
                .attr('pointer-events', 'painted')
                .attr('width', chartWidth)
                .attr('height', height)
                .attr('visibility', 'hidden')
                .on('mouseup', moveBrush);

        // draw the selection area
        var delta = (x_mini.domain()[1] - x_mini.domain()[0]) / 8;

        var newBrush = d3.svg.brush()
                            .x(x_mini)
                            .extent([current - delta, current + delta])
                            .on("brush", function() {
                                sync(newBrush, trackName)
                            });

        track.append('g')
                        .attr('class', 'x brush')
                        .call(newBrush)
                        .selectAll('rect')
                        .attr('y', -offsetY)
                        .attr('height', height + offsetY);

        track.selectAll('rect.background').remove();
        return newBrush;
    }

    function getNextMaxCovValue(maxY) {
        var factor = Math.max(1, Math.ceil(Math.log(maxY) * Math.LOG10E));
        return Math.pow(10, factor) * 1.1
    }

    function setupCoverage() {
        // draw mini coverage
        x_cov_mini_S = x_mini,      // x coverage scale
        y_max = Math.max.apply(null, coverage_data[CHROMOSOME]);

        y_cov_mini_S = d3.scale.log()
                .domain([getNextMaxCovValue(y_max), .1])
                .range([0, coverageHeight]),
        y_cov_main_S = y_cov_mini_S;

        y_cov_vals = function(tickValue) {
            var i = 0;
            for (; Math.pow(10, i) < tickValue; ++i);
            if (tickValue == Math.pow(10, i) && tickValue <= y_cov_main_S.domain()[0]) return tickValue;
        };

        y_cov_mini_A = d3.svg.axis()
                .scale(y_cov_mini_S)
                .orient('left')
                .tickValues(y_cov_mini_S.ticks().filter(y_cov_vals))
                .tickFormat(function(tickValue) {
                    return tickValue;
                })
                .tickSize(2, 0);
        mini_cov = chart.append('g')
                .attr('class', 'coverage')
                .attr('transform', 'translate(' + margin.left + ', ' + covMiniOffsetY + ')');
        mini_cov.append('g')
                .attr('class', 'y')
                .call(y_cov_mini_A);

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
                .text('Coverage')
                .attr('transform', 'rotate(-90 20, 80)');

        // draw main coverage

        y_cov_main_A = y_cov_mini_A = d3.svg.axis()
                .scale(y_cov_main_S)
                .orient('left')
                .tickValues(y_cov_main_S.ticks().filter(y_cov_vals))
                .tickFormat(function(tickValue) {
                    return tickValue;
                })
                .tickSize(2, 0);

        var x_cov_main_A = xMainAxis;

        main_cov = chart.append('g')
                .attr('class', 'COV')
                .attr('transform', 'translate(' + margin.left + ', ' + covMainOffsetY + ')');

        main_cov.attr('display', 'none');
        main_cov.append('g')
                .attr('class', 'y')
                .attr('transform', 'translate(0, 0)')
                .call(y_cov_main_A);
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
        var miniPathHeight = 10;

        var paths = {}, d, result = [];
        var misassemblies = {};
        var curLane = 0;
        var isSimilarNow = "False";
        var numItem = 0;

        var countSupplementary = 0;
        for (var c, i = 0; i < items.length; i++) {
            d = items[i];
            if (d.lane != curLane) {
                numItem = 0;
                countSupplementary = 0;
            }
            var isSmall = x_mini(d.corr_end) - x_mini(d.corr_start) < miniPathHeight;

            d.misassembled = d.misassemblies ? "True" : "False";
            c = (d.misassembled == "False" ? "" : "misassembled");
            c += (d.similar == "True" ? " similar" : "");
            //c += ((!d.supp && !isSmall) ? " light_color" : "");
            if (d.supp) countSupplementary++;
            if (INTERLACE_BLOCKS_COLOR) c += ((numItem - countSupplementary) % 2 == 0 ? " odd" : "");
            var text = '';
            if (isContigSizePlot) {
                if (d.type == "small_contigs") c += " disabled";
                else c += " unknown";
            }

            if (d.marks) {  // NX for contig size plot
              var marks = d.marks;
              text = marks;
              marks = marks.split(', ');
              for (var m = 0; m < marks.length; m++)
                c += " " + marks[m].toLowerCase();
            }

            items[i].class = c;
            items[i].order = numItem - countSupplementary;

            if (!paths[c]) paths[c] = '';

            var startX = d.supp == "R" ? x_mini(d.corr_end) : x_mini(d.corr_start);
            var pathEnd = x_mini(d.corr_end);
            var startY = y_mini(d.lane);
            if (INTERLACE_BLOCKS_VERT_OFFSET) startY += offsetsMiniY[items[i].order % 3] * miniLanesHeight;
            if (!d.supp) startY += .45 * miniLanesHeight;
            if (d.supp) startY += .18 * miniLanesHeight;
            if (!d.supp || isSmall) path = ['M', startX, startY, 'H', pathEnd].join(' ');
            else if (d.supp == "L") path = ['M', startX, startY, 'L', startX + (Math.sqrt(3) * miniPathHeight / 2), startY + miniPathHeight / 2,
              'L', startX, startY + miniPathHeight, 'L',  startX, startY].join(' ');
            else if (d.supp == "R") path = ['M', startX, startY, 'L', startX - (Math.sqrt(3) * miniPathHeight / 2), startY + miniPathHeight / 2,
              'L', startX, startY + miniPathHeight, 'L',  startX, startY].join(' ');
            misassemblies[c] = d.misassemblies;
            isSimilarNow = d.similar;
            curLane = d.lane;
            numItem++;
            result.push({class: d.class, path: path, misassemblies: misassemblies[d.class], supp: d.supp,
                x: startX, y: startY, size: d.size, text: text, id: d.id});
        }
        return result;
    }


    function getTextSize(text, size) {

        return text.length * size;
    }


    function glow() {
        itemNonRects.append('rect')
                .attr('class', 'glow')
                .attr('pointer-events', 'none')
                .attr('width', d3.select(this).attr('width'))
                .attr('height', d3.select(this).select('rect').attr('height'))
                .attr('fill', 'white')
                .attr('opacity', .3)
                .attr('transform', d3.select(this).attr('transform'));
    }


    function disglow() {
        itemNonRects.select('.glow').remove();
    }


    function getVisibleText(fullText, l, lenChromosome) {
        var t = '';
        if ((fullText.length - 1) * letterSize > l) {
            t = fullText.slice(0, fullText.length - 1);
            while ((t.length - 1) * letterSize > l && t.length > 3) {
                t = fullText.slice(0, t.length - 1);
            }
        }
        else t = fullText;
        if (lenChromosome && t.length == fullText.length) {
            var t_plus_len = fullText + ' (' + lenChromosome + ' bp)';
            if ((t_plus_len.length - 2)* letterSize <= l) return t_plus_len;
        }
        return (t.length < fullText.length && t.length <= 3 ? '' : t + (t.length >= fullText.length ? '' : '...'));
    }


    function changeInfo(d) {
        info.selectAll('p')
                .remove();

        info.selectAll('span')
                .remove();
        setBaseChartHeight();
        info.append('p')
                .style({'display': 'block', 'word-break': 'break-all', 'word-wrap': 'break-word'})
                .text('Name: ' + d.name, 280);

        if (d.structure) {
            var contig_type = d.misassemblies ? 'misassembled' : 'correct';
            if (d.similar == "True" && !d.misassemblies) contig_type += ' (similar in >= 50% of the assemblies)';
            if (d.misassemblies) {
                var misassemblies = d.misassemblies.split(';');
                if (misassemblies[0] && misassemblies[1])
                    contig_type += ' (both sides';
                else if (misassemblies[0])
                    contig_type += ' (left side';
                else
                    contig_type += ' (right side';

                if (d.similar == "True") contig_type += ', similar in >= 50% of the assemblies';
                contig_type += ')'
            }
            info.append('p')
                .text('Type: ' + contig_type);
        }
        else info.append('p')
                .text('Size: ' + d.size + ' bp');

        var appendPositionElement = function(data, start, end, assembly, whereAppend, start_in_contig, end_in_contig, prev_start, is_expanded) {
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

            var e = !data ? '' : data.filter(function (d) {
                if (d.type == 'A') {
                    if (start_in_contig && d.start_in_contig == start_in_contig && d.end_in_contig == end_in_contig) return d;
                    else if (!start_in_contig && d.corr_start <= start && end <= d.corr_end) return d;
                }
            })[0];
            if (!e) return;
            var ndash = String.fromCharCode(8211);
            if (is_expanded)
                var whereAppendBlock = whereAppend.append('p')
                        .attr('class', 'head_plus collapsed')
                        .on('click', function(d, i) {
                            var eventX = d3.event.x || d3.event.clientX;
                            if (eventX < whereAppendBlock[0][0].offsetLeft + 15)
                                openClose(whereAppendBlock[0][0]);
                        });
            else var whereAppendBlock = whereAppend;
            var d = whereAppendBlock.append('span')
                    .attr('class', is_expanded ? 'head' : 'head main')
                    .append('text');
            d.append('tspan')
                .attr('x', -50)
                .text('Position: ');
            d.append('tspan')
                .style('text-decoration', 'underline')
                .style('color', '#7ED5F5')
                .style('cursor', 'pointer')
                .text([posVal(e.start), ndash, posVal(e.end), mainTickValue, ' '].join(' '))
                .on('click',function(d) {
                    var brushExtent = brush.extent();
                    var brushSize = brushExtent[1] - brushExtent[0];
                    if (prev_start && prev_start > e.corr_start) point = e.corr_end;
                    else if (prev_start) point = e.corr_start;
                    setCoords([point - brushSize / 2, point + brushSize / 2], true);
                    for (var i = 0; i < items.length; i++) {
                        if (items[i].assembly == assembly && items[i].corr_start == e.corr_start && items[i].corr_end == e.corr_end) {
                            selected_id = items[i].groupId;
                            showArrows(items[i]);
                            changeInfo(items[i]);
                            display();
                            break;
                        }
                    }
                    d3.event.stopPropagation();
                });

            if (is_expanded) {
                if (prev_start == start)
                    d.append('div')
                     .attr('id', 'circle' + start+ '_' + end)
                     .attr('class', 'block_circle selected');
                else
                    d.append('div')
                     .attr('id', 'circle' + start + '_' + end)
                     .attr('class', 'block_circle');
            }

            if (chrContigs.indexOf(e.chr) == -1) {
                d.append('a')
                        .attr('href', (links_to_chromosomes ? links_to_chromosomes[e.chr] : e.chr) + '.html')
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
        appendPositionElement(d.structure, d.corr_start, d.corr_end, d.assembly, info);

        showArrows(d);
        if (d.structure) {
            var blocks = info.append('p')
                    .attr('class', 'head main')
                    .text('Blocks: ' + d.structure.filter(function(d) { if (d.type == "A") return d;}).length);

            for (var i = 0; i < d.structure.length; ++i) {
                var e = d.structure[i];
                if (e.type == "A") {
                    appendPositionElement(d.structure, e.corr_start, e.corr_end, d.assembly, blocks, e.start_in_contig,
                        e.end_in_contig, d.corr_start, true);
                } else {
                    blocks.append('p')
                            .text(e.mstype);
                }
            }
        }
        var blockHeight = info[0][0].offsetHeight;
        curChartHeight += blockHeight;
        chart.attr('height', curChartHeight);
    }


    function showArrows(d) {
        var verticalShift = -7;
        arrows = [];
        mini.selectAll('.arrow').remove();
        mini.selectAll('.arrow_selected').remove();
        var y = y_mini(d.lane) - 1;

        if (d.structure) {
            for (var i = 0; i < d.structure.length; ++i) {
                var e = d.structure[i];
                if (e.type == "A") {
                    if (!(e.corr_start <= d.corr_start && d.corr_end <= e.corr_end) && chrContigs.indexOf(e.chr) != -1) {
                        arrows.push({start: e.corr_start, end: e.corr_end, lane: d.lane, selected: false});
                        mini.append('g')
                                .attr('transform', 'translate(' + x_mini((e.corr_end + e.corr_start) / 2) + ',' + verticalShift +')')
                                .attr('class', 'arrow')
                                .append("svg:path")
                                .attr("d", 'M0,0V' + (Math.abs(verticalShift) + 1 + d.lane * miniLanesHeight))
                                .attr("class", function (d) {
                                    return "path arrow";
                                })
                                .attr("marker-start", function (d) {
                                    return "url(#start_arrow)";
                                })
                                .attr("marker-end", function (d) {
                                    return "url(#arrow)";
                                });
                    }
                }
            }
        }

        arrows.push({start: d.corr_start, end: d.corr_end, lane: d.lane, selected: true});
        mini.append('g')
                .attr('transform', 'translate(' + x_mini((d.corr_end + d.corr_start) / 2) + ',' + verticalShift +')')
                .attr('class', 'arrow_selected')
                .append("svg:path")
                .attr("d", 'M0,0V' + (Math.abs(verticalShift) + 1 + d.lane * miniLanesHeight))
                .attr("class", function (d) {
                    return "path arrow_selected";
                })
                .attr("marker-start", function (d) {
                    return "url(#start_arrow_selected)";
                })
                .attr("marker-end", function (d) {
                    return "url(#arrow_selected)";
                });
        display();
    }

    function openClose(d) {
        var c = d3.select(d);
        if (c.attr('class') == 'head_plus expanded' || c.attr('class') == 'head_plus collapsed' ){
            c.attr('class', c.attr('class') == 'head_plus expanded' ? 'head_plus collapsed' : 'head_plus expanded');
            p = c.select('span').select('p');
            if (p.attr('class') == 'close') {
                p.attr('class', 'open');
                var blockHeight = c[0][0].offsetHeight;
                curChartHeight += blockHeight;
            }
            else {
                var blockHeight = c[0][0].offsetHeight;
                curChartHeight -= blockHeight;
                p.attr('class', 'close');
            }
            chart.attr('height', curChartHeight);
        }
        d3.event.stopPropagation();
    }

    function showMisassemblies() {
        for (var numItem = 0; numItem < items.length; numItem++) {
            if (items[numItem].misassemblies) {
                var msTypes = items[numItem].misassemblies.split(';');
                var isMisassembled = "False";
                for (var i = 0; i < msTypes.length; i++) {
                    if (msTypes[i] && document.getElementById(msTypes[i]).checked) isMisassembled = "True";
                }
                if (isMisassembled == "True" && items[numItem].misassembled == "False") {
                    items[numItem].class = items[numItem].class.replace("disabled", "misassembled");
                }
                else if (isMisassembled == "False")
                    items[numItem].class = items[numItem].class.replace(/\bmisassembled\b/g, "disabled");
                items[numItem].misassembled = isMisassembled;
            }
        }
        chart.selectAll('g')
            .classed('misassembled', function (d) {
                if (d && d.misassemblies) {
                    return d.misassembled == 'True';
                }
            })
            .classed('disabled', function (d) {
                if (d && d.misassemblies) {
                    return d.misassembled != 'True';
                }
            })
            .attr('opacity', function (d) {
                if (d && d.misassemblies) {
                  return (d.supp && d.misassembled != 'True' ? 0 : 1);
                }
            });
        chart.selectAll('path')
            .classed('misassembled', function (d) {
                if (d && d.misassemblies) {
                    var msTypes = d.misassemblies.split(';');
                    for (var i = 0; i < msTypes.length; i++) {
                        if (msTypes[i] && document.getElementById(msTypes[i]).checked) return true;
                    }
                    return false;
                }
            })
            .classed('disabled', function (d) {
                if (d && d.misassemblies) {
                    var msTypes = d.misassemblies.split(';');
                    for (var i = 0; i < msTypes.length; i++) {
                        if (msTypes[i] && document.getElementById(msTypes[i]).checked) return false;
                    }
                    return true;
                }
            });
    }

    function appendLegend() {
        var menu = d3.select('body').append('div')
                .attr('id', 'legend')
                .attr('class', 'expanded');
        var block = menu.append('div')
                .attr('class', 'block')
                .style('float', 'left');
        var header = block.append('p')
                .style('text-align', 'center')
                .style('font-size', '16px')
                .style('margin-top', '5px')
                .text('Legend');
        var legend = block.append('svg:svg')
            .attr('width', "100%")
            .attr('class', 'legend');

        var legendHeight = 0;
        if (isContigSizePlot) legendHeight = appendLegendContigSize(legend);
        else legendHeight = appendLegendAlignmentViewer(legend);
        legend.attr('height', legendHeight);

        header.on('click', function() {
            menu.attr('class', function() {
                return menu.attr('class') == 'collapsed' ? 'expanded' : 'collapsed';
            });
            legend.attr('class', function() {
                return legend.attr('class') == 'collapsed' ? 'expanded' : 'collapsed';
            })
        });
    }

    function appendLegendAlignmentViewer(legend) {
        var classes = ['', 'similar', 'misassembled light_color', 'misassembled', 'misassembled similar', 'disabled', 'annotation'];
        var classDescriptions = ['correct contigs', 'correct contigs similar among >= 50% assemblies', 'misassembled blocks ' +
        '(misassembly event on the left side, on the right side)', 'misassembled blocks (zoom in to get details about misassembly event side)',
            'misassembled blocks similar among >= 50% assemblies', 'unchecked misassembled blocks (see checkboxes)', 'genome features (e.g. genes)'];
        var prevOffsetY = 0;
        var offsetY = 0;
        for (var numClass = 0; numClass < classes.length; numClass++) {
            offsetY = addLegendItemWithText(legend, prevOffsetY, classes[numClass], classDescriptions[numClass], true);
            if (classes[numClass] == 'misassembled light_color') {
                legend.append('path')
                    .attr('transform',  function (d) {
                        return 'translate(0.5,' + prevOffsetY + ')';
                    })
                    .attr('class', function (d) {
                        return 'mainItem end misassembled';
                    })
                    .attr('d', function (d) {
                        var startX = 0;
                        var startY = 0;
                        path = ['M', startX, startY, 'L', startX + (Math.sqrt(1) * (legendItemHeight - startY) / 2),
                            (startY + (legendItemHeight - startY)) / 2, 'L', startX, legendItemHeight - startY, 'L',  startX, startY].join(' ');
                        return path;
                    });
                legend.append('path')
                    .attr('transform',  function (d) {
                        return 'translate(' + (legendItemWidth * 2 - 0.5) + ',' + (prevOffsetY + legendItemOddOffset) + ')';
                    })
                    .attr('class', function (d) {
                        return 'mainItem end misassembled odd';
                    })
                    .attr('d', function (d) {
                        var startX = 0;
                        var startY = 0;
                        path = ['M', startX, startY, 'L', startX - (Math.sqrt(1) * (legendItemHeight - startY) / 2),
                            (startY + (legendItemHeight - startY)) / 2, 'L', startX, legendItemHeight - startY, 'L',  startX, startY].join(' ');
                        return path;
                    });
            }
            prevOffsetY = offsetY;
        }
        return offsetY;
    }

    function appendLegendContigSize(legend) {
        var classes = ['unknown', '', '', ''];
        var classMarks = ['', 'N50', 'NG50', 'N50, NG50'];
        var classDescriptions = ['contigs', 'contig of length = Nx statistic (x is 50 or 75)',
        'contig of length = NGx statistic (x is 50 or 75)', 'contig of length = Nx and NGx simultaneously'];
        var offsetY = 0;
        for (var numClass = 0; numClass < classes.length; numClass++) {
            offsetY = addLegendItemWithText(legend, offsetY, classes[numClass], classDescriptions[numClass],
                numClass == 0, classMarks[numClass])
        }
        return offsetY;
    }

    function addLegendItemWithText(legend, offsetY, className, description, addOdd, marks) {
        legend.append('g')
                .attr('class', 'mainItem legend ' + className)
                .append('rect')
                .attr('width', legendItemWidth)
                .attr('height', legendItemHeight)
                .attr('x', addOdd ? 0 : legendItemWidth / 2)
                .attr('y', offsetY)
                .attr('fill', function (d) {
                    d = {id: className};
                    if (marks) return addGradient(d, marks, false, false);
                });
        if (addOdd)
            legend.append('g')
                .attr('class', 'mainItem legend ' + className + ' odd')
                .append('rect')
                .attr('width', legendItemWidth)
                .attr('height', legendItemHeight)
                .attr('x', legendItemWidth)
                .attr('y', offsetY + legendItemOddOffset);
        legend.append('text')
                .attr('x', legendTextOffsetX)
                .attr('y', offsetY + legendItemOddOffset)
                .attr('dy', '.5ex')
                .style('fill', 'white')
                .text(description)
                .call(wrap, 125, false, false, legendTextOffsetX, ' ');
        offsetY += legendItemHeight;
        offsetY += legendItemYSpace;
        offsetY += 10 * Math.max(0, Math.ceil(description.length / 13 - 3));
        return offsetY;
    }

    function parseFeaturesData(chr) {
      var lanes = [];
      var features = [];
      var data = [];
      var laneId = 0, itemId = 0;

      for (var numContainer = 0; numContainer < features_data.length; numContainer++) {
        for (var i = 0; i < features_data[numContainer].length; i++) {
          if (!oneHtml && features_data[numContainer][i].chr != references_id[chr]) continue;
          if (!data[numContainer])
              data[numContainer] = [];
          data[numContainer].push(features_data[numContainer][i]);
        }
      }

      for (var numContainer = 0; numContainer < data.length; numContainer++) {
        var lane = data[numContainer];
        var numItems = 0;
        for (var i = 0; i < lane.length; i++) {
            var item = lane[i];
            item.lane = laneId;
            item.id = itemId;
            features.push(item);
            itemId++;
            numItems++;
        }
        if (numItems > 0) {
            lanes.push({
            id: laneId,
            label: lane[0].kind });
            laneId++;
        }
      }
      return {lanes: lanes, features: features}
    }

    function addFeatureTrackItems(annotations, scale) {
        var annotationsItems = annotations.append('g').selectAll('miniItems')
            .data(featurePaths)
            .enter().append('rect')
            .attr('class', function (d) {
              return d.class;
            })
            .attr('transform', function (d) {
              return 'translate(' + d.x + ', ' + d.y + ')';
            })
            .attr('width', function (d) {
              return scale(d.end - d.start);
            })
            .attr('height', featureMiniHeight)
            .on('mouseenter', selectFeature)
            .on('mouseleave', deselectFeature)
            .on('click',  function(d) {
                addTooltip(d);
            });
        var visFeatureTexts = featurePaths.filter(function (d) {
                if (scale(d.end) - scale(d.start) > 45) return d;
        });
        annotations.append('g').selectAll('miniItems')
                            .data(visFeatureTexts)
                            .enter().append('text')
                            .style("font-size", "10px")
                            .text(function (d) { return getVisibleText(d.name ? d.name : 'ID=' + d.id, scale(d.end) - scale(d.start)) } )
                            .attr('class', 'featureLabel')
                            .attr('transform', function (d) {
                              return 'translate(' + (d.x + 3) + ', ' + (d.y + featureMiniHeight / 2 + 3) + ')';
                            });
    }

    function selectFeature() {
        d3.select(this)
                .transition()
                .style({'opacity': .5})
                .select('rect');
    }

    function deselectFeature() {
        d3.select(this)
                .transition()
                .style({'opacity': 1})
                .select('rect');
    }

    function addFeatureTrackInfo (annotations, scale) {
        annotations.append('g').selectAll('.laneLines')
            .data(featuresData.lanes)
            //.enter().append('line')
            .attr('x1', 0)
            .attr('y1', function (d) {
                return d3.round(scale(d.id)) + .5;
            })
            .attr('x2', chartWidth)
            .attr('y2', function (d) {
                return d3.round(scale(d.id)) + .5;
            })
            .attr('stroke', function (d) {
                return d.label === '' ? 'white' : 'lightgray'
            });

        annotations.append('g').selectAll('.laneText')
            .data(featuresData.lanes)
            .enter().append('text')
            .text(function (d) {
                return d.label;
            })
            .attr('x', -10)
            .attr('y', function (d) {
                return scale(d.id + .5);
            })
            .attr('dy', '.5ex')
            .attr('text-anchor', 'end')
            .attr('class', 'laneText');
    }

    function getFeaturePaths(features) {
        var d, result = [];
        var curLane = 0;
        var numItem = 0;

        for (var c, i = 0; i < features.length; i++) {
            d = features[i];
            if (d.lane != curLane) numItem = 0;
            c = "annotation ";
            if (INTERLACE_BLOCKS_COLOR) c += (numItem % 2 == 0 ? "odd" : "");

            features[i].class = c;

            var x = x_mini(d.start);
            var y = y_anno_mini(d.lane);
            y += .15 * annotationMiniLanesHeight;
            if (d.class.search("odd") != -1)
                y += .04 * annotationMiniLanesHeight;

            result.push({class: c, name: d.name, start: d.start, end: d.end, id: d.id_, y: y, x: x, lane: d.lane, order: i});
            curLane = d.lane;
            numItem++;
        }
        return result;
    }

    function drawFeaturesMain(minExtent, maxExtent) {
        var featuresItems = featurePaths.filter(function (d) {
                if (d.start < maxExtent && d.end > minExtent) {
                    var drawLimit = 0;
                    var visibleLength = x_main(Math.min(maxExtent, d.end)) - x_main(Math.max(minExtent, d.start));
                    if (visibleLength > drawLimit)
                        return d;
                }
            });
        var featureRects = featurePath.selectAll('g')
                .data(featuresItems, function (d) {
                    return d.id;
                })
                .attr('transform', function (d) {
                    var x = x_main(Math.max(minExtent, d.start));
                    var y = y_anno(d.lane) + .15 * featureHeight;
                    if (INTERLACE_BLOCKS_VERT_OFFSET) y += offsetsMiniY[d.order % 2] * featureHeight;
                    return 'translate(' + x + ', ' + y + ')';
                })
                .attr('width', function (d) {
                    return x_main(Math.min(maxExtent, d.end)) - x_main(Math.max(minExtent, d.start));
                });

        featureRects.select('.R')
                .attr('width', function (d) {
                    var w = x_main(Math.min(maxExtent, d.end)) - x_main(Math.max(minExtent, d.start));
                    return w;
                })
                .attr('height', function (d) {
                    return featureHeight;
                });
        featureRects.exit().remove();
        featurePath.selectAll('text')
                .remove();

        var otherFeatures = featureRects.enter().append('g')
                .attr('class', function (d) {
                    return d.class;
                })
                .attr('transform', function (d) {
                    var x = x_main(Math.max(minExtent, d.start));
                    var y = y_anno(d.lane) + .15 * featureHeight;
                    if (INTERLACE_BLOCKS_VERT_OFFSET) y += offsetsMiniY[d.order % 2] * featureHeight;

                    return 'translate(' + x + ', ' + y + ')';
                })
                .attr('width', function (d) {
                    return x_main(Math.min(maxExtent, d.end)) - x_main(Math.max(minExtent, d.start));
                });

        otherFeatures.append('rect')
                .attr('class', 'R')
                .attr('width', function (d) {
                    var w = x_main(Math.min(maxExtent, d.end)) - x_main(Math.max(minExtent, d.start));
                    return w;
                })
                .attr('height', function (d) {
                    return featureHeight;
                })
                .on('mouseenter', selectFeature)
                .on('mouseleave', deselectFeature)
                .on('click',  function(d) {
                    addTooltip(d);
                });
        var visFeatureTexts = featuresItems.filter(function (d) {
            if (x_main(Math.min(maxExtent, d.end)) - x_main(Math.max(minExtent, d.start)) > 45) return d;
        });
        featurePath.selectAll('text')
            .data(visFeatureTexts, function (d) {
                return d.id;
            })
            .enter().append('text')
            .attr('fill', 'white')
            .attr('class', 'featureLabel')
            .style("font-size", "10px")
            .attr('x', function(d) {
               return x_main(Math.max(minExtent, d.start)) + 2;
            })
            .attr('y', function(d) {
                var y = y_anno(d.lane) + .15 * featureHeight;
                if (INTERLACE_BLOCKS_VERT_OFFSET) y += offsetsMiniY[d.order % 2] * featureHeight;
                return y + featureHeight / 2;
            })
            .text(function(d) {
                var w = x_main(Math.min(maxExtent, d.end)) - x_main(Math.max(minExtent, d.start));
                return getVisibleText(d.name ? d.name : 'ID=' + d.id, w - 10);
            });
    }


    function hideTrack(track, pane, doHide) {
        removeTooltip();
        var hideBtnCoverageMain = document.getElementById('hideBtnCovMain');
        var hideBtnCoverageMini = document.getElementById('hideBtnCovMini');
        var paneToHide, hideBtn, textToShow, offsetY;
        if (track == 'features') {
            textToShow = 'Show annotation';
            if (pane == 'main') {
                paneToHide = annotationsMain;
                hideBtn = hideBtnAnnotationsMain;
                if (doHide) {
                    covMainOffsetY -= annotationsHeight;
                    miniOffsetY -= annotationsHeight;
                    annotationsMiniOffsetY -= annotationsHeight;
                    covMiniOffsetY -= annotationsHeight;
                    hideBtnAnnotationsMiniOffsetY -= annotationsHeight;
                    hideBtnCoverageMainOffsetY -= annotationsHeight;
                    hideBtnCoverageMiniOffsetY -= annotationsHeight;
                }
                else {
                    covMainOffsetY += annotationsHeight;
                    miniOffsetY += annotationsHeight;
                    annotationsMiniOffsetY += annotationsHeight;
                    covMiniOffsetY += annotationsHeight;
                    hideBtnAnnotationsMiniOffsetY += annotationsHeight;
                    hideBtnCoverageMainOffsetY += annotationsHeight;
                    hideBtnCoverageMiniOffsetY += annotationsHeight;
                }
                featuresMainHidden = doHide;
            }
            else {
                paneToHide = annotationsMini;
                hideBtn = hideBtnAnnotationsMini;
                if (doHide) {
                    covMiniOffsetY -= annotationsMiniHeight;
                    hideBtnCoverageMiniOffsetY -= annotationsMiniHeight;
                }
                else {
                    covMiniOffsetY += annotationsMiniHeight;
                    hideBtnCoverageMiniOffsetY += annotationsMiniHeight;
                }
            }
        }
        else {
            textToShow = 'Show read coverage';
            if (pane == 'main') {
                paneToHide = main_cov;
                hideBtn = hideBtnCoverageMain;
                if (doHide) {
                    miniOffsetY -= coverageHeight;
                    annotationsMiniOffsetY -= coverageHeight;
                    covMiniOffsetY -= coverageHeight;
                    hideBtnAnnotationsMiniOffsetY -= coverageHeight;
                    hideBtnCoverageMiniOffsetY -= coverageHeight;
                }
                else {
                    miniOffsetY += coverageHeight;
                    annotationsMiniOffsetY += coverageHeight;
                    covMiniOffsetY += coverageHeight;
                    hideBtnAnnotationsMiniOffsetY += coverageHeight;
                    hideBtnCoverageMiniOffsetY += coverageHeight;
                }
                coverageMainHidden = doHide;
            }
            else {
                paneToHide = mini_cov;
                hideBtn = hideBtnCoverageMini;
            }
        }
        if (track == 'features') {
            if (pane == 'main') {
                if (main_cov)
                    main_cov.transition()
                      .duration(200)
                      .attr('transform', function(d) {
                        return 'translate(' + margin.left + ',' + covMainOffsetY + ')'
                        });
                mini.transition()
                      .duration(200)
                      .attr('transform', function(d) {
                        return 'translate(' + margin.left + ',' + miniOffsetY + ')'
                        });
                annotationsMini.transition()
                      .duration(200)
                      .attr('transform', function(d) {
                        return 'translate(' + margin.left + ',' + annotationsMiniOffsetY + ')'
                        });
                if (main_cov)
                    hideBtnCoverageMain.style.top = hideBtnCoverageMainOffsetY;
                if (mini_cov)
                    hideBtnCoverageMini.style.top = hideBtnCoverageMiniOffsetY;
                hideBtnAnnotationsMini.style.top = hideBtnAnnotationsMiniOffsetY;
                if (mini_cov)
                    mini_cov.transition()
                            .duration(200)
                            .attr('transform', function(d) {
                                return 'translate(' + margin.left + ',' + covMiniOffsetY + ')'
                            });
            }
            else {
                if (mini_cov) {
                    hideBtnCoverageMini.style.top = hideBtnCoverageMiniOffsetY;
                    mini_cov.transition()
                            .duration(200)
                            .attr('transform', function(d) {
                                return 'translate(' + margin.left + ',' + covMiniOffsetY + ')'
                            });
                }
            }
        }
        else {
            if (pane == 'main') {
                mini.transition()
                    .duration(200)
                    .attr('transform', function(d) {
                        return 'translate(' + margin.left + ',' + miniOffsetY + ')'
                    });
                if (!featuresHidden) {
                    annotationsMini.transition()
                                   .duration(200)
                                   .attr('transform', function(d) {
                                       return 'translate(' + margin.left + ',' + annotationsMiniOffsetY + ')'
                                   });
                    hideBtnAnnotationsMini.style.top = hideBtnAnnotationsMiniOffsetY;
                }
                hideBtnCoverageMini.style.top = hideBtnCoverageMiniOffsetY;
                mini_cov.transition()
                        .duration(200)
                        .attr('transform', function(d) {
                            return 'translate(' + margin.left + ',' + covMiniOffsetY + ')'
                        });
            }
        }
        if (doHide) {
            paneToHide.attr('display', 'none');
            hideBtn.onclick = function() {
                hideTrack(track, pane, false);
            };
            hideBtn.innerHTML = textToShow;
        }
        else {
            paneToHide.transition()
                      .delay(150)
                      .attr('display', '');
            hideBtn.onclick = function() {
                hideTrack(track, pane, true);
            };
            hideBtn.innerHTML = 'Hide';
            display();
        }
    }