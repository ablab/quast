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
    var genePrediction = false;
    var ContigData = function(chromosome) {
        return parseData(contig_data[chromosome]);
    };

    var root = typeof exports !== "undefined" && exports !== null ? exports : window;
    root.contigData = ContigData;

    var isContigSizePlot = !chromosome;
    if (chromosome) var data = contigData(chromosome);
    else var data = parseData(contig_data);
    var lanes = data.lanes, items = data.items;
    var shortRefNames = typeof chrContigs !== "undefined" ? trimChrNames(chrContigs) : null;

    var contigInfoWidth = 240;
    var marginTop = 20, marginRight = 15, marginBottom = 15, marginLeft = 155;

    var w = ((window.innerWidth || document.documentElement.clientWidth || document.body.clientWidth)
             - contigInfoWidth - marginRight - marginLeft - 50);
    var margin = {
                top: marginTop, right: marginRight, bottom: marginBottom, left: marginLeft
            },
            mainLanesHeight = 45,
            miniLanesHeight = 18,
            mainGenesHeight = 2,
            mainGenesSelectedHeight = 8,
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
            paleContigsOpacity = .35,
            width = w,
            chartWidth = w,
            miniHeight = lanes.length * miniLanesHeight,
            mainHeight = lanes.length * (mainLanesHeight + lanesInterval),
            coverageHeight = typeof coverage_data != 'undefined' ? 125 : 0;
            coverageSpace = typeof coverage_data != 'undefined' ? 50 : 0;

    var contigsColors = {'Nx': '#7437BA', 'NGx': '#B53778',
        'correct': '#059B00', 'misassembled': '#DD4343', 'unaligned': '#8e8e8e', 'unknown': '#7DA6B9'};

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
    var genomeStartPos = isContigSizePlot ? 0 : 1;
    var x_mini = d3.scale.linear()
            .domain([genomeStartPos, total_len])
            .range([genomeStartPos, chartWidth]);
    var x_main = d3.scale.linear()
            .range([genomeStartPos, chartWidth]);

    var ext = d3.extent(lanes, function (d) {
        return d.id;
    });
    var minBrushExtent = 10;
    var y_main = d3.scale.linear().domain([ext[0], ext[1] + 1]).range([0, mainHeight]);
    var y_mini = d3.scale.linear().domain([ext[0], ext[1] + 1]).range([0, miniHeight]);
    var hideBtnAnnotationsMini, hideBtnAnnotationsMain, hideBtnCoverageMini, hideBtnCoverageMain,
        hideBtnGCMain, hideBtnGCMini,
        hideBtnPhysicalCoverageMini, hideBtnPhysicalCoverageMain, covMiniControls, covMainControls;

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

    var coverageFactor = 10, gcFactor = 100, maxCovDots = chartWidth * 4;
    var featuresHidden = false, drawCoverage = false;
    var coverageMainHidden = true, physicalCoverageHidden = true, physicalMiniCoverageHidden = true,
        gcHidden = false, gcMiniHidden = false;
    if (!featuresData || featuresData.features.length == 0)
      featuresHidden = true;
    if (typeof coverage_data != "undefined")
        drawCoverage = true;
    if (typeof gc_window_size != "undefined")
        gcFactor = gc_window_size;
    var featuresMainHidden = featuresHidden || lanes.length > 3;
    var brush, brush_cov, brush_anno;
    var offsetX = 20;

    var chart = d3.select('body').append('div').attr('id', 'chart')
            .append('svg:svg')
            .attr('width', width + margin.right + margin.left + offsetX)
            .attr('class', 'chart');

    var main = chart.append('g')
            .attr('transform', 'translate(' + margin.left + ',' + margin.top + ')')
            .attr('width', chartWidth)
            .attr('height', mainHeight + mainScale)
            .attr('class', 'main');
    addLanesBackgrounds();

    var spaceAfterMain = 15;
    var spaceAfterTrack = 40;

    var baseOffsetY, annotationsMiniOffsetY, annotationsMainOffsetY, covMiniOffsetY, covMainOffsetY, extraOffsetY;
    var hideBtnAnnotationsMiniOffsetY, hideBtnAnnotationsMainOffsetY,
        hideBtnCoverageMiniOffsetY, hideBtnCoverageMainOffsetY, hideBtnGCMiniOffsetY, hideBtnGCMainOffsetY,
        hideBtnPhysicalMiniCoverageOffsetY, hideBtnPhysicalCoverageOffsetY;

    setInterfaceCoordinates();

    var baseChartHeight = covMiniOffsetY + coverageHeight * 2 + annotationsHeight + margin.top + margin.bottom + 100;
    var curChartHeight = baseChartHeight;

    var manyChromosomes = !isContigSizePlot && chrContigs.length > 1;
    var chrLabelsOffsetY = manyChromosomes ? 6 : 0;

    chart.attr('height', curChartHeight);
    chart.append('defs').append('clipPath')
            .attr('id', 'clip')
            .append('rect')
            .attr('width', width)
            .attr('height', mainHeight + chrLabelsOffsetY);

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

    drawChartLanes();

    // draw the lanes for the annotations chart
    if (!featuresHidden) {
        var featurePaths = getFeaturePaths(featuresData.features);
        addFeatureTrackInfo(annotationsMini, y_anno_mini);
        addFeatureTrackInfo(annotationsMain, y_anno);
    }

    var mini_cov, main_cov, x_cov_mini_S, y_cov_mini_S, y_cov_mini_A, y_cov_main_S, y_cov_main_A, y_max, y_max_log, numYTicks;
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
    miniRects = miniItems.filter(function (block) {
        if (isContigSizePlot && !block.fullContig) return;
        if (!block.path) return block;
    });
    miniPaths = miniItems.filter(function (block) {
        if (block.path) return block;
    });

    mini.append('g').selectAll('miniItems')
            .data(miniRects)
            .enter().append('rect')
            .attr('class', function (block) {
                if (block.text && !block.contig_type) return 'block gradient';
                return 'block miniItem ' + block.objClass;
            })
            .attr('fill', function (block) {
                if (block.text && !block.contig_type) return addGradient(block, block.text, false);
            })
            .attr('transform', function (block) {
                return 'translate(' + block.start + ', ' + block.y + ')';
            })
            .attr('width', function (block) {
                itemWidth = block.end - block.start;
                return itemWidth;
            })
            .attr('height', miniItemHeight)
            .attr('opacity', function (block) {
                if (block.contig_type == 'short_contigs')
                    return paleContigsOpacity;
                return 1;
            });
    mini.append('g').selectAll('miniItems')
            .data(miniPaths)
            .enter().append('path')
            .attr('class', function (block) {
              return 'mainItem end ' + block.objClass;
            })
            .attr('d', function (block) {
              return block.path;
            });

    var featureTip = d3.select('body').append('div')
                        .attr('class', 'feature_tip')
                        .style('opacity', 0);
    if (!featuresHidden) addFeatureTrackItems(annotationsMini, x_mini);

    addSelectionAreas();

    d3.select('body').on("keypress", keyPressAnswer);
    d3.select('body').on("keydown", keyDownAnswer);

    // draw contig info menu
    var menuContainer = d3.select('body').append('div')
                            .attr('class', 'menu_div');
    var menu = menuContainer.append('div')
                            .attr('class', 'menu_div')
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
                               y1: 1, len: chrLen});
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
        var blocksN50 = [];
        for (var i = 0; i < items.length; i++) {
            addGradient(items[i], items[i].marks, true);
            if (items[i].marks)
                blocksN50.push(items[i]);
        }
        mini.append('g').selectAll('miniItems')
            .data(blocksN50)
            .enter().append('text')
            .attr('class', 'miniItems text')
            .text(function (block) {
                return block.marks;
            })
            .style('fill', 'white')
            .attr('transform', function (block) {
                var x = Math.max(x_mini(block.corr_end) - x_mini(block.size) + 1,
                    (x_mini(block.corr_end) - x_mini(block.size) / 2) - getSize(block.marks) / 2);
                var y = y_mini(block.lane) + miniLanesHeight - 5;
                return 'translate(' + x + ', ' + y + ')';
            });
    }

    var linesOffset = 20 + Math.max(0, extraOffsetY + 23);
    var linesLabelsLayer = d3.select('body').append('div').attr('id', 'lines_labels')
                                    .append('svg:svg')
                                    .style('position', 'absolute')
                                    .attr('width', width)
                                    .attr('height', mainHeight + linesOffset)
                                    .style('top', itemSvgOffsetY - 10)
                                    .style('left', margin.left)
                                    .attr('pointer-events', 'none');
    var itemLines = linesLabelsLayer.append('g')
                                    .attr('pointer-events', 'painted');
    var itemLabels = linesLabelsLayer.append('g');
    var textLayer = itemsLayer.append('g');
    if (!featuresHidden)
      var featurePath = annotationsMain.append('g')
        .attr('clip-path', 'url(#clip)');
    var lineNumberLayer = linesLabelsLayer.append('g')
                            .attr('pointer-events', 'painted');
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

        var lineCountContigs = lineNumberLayer.append('g')
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
        var prevPos = 0;
        var prevBreakpointPos = 0;
        var prevLineType = null;
        var fullsizeBlock = false;
        for (var i = 0; i < items.length; i++) {
        	block = items[i];
            if (block.notActive) {
                if (block.size - (block.corr_end - block.corr_start) < 5) {
                    fullsizeBlock = true;
                    continue;
                }
                fullsizeBlock = false;
                y = y_main(block.lane) + .25 * lanesInterval + 10;
                if (prevBreakpointPos && Math.abs(prevBreakpointPos - block.corr_start) > 2) {
                    lines.push({pos: block.corr_start, y: y, type: prevLineType});
                }
                var linePos = block.corr_end;
                if (Math.abs(prevPos - linePos) > 2) {
                    if (block.mstype != "skip" && block.contig_type != "correct") {
                        prevBreakpointPos = linePos;
                        prevLineType = block.mstype;
                        lines.push({pos: linePos, y: y, type: block.mstype});
                    }
                    else prevBreakpointPos = null;
                }
                else prevBreakpointPos = null;
                prevPos = linePos;
            }
            else {
                if (!fullsizeBlock && prevBreakpointPos) lines.pop();
                prevBreakpointPos = null;
            }
        }
        return lines;
    }

    function isOverlapping (block, lane) {
        if (lane)
            for (var i = 0; i < lane.length; i++)
                if (block.corr_start <= lane[i].corr_end && lane[i].corr_start <= block.corr_end)
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
                    description += 'mis: ' + assemblies_misassemblies[assemblyName];
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

        function parseItem(block, fullInfo, misassembly) {
            block.misassembledEnds = '';
            block.lane = laneId;
            block.id = itemId;
            block.groupId = groupId;
            block.assembly = assemblyName;
            if (isContigSizePlot) {
                if (!fullInfo) {
                    block.corr_start = currentLen;
                    currentLen += block.size;
                    block.corr_end = currentLen;
                    block.fullContig = true;
                }
                else {
                    block.start_in_ref = block.corr_start;
                    block.end_in_ref = block.corr_end;
            	    start_in_contig = Math.min(block.start_in_contig, block.end_in_contig);
            	    end_in_contig = Math.max(block.start_in_contig, block.end_in_contig);
                    block.corr_start = currentLen + start_in_contig - 1;
                    block.corr_end = currentLen + end_in_contig;
                    block.notActive = true;
                    block.contig_type = fullInfo.contig_type;
                    block.mstype = misassembly ? misassembly.mstype : null;
                }
            }
            var nonOverlappingLaneId = 0;
            var blockSize = block.corr_end - block.corr_start;
            var minOverlap = Math.min(500, blockSize * 0.1);
            if (!isContigSizePlot) {
                for (var nonOverlappingLaneId = 0; nonOverlappingLaneId < lastPosInLanes.length; nonOverlappingLaneId++){
                    if (lastPosInLanes[nonOverlappingLaneId] - block.corr_start < Math.min(minOverlap, lastBlockSizesInLanes[nonOverlappingLaneId] * 0.1))
                        break;
                }
                block.nonOverlappingLane = nonOverlappingLaneId;
                if (nonOverlappingLaneId >= lastPosInLanes.length) {
                    lastPosInLanes.push(block.corr_end);
                    lastBlockSizesInLanes.push(blockSize);
                }
                else {
                    if (block.corr_end > lastPosInLanes[nonOverlappingLaneId]) {
                        lastPosInLanes[nonOverlappingLaneId] = block.corr_end;
                        lastBlockSizesInLanes[nonOverlappingLaneId] = blockSize;
                    }
                    else {
                        lastPosInLanes[nonOverlappingLaneId] = lastPosInLanes[nonOverlappingLaneId];
                    }
                }
            }
            block.triangles = Array();
            itemId++;
            numItems++;
            var structure = getBlockStructure(block);
            if (block.genes) {
                genePrediction = true;
                var corr_start = block.corr_start;
                if (block.contig_type != 'ambiguous' && structure && structure.length > 0)
                    corr_start =  structure[0].corr_start;
                for (var gene_n = 0; gene_n < block.genes.length; gene_n++) {
                    if (!block.genes[gene_n].corr_start) {
                        block.genes[gene_n].corr_start = Math.max(block.genes[gene_n].start + corr_start, block.corr_start);
                        block.genes[gene_n].corr_end = Math.min(block.genes[gene_n].end + corr_start, block.corr_end);
                    }
                    block.genes[gene_n].groupId = block.groupId;
                    block.genes[gene_n].lane = block.lane;
                    block.genes[gene_n].nonOverlappingLane = block.nonOverlappingLane;
                    block.genes[gene_n].objClass = 'gene predicted_gene';
                    block.genes[gene_n].notActive = true;
                }
            }
            if (block.mis_ends && misassembled_ends) {
                for (var num = 0; num < misassembled_ends.length; num++) {
                    if (!misassembled_ends[num]) continue;
                    var triangleItem = {};
                    triangleItem.name = block.name;
                    triangleItem.corr_start = block.corr_start;
                    triangleItem.corr_end = block.corr_end;
                    triangleItem.assembly = block.assembly;
                    triangleItem.id = itemId;
                    triangleItem.lane = laneId;
                    triangleItem.nonOverlappingLane = block.nonOverlappingLane;
                    triangleItem.groupId = groupId;
                    triangleItem.misassembledEnds = misassembled_ends[num];
                    triangleItem.misassemblies = block.misassemblies.split(';')[num];
                    block.triangles.push(triangleItem);
                    itemId++;
                    numItems++;
                }
            }
            return block
        }

        for (var assemblyName in chart.assemblies) {
            var lane = chart.assemblies[assemblyName];
            var currentLen = 0;
            var numItems = 0;
            var lastPosInLanes = [];
            var lastBlockSizesInLanes = [];
            var laneItems = [];
            for (var i = 0; i < lane.length; i++) {
                var block = lane[i];
                if (block.mis_ends) var misassembled_ends = block.mis_ends.split(';');
                if (isContigSizePlot) {
                    var blocks = getBlockStructure(block);
                    if (blocks) {
                        for (var k = 0; k < blocks.length; k++) {
                            var misassembly = (k < blocks.length - 1 && blocks[k + 1].contig_type == 'M') ? blocks[k + 1] : null;
                            if (blocks[k].contig_type != 'M' && block.contig_type != 'ambiguous')
                                laneItems.push(parseItem(blocks[k], block, misassembly));
                            if (block.contig_type == 'ambiguous') {
                                blocks[k].start_in_ref = blocks[k].corr_start;
                                blocks[k].end_in_ref = blocks[k].corr_end;
                            }
                        }
                    }
                }
                var newItem = parseItem(block);
                if (!block.is_best) {
                    newItem.best_group = getBlockStructure(block);
                    newItem.structure = null;
                }
                laneItems.push(newItem);
                groupId++;
            }
            for (var i = 0; i < laneItems.length; i++) {
                item = laneItems[i];
                if (!isContigSizePlot) {
                    item.nonOverlappingLane = lastPosInLanes.length - item.nonOverlappingLane - 1;
                    if (item.triangles) {
                        for (var j = 0; j < item.triangles.length; j++) {
                            item.triangles[j].nonOverlappingLane = lastPosInLanes.length - item.triangles[j].nonOverlappingLane - 1;
                        }
                    }
                    if (item.genes) {
                        for (var j = 0; j < item.genes.length; j++) {
                            item.genes[j].nonOverlappingLane = lastPosInLanes.length - item.genes[j].nonOverlappingLane - 1;
                        }
                    }
                }
                items.push(item);
            }

            lanes.push({
                id: laneId,
                label: assemblyName,
                maxLines: lastPosInLanes.length,
                isExpanded: false,
            });
            laneId++;
        }

        addAssemblyDescription(lanes);
        return {lanes: lanes, items: items};
    }

    function setupCoverage() {
        numYTicks = 5;
        // draw mini coverage
        x_cov_mini_S = x_mini,      // x coverage scale
        y_max = typeof reads_max_depth !== 'undefined' ? Math.max(reads_max_depth[chromosome], physical_max_depth[chromosome]) :
            max_depth[chromosome];

        y_cov_mini_S = setYScaleCoverage();
        y_cov_main_S = y_cov_mini_S;

        y_gc_scale = d3.scale.linear().domain([100, .1]).range([0, coverageHeight]);
        y_gc_ticks = [20, 40, 60, 80, 100];
        y_gc_axis = d3.svg.axis()
            .orient('right')
            .tickFormat(function(val, i) {
                return i < y_gc_ticks.length - 1 ? val + '%' : "GC";
            })
            .tickSize(2, 0)
            .tickValues(y_gc_ticks);
        y_gc_axis.scale(y_gc_scale);
        mini_cov = chart.append('g')
            .attr('class', 'coverage')
            .attr('transform', 'translate(' + margin.left + ', ' + covMiniOffsetY + ')');
        mini_cov.append('g')
            .attr('class', 'y cov')
            .style("fill", "steelblue");
        mini_cov.append('g')
            .attr('class', 'y gc_plot')
            .style("fill", "#3d6f15")
            .attr('transform', 'translate(' + width + ', 0)');
        mini_cov.select('.y.gc_plot').call(y_gc_axis);

        function calculateTicks(scale) {
            var maxValue = scale.domain()[0];
            var covTicks = d3.scale.linear().domain(scale.domain()).ticks(4);
            var tickStep = covTicks[1] - covTicks[0];
            covTicks = covTicks.filter(function (x) {
                return x < maxValue - tickStep / 2;
            });
            covTicks.push(maxValue);
            return covTicks;
        }
        // draw main coverage
        y_cov_mini_A = d3.svg.axis()
            .orient('left')
            .tickFormat(function(val, i, n) {
                return val < y_cov_mini_S.domain()[0] ? val + 'x' : "Cov";
            })
            .tickSize(2, 0)
            .tickValues(function () {
                return calculateTicks(y_cov_mini_S);
            });

        y_cov_main_A = d3.svg.axis()
            .orient('left')
            .tickFormat(function(val, i, n) {
                return val < y_cov_main_S.domain()[0] ? val + 'x' : "Cov";
            })
            .tickSize(2, 0)
            .tickValues(function () {
                return calculateTicks(y_cov_main_S);
            });

        var x_cov_main_A = xMainAxis;
        main_cov = chart.append('g')
            .attr('class', 'COV')
            .attr('transform', 'translate(' + margin.left + ', ' + covMainOffsetY + ')');

        main_cov.attr('display', 'none');
        main_cov.append('g')
            .attr('class', 'y gc_plot')
            .style("fill", "#3d6f15")
            .attr('transform', 'translate(' + width + ', 0)');
        main_cov.select('.y.gc_plot').call(y_gc_axis);
        main_cov.append('g')
            .attr('class', 'y cov')
            .style("fill", "steelblue");

        setYScaleLabels(mini_cov, y_cov_mini_A, y_cov_mini_S);
        setYScaleLabels(main_cov, y_cov_main_A, y_cov_main_S);
        appendPaths(mini_cov);
        appendPaths(main_cov);
        totalMaxYMini = y_max;
        totalMaxYMain = y_max;
        if (typeof physical_coverage_data !== 'undefined') {
            drawCoverageLine(x_mini.domain()[0], x_mini.domain()[1], false, physical_coverage_data, '.phys_covered');
            togglePhysCoverageMini();
        }
        drawCoverageLine(x_mini.domain()[0], x_mini.domain()[1], false, coverage_data, '.covered');
        drawCoverageLine(x_mini.domain()[0], x_mini.domain()[1], false, gc_data, '.gc');
    }

    function appendPaths(track) {
        track.append('g')
            .attr('class', 'phys_covered')
            .append('path');
        track.append('g')
            .attr('class', 'covered')
            .append('path');
        track.append('g')
            .attr('class', 'gc')
            .append('path');
    }

    // generates a single path for each block class in the mini display
    // ugly - but draws mini 2x faster than append lines or line generator
    // is there a better way to do a bunch of lines as a single path with d3?
    function getMiniItems(items) {
        var result = [];
        var curLane = 0;
        var numItem = 0;

        var countSupplementary = 0;
        for (var c, i = 0; i < items.length; i++) {
            block = items[i];
            if (block.lane != curLane) {
                numItem = 0;
                countSupplementary = 0;
            }
            result.push(createMiniItem(block, curLane, numItem, countSupplementary));
            curLane = block.lane;
            if (!block.notActive) numItem++;
            if (block.triangles && block.triangles.length > 0)
                for (var j = 0; j < block.triangles.length; j++) {
                    result.push(createMiniItem(block.triangles[j], curLane, numItem, countSupplementary));
                    numItem++;
                    countSupplementary++;
                }
        }
        return result;
    }

    function createMiniItem(block, curLane, numItem, countSupplementary) {
        var miniPathHeight = 10;
        var isSmall = x_mini(block.corr_end) - x_mini(block.corr_start) < miniPathHeight;

        block.misassembled = block.misassemblies ? "True" : "False";
        c = (block.misassembled == "False" ? "" : "misassembled");
        c += (block.similar && block.similar == "True" ? " similar" : "");
        if (block.more_unaligned && block.misassembled == "True") {
            c = "mis_unaligned";
            block.misassemblies = "";
        }
        else if (block.more_unaligned) {
            c = "correct_unaligned";
        }
        if (block.ambiguous) c = "ambiguous";
        if (!block.is_best && block.ambiguous_alignments && block.ambiguous_alignments.length > 0) c = "alternative";
        //c += ((!block.misassembledEnds && !isSmall) ? " light_color" : "");
        if (INTERLACE_BLOCKS_COLOR) c += ((numItem - countSupplementary) % 2 == 0 ? " odd" : "");
        var text = '';
        if (isContigSizePlot) {
            if (block.contig_type == "short_contigs") c += " disabled";
            else if (block.contig_type == "correct") c += "";
            else if (!block.contig_type) c += " unknown";
            else c += " " + block.contig_type;
        }

        if (block.marks) {  // NX for contig size plot
          var marks = block.marks;
          text = marks;
          marks = marks.split(', ');
          for (var m = 0; m < marks.length; m++)
            c += " " + marks[m].toLowerCase();
        }

        block.objClass = c;
        block.order = numItem - countSupplementary;

        var startX = block.misassembledEnds == "R" ? x_mini(block.corr_end) : x_mini(block.corr_start);
        var endX = x_mini(block.corr_end);
        var pathEnd = x_mini(block.corr_end);
        var startY = y_mini(block.lane) + .18 * miniLanesHeight;
        if (INTERLACE_BLOCKS_VERT_OFFSET) startY += offsetsMiniY[items[i].order % 3] * miniLanesHeight;
        var path = '';
        if (!isSmall) {
            if (block.misassembledEnds == "L") path = ['M', startX, startY, 'L', startX + (Math.sqrt(3) * miniPathHeight / 2), startY + miniPathHeight / 2,
              'L', startX, startY + miniPathHeight, 'L',  startX, startY].join(' ');
            else if (block.misassembledEnds == "R") path = ['M', startX, startY, 'L', startX - (Math.sqrt(3) * miniPathHeight / 2), startY + miniPathHeight / 2,
              'L', startX, startY + miniPathHeight, 'L',  startX, startY].join(' ');
        }
        return {objClass: block.objClass, path: path, misassemblies: block.misassemblies, misassembledEnds: block.misassembledEnds,
            start: startX, end: endX, y: startY, size: block.size, text: text, id: block.id, contig_type: block.contig_type, fullContig: block.fullContig};
    }

    function parseFeaturesData(chr) {
      var lanes = [];
      var features = [];
      var data = [];
      var laneId = 0, itemId = 0;

      for (var numContainer = 0; numContainer < features_data.length; numContainer++) {
          var lane = features_data[numContainer];
          var numItems = 0;
          var chrIndex = 0;
          for (var i = 0; i < lane.length; i++) {
              chrIndex = typeof references_by_id !== 'undefined' ? chrContigs.indexOf(references_by_id[lane[i].chr]) : lane[i].chr;
              if (!oneHtml && chrIndex == -1) continue;
              var block = lane[i];
              block.lane = laneId;
              block.id = itemId;
              block.chr = chrIndex;
              block.corr_start = block.corr_start ? block.corr_start : block.start;
              block.corr_end = block.corr_end ? block.corr_end : block.end;
              features.push(block);
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
              return scale(d.corr_end - d.corr_start);
            })
            .attr('height', featureMiniHeight)
            .on('mouseenter', selectFeature)
            .on('mouseleave', deselectFeature)
            .on('click',  function(d) {
                addTooltip(d);
            });
        var visFeatureTexts = featurePaths.filter(function (d) {
                if (scale(d.corr_end) - scale(d.corr_start) > 45) return d;
        });
        annotations.append('g').selectAll('miniItems')
                            .data(visFeatureTexts)
                            .enter().append('text')
                            .style("font-size", "10px")
                            .text(function (d) {
                                var textContent = d.name ? d.name : 'ID=' + d.id;
                                var textItemLen = scale(d.corr_end) - scale(d.corr_start);
                                return getVisibleText(textContent, textItemLen)
                            })
                            .attr('class', 'featureLabel')
                            .attr('transform', function (d) {
                              return 'translate(' + (d.x + 3) + ', ' + (d.y + featureMiniHeight / 2 + 3) + ')';
                            });
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
            c = "gene ";
            if (INTERLACE_BLOCKS_COLOR) c += (numItem % 2 == 0 ? "odd" : "");

            features[i].objClass = c;

            var x = x_mini(d.corr_start);
            var y = y_anno_mini(d.lane);
            y += .15 * annotationMiniLanesHeight;
            if (d.objClass.search("odd") != -1)
                y += .04 * annotationMiniLanesHeight;

            result.push({objClass: c, name: d.name, start: d.start, end: d.end, corr_start: d.corr_start, corr_end: d.corr_end,
                id: d.id_, y: y, x: x, lane: d.lane, order: i});
            curLane = d.lane;
            numItem++;
        }
        return result;
    }

    function drawChartLanes() {
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
        addLanesText(main, y_main, lanes, laneLabelOffsetX, true, !isContigSizePlot);
        if (!isContigSizePlot)
            addExpandBtns();

        // draw the lanes for the mini chart
        laneLabelOffsetX = 100;
        mini.append('g').selectAll('.laneLines')
            .data(lanes)
            //.enter().append('line')
            .attr('x1', 0)
            .attr('y1', function (lane) {
                return d3.round(y_mini(lane.id)) + .5;
            })
            .attr('x2', chartWidth)
            .attr('y2', function (lane) {
                return d3.round(y_mini(lane.id)) + .5;
            })
            .attr('stroke', function (lane) {
                return lane.label === '' ? 'white' : 'lightgray'
            });
        addLanesText(mini, y_mini, lanes, laneLabelOffsetX, false, false);
    }

    function addLanesText(track, scale, lanes, laneLabelOffsetX, isMain, addStdoutLink) {
        track.selectAll('.laneText').remove();
        track.selectAll('.laneText')
                .data(lanes)
                .enter().append('text')
                .attr('x', -10)
                .attr('y', function (lane) {
                    var y = getExpandedLanesCount(lane.id);
                    return scale(y + (isMain ? .1 :.5));
                })
                .attr('dy', '.5ex')
                .attr('text-anchor', 'end')
                .attr('class', 'laneText')
                .text(function(lane) { return isMain ? lane.description : lane.label; })
                .call(wrap, laneLabelOffsetX, true, addStdoutLink, -10, /\n/);
    }

    function addLanesBackgrounds() {
        main.selectAll('.lane_bg')
                .data(lanes)
                .enter().append('rect')
                .attr('x', 0)
                .attr('y', function (lane) {
                    return y_main(lane.id) - 1;
                })
                .attr('width', chartWidth)
                .attr('height', mainLanesHeight + 10)
                .attr('display', 'none')
                .attr('class', 'lane_bg');
    }

    function addExpandBtns() {
        var expandableLanes = [];
        for (var lanes_n = 0; lanes_n < lanes.length; lanes_n++) {
            lane = lanes[lanes_n];
            if (lane.maxLines > 1)
                expandableLanes.push(lane);
        }
        d3.select('body').selectAll('.expandBtn')
                .data(expandableLanes)
                .enter().append('div')
                .style('display', 'none')
                .style('position', 'absolute')
                .style('left', (margin.left + chartWidth + 5) + 'px')
                .style('width', '10px')
                .style('height', '15px')
                .attr('class', 'expandBtn collapsedBtn btn btn-mini btn-inverse')
                .attr('data-toggle', 'tooltip')
                .attr('title', 'Expand overlapping blocks')
                .html('+')
                .on('click', function (lane) {
                    var isCollapsed = $(this).attr('class').search("collapsed") != -1;
                    //$(this).attr('collapsed', function(_, attr){ return !attr});
                    if (isCollapsed)
                        $(this).html('&minus;');
                    else $(this).html('&plus;');
                    expandLane(lane.id, isCollapsed);
                    d3.select(this).classed('collapsedBtn', !isCollapsed);
                })
                .on('hover', function (lane) {
                    var isCollapsed = $(this).attr('class').search("collapsed") != -1;
                    //$(this).attr('collapsed', function(_, attr){ return !attr});
                    if (isCollapsed)
                        $(this).html('&minus;');
                    else $(this).html('&plus;');
                    expandLane(lane.id, isCollapsed);
                    d3.select(this).classed('collapsedBtn', !isCollapsed);
                });
    }

    function moveExpandBtns() {
        d3.select('body').selectAll('.expandBtn')
                         .style('top', function (lane) {
                            var y = getExpandedLanesCount(lane.id);
                            return (y_main(y) + mainOffsetY + 3) + 'px';
                         })
                         .style('display', '');
    }