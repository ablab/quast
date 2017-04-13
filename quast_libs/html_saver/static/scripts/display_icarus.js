function display() {
    x_main = d3.scale.linear()
        .range([genomeStartPos, chartWidth]);
    var rects
        , minExtent = Math.max(brush.extent()[0], x_mini.domain()[0])
        , maxExtent = Math.min(brush.extent()[1], x_mini.domain()[1])
        ,
        visibleLines = separatedLines.filter(function (line) {
            if (line.corr_end < maxExtent) return line;
        }),
        visibleBreakpointLines = breakpointLines.filter(function (line) {
            if (line.pos < maxExtent) return line;
        });
    visItems = items.filter(function (block) {
        if (block.corr_start < maxExtent && block.corr_end > minExtent) {
            var drawLimit = 1;
            var visibleLength = getItemWidth(block, minExtent, maxExtent);
            if (visibleLength > drawLimit)
                return block;
        }
    });
    mini.select('.brush').call(brush.extent([minExtent, maxExtent]));
    if (drawCoverage)
        mini_cov.select('.brush').call(brush_cov.extent([minExtent, maxExtent]));
    if (!featuresHidden)
        annotationsMini.select('.brush').call(brush_anno.extent([minExtent, maxExtent]));

    x_main.domain([minExtent, maxExtent]);
    document.getElementById('input_coords_start').value = getChromCoords(Math.round(minExtent), 0);
    document.getElementById('input_coords_end').value = getChromCoords(Math.round(maxExtent), 1);

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
        .data(visibleLines, function (line) {
            return line.id;
        });

    var lines = lineContigs.enter().append('g')
        .attr('class', 'main_lines')
        .attr('transform', function (line) {
            var x = x_main(line.corr_end);
            var y = line.assembly ? y_main(line.lane) + 10 + extraOffsetY : 10 + extraOffsetY;

            return 'translate(' + x + ', ' + y + ')';
        });
    lines.append('rect')
        .attr('width', 1)
        .attr('height', function (line) {
            return line.assembly ? mainLanesHeight + lanesInterval : getExpandedLanesHeight() + chrLabelsOffsetY;
        })
        .attr('fill', '#300000');

    //misassemblies breakpoints lines
    linesLabelsLayer.selectAll('.dashed_lines').remove();

    lines = itemLines.selectAll('.g')
        .data(visibleBreakpointLines, function (line) {
            return line.id;
        })
        .enter().append('g')
        .attr('class', 'dashed_lines')
        .attr('transform', function (line) {
            return 'translate(' + x_main(line.pos) + ', ' + line.y + ')';
        });
    lines.append('path')
        .attr('d', 'M0,0V' + mainLanesHeight)
        .attr('stroke', function(line) {
            if (line.type == 'fake') return '#000000';
            else return '#8A0808';
        })
        .attr('stroke-width', '1');

    //update features
    removeTooltip();
    if (!featuresMainHidden) drawFeaturesMain(minExtent, maxExtent);

    // update the block rects
    visPaths = [];
    visRects = [];
    visGenesRects = [];
    for (var item_n = 0; item_n < visItems.length; item_n++) {
        visRects.push(visItems[item_n]);
        if (visItems[item_n].triangles) {
            for (var i = 0; i < visItems[item_n].triangles.length; i++) {
                var triangle = visItems[item_n].triangles[i];
                if ((triangle.misassembledEnds == "R" && triangle.corr_end > maxExtent) ||
                    (triangle.misassembledEnds == "L" && triangle.corr_start < minExtent))
                    continue;
                var w = getItemWidth(triangle, minExtent, maxExtent);
                var triangle_width = Math.sqrt(0.5) * mainLanesHeight / 2;
                if (w > triangle_width * 1.5) visPaths.push(triangle);
            }
        }
        if (visItems[item_n].genes && visItems[item_n].contig_type != "short_contigs") {
            for (var i = 0; i < visItems[item_n].genes.length; i++) {
                var gene = visItems[item_n].genes[i];
                if (gene.corr_start >= maxExtent || gene.corr_end <= minExtent)
                    continue;
                var visibleLength = getItemWidth(gene, minExtent, maxExtent);
                if (visibleLength > 0) visGenesRects.push(gene);
            }
        }
    }

    var newItems = createItems(visRects, 'rect', minExtent, maxExtent, '.block');
    createItems(visPaths, 'path', minExtent, maxExtent, '.block');
    createItems(visGenesRects, 'rect', minExtent, maxExtent, '.gene.predicted_gene');

    newItems.on('click', function (block) {
            selected_id = block.groupId;
            changeInfo(block);
        })
        .on('mouseenter', glow)
        .on('mouseleave', disglow);

    addLabels(visRects, minExtent, maxExtent);
    // upd coverage
    if (drawCoverage && (!coverageMainHidden || !physicalCoverageHidden))
        updateMainCoverage(minExtent, maxExtent);
}

function createItems(visData, itemFigure, minExtent, maxExtent, class_) {
    var oldItems = itemsContainer.selectAll(itemFigure + class_)
        .data(visData, function (block) {
            return block.id;
        })
        .attr('transform', function (block) {
            return getTranslate(block, selected_id, minExtent);
        })
        .attr('width', function (block) {
            return getItemWidth(block, minExtent, maxExtent);
        })
        .attr('height', function (block) {
            return getItemHeight(block);
        })
        .attr('stroke-opacity', function (block) {
            return getItemStrokeOpacity(block, selected_id);
        })
        .attr('stroke-width', function (block) {
            return getItemStrokeWidth(block, selected_id);
        })
        .attr('fill-opacity', function (block) {
            return getItemOpacity(block);
        })
        .attr('d', function(block) {
            if (block.misassembledEnds) return make_triangle(block);
        });
    oldItems.exit().remove();

    function make_triangle(block) {
        var isSelected = block.groupId == selected_id;
        var startX = isSelected ? (block.misassembledEnds == "L" ? 0.5 : -0.5) : 0;
        var startY = isSelected ? 1.5 : 0;
        if (block.misassembledEnds == "L")
            path = ['M', startX, startY, 'L', startX + (0.5 * (mainLanesHeight - startY) / 2),
                (startY + (mainLanesHeight - startY)) / 2, 'L', startX, mainLanesHeight - startY, 'L',  startX, startY].join(' ');
        if (block.misassembledEnds == "R")
            path = ['M', startX, startY, 'L', startX - (0.5 * (mainLanesHeight - startY) / 2),
                (startY + (mainLanesHeight - startY)) / 2, 'L', startX, mainLanesHeight - startY, 'L',  startX, startY].join(' ');
        return path;
    }

    var newItems = oldItems.enter().append(itemFigure)
        .attr('class', function (block) {
            if (block.misassembledEnds) {
                if (!block.objClass) block.objClass = 'misassembled';
                return 'block end ' + block.objClass;
            }
            if (!block.marks || block.contig_type)
                return 'block mainItem ' + block.objClass;
            else return 'block';
        })// Define the gradient
        .attr('fill', function (block) {
            if (block.marks && !block.contig_type)
                return addGradient(block, block.marks, true);
        })
        .attr('transform', function (block) {
            return getTranslate(block, selected_id, minExtent);
        })
        .attr('width', function (block) {
            return getItemWidth(block, minExtent, maxExtent);
        })
        .attr('height', function (block) {
            return getItemHeight(block);
        })
        .attr('stroke', 'black')
        .attr('stroke-opacity', function (block) {
            return getItemStrokeOpacity(block, selected_id);
        })
        .attr('stroke-width', function (block) {
            return getItemStrokeWidth(block, selected_id);
        })
        .attr('fill-opacity', function (block) {
            return getItemOpacity(block);
        })
        .attr('pointer-events', function (block) {
            return (block.misassembledEnds || block.notActive) ? 'none' : 'painted';
        })
        .attr('d', function(block) {
            if (block.misassembledEnds) return make_triangle(block);
        });
    return newItems;
}

function addLabels(visRects, minExtent, maxExtent) {
    var visibleLinesLabels = separatedLines.filter(function (line) {
        if (line.name && line.corr_start < maxExtent && line.corr_end > minExtent) return line;
        if (line.label) {
            var textSize = line.label.length * letterSize / 2;
            if (line.label && line.corr_end - textSize > minExtent && line.corr_end + textSize < maxExtent) return line;
        }
    });
    var prevX = 0;
    var prevLane = -1;
    var visTexts = visRects.filter(function (block) {
        if (!block.name) return;
        var textStart = getItemStart(block, minExtent);
        if (textStart - prevX > 20 || block.lane != prevLane) {
            var visWidth = getItemEnd(block, maxExtent) - textStart;
            if (visWidth > 20) {
                textLen = block.name.length * letterSize;
                prevX = textStart + Math.min(textLen, visWidth) - 30;
                prevLane = block.lane;
                if (block.marks) {
                    visibleLinesLabels.push({label: block.marks, lane: block.lane, corr_start: block.corr_start})
                }
                return block;
            }
        }
    });

    function checkItemSize(textItem) {
        return !textItem.size || textItem.size > minContigSize;
    }

    function getItemY(textItem) {
        var y = getYForExpandedLanes(textItem);
        //if (INTERLACE_BLOCKS_VERT_OFFSET) y += offsetsY[textItem.order % 3] * lanesInterval;
        return y + 20;
    }

    var texts = textLayer.selectAll('text')
        .data(visTexts, function (textItem) {
            return textItem.id;
        })
        .attr('x', function(textItem) {
            return getItemStart(textItem, minExtent) + 5;
        })
        .attr('y', function(textItem) {
            return getItemY(textItem)
        })
        .text(function(textItem) {
            if (checkItemSize(textItem))
                return getText(textItem, minExtent, maxExtent);
        });
    texts.exit().remove();

    var newTexts = texts.enter().append('text')
        .attr('class', 'itemLabel')
        .attr('x', function(textItem) {
            return getItemStart(textItem, minExtent) + 5;
        })
        .attr('y', function(textItem) {
            return getItemY(textItem)
        })
        .attr('pointer-events', 'none')
        .text(function(textItem) {
            if (checkItemSize(textItem))
                return getText(textItem, minExtent, maxExtent);
        });
    if (isContigSizePlot)
        getNumberOfContigs(d3.transform(d3.select('#countLine').attr("transform")).translate[0]);

    linesLabelsLayer.selectAll('.main_labels').remove();

    var visibleItemLabels = itemLabels.selectAll('.g')
        .data(visibleLinesLabels, function (labelItem) {
            return labelItem.id;
        });

    var labelOffset = Math.max(0, extraOffsetY - 3) + 8;
    var labels = visibleItemLabels.enter().append('g')
        .attr('class', 'main_labels')
        .attr('transform', function (labelItem) {
            var x = getItemStart(labelItem, minExtent) + 5;
            if (labelItem.lane != null) {
                y = y_main(labelItem.lane) + mainLanesHeight;
            }
            else y = getExpandedLanesHeight() + chrLabelsOffsetY;
            y += labelOffset;

            return 'translate(' + x + ', ' + y + ')';
        });
    labels.append('rect')
        .attr('class', 'main_labels')
        .attr('height', 15)
        .attr('transform', 'translate(0, -12)');

    var labelsText = labels.append('text')
        .text(function (labelItem) {
            return getText(labelItem, minExtent, maxExtent);
        })
        .attr('text-anchor', 'start');
    if (isContigSizePlot)
        labelsText.attr('class', 'lineLabelText');
    else labelsText.attr('class', 'itemText');
}

function updateMainCoverage(minExtent, maxExtent) {
    if (!minExtent)
        minExtent = Math.max(brush.extent()[0], x_mini.domain()[0]);
    if (!maxExtent)
        maxExtent = Math.min(brush.extent()[1], x_mini.domain()[1]);
    if (!physicalCoverageHidden)
        drawCoverageLine(minExtent, maxExtent, true, physical_coverage_data, '.phys_covered');
    if (!coverageMainHidden) {
        drawCoverageLine(minExtent, maxExtent, true, coverage_data, '.covered');
        drawCoverageLine(minExtent, maxExtent, true, gc_data, '.gc');
    }
    //main_cov.select('.y').call(y_cov_main_A);
}

function getNextCovValue(covData, startIdx, endIdx, maxValue, xScale, yScale, minExtent, maxExtent, startPos, plotFactor) {
    var coverage = covData[chromosome].slice(startIdx, endIdx);
    var covDots = coverage.length;
    if (covDots == 0) return;
    var coverageSum = coverage.reduce(function(pv, cv) { return pv + cv; }, 0);
    var avgCoverage = coverageSum / covDots;
    var yValue = Math.min(avgCoverage, maxValue);

    if (startIdx == startPos) start = minExtent;
    else start = startIdx * plotFactor;
    end = Math.min(endIdx * plotFactor, maxExtent);
    if (avgCoverage < 1) yValue = 0.1;
    return [xScale(start), yScale(yValue), xScale(end)];
}

function drawCoverageLine(minExtent, maxExtent, useMainCov, covData, plotClass) {
    var line = '',
        isGC = plotClass == '.gc',
        plotFactor = isGC ? gcFactor : coverageFactor,
        l = (maxExtent - minExtent) / plotFactor,
        cov_main_dots_amount = Math.min(maxCovDots, l),
        step = Math.round(l / cov_main_dots_amount);

    if (useMainCov) {
        track = main_cov;
        xScale = x_main;
        yScale = y_cov_main_S;
        maxValue = totalMaxYMain;
    }
    else {
        track = mini_cov;
        xScale = x_mini;
        yScale = y_cov_mini_S;
        maxValue = totalMaxYMini;
    }
    if (isGC) {
        maxValue = 100;
        yScale = d3.scale.linear().domain([maxValue, .1]).range([0, coverageHeight]);
    }

    var cov_lines = [];
    var startPos = Math.floor(minExtent / plotFactor / step) * step;
    var nextPos;
    var lastPos = maxExtent / plotFactor;
    for (var i = startPos; i < lastPos; i += step) {
        nextPos = i + step;
        cov_line = getNextCovValue(covData, i, nextPos, maxValue, xScale, yScale, minExtent, maxExtent, startPos, plotFactor);
        if (!cov_line) break;
        cov_lines.push(cov_line);
    }
    if (nextPos < lastPos) {
        cov_line = getNextCovValue(covData, nextPos, lastPos, maxValue, xScale, yScale, minExtent, maxExtent, startPos, plotFactor);
        cov_lines.push(cov_line);
    }

    var cov_line;
    var startY = isGC ? cov_lines[0][1] : yScale(0.1);
    line += ['M', cov_lines[0][0], startY].join(' ');
    for (i = 0; i < cov_lines.length; i++) {
        cov_line = cov_lines[i];
        if (cov_line) line += 'V' + ' ' + cov_line[1] + 'H' + ' ' + cov_line[2];
    }
    if (!isGC) line += ['V', yScale(0.1), 'Z'].join(' ');
    track.select(plotClass).select('path').attr('d', line);
}

function drawFeaturesMain(minExtent, maxExtent) {
    var featuresItems = featurePaths.filter(function (block) {
        if (block.corr_start < maxExtent && block.corr_end > minExtent) {
            var drawLimit = 0;
            var visibleLength = getItemWidth(block, minExtent, maxExtent);
            if (visibleLength > drawLimit)
                return block;
        }
    });
    var featureRects = featurePath.selectAll('g')
        .data(featuresItems, function (block) {
            return block.id;
        })
        .attr('transform', function (block) {
            var x = getItemStart(block, minExtent);
            var y = y_anno(block.lane) + .15 * featureHeight;
            //if (INTERLACE_BLOCKS_VERT_OFFSET) y += offsetsMiniY[block.order % 2] * featureHeight;
            return 'translate(' + x + ', ' + y + ')';
        });

    featureRects.select('.R')
        .attr('width', function (block) {
            return getItemWidth(block, minExtent, maxExtent);
        })
        .attr('height', featureHeight);
    featureRects.exit().remove();
    featurePath.selectAll('text')
        .remove();

    var otherFeatures = featureRects.enter().append('g')
        .attr('class', function (block) {
            return block.objClass;
        })
        .attr('transform', function (block) {
            var x = getItemStart(block, minExtent);
            var y = y_anno(block.lane) + .15 * featureHeight;
            // if (INTERLACE_BLOCKS_VERT_OFFSET) y += offsetsMiniY[block.order % 2] * featureHeight;

            return 'translate(' + x + ', ' + y + ')';
        });

    otherFeatures.append('rect')
        .attr('class', 'R')
        .attr('width', function (block) {
            return getItemWidth(block, minExtent, maxExtent);
        })
        .attr('height', featureHeight)
        .on('mouseenter', selectFeature)
        .on('mouseleave', deselectFeature)
        .on('click',  function(block) {
            addTooltip(block);
        });
    var visFeatureTexts = featuresItems.filter(function (block) {
        if (getItemWidth(block, minExtent, maxExtent) > 45) return block;
    });
    featurePath.selectAll('text')
        .data(visFeatureTexts, function (textItem) {
            return textItem.id;
        })
        .enter().append('text')
        .attr('fill', 'white')
        .attr('class', 'featureLabel')
        .style("font-size", "10px")
        .attr('x', function(textItem) {
            return getItemStart(textItem, minExtent) + 2;
        })
        .attr('y', function(textItem) {
            var y = y_anno(textItem.lane) + .15 * featureHeight;
            if (INTERLACE_BLOCKS_VERT_OFFSET) y += offsetsMiniY[textItem.order % 2] * featureHeight;
            return y + featureHeight / 2;
        })
        .text(function(textItem) {
            var w = getItemWidth(textItem, minExtent, maxExtent);
            return getVisibleText(textItem.name ? textItem.name : 'ID=' + textItem.id, w - 10);
        });
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
        .tickFormat(function(value) {
            return formatValue(value, tickValue);
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
        var pos = startPos + tickValue * domain;
        var currentLen = 0;
        if (!isContigSizePlot && chrContigs.length > 1) { // correct coordinates for chromosomes
            for (var i = 0; i < chrContigs.length; i++) {
                chrName = chrContigs[i];
                chrLen = chromosomes_len[chrName];
                if (currentLen + chrLen >= pos) {
                    pos -= currentLen;
                    break;
                }
                if (pos == 0)
                    pos = 1;
                currentLen += chrLen;
            }
        }
        return formatValue(pos, mainTickValue);
    });
    updateTrack(main);
    if (!featuresMainHidden) updateTrack(annotationsMain);
    if (!coverageMainHidden) updateTrack(main_cov);
}

function updateTrack(track) {
    track.select('.main.axis').call(xMainAxis);
    var lastTick = track.select(".axis").selectAll("g")[0].pop();
    var textSize = Math.max(0, (formatValue(x_main.domain()[1], mainTickValue).toString().length - 2) * numberSize);
    d3.select(lastTick).select('text').text(lastTick.textContent + ' ' + mainTickValue)
        .attr('transform', 'translate(-' + textSize + ', 0)');
}

function hideTrack(track, pane, doHide) {
    removeTooltip();
    var animationDuration = 200, transitionDelay = 150;
    var paneToHide, hideBtn, textToShow, newOffset;
    var changedTracks = [], changedBtns = [];
    var mainPane = (pane == 'main');

    function setBtnTopPos(btn) {
        if (!btn) return;
        btn.style.transition = 'all 0.2s';
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
            changedBtns = [hideBtnCoverageMain, hideBtnPhysicalCoverageMain, hideBtnGCMain, covMainControls,
                hideBtnAnnotationsMini, hideBtnCoverageMini, hideBtnPhysicalCoverageMini, hideBtnGCMini, covMiniControls];
        }
        else {
            changedTracks = [mini_cov];
            changedBtns = [hideBtnCoverageMini, hideBtnPhysicalCoverageMini, hideBtnGCMini, covMiniControls];
        }
    }
    else if (track == 'cov') {
        textToShow = 'Show read coverage and GC distribution';
        paneToHide = mainPane ? main_cov : mini_cov;
        hideBtn = mainPane ? hideBtnCoverageMain : hideBtnCoverageMini;
        hidePhysicalCovBtn = mainPane ? hideBtnPhysicalCoverageMain : hideBtnPhysicalCoverageMini;
        hideGCBtn = mainPane ? hideBtnGCMain : hideBtnGCMini;
        coverageBtns = mainPane ? covMainControls : covMiniControls;
        newOffset = coverageHeight;
        if (mainPane) {
            coverageMainHidden = doHide;
            changedTracks = [mini, annotationsMini, mini_cov];
            changedBtns = [hideBtnAnnotationsMini, hideBtnCoverageMini, hideBtnPhysicalCoverageMini, hideBtnGCMini, covMiniControls];
        }
        if (doHide) {
            hidePhysicalCovBtn.style.display = 'none';
            hideGCBtn.style.display = 'none';
            coverageBtns.style.display = 'none';
        }
        else {
            hidePhysicalCovBtn.style.display = '';
            hideGCBtn.style.display = '';
            coverageBtns.style.display = '';
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

function drawBrush(track, height, trackName) {
    var offsetY = 7;
    track.append('rect')
        .attr('pointer-events', 'painted')
        .attr('width', chartWidth)
        .attr('height', height)
        .attr('visibility', 'hidden')
        .on('mouseup', moveBrush);

    // draw the selection area
    var delta = (x_mini.domain()[1] - x_mini.domain()[0]) / 16;
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