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

    INTERLACE_BLOCKS_VERT_OFFSET = false;
    INTERLACE_BLOCKS_COLOR = false;
    BLOCKS_SHADOW = false;

    /**
     * Allow library to be used within both the browser and node.js
     */
    var ContigData = function(chromosome) {
        return parseData(contig_data[chromosome]);
    };

    var root = typeof exports !== "undefined" && exports !== null ? exports : window;
    root.contigData = ContigData;

    var isContigSizePlot = !chromosome;
    if (chromosome) var data = contigData(chromosome);
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
            miniItemHeight = 10;
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
    var legendItemOddOffset = 10;
    var legendTextOffsetX = legendItemWidth + legendItemXSpace * 2;

    var total_len = 0;
    if (!isContigSizePlot) {
      for (var chr in chromosomes_len) {
          total_len += chromosomes_len[chr];
      }
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
    if (chromosome) {
      var featuresData = parseFeaturesData(chromosome);
      annotationsHeight = annotationLanesHeight * featuresData.lanes.length;
      annotationsMiniHeight = annotationMiniLanesHeight * featuresData.lanes.length;
      var ext = d3.extent(featuresData.lanes, function (d) {
          return d.id;
      });
      var y_anno_mini = d3.scale.linear().domain([ext[0], ext[1] + 1]).range([0, annotationsMiniHeight]);
      var y_anno = d3.scale.linear().domain([ext[0], ext[1] + 1]).range([0, annotationsHeight]);
    }

    var coverageFactor = 10, maxCovDots = chartWidth;
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

    var manyChromosomes = !isContigSizePlot && chrContigs.length > 1;
    var chrLabelsOffsetY = manyChromosomes ? 6 : 0;

    var chart = d3.select('body').append('div').attr('id', 'chart')
            .append('svg:svg')
            .attr('width', width + margin.right + margin.left)
            .attr('height', curChartHeight)
            .attr('class', 'chart');

    chart.append('defs').append('clipPath')
            .attr('id', 'clip')
            .append('rect')
            .attr('width', width)
            .attr('height', mainHeight + chrLabelsOffsetY);

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

    var laneLabelOffsetX = 80 + (isContigSizePlot ? 20 : 0);
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
            .call(wrap, laneLabelOffsetX, true, !isContigSizePlot, -10, /\n/);

    function addTooltipTspan(displayedText, tspan, width) {
        var visibleLabel = getVisibleText(displayedText, width);
        if (visibleLabel.length < displayedText.length) {
            var fullName = displayedText;
            tspan.on('mouseover',function(d) {
                addTooltip(d, '<span class="lane_tooltip">' + fullName + '</span>');
            });
            tspan.on('mouseout',function(d) {
                removeTooltip();
            });
            displayedText = visibleLabel;
        }
        return displayedText
    }

    function wrap(text, width, cutText, addStdoutLink, offsetX, separator) {
      var stdoutLinkWidth = getSize('(text)') + 10;
      text.each(function() {
          var text = d3.select(this),
              words = text.text().split(separator).reverse(),
              word,
              line = [],
              lineNumber = 0,
              lineHeight = 1.1,
              y = text.attr('y'),
              dy = parseFloat(text.attr('dy')),
              tspan = text.text(null).append('tspan').attr('x', addStdoutLink ? -stdoutLinkWidth : offsetX)
                                    .attr('y', y).attr('dy', dy + 'em')
                                    .style('font-weight', 'bold');
          var firstLine = true;
          while (word = words.pop()) {
            line.push(word);
            var displayedText = line.join(' ');
            tspan.text(displayedText);
            var doCut = firstLine && cutText;
            if ((tspan.node().getComputedTextLength() > width || doCut) && line.length > 1) {
                line.pop();
                displayedText = line.join(' ');
                displayedText = doCut ? addTooltipTspan(line[0], tspan, width) : displayedText;
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
            else if (doCut) {
                displayedText = addTooltipTspan(line[0], tspan, width);
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
            .call(wrap, 100, true, false, -10, /\n/);

    // draw the lanes for the annotations chart
    if (!featuresHidden) {
        var featurePaths = getFeaturePaths(featuresData.features);
        addFeatureTrackInfo(annotationsMini, y_anno_mini);
        addFeatureTrackInfo(annotationsMain, y_anno);
    }

    var mini_cov, main_cov, x_cov_mini_S, y_cov_main_S, y_cov_main_A, numYTicks;
    if (drawCoverage)
        setupCoverage();
    // draw the x axis
    var xMainAxis, xMiniAxis;
    setupXAxis();

    var centerPos = (x_mini.domain()[1] + x_mini.domain()[0]) / 2;

    // draw a line representing today's date
    main.append('line')
            .attr('y1', 0)
            .attr('y2', mainHeight)
            .attr('class', 'main curSegment')
            .attr('clip-path', 'url(#clip)');

    mini.append('line')
            .attr('x1', x_mini(centerPos) + .5)
            .attr('y1', 0)
            .attr('x2', x_mini(centerPos) + .5)
            .attr('y2', miniHeight)
            .attr('class', 'curSegment');

    var visItems = null;

    // draw the items
    var itemSvgOffsetY = margin.top + document.getElementById('chart').offsetTop;
    var itemsLayer = d3.select('body').append('div').attr('id', 'items')
                                    .append('svg:svg')
                                    .style('position', 'absolute')
                                    .attr('width', width)
                                    .attr('height', mainHeight)
                                    .style('top', itemSvgOffsetY)
                                    .style('left', margin.left);

    itemsLayer.append('rect')
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
                var e = itemsContainer.selectAll(".mainItem").filter(function () {
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
    var itemsContainer = itemsLayer.append('g');

    var miniItems = getMiniItems(items);
    miniRects = miniItems.filter(function (item) {
        if (isContigSizePlot && !item.fullContig) return;
        if (!item.path) return item;
    });
    miniPaths = miniItems.filter(function (item) {
        if (item.path) return item;
    });

    mini.append('g').selectAll('miniItems')
            .data(miniRects)
            .enter().append('rect')
            .attr('class', function (item) {
                if (item.text && !item.type) return 'item gradient';
                return 'item miniItem ' + item.objClass;
            })                
            .attr('fill', function (item) {
                if (item.text && !item.type) return addGradient(item, item.text, false);
            })
            .attr('transform', function (item) {
                return 'translate(' + item.start + ', ' + item.y + ')';
            })
            .attr('width', function (item) {
                itemWidth = item.end - item.start;
                return itemWidth;
            })
            .attr('height', miniItemHeight);
    mini.append('g').selectAll('miniItems')
            .data(miniPaths)
            .enter().append('path')
            .attr('class', function (item) {
              return 'mainItem end ' + item.objClass;
            })
            .attr('d', function (item) {
              return item.path;
            });

    var featureTip = d3.select('body').append('div')
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

    var separatedLines = [], breakpointLines = [];
    var currentLen = 0;
    if (!isContigSizePlot) {
        if (chrContigs.length > 1) {
            for (var i = 0; i < chrContigs.length; i++) {
                chrName = chrContigs[i];
                chrLen = chromosomes_len[chrName];
                separatedLines.push({name: chrName, corr_start: currentLen, corr_end: currentLen + chrLen,
                               y1: 0, y2: mainHeight + chrLabelsOffsetY, len: chrLen});
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
        breakpointLines = getBreakpointLines();
        for (var i = 0; i < items.length; i++) addGradient(items[i], items[i].marks, true);
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
                var y = y_mini(d.lane) + miniLanesHeight - 5;
                return 'translate(' + x + ', ' + y + ')';
            });
    }

    var linesLabelsLayer = d3.select('body').append('div').attr('id', 'lines_labels')
                                    .append('svg:svg')
                                    .style('position', 'absolute')
                                    .attr('width', width)
                                    .attr('height', mainHeight + 20)
                                    .style('top', itemSvgOffsetY - 10)
                                    .style('left', margin.left)
                                    .attr('pointer-events', 'none');
    var itemLabels = linesLabelsLayer.append('g');
    var itemLines = linesLabelsLayer.append('g')
                                    .attr('pointer-events', 'painted');
    var textLayer = itemsLayer.append('g');
    if (!featuresHidden)
      var featurePath = annotationsMain.append('g')
        .attr('clip-path', 'url(#clip)');

    var visRectsAndPaths = [];

    if (isContigSizePlot) {
        var drag = d3.behavior.drag()
            .on('dragstart', function () {
                d3.event.sourceEvent.stopPropagation();
            })
             .on('drag', function() {
                d3.event.sourceEvent.stopPropagation();
                if (d3.event.x < 10 || d3.event.x > chartWidth - 10) return;
                lineCountContigs.attr('transform', 'translate(' + d3.event.x + ',10)');
                getNumberOfContigs(d3.event.x);
            });
        var startPos = 400;

        var lineCountContigs = itemLines.append('g')
                .attr('id', 'countLine')
                .attr('transform', function (d) {
                    return 'translate(' + startPos + ', 10)';
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

    getCoordsFromURL();

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
                visibleBreakpointLines = breakpointLines.filter(function (d) {
                    if (d.pos < maxExtent) return d;
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
            });
        mini.select('.brush').call(brush.extent([minExtent, maxExtent]));
        if (drawCoverage)
            mini_cov.select('.brush').call(brush_cov.extent([minExtent, maxExtent]));
        if (!featuresHidden)
            annotationsMini.select('.brush').call(brush_anno.extent([minExtent, maxExtent]));

        x_main.domain([minExtent, maxExtent]);
        document.getElementById('input_coords').value = Math.round(minExtent) + "-" + Math.round(maxExtent);

        // shift the today line
        main.select('.main.curLine')
                .attr('x1', x_main(centerPos) + .5)
                .attr('x2', x_main(centerPos) + .5);

        mainAxisUpdate();

        //upd arrows
        var shift = 4.03;

        //lines between reference contigs
        linesLabelsLayer.selectAll('.main_lines').remove();
        var lineContigs = itemLines.selectAll('.g')
                .data(visibleLines, function (d) {
                    return d.id;
                });

        var lines = lineContigs.enter().append('g')
                .attr('class', 'main_lines')
                .attr('transform', function (d) {
                    var x = x_main(d.corr_end);
                    var y = d.assembly ? y_main(d.lane) + 10 : 10;

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

        //misassemblies breakpoints lines
        linesLabelsLayer.selectAll('.dashed_lines').remove();

        lines = itemLines.selectAll('.g')
                .data(visibleBreakpointLines, function (d) {
                    return d.id;
                })
                .enter().append('g')
                .attr('class', 'dashed_lines')
                .attr('transform', function (d) {
                    return 'translate(' + x_main(d.pos) + ', ' + d.y + ')';
                });
        lines.append('path')
                .attr('d', 'M0,0V' + mainLanesHeight)
                .attr('fill', '#300000');

        //update features
        removeTooltip();
        if (!featuresMainHidden) drawFeaturesMain(minExtent, maxExtent);

        // update the item rects
        visRectsAndPaths = [];
        for (var item = 0; item < visItems.length; item++) {
            visRectsAndPaths.push(visItems[item]);
            if (visItems[item].triangles)
                for (var i = 0; i < visItems[item].triangles.length; i++)
                {
                    var triangle = visItems[item].triangles[i];
                    var w = x_main(triangle.corr_end) - x_main(triangle.corr_start);
                    var triangle_width = Math.sqrt(0.5) * mainLanesHeight / 2;
                    if (w > triangle_width * 1.5) visRectsAndPaths.push(triangle);
                }
        }
        var oldItems = itemsContainer.selectAll('.item')
                .data(visRectsAndPaths, function (item) {
                    return item.id;
                })
                .attr('transform', function (item) {
                    return getTranslate(item);
                })
                .attr('width', function (item) {
                    return getItemWidth(item);
                })
                .attr('stroke-width', function (item) {
                    return getItemStrokeWidth(item);
                })
                .attr('fill-opacity', function (item) {
                    return getItemOpacity(item);
                });
        oldItems.exit().remove();

        var newItems = oldItems.enter().append('g').each(function(itemData) {
            var container = d3.select(this);
            var itemFigure = itemData.misassembledEnds ? container.append('path') : container.append('rect');
            itemFigure.attr('class', function (item) {
                            if (item.misassembledEnds) {
                                if (!item.objClass) item.objClass = 'misassembled';
                                return 'item end ' + item.objClass;
                            }
                            if (!item.marks || item.type)
                                return 'item mainItem ' + item.objClass;
                            else return 'item';
                        })// Define the gradient
                        .attr('fill', function (item) {
                            if (item.marks && !item.type)
                                return addGradient(item, item.marks, true);
                        })
                        .attr('transform', function (item) {
                            return getTranslate(item);
                        })
                        .attr('width', function (item) {
                            return getItemWidth(item);
                        })
                        .attr('height', mainLanesHeight)
                        .attr('stroke', 'black')
                        .attr('stroke-width', function (item) {
                            return getItemStrokeWidth(item);
                        })
                        .attr('fill-opacity', function (item) {
                            return getItemOpacity(item);
                        })
                        .attr('pointer-events', function (item) {
                            return (item.misassembledEnds || item.notActive) ? 'none' : 'painted';
                        })
                        .attr('d', function(item) {
                            if (item.misassembledEnds) return make_triangle(item);
                        });
        });

        function getItemWidth(item) {
            var w = x_main(Math.min(maxExtent, item.corr_end)) - x_main(Math.max(minExtent, item.corr_start));
            return w;
        }

        function getItemStrokeWidth(item) {
            if (item.misassembledEnds) return 0;
            if (item.notActive) return 0;
            return (item.groupId == selected_id ? 2 : .4);
        }

        function getItemOpacity(item) {
            var defOpacity = 0.65;
            if (isContigSizePlot && (!item.type || item.type == 'unaligned')) defOpacity = 1;
            if (item.misassembledEnds) return 1;
            if (item.fullContig && item.type && item.type != 'unaligned') return 0.05;
            if (!item || !item.size) return defOpacity;
            return item.size > minContigSize ? defOpacity : paleContigsOpacity;
        }

        function getTranslate(item) {
            if (item.misassembledEnds) {
                var x = item.misassembledEnds == "L" ? x_main(item.corr_start) : x_main(item.corr_end);
                var y = y_main(item.lane) + .25 * lanesInterval;
                if (INTERLACE_BLOCKS_VERT_OFFSET) y += offsetsY[item.order % 3] * lanesInterval;
                if (item.groupId == selected_id) {
                    if (item.misassembledEnds == "L") x += 1;
                    else x += -1;
                }
                return 'translate(' + x + ', ' + y + ')';
            }
            var x = x_main(Math.max(minExtent, item.corr_start));
            var y = y_main(item.lane) + .25 * lanesInterval;
            if (INTERLACE_BLOCKS_VERT_OFFSET) y += offsetsY[item.order % 3] * lanesInterval;
            return 'translate(' + x + ', ' + y + ')';
        }

        if (BLOCKS_SHADOW) other.attr('filter', 'url(#shadow)');

        function make_triangle(item) {
            var startX = 0;
            var startY = item.groupId == selected_id ? 2 : 0;
            if (item.misassembledEnds == "L")
                path = ['M', startX, startY, 'L', startX + (0.5 * (mainLanesHeight - startY) / 2),
                    (startY + (mainLanesHeight - startY)) / 2, 'L', startX, mainLanesHeight - startY, 'L',  startX, startY].join(' ');
            if (item.misassembledEnds == "R")
                path = ['M', startX, startY, 'L', startX - (0.5 * (mainLanesHeight - startY) / 2),
                    (startY + (mainLanesHeight - startY)) / 2, 'L', startX, mainLanesHeight - startY, 'L',  startX, startY].join(' ');
            return path;
        }

        newItems.on('click', function (item) {
                        selected_id = item.groupId;
                        changeInfo(item);
                    })
                .on('mouseenter', glow)
                .on('mouseleave', disglow);
        var prevX = 0;
        var prevLane = -1;
        var visTexts = visRectsAndPaths.filter(function (d) {
            if (!d.name) return;
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
        var texts = textLayer.selectAll('text')
                    .data(visTexts, function (d) {
                        return d.id;
                    })
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
        texts.exit().remove();

        var newTexts = texts.enter().append('text')
                            .attr('class', 'itemLabel')
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
        if (isContigSizePlot)
            getNumberOfContigs(d3.transform(d3.select('#countLine').attr("transform")).translate[0]);

        // upd coverage
        if (drawCoverage && !coverageMainHidden) updateMainCoverage(minExtent, maxExtent, coverageFactor);

        linesLabelsLayer.selectAll('.main_labels').remove();

        var visibleLabels = itemLabels.selectAll('.g')
                            .data(visibleLinesLabels, function (d) {
                                return d.id;
                            });

        var labels = visibleLabels.enter().append('g')
                        .attr('class', 'main_labels')
                        .attr('transform', function (d) {
                            var x = d.label ? x_main(d.corr_end) - d.label.length * letterSize :
                                                   x_main(Math.max(minExtent, d.corr_start)) + 5 ;
                            var y = d.y2 ? d.y2 + 6 : y_main(d.lane) + 13;

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


    function parseData (data) {
        chart = { assemblies: {} };

        for (var assembly in data) {
            var alignments = data[assembly];
            if (!chart.assemblies[assembly])
                chart.assemblies[assembly] = [];
            for (var numAlign = 0; numAlign < alignments.length; numAlign++)
                chart.assemblies[assembly].push(alignments[numAlign]);
        }

        return collapseLanes(chart);
    }

    function getBreakpointLines() {
        var lines = [];
        contigStart = true;
        prev_pos = 0;
        for (var i = 0; i < items.length; i++) {
        	item = items[i];
            if (item.notActive) {
            	y = y_main(item.lane) + .25 * lanesInterval + 10;
            	if (!contigStart) {
            		if (Math.abs(prev_pos - item.corr_start) > 2) {
		            	lines.push({pos:item.corr_start, y: y});
            		}
            	}
            	else contigStart = false;
            	prev_pos = item.corr_end;
            	lines.push({pos:item.corr_end, y: y});
            }
            else {
            	contigStart = true;
            	if (item.type != 'unaligned') lines.pop();
            }
        }
        return lines;
    }

    function isOverlapping (item, lane) {
        if (lane)
            for (var i = 0; i < lane.length; i++)
                if (item.corr_start <= lane[i].corr_end && lane[i].corr_start <= item.corr_end)
                    return true;

        return false;
    }

    function addAssemblyDescription (lanes) {
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
        return lanes;
    }

    function collapseLanes (chart) {
        var lanes = [], items = [], laneId = 0, itemId = 0, groupId = 0;

        function parseItem(item, fullInfo) {
            item.misassembledEnds = '';
            item.lane = laneId;
            item.id = itemId;
            item.groupId = groupId;
            item.assembly = assemblyName;
            if (isContigSizePlot) {
                if (!fullInfo) {
                    item.corr_start = currentLen;
                    currentLen += item.size;
                    item.corr_end = currentLen;
                    item.fullContig = true;
                }
                else {
                    item.start_in_ref = item.corr_start;
                    item.end_in_ref = item.corr_end;
            	    start_in_contig = Math.min(item.start_in_contig, item.end_in_contig);
            	    end_in_contig = Math.max(item.start_in_contig, item.end_in_contig);
                    item.corr_start = currentLen + start_in_contig - 1;
                    item.corr_end = currentLen + end_in_contig - 1;
                    item.notActive = true;
                    item.type = fullInfo.type;
                }
            }
            item.triangles = Array();
            itemId++;
            numItems++;
            if (item.mis_ends && misassembled_ends) {
                for (var num = 0; num < misassembled_ends.length; num++) {
                    if (!misassembled_ends[num]) continue;
                    var triangleItem = {};
                    triangleItem.name = item.name;
                    triangleItem.corr_start = item.corr_start;
                    triangleItem.corr_end = item.corr_end;
                    triangleItem.assembly = item.assembly;
                    triangleItem.id = itemId;
                    triangleItem.lane = laneId;
                    triangleItem.groupId = groupId;
                    triangleItem.misassembledEnds = misassembled_ends[num];
                    triangleItem.misassemblies = item.misassemblies.split(';')[num];
                    item.triangles.push(triangleItem);
                    itemId++;
                    numItems++;
                }
            }
            return item
        }

        for (var assemblyName in chart.assemblies) {
            var lane = chart.assemblies[assemblyName];
            var currentLen = 0;
            var numItems = 0;
            for (var i = 0; i < lane.length; i++) {
                var item = lane[i];
                if (item.mis_ends) var misassembled_ends = item.mis_ends.split(';');
                if (isContigSizePlot) {
                    var blocks = item.structure;
                    for (var k = 0; k < blocks.length; k++) {
                        if (blocks[k].type != 'M')
                            items.push(parseItem(blocks[k], item));
                    }
                }
                items.push(parseItem(item));
                groupId++;
            }

            if (numItems > 0) {
                lanes.push({
                    id: laneId,
                    label: assemblyName
                });
                laneId++;
            }
        }

        addAssemblyDescription(lanes);
        return {lanes: lanes, items: items};
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
        itemsContainer.select('.glow').remove();
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
        if (tooltipText && featureTip.html() != tooltipText) {
            featureTip.style('opacity', 1);
            featureTip.html(tooltipText)
                .style('left', (d3.event.pageX - 50) + 'px')
                .style('top', (d3.event.pageY + 5) + 'px');
        }
        else removeTooltip();
    }

    function removeTooltip() {
        featureTip.style('opacity', 0);
        featureTip.html('');
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
        hideBtn.style.left = (margin.left - hideBtnExpandWidth) + "px";
        hideBtn.style.top = offsetY + "px";
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
            if (distance < 5) return;
            var distRange = distance / (ext[1] - ext[0]);
            if (distRange < 0.5) {
                brush.extent([startCoord, endCoord]);
                display();
                return
            }
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
            //only for contig size plot
            mini.selectAll('.item')
                .attr('opacity', function (d) {
                  if (!d || !d.size) return 1;
                  return d.size > minContigSize ? 1 : paleContigsOpacity;
            });
            display();
        }
    }

    function getNumberOfContigs(x) {
        lineCountContigs.selectAll('g')
                .remove();
        for (var item = 0; item < visRectsAndPaths.length; item++) {
            if (x_main(visRectsAndPaths[item].corr_start) <= x && x <= x_main(visRectsAndPaths[item].corr_end)) {
                var curItem = visRectsAndPaths[item];
                if (curItem.objClass.search("disabled") != -1)
                    continue;
                order = (curItem.order + 1).toString();
                offsetY = y_main(curItem.lane) + mainLanesHeight / 2;
                var suffix = 'th';
                var lastNumber = order.slice(-1);
                if (lastNumber == '1' && order != "11") suffix = 'st';
                else if (lastNumber == '2' && order != "12") suffix = 'nd';
                else if (lastNumber == '3' && order != "13") suffix = 'rd';
                var container = lineCountContigs.append('g')
                        .attr('transform', function (d) {
                            return 'translate(-3, ' + offsetY + ')';
                        })
                        .attr('width', function (d) {
                        });
                var numberLabel = container.append('text')
                        .text(order + suffix + ' contig')
                        .attr('text-anchor', 'end')
                        .attr('class', 'itemLabel');
                var labelRect = numberLabel.node().getBBox();
                container.insert('rect', 'text')
                        .attr('x', labelRect.x - 2)
                        .attr('y', labelRect.y)
                        .attr('height', labelRect.height + 2)
                        .attr('width', labelRect.width + 5)
                        .attr('fill', '#fff');
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

    function addGradient(d, marks, gradientExists) {
      if (!marks) return;
      var gradientId = 'gradient' + d.id;
      marks = marks.split(', ');
      if (marks.length == 1) return contigsColors[marks[0]];
      if (gradientExists) return 'url(#' + gradientId + ')';
      var gradient = chart.append("svg:defs")
          .append("svg:linearGradient")
          .attr("id", gradientId);
      gradient.attr("x1", "0%")
              .attr("y1", "0%")
              .attr("x2", "0%")
              .attr("y2", "100%");
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
        addMainXAxis(main, mainHeight + chrLabelsOffsetY);
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
        var startPos = x_main.domain()[0];
        var endPos = x_main.domain()[1];
        var domain = endPos - startPos;
        mainTickValue = getTickValue(domain);

        xMainAxis.tickFormat(function(tickValue) {
                              return formatValue(startPos + tickValue * domain, mainTickValue);
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
        var brushPos = isContigSizePlot ? delta : centerPos;

        var newBrush = d3.svg.brush()
                            .x(x_mini)
                            .extent([brushPos - delta, brushPos + delta])
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

    function getNextMaxCovValue(maxY, ticksVals) {
        var factor = ticksVals[1] - ticksVals[0];
        maxY = Math.max(factor, Math.ceil(maxY / factor) * factor);
        return maxY;
    }

    function setupCoverage() {
        numYTicks = 5;
        // draw mini coverage
        x_cov_mini_S = x_mini,      // x coverage scale
        y_max = max_depth[chromosome];

        y_cov_mini_S = d3.scale.linear()
                .domain([y_max, .1])
                .range([0, coverageHeight]);
        y_max = getNextMaxCovValue(y_max, y_cov_mini_S.ticks(numYTicks));
        y_cov_mini_S.domain([y_max, .1]);
        y_cov_main_S = y_cov_mini_S;

        y_cov_mini_A = d3.svg.axis()
                .scale(y_cov_mini_S)
                .orient('left')
                .tickFormat(function(tickValue) {
                    return tickValue;
                })
                .tickSize(2, 0)
                .ticks(numYTicks);
        mini_cov = chart.append('g')
                .attr('class', 'coverage')
                .attr('transform', 'translate(' + margin.left + ', ' + covMiniOffsetY + ')');
        mini_cov.append('g')
                .attr('class', 'y')
                .call(y_cov_mini_A);
        mini_cov.append('text')
                .text('Coverage')
                .attr('transform', 'rotate(-90 20, 80)');

        // draw main coverage
        y_cov_main_A = y_cov_mini_A = d3.svg.axis()
                .scale(y_cov_main_S)
                .orient('left')
                .tickFormat(function(tickValue) {
                    return tickValue;
                })
                .tickSize(2, 0)
                .ticks(numYTicks);

        var x_cov_main_A = xMainAxis;
        main_cov = chart.append('g')
                .attr('class', 'COV')
                .attr('transform', 'translate(' + margin.left + ', ' + covMainOffsetY + ')');

        main_cov.attr('display', 'none');
        main_cov.append('g')
                .attr('class', 'y')
                .attr('transform', 'translate(0, 0)');
        main_cov.select('.y').call(y_cov_main_A);

        drawCoverageLine(x_mini.domain()[0], x_mini.domain()[1], coverageFactor, mini_cov, x_mini);
    }

    function updateMainCoverage(minExtent, maxExtent, coverageFactor) {
        main_cov.select('.covered').remove();
        main_cov.select('.notCovered').remove();
        drawCoverageLine(minExtent, maxExtent, coverageFactor, main_cov, x_main);
        //main_cov.select('.y').call(y_cov_main_A);
    }

    function drawCoverageLine(minExtent, maxExtent, coverageFactor, track, scale) {
        var line = '',
            l = (maxExtent - minExtent) / coverageFactor,
            cov_main_dots_amount = Math.min(maxCovDots, l),
            step = Math.round(l / cov_main_dots_amount);

        var cov_lines = [];
        var nextPos = 0;
        var startPos = Math.floor(minExtent / coverageFactor / step) * step;
        for (var s, i = startPos;; i += step) {
            nextPos = Math.min(maxExtent / coverageFactor, i + step);
            coverage = coverage_data[chromosome].slice(i, i + step);
            if (coverage.length == 0) break;
            s = d3.sum(coverage, function (d) {
                        return d
                    }) / coverage.length;
            //y_max = Math.max(y_max, s);
            if (i == startPos) start = minExtent;
            else start = i * coverageFactor;
            end = nextPos * coverageFactor;
            if (s >= 1)
                cov_lines.push([scale(start), s, scale(end)]);
            else
                cov_lines.push([scale(start), 0, scale(end)]);
            if (nextPos >= (maxExtent / coverageFactor)) break;
        }
        //y_max = getNextMaxCovValue(y_max, y_cov_main_S.ticks(numYTicks));
        //y_cov_main_S.domain([y_max, .1]);
        //y_cov_main_A.scale(y_cov_main_S);

        line += ['M', cov_lines[0][0], y_cov_main_S(0)].join(' ');
        for (i = 0; i < cov_lines.length; i++) {
            cov_line = cov_lines[i];
            line += ['V', y_cov_main_S(cov_line[1])].join(' ');
            line += ['H', cov_line[2]].join(' ');
        }
        line += ['V', y_cov_main_S(0), 'Z'].join(' ');
        track.append('g')
                .attr('class', 'covered')
                .append('path')
                .attr('d', line);

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
    function getMiniItems(items) {
        var result = [];
        var curLane = 0;
        var numItem = 0;

        var countSupplementary = 0;
        for (var c, i = 0; i < items.length; i++) {
            item = items[i];
            if (item.lane != curLane) {
                numItem = 0;
                countSupplementary = 0;
            }
            result.push(createMiniItem(item, curLane, numItem, countSupplementary));
            curLane = item.lane;
            if (!item.notActive) numItem++;
            if (item.triangles && item.triangles.length > 0)
                for (var j = 0; j < item.triangles.length; j++) {
                    result.push(createMiniItem(item.triangles[j], curLane, numItem, countSupplementary));
                    numItem++;
                    countSupplementary++;
                }
        }
        return result;
    }

    function createMiniItem(item, curLane, numItem, countSupplementary) {
        var miniPathHeight = 10;
        var isSmall = x_mini(item.corr_end) - x_mini(item.corr_start) < miniPathHeight;

        item.misassembled = item.misassemblies ? "True" : "False";
        c = (item.misassembled == "False" ? "" : "misassembled");
        c += (item.similar == "True" ? " similar" : "");
        //c += ((!item.misassembledEnds && !isSmall) ? " light_color" : "");
        if (INTERLACE_BLOCKS_COLOR) c += ((numItem - countSupplementary) % 2 == 0 ? " odd" : "");
        var text = '';
        if (isContigSizePlot) {
            if (item.type == "small_contigs") c += " disabled";
            else if (item.type == "unaligned") c += " unaligned";
            else if (item.type == "misassembled") c += " misassembled";
            else if (item.type == "correct") c += "";
            else c += " unknown";
        }

        if (item.marks) {  // NX for contig size plot
          var marks = item.marks;
          text = marks;
          marks = marks.split(', ');
          for (var m = 0; m < marks.length; m++)
            c += " " + marks[m].toLowerCase();
        }

        item.objClass = c;
        item.order = numItem - countSupplementary;

        var startX = item.misassembledEnds == "R" ? x_mini(item.corr_end) : x_mini(item.corr_start);
        var endX = x_mini(item.corr_end);
        var pathEnd = x_mini(item.corr_end);
        var startY = y_mini(item.lane) + .18 * miniLanesHeight;
        if (INTERLACE_BLOCKS_VERT_OFFSET) startY += offsetsMiniY[items[i].order % 3] * miniLanesHeight;
        var path = '';
        if (!isSmall) {
            if (item.misassembledEnds == "L") path = ['M', startX, startY, 'L', startX + (Math.sqrt(3) * miniPathHeight / 2), startY + miniPathHeight / 2,
              'L', startX, startY + miniPathHeight, 'L',  startX, startY].join(' ');
            else if (item.misassembledEnds == "R") path = ['M', startX, startY, 'L', startX - (Math.sqrt(3) * miniPathHeight / 2), startY + miniPathHeight / 2,
              'L', startX, startY + miniPathHeight, 'L',  startX, startY].join(' ');
        }
        return {objClass: item.objClass, path: path, misassemblies: item.misassemblies, misassembledEnds: item.misassembledEnds,
            start: startX, end: endX, y: startY, size: item.size, text: text, id: item.id, type: item.type, fullContig: item.fullContig};
    }

    function getTextSize(text, size) {
        return text.length * size;
    }

    function glow() {
        var selectedItem = d3.select(this).select('rect');
        itemsContainer.append('rect')
                .attr('class', 'glow')
                .attr('pointer-events', 'none')
                .attr('width', selectedItem.attr('width'))
                .attr('height', selectedItem.attr('height'))
                .attr('fill', 'white')
                .attr('opacity', .3)
                .attr('transform', selectedItem.attr('transform'));
    }

    function disglow() {
        itemsContainer.select('.glow').remove();
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

    function changeInfo(block) {
        info.selectAll('p')
                .remove();

        info.selectAll('span')
                .remove();
        setBaseChartHeight();
        info.append('p')
                .style({'display': 'block', 'word-break': 'break-all', 'word-wrap': 'break-word'})
                .text('Name: ' + block.name, 280);

        if (block.structure) {
            if (isContigSizePlot)
                var contig_type = block.type ? block.type : '';
            else {
                var contig_type = block.misassemblies ? 'misassembled' : 'correct';
                if (block.similar == "True" && !block.misassemblies) contig_type += ' (similar in > 50% of the assemblies)';
                if (block.misassemblies) {
                    var misassemblies = block.misassemblies.split(';');
                    if (misassemblies[0] && misassemblies[1])
                        contig_type += ' (both sides';
                    else if (misassemblies[0])
                        contig_type += ' (left side';
                    else
                        contig_type += ' (right side';

                    if (block.similar == "True") contig_type += ', similar in > 50% of the assemblies';
                    contig_type += ')'
                }
            }
            if (contig_type)
                info.append('p')
                    .text('Type: ' + contig_type);
        }
        if (block.size)
            info.append('p')
                .text('Size: ' + block.size + ' bp');

        var appendPositionElement = function(data, start, end, contigName, assembly, whereAppend, start_in_contig, end_in_contig,
                                             prev_start, prev_end, is_expanded, overlapped_block) {
            var posVal = function (val) {
                if (mainTickValue == 'Gbp')
                    return d3.round(val / 1000000000, 2);
                else if (mainTickValue == 'Mbp')
                    return d3.round(val / 1000000, 2);
                else if (mainTickValue == 'kbp')
                    return d3.round(val / 1000, 2);
                else
                    return val;
            };
            var format = function (val) {
                val = val.toString();
                for (var i = 3; i < val.length; i += 4 )
                    val = val.slice(0 , val.length - i) + ' ' + val.slice(length - i, val.length);
                return val;
            };

            var curBlock = !data ? (overlapped_block ? overlapped_block : '') : data.filter(function (block) {
                if (block.type != "M" && block.contig == contigName) {
                    if (start_in_contig && block.start_in_contig == start_in_contig && block.end_in_contig == end_in_contig
                        && block.corr_start == start) return block;
                    else if (!start_in_contig && block.corr_start <= start && end <= block.corr_end) return block;
                }
            })[0];
            if (!curBlock) return;
            var ndash = String.fromCharCode(8211);
            if (is_expanded)
                var whereAppendBlock = whereAppend.append('p')
                        .attr('class', 'head_plus collapsed')
                        .on('click', function() {
                            var eventX = d3.event.x || d3.event.clientX;
                            if (eventX < whereAppendBlock[0][0].offsetLeft + 15)
                                openClose(whereAppendBlock[0][0]);
                        });
            else var whereAppendBlock = whereAppend;
            if (is_expanded || !isContigSizePlot) {
                var block = whereAppendBlock.append('span')
                    .attr('class', is_expanded ? 'head' : 'head main')
                    .append('text');
                block.append('tspan')
                    .attr('x', -50)
                    .text('Position: ');
                if (isContigSizePlot) var positionLink = block.append('a');
                else positionLink = block.append('tspan');
                positionLink.attr('id', 'position_link' + numBlock)
                            .style('cursor', 'pointer')
                            .text([posVal(curBlock.start), ndash, posVal(curBlock.end), mainTickValue, ' '].join(' '));
                if (is_expanded && !isContigSizePlot && chrContigs.indexOf(curBlock.chr) != -1)  // chromosome on this screen
                    positionLink.style('text-decoration', 'underline')
                        .style('color', '#7ED5F5')
                        .on('click', function () {
                            var brushExtent = brush.extent();
                            var brushSize = brushExtent[1] - brushExtent[0];
                            if (prev_start && prev_start > curBlock.corr_start) point = curBlock.corr_end;
                            else if (prev_start) point = curBlock.corr_start;
                            setCoords([point - brushSize / 2, point + brushSize / 2], true);
                            for (var i = 0; i < items.length; i++) {
                                if (items[i].assembly == assembly && items[i].name == contigName && 
                                        items[i].corr_start == curBlock.corr_start && items[i].corr_end == curBlock.corr_end) {
                                    selected_id = items[i].groupId;
                                    showArrows(items[i]);
                                    changeInfo(items[i]);
                                    display();
                                    break;
                                }
                            }
                            d3.event.stopPropagation();
                        });
                if (isContigSizePlot) {
                    positionLink.attr('href', (typeof links_to_chromosomes !== 'undefined' ? links_to_chromosomes[curBlock.chr] : 'alignment_viewer') +
                                    '.html?assembly=' + assembly + '&contig=' + contigName  + '&start=' + curBlock.start_in_ref + '&end=' + curBlock.end_in_ref)
                                .attr('target', '_blank')
                                .style('text-decoration', 'underline')
                                .style('color', '#7ED5F5');
                    if (typeof links_to_chromosomes !== 'undefined' && curBlock.chr)
                        positionLink.text(document.getElementById('position_link' + numBlock).textContent + '(' + curBlock.chr + ')');
                }
                if (is_expanded && !isContigSizePlot) {
                    if (prev_start == start && prev_end == end)
                        block.append('div')
                         .attr('id', 'circle' + start + '_' + end)
                         .attr('class', 'block_circle selected');
                    else
                        block.append('div')
                         .attr('id', 'circle' + start + '_' + end)
                         .attr('class', 'block_circle');
                }
                if (!isContigSizePlot) {
                    if (chrContigs.indexOf(curBlock.chr) == -1) {
                        block.append('a')
                                .attr('href', (typeof links_to_chromosomes !== 'undefined' ? links_to_chromosomes[curBlock.chr] : curBlock.chr) +
                                      '.html?assembly=' + assembly + '&contig=' + contigName  + '&start=' + curBlock.corr_start + '&end=' + curBlock.corr_end)
                                .attr('target', '_blank')
                                .style('text-decoration', 'underline')
                                .style('color', '#7ED5F5')
                                .text('(' + curBlock.chr + ')');
                    }
                    else if (chrContigs.length > 1) {
                        block.append('span')
                                .text('(' + curBlock.chr + ')');
                    }
                }
                block = block.append('p')
                        .attr('class', is_expanded ? 'close' : 'open');
                block.append('p')
                        .text(['reference:',
                            format(curBlock.start), ndash, format(curBlock.end),
                            '(' + format(Math.abs(curBlock.end - curBlock.start) + 1) + ')', 'bp'].join(' '));
                block.append('p')
                        .text(['contig:',
                            format(curBlock.start_in_contig), ndash,  format(curBlock.end_in_contig),
                            '(' + format(Math.abs(curBlock.end_in_contig - curBlock.start_in_contig) + 1) + ')', 'bp'].join(' '));
                block.append('p')
                        .text(['IDY:', curBlock.IDY, '%'].join(' '));
                numBlock++;
            }
            
        };
        var numBlock = 0;
        appendPositionElement(block.structure, block.corr_start, block.corr_end, block.name, block.assembly, info);

        showArrows(block);
        if (block.structure && block.structure.length > 0) {
            var blocks = info.append('p')
                    .attr('class', 'head main');
            var blocksText = (block.ambiguous ? 'Alternatives: ' : 'Blocks: ') + block.structure.filter(function(nextBlock) {
                                    if (nextBlock.type != "M") return nextBlock;
                                }).length;
            blocks.text(block.ambiguous ? 'Ambiguously mapped.' : blocksText);
            if (block.ambiguous)
                blocks.append('p')
                      .text(blocksText);

            for (var i = 0; i < block.structure.length; ++i) {
                var nextBlock = block.structure[i];
                if (nextBlock.type != "M") {
                    appendPositionElement(block.structure, nextBlock.corr_start, nextBlock.corr_end, block.name, block.assembly, blocks, nextBlock.start_in_contig,
                        nextBlock.end_in_contig, block.corr_start, block.corr_end, true);

                    if (block.ambiguous && i < block.structure.length - 1)
                        blocks.append('p')
                              .text('or');
                } else {
                    blocks.append('p')
                            .text(nextBlock.mstype);
                }
            }
        }
        if (block.overlaps && block.overlaps.length > 0) {
            var overlapsInfo = info.append('p')
                .attr('class', 'head main');
            var overlapsText = 'Overlaps with other contigs: ' + block.overlaps.length;
            overlapsInfo.text(overlapsText);

            for (var i = 0; i < block.overlaps.length; ++i) {
                var nextBlock = block.overlaps[i];
                appendPositionElement(null, nextBlock.corr_start,
                    nextBlock.corr_end, nextBlock.contig, block.assembly, overlapsInfo, nextBlock.start_in_contig,
                    nextBlock.end_in_contig, block.corr_start, block.corr_end, true, nextBlock);
            }
        }
        var blockHeight = info[0][0].offsetHeight;
        curChartHeight += blockHeight;
        chart.attr('height', curChartHeight);
        display();
    }

    function showArrows(block) {
        var verticalShift = -7;
        arrows = [];
        mini.selectAll('.arrow').remove();
        mini.selectAll('.arrow_selected').remove();
        var y = y_mini(block.lane) - 1;

        if (block.structure) {
            for (var i = 0; i < block.structure.length; ++i) {
                var nextBlock = block.structure[i];
                if (nextBlock.type != "M" && !nextBlock.notActive) {
                    if (!(nextBlock.corr_start <= block.corr_start && block.corr_end <= nextBlock.corr_end) &&
                        (isContigSizePlot || chrContigs.indexOf(nextBlock.chr) != -1)) {
                        arrows.push({start: nextBlock.corr_start, end: nextBlock.corr_end, lane: block.lane, selected: false});
                        mini.append('g')
                                .attr('transform', 'translate(' + x_mini((nextBlock.corr_end + nextBlock.corr_start) / 2) + ',' + verticalShift +')')
                                .attr('class', 'arrow')
                                .append("svg:path")
                                .attr("d", 'M0,0V' + (Math.abs(verticalShift) + 1 + block.lane * miniLanesHeight))
                                .attr("class", function () {
                                    return "path arrow";
                                })
                                .attr("marker-start", function () {
                                    return "url(#start_arrow)";
                                })
                                .attr("marker-end", function () {
                                    return "url(#arrow)";
                                });
                    }
                }
            }
        }

        arrows.push({start: block.corr_start, end: block.corr_end, lane: block.lane, selected: true});
        mini.append('g')
                .attr('transform', 'translate(' + x_mini((block.corr_end + block.corr_start) / 2) + ',' + verticalShift +')')
                .attr('class', 'arrow_selected')
                .append("svg:path")
                .attr("d", 'M0,0V' + (Math.abs(verticalShift) + 1 + block.lane * miniLanesHeight))
                .attr("class", function () {
                    return "path arrow_selected";
                })
                .attr("marker-start", function () {
                    return "url(#start_arrow_selected)";
                })
                .attr("marker-end", function () {
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
                items[numItem] = changeMisassembledStatus(items[numItem]);
                if (items[numItem].triangles && items[numItem].triangles.length > 0)
                    for (var i = 0; i < items[numItem].triangles.length; i++) {
                        if (!items[numItem].triangles[i].objClass) items[numItem].triangles[i].objClass = "misassembled";
                        items[numItem].triangles[i] = changeMisassembledStatus(items[numItem].triangles[i]);
                    }
            }
        }
        hideUncheckedMisassemblies(itemsContainer);
        hideUncheckedMisassemblies(chart);
    }

    function changeMisassembledStatus(item) {
        var msTypes = item.misassemblies.split(';');
        var isMisassembled = "False";
        for (var i = 0; i < msTypes.length; i++) {
            if (msTypes[i] && document.getElementById(msTypes[i]).checked) isMisassembled = "True";
        }
        if (isMisassembled == "True" && item.misassembled == "False") {
            item.objClass = item.objClass.replace("disabled", "misassembled");
        }
        else if (isMisassembled == "False")
            item.objClass = item.objClass.replace(/\bmisassembled\b/g, "disabled");
        item.misassembled = isMisassembled;
        return item;
    }

    function hideUncheckedMisassemblies(track) {
        track.selectAll('.item')
            .classed('misassembled', function (item) {
                if (item && item.misassemblies) {
                    if (item.misassembled) return item.misassembled == 'True';
                    return checkMsTypeToShow(item);
                }
            })
            .classed('disabled', function (item) {
                if (item && item.misassemblies) {
                    if (item.misassembled) return item.misassembled != 'True';
                    return !checkMsTypeToShow(item);
                }
            });
        track.selectAll('path')
            .classed('misassembled', function (item) {
                if (item && item.misassemblies)
                    return checkMsTypeToShow(item);
            })
            .classed('disabled', function (item) {
                if (item && item.misassemblies)
                    return !checkMsTypeToShow(item);
            });
    }

    function checkMsTypeToShow(item) {
        var msTypes = item.misassemblies.split(';');
        for (var i = 0; i < msTypes.length; i++) {
            if (msTypes[i] && document.getElementById(msTypes[i]).checked) return true;
        }
        return false;
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
        var classDescriptions = ['correct contigs', 'correct contigs similar among > 50% assemblies', 'misassembled blocks ' +
        '(misassembly event on the left side, on the right side)', 'misassembled blocks (zoom in to get details about misassembly event side)',
            'misassembled blocks similar among > 50% assemblies', 'unchecked misassembled blocks (see checkboxes)', 'genome features (e.g. genes)'];
        var prevOffsetY = 0;
        var offsetY = 0;
        for (var numClass = 0; numClass < classes.length; numClass++) {
            offsetY = addLegendItemWithText(legend, prevOffsetY, classes[numClass], classDescriptions[numClass]);
            if (classes[numClass] == 'misassembled light_color') {
                legend.append('path')
                    .attr('transform',  function () {
                        return 'translate(0,' + prevOffsetY + ')';
                    })
                    .attr('class', function () {
                        return 'mainItem end misassembled';
                    })
                    .attr('d', function () {
                        var startX = 0;
                        var startY = 0;
                        path = ['M', startX, startY, 'L', startX + (Math.sqrt(1) * (legendItemHeight - startY) / 2),
                            (startY + (legendItemHeight - startY)) / 2, 'L', startX, legendItemHeight - startY, 'L',  startX, startY].join(' ');
                        return path;
                    });
                legend.append('path')
                    .attr('transform',  function () {
                        return 'translate(' + legendItemWidth + ',' + prevOffsetY + ')';
                    })
                    .attr('class', function () {
                        return 'mainItem end misassembled odd';
                    })
                    .attr('d', function () {
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
        if (items[0].type && items[0].type != 'unknown') {
            var classes = ['correct', 'misassembled', 'unaligned'];
            var classMarks = ['', '', ''];
            var classDescriptions = ['correct contigs', 'misassembled contigs', 'unaligned contigs'];
        }
        else {
            var classes = ['unknown', ''];
            var classMarks = ['', 'N50'];
            var classDescriptions = ['contigs', 'contig of length = Nx statistic (x is 50 or 75)'];
            for (var i = 0; i < items.length; i++) {
                if (items[i].marks && items[i].marks.search('ng') != -1) {
                    classes = ['unknown', '', '', ''];
                    classMarks = ['', 'N50', 'NG50', 'N50, NG50'];
                    classDescriptions = ['contigs', 'contig of length = Nx statistic (x is 50 or 75)',
                        'contig of length = NGx statistic (x is 50 or 75)', 'contig of length = Nx and NGx simultaneously'];
                    break;
                }
            }
        }
        var offsetY = 0;
        for (var numClass = 0; numClass < classes.length; numClass++) {
            offsetY = addLegendItemWithText(legend, offsetY, classes[numClass], classDescriptions[numClass], classMarks[numClass])
        }
        return offsetY;
    }

    function addLegendItemWithText(legend, offsetY, className, description, marks) {
        legend.append('g')
                .attr('class', 'item miniItem legend ' + className)
                .append('rect')
                .attr('width', legendItemWidth)
                .attr('height', legendItemHeight)
                .attr('x', 0)
                .attr('y', offsetY)
                .attr('fill', function (d) {
                    d = {id: className};
                    if (marks) return addGradient(d, marks, false);
                });
        legend.append('text')
                .attr('x', legendTextOffsetX)
                .attr('y', offsetY + 5)
                .attr('dy', '.5ex')
                .style('fill', 'white')
                .text(description)
                .call(wrap, 155, false, false, legendTextOffsetX, ' ');
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
          var lane = features_data[numContainer];
          var numItems = 0;
          for (var i = 0; i < lane.length; i++) {
              if (!oneHtml && lane[i].chr != references_id[chr]) continue;
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
              return d.objClass;
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

            features[i].objClass = c;

            var x = x_mini(d.start);
            var y = y_anno_mini(d.lane);
            y += .15 * annotationMiniLanesHeight;
            if (d.objClass.search("odd") != -1)
                y += .04 * annotationMiniLanesHeight;

            result.push({objClass: c, name: d.name, start: d.start, end: d.end, id: d.id_, y: y, x: x, lane: d.lane, order: i});
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
                    return d.objClass;
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
        var animationDuration = 200, transitionDelay = 150;
        var paneToHide, hideBtn, textToShow, newOffset;
        var changedTracks = [], changedBtns = [];
        var mainPane = (pane == 'main');

        function setBtnTopPos(btn) {
            if (!btn) return;
            btn.style.top = parseInt(btn.style.top) + newOffset + 'px';
        }

        function setTrackPos(track) {
            if (!track) return;
            var trackY = d3.transform(track.attr("transform")).translate[1];
            trackY += newOffset;
            track.transition()
                 .duration(animationDuration)
                 .attr('transform', function(d) {
                    return 'translate(' + margin.left + ',' + trackY + ')'
                 });
        }

        if (track == 'features') {
            textToShow = 'Show annotation';
            paneToHide = mainPane ? annotationsMain : annotationsMini;
            hideBtn = mainPane ? hideBtnAnnotationsMain : hideBtnAnnotationsMini;
            newOffset = mainPane ? annotationsHeight : annotationsMiniHeight;
            if (mainPane) {
                featuresMainHidden = doHide;
                changedTracks = [main_cov, mini, annotationsMini, mini_cov];
                changedBtns = [hideBtnCoverageMain, hideBtnAnnotationsMini, hideBtnCoverageMini];
            }
            else {
                changedTracks = [mini_cov];
                changedBtns = [hideBtnCoverageMini];
            }
        }
        else {
            textToShow = 'Show read coverage';
            paneToHide = mainPane ? main_cov : mini_cov;
            hideBtn = mainPane ? hideBtnCoverageMain : hideBtnCoverageMini;
            newOffset = coverageHeight;
            if (mainPane) {
                coverageMainHidden = doHide;
                changedTracks = [mini, annotationsMini, mini_cov];
                changedBtns = [hideBtnAnnotationsMini, hideBtnCoverageMini];
            }
        }
        if (doHide) newOffset *= -1;
        if (!doHide) textToShow = 'Hide';
        for (var track_n = 0; track_n < changedTracks.length; track_n++)
            setTrackPos(changedTracks[track_n])
        for (var btn_n = 0; btn_n < changedBtns.length; btn_n++)
            setBtnTopPos(changedBtns[btn_n])
        if (doHide) paneToHide.attr('display', 'none');
        else paneToHide.transition().delay(transitionDelay).attr('display', '');
        hideBtn.onclick = function() {
            hideTrack(track, pane, !doHide);
        };
        hideBtn.innerHTML = textToShow;
        display();
    }

    function getCoordsFromURL() {
        var query = document.location.search;
        query = query.split('+').join(' ');

        var params = {},
            tokens,
            re = /[?&]?([^=]+)=([^&]*)/g;

        while (tokens = re.exec(query)) {
            params[decodeURIComponent(tokens[1])] = decodeURIComponent(tokens[2]);
        }
        if (params && params.assembly && params.contig && params.start && params.end) {
            var delta = 1000;
            setCoords([parseInt(params.start) - delta, parseInt(params.end) + delta]);
            for (var i = 0; i < items.length; i++) {
                if (items[i].assembly == params.assembly && items[i].name == params.contig &&
                        items[i].corr_start == params.start && items[i].corr_end == params.end) {
                    selected_id = items[i].groupId;
                    showArrows(items[i]);
                    changeInfo(items[i]);
                    display();
                    break;
                }
            }
        }
        return params;
    }