/*
 DT v1.0
 June 26, 2008
 Dan Vanderkam, http://danvk.org/DT/
 http://code.google.com/p/DT/

 This is code was based on:
 - Stuart Langridge's SortTable (kryogenix.org/code/browser/sorttable)
 - Mike Hall's draggable class (http://www.brainjar.com/dhtml/drag/)
 - A discussion of permuting table columns on comp.lang.javascript

 Licensed under the MIT license.
 */

// Here's the notice from Mike Hall's draggable script:
//*****************************************************************************
// Do not remove this notice.
//
// Copyright 2001 by Mike Hall.
// See http://www.brainjar.com for terms of use.
//*****************************************************************************

var classes = {
    cell_of_emptySpace: 'cell_of_empty_space',
    cell_of_movingTable: 'cell_of_moving_table',
    newTable: 'moving_table',
    handle: 'drag_handle',
    hoveredCell: 'hovered_column_td',
    assemblyName: 'assembly_name',

    bottomCell_of_emptySpace: 'bottom_cell_of_empty_space',
    topCell_of_emptySpace: 'top_cell_of_empty_space',
    leftToCell_of_emptySpace: 'left_to_cell_of_empty_space',
    rightToCell_of_emptySpace: 'right_to_cell_of_empty_space',

    mainReportTable_id: 'main_report_table',
    totalReportJson_id: 'total-report-json',
};

var dragTable = {
    isDragging: false,

    // How far should the mouse move before it's considered a drag, not a click?
    dragRadius2: 0, // 100,
    setMinDragDistance: function(x) {
        DT.dragRadius2 = x * x;
    },

    // How long should cookies persist? (in days)
    cookieDays: 365,
    setCookieDays: function(x) {
        DT.cookieDays = x;
    },

    // Determine browser and version.
    // TODO: eliminate browser sniffing except where it's really necessary.
    Browser: function() {
        var ua, s, i;

        this.isIE    = false;
        this.isNS    = false;
        this.version = null;
        ua = navigator.userAgent;

        s = "MSIE";
        if ((i = ua.indexOf(s)) >= 0) {
            this.isIE = true;
            this.version = parseFloat(ua.substr(i + s.length));
            return;
        }

        s = "Netscape6/";
        if ((i = ua.indexOf(s)) >= 0) {
            this.isNS = true;
            this.version = parseFloat(ua.substr(i + s.length));
            return;
        }

        // Treat any other "Gecko" browser as NS 6.1.
        s = "Gecko";
        if ((i = ua.indexOf(s)) >= 0) {
            this.isNS = true;
            this.version = 6.1;
            return;
        }
    },
    browser: null,

    // Detect all draggable tables and attach handlers to their headers.
    init: function() {
        // Don't initialize twice
        if (arguments.callee.done)
            return;
        else
            arguments.callee.done = true;

//    if (_dgtimer)
//      clearInterval(_dgtimer);

        if (!document.createElement || !document.getElementsByTagName)
            return;

        DT.dragObj.zIndex = 0;
        DT.browser = new DT.Browser();
        forEach(document.getElementsByTagName('table'), function(table) {
            if ($(table).hasClass('draggable')) {
                DT.makeDraggable(table);
            }
        });
    },

    // The thead business is taken straight from sorttable.
    makeDraggable: function(table) {
        DT.dragObj.table = table;

        if (table.getElementsByTagName('thead').length == 0) {
            thead = document.createElement('thead');
            thead.appendChild(table.rows[0]);
            table.insertBefore(thead, table.firstChild);
        }

        // Safari doesn't support table.tHead, sigh
        if (table.tHead == null) {
            table.tHead = table.getElementsByTagName('thead')[0];
        }

        var handles = $('.' + classes.handle) || table.tHead.rows[0].cells;

        forEach(handles, function(handle) {
            handle.onmousedown = DT.dragStart;
        });
    },

    // Global object to hold drag information.
    dragObj: {},

    getEventPosition: function(event) {
        var x, y;
        if (DT.browser.isIE) {
            x = window.event.clientX + document.documentElement.scrollLeft
                + document.body.scrollLeft;
            y = window.event.clientY + document.documentElement.scrollTop
                + document.body.scrollTop;
            return {x: x, y: y};
        }
        return {x: event.pageX, y: event.pageY};
    },

    // Determine the position of this element on the page. Many thanks to Magnus
    // Kristiansen for help making this work with "position: fixed" elements.
    getPositionOnThePage: function(elt, stopAtRelative) {
        var ex = 0, ey = 0;
        do {
            var curStyle = DT.browser.isIE ? elt.currentStyle
                : window.getComputedStyle(elt, '');
            var supportFixed = !(DT.browser.isIE &&
                DT.browser.version < 7);
            if (stopAtRelative && curStyle.position == 'relative') {
                break;
            } else if (supportFixed && curStyle.position == 'fixed') {
                // Get the fixed el's offset
                ex += parseInt(curStyle.left, 10);
                ey += parseInt(curStyle.top, 10);
                // Compensate for scrolling
                ex += document.body.scrollLeft;
                ey += document.body.scrollTop;
                // End the loop
                break;
            } else {
                ex += elt.offsetLeft;
                ey += elt.offsetTop;
            }
        } while (elt = elt.offsetParent);
        return {x: ex, y: ey};
    },

    // MouseDown handler -- sets up the appropriate mousemove/mouseup handlers
    // and fills in the global DT.dragObj object.
    dragStart: function(event, id) {
        DT.isDragging = true;

        var drag_obj = DT.dragObj;

        var browser = DT.browser;
        var clicked_node = browser.isIE ? window.event.srcElement : event.target;
        var pos = DT.getEventPosition(event);

        var table = $(clicked_node).closest('table')[0];
        var top_cell = $(clicked_node).closest('td, th')[0];
        drag_obj.topCell = top_cell;
        drag_obj.table = table;

        var col_index = top_cell.cellIndex;
        drag_obj.movingColIndex = drag_obj.startIndex = col_index;
        drag_obj.movingColWidth = $(top_cell).width();

        // Since a column header can't be dragged directly, duplicate its contents
        // in a div and drag that instead.
        var new_table = DT.fullCopy(table, false);

        // Copy the entire column
        var forEachSection = function(table, func) {
            if (table.tHead) {
                func(table.tHead);
            }
            forEach(table.tBodies, function(tbody) {
                func(tbody);
            });
            if (table.tFoot) {
                func(table.tFoot);
            }
        };

        forEachSection(table, function(sec) {
            var new_sec = DT.fullCopy(sec, false);

            forEach(sec.rows, function(row) {
                var cell = row.cells[col_index];
                var new_tr = DT.fullCopy(row, false);
                if (row.offsetHeight) new_tr.style.height = row.offsetHeight + "px";
                var new_td = DT.fullCopy(cell, true);
                if (cell.offsetWidth) new_td.style.width = cell.offsetWidth + "px !important";
                new_tr.appendChild(new_td);
                new_sec.appendChild(new_tr);
            });

            new_table.appendChild(new_sec);
        });

        var obj_pos = DT.getPositionOnThePage(top_cell, true);
        new_table.style.position = "absolute";
        new_table.style.left = obj_pos.x + "px";
        new_table.style.top = obj_pos.y + "px";
        // new_table.style.width = dragObj.origNode.offsetWidth + "px";
        // new_table.style.height = dragObj.origNode.offsetHeight + "px";

        // Hold off adding the element until this is clearly a drag.
        drag_obj.addedNode = false;

        drag_obj.tableContainer = drag_obj.table.parentNode || document.body;
        drag_obj.newTable = new_table;

        // Save starting positions of cursor and element.
        drag_obj.startPos = pos;
        drag_obj.prevPos = pos;
        drag_obj.elStartLeft = parseInt(drag_obj.newTable.style.left, 10);
        drag_obj.elStartTop = parseInt(drag_obj.newTable.style.top,  10);

        if (isNaN(drag_obj.elStartLeft)) drag_obj.elStartLeft = 0;
        if (isNaN(drag_obj.elStartTop))  drag_obj.elStartTop  = 0;

        // Update element's z-index.
        drag_obj.newTable.style.zIndex = ++drag_obj.zIndex;

        // Capture mousemove and mouseup events on the page.
        if (browser.isIE) {
            document.attachEvent("onmousemove", DT.dragMove);
            document.attachEvent("onmouseup",   DT.dragEnd);
            window.event.cancelBubble = true;
            window.event.returnValue = false;
        } else {
            document.addEventListener("mousemove", DT.dragMove, true);
            document.addEventListener("mouseup",   DT.dragEnd, true);
            event.preventDefault();
        }
    },

    // Move the floating column header with the mouse
    // TODO: Reorder columns as the mouse moves for a more interactive feel.
    dragMove: function(event) {
        var x, y;
        var drag_obj = DT.dragObj;
        var col_index = drag_obj.movingColIndex;

        // Get cursor position with respect to the page.
        var pos = DT.getEventPosition(event);

        var full_dx = drag_obj.startPos.x - pos.x;
        var full_dy = drag_obj.startPos.y - pos.y;

        if (!drag_obj.addedNode && full_dx * full_dx + full_dy * full_dy > DT.dragRadius2) {
            // Real dragging has began, the following block performs once

            // Attach a moving table
            drag_obj.tableContainer.insertBefore(drag_obj.newTable, drag_obj.table);
            drag_obj.addedNode = true;

            // Add classes for the dragged column (original and the clone)
            $(drag_obj.newTable).addClass(classes.newTable);
            $(DT.getColumn(drag_obj.newTable)).addClass(classes.cell_of_movingTable);

            $(DT.getColumn(drag_obj.table, col_index)).addClass(classes.cell_of_emptySpace);
            $(DT.getColumn(drag_obj.table, col_index)[0]).addClass(classes.topCell_of_emptySpace);
            var visibleCells = $(DT.getColumn(drag_obj.table, col_index)).filter(':visible');
            $(visibleCells[visibleCells.length - 1]).addClass(classes.bottomCell_of_emptySpace);

            DT.addStylesForAdjacentColumns(col_index);
        }

        // Move drag element by the same amount the cursor has moved.
        var style = drag_obj.newTable.style;
        style.left = (drag_obj.elStartLeft + pos.x - drag_obj.startPos.x) + "px";
        style.top  = (drag_obj.elStartTop  + pos.y - drag_obj.startPos.y) + "px";

        if (DT.browser.isIE) {
            window.event.cancelBubble = true;
            window.event.returnValue = false;
        } else {
            event.preventDefault();
        }

        // If moving over an adjacent column, do swapping, swapping with the adjacent column
        var column_pos_x = DT.getPositionOnThePage(DT.getColumn(drag_obj.newTable)[0]).x;
        var column_center_x = column_pos_x + drag_obj.movingColWidth / 2;
        var hovered_col_index = DT.getColumnAtPosition(drag_obj.table, column_center_x);

        var dx = drag_obj.prevPos.x - pos.x;

        if (DT.isSuitableColumnToSwap(hovered_col_index, col_index)) {
            if (dx < 0 && hovered_col_index > col_index ||  // moving right
                dx > 0 && hovered_col_index < col_index) {  // moving left
                DT.removeStylesForAdjacentColumns(col_index);
                DT.moveColumn(drag_obj.table, col_index, hovered_col_index);
                DT.addStylesForAdjacentColumns(hovered_col_index);
                drag_obj.movingColIndex = hovered_col_index;
            }
        }
        drag_obj.prevPos = pos;
    },

    removeStylesForAdjacentColumns: function(oldIndex) {
        var table = DT.dragObj.table;
        var rows = table.rows;
        var oldColumn = DT.getColumn(table, oldIndex);
        $(DT.getColumn(table, oldIndex - 1)).removeClass(classes.leftToCell_of_emptySpace);
        $(DT.getColumn(table, oldIndex + 1)).removeClass(classes.rightToCell_of_emptySpace);
    },

    addStylesForAdjacentColumns: function(index) {
        var table = DT.dragObj.table;
        var rows = table.rows;
        var column = DT.getColumn(table, index);
        $(DT.getColumn(table, index - 1)).addClass(classes.leftToCell_of_emptySpace);
        $(DT.getColumn(table, index + 1)).addClass(classes.rightToCell_of_emptySpace);
    },

    // Stop capturing mousemove and mouseup events.
    // Determine which (if any) column we're over and shuffle the table.
    dragEnd: function(event) {
        if (DT.browser.isIE) {
            document.detachEvent("onmousemove", DT.dragMove);
            document.detachEvent("onmouseup", DT.dragEnd);
        } else {
            document.removeEventListener("mousemove", DT.dragMove, true);
            document.removeEventListener("mouseup", DT.dragEnd, true);
        }

        var drag_obj = DT.dragObj;
        var col_index = drag_obj.movingColIndex;

        // If the floating header wasn't added, the mouse didn't move far enough.
        if (drag_obj.addedNode) {
            // TODO: Move with animation
            var final_pos = DT.getPositionOnThePage(DT.getColumn(drag_obj.table, col_index)[0]);

            $(drag_obj.newTable).animate({
                left: final_pos.x,
                top: final_pos.y,
            }, 'fast', function() {
                $(drag_obj.newTable).remove();
                $(DT.getColumn(drag_obj.table, col_index)).removeClass(classes.cell_of_emptySpace);
                $(DT.getColumn(drag_obj.table, col_index)[0]).removeClass(classes.topCell_of_emptySpace);
                var visibleCells = $(DT.getColumn(drag_obj.table, col_index)).filter(':visible');
                $(visibleCells[visibleCells.length - 1]).removeClass(classes.bottomCell_of_emptySpace);

                DT.removeStylesForAdjacentColumns(col_index);
            });

            DT.storeDrag(drag_obj.table);
        }
        DT.isDragging = false;
    },

    moveColumn: function(table, index, nextIndex) {
        forEach(table.rows, function(row) {
            var startCell = $(row.cells[index]);
            var finishCell = $(row.cells[nextIndex]);

            // startCell.animate({
            // left: finishCell.offset().left,
            // }, 'slow', function() {
            if (nextIndex > index) {
                startCell.before(finishCell);
            } else {
                startCell.after(finishCell);
            }
            // });
        });
    },

    // clone an element, copying its style and class.
    fullCopy: function(elt, deep) {
        if (!elt) {
            return null;
        }
        var new_elt = elt.cloneNode(deep);
        new_elt.className = elt.className;
        $(new_elt).addClass('clone');
        forEach(elt.style, function(value, key, object) {
            if (value == null) return;
            if (typeof(value) == "string" && value.length == 0) return;

            new_elt.style[key] = elt.style[key];
        });
        return new_elt;
    },

    getColumn: function(table, index) {
        index = typeof index !== 'undefined' ? index : 0;

        if (index < 0 || index >= table.rows[0].cells.length) {
            return [];

        } else {
            var column = [];
            forEach(table.rows, function(row) {
                column.push(row.cells[index]);
            });
            return column;
        }
    },

    isSuitableColumnToSwap: function(targetIndex, movingColIndex) {
        if (targetIndex != -1) {
            if (typeof movingColIndex === 'undefined' || targetIndex != movingColIndex) {  // check if equals the moving one
                var top_cell = $(DT.getColumn(DT.dragObj.table, targetIndex)[0]);
                if (top_cell.find('.' + classes.handle).addBack('.' + classes.handle).exists()) {
                    return true;
                }
            }
        }
        return false;
    },

    // Which column does the x value fall inside of? x should include scrollLeft.
    getColumnAtPosition: function(table, x) {
        var header = table.tHead.rows[0].cells;
        for (var i = 0; i < header.length; i++) {
            //var left = header[i].offsetLeft;
            var pos = DT.getPositionOnThePage(header[i]);
            //if (left <= x && x <= left + header[i].offsetWidth) {
            var px = pos.x;
            var ow = header[i].offsetWidth;
            var xpow = pos.x + header[i].offsetWidth;
            if (pos.x <= x && x <= pos.x + header[i].offsetWidth) {
                return i;
            }
        }
        return -1;
    },

    // Store a column swap in a cookie for posterity.
    storeDrag: function(table) {
        var order = [];
        forEach(table.rows[0].cells, function(cell) {
            var pos = cell.getAttribute('position');
            if (pos)
                order.push(pos);
        });

        var order_string = order[0];
        for (var i = 1; i < order.length; i++)
            order_string += ' ' + order[i];

        DT.storeInCookies(order_string);
        DT.sendOrderToServer(order_string);
    },

    storeInCookies: function(string) {
        if (navigator.cookieEnabled) {
            var cookieName = 'order';
            createCookie(cookieName, string, DT.cookieDays);
        }
    },

    sendOrderToServer: function(string) {
        if (document.reportId) {
            $.ajax({
                type: 'GET',
                url: '/reorder-report-columns',
                dataType: 'json',
                data: {
                    reportId: document.reportId,
                    order: string,
                },
            });
        }
    },
};

var DT = dragTable;

/* ******************************************************************
 Supporting functions: bundled here to avoid depending on a library
 ****************************************************************** */


//// Dean Edwards/Matthias Miller/John Resig
//// has a hook for DT.init already been added? (see below)
//var dgListenOnLoad = false;
//
///* for Mozilla/Opera9 */
//if (document.addEventListener) {
//  dgListenOnLoad = true;
//  document.addEventListener("DOMContentLoaded", DT.init, false);
//}
//
///* for Internet Explorer */
///*@cc_on @*/
///*@if (@_win32)
//  dgListenOnLoad = true;
//  document.write("<script id=__dt_onload defer src=//0)><\/script>");
//  var script = document.getElementById("__dt_onload");
//  script.onreadystatechange = function() {
//    if (this.readyState == "complete") {
//      DT.init(); // call the onload handler
//    }
//  };
///*@end @*/
//
///* for Safari */
//if (/WebKit/i.test(navigator.userAgent)) { // sniff
//  dgListenOnLoad = true;
//  var _dgtimer = setInterval(function() {
//    if (/loaded|complete/.test(document.readyState)) {
//      DT.init(); // call the onload handler
//    }
//  }, 10);
//}
//
///* for other browsers */
///* Avoid this unless it's absolutely necessary (it breaks sorttable) */
//if (!dgListenOnLoad) {
//  window.onload = DT.init;
//}
