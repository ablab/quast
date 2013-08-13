/*
  DT v1.0
  June 26, 2008
  Dan Vanderkam, http://danvk.org/DT/
                 http://code.google.com/p/DT/

  Instructions:
    - Download this file
    - Add <script src="DT.js"></script> to your HTML.
    - Add class="draggable" to any table you might like to reorder.
    - Drag the headers around to reorder them.

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
  isLocally: window.location.protocol == 'file:',
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

    // Replay drags from cookies if there are any.
    if (DT.isLocally && navigator.cookieEnabled && table.id) {
      DT.replayDrags(table);
    }
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
    var new_table = fullCopy(table, false);

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
      var new_sec = fullCopy(sec, false);

      forEach(sec.rows, function(row) {
        var cell = row.cells[col_index];
        var new_tr = fullCopy(row, false);
        if (row.offsetHeight) new_tr.style.height = row.offsetHeight + "px";
        var new_td = fullCopy(cell, true);
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
      $(getColumn(drag_obj.newTable)).addClass(classes.cell_of_movingTable);

      $(getColumn(drag_obj.table, col_index)).addClass(classes.cell_of_emptySpace);
      $(getColumn(drag_obj.table, col_index)[0]).addClass(classes.topCell_of_emptySpace);
      var visibleCells = $(getColumn(drag_obj.table, col_index)).filter(':visible');
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
    var column_pos_x = DT.getPositionOnThePage(getColumn(drag_obj.newTable)[0]).x;
    var column_center_x = column_pos_x + drag_obj.movingColWidth / 2;
    var hovered_col_index = DT.getColumnAtPosition(drag_obj.table, column_center_x);

    var dx = drag_obj.prevPos.x - pos.x;

    if (DT.isSuitableColumnToSwap(hovered_col_index, col_index)) {
      if (dx < 0 && hovered_col_index > col_index ||  // moving right
          dx > 0 && hovered_col_index < col_index) {  // moving left
        DT.removeStylesForAdjacentColumns(col_index);
        moveColumn(drag_obj.table, col_index, hovered_col_index);
        DT.addStylesForAdjacentColumns(hovered_col_index);
        drag_obj.movingColIndex = hovered_col_index;
      }
    }
    drag_obj.prevPos = pos;
  },

  removeStylesForAdjacentColumns: function(oldIndex) {
    var table = DT.dragObj.table;
    var rows = table.rows;
    var oldColumn = getColumn(table, oldIndex);
    $(getColumn(table, oldIndex - 1)).removeClass(classes.leftToCell_of_emptySpace);
    $(getColumn(table, oldIndex + 1)).removeClass(classes.rightToCell_of_emptySpace);
  },

  addStylesForAdjacentColumns: function(index) {
    var table = DT.dragObj.table;
    var rows = table.rows;
    var column = getColumn(table, index);
    $(getColumn(table, index - 1)).addClass(classes.leftToCell_of_emptySpace);
    $(getColumn(table, index + 1)).addClass(classes.rightToCell_of_emptySpace);
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
      var final_pos = DT.getPositionOnThePage(getColumn(drag_obj.table, col_index)[0]);

      $(drag_obj.newTable).animate({
        left: final_pos.x,
        top: final_pos.y,
      }, 'fast', function() {
        $(drag_obj.newTable).remove();
        $(getColumn(drag_obj.table, col_index)).removeClass(classes.cell_of_emptySpace);
        $(getColumn(drag_obj.table, col_index)[0]).removeClass(classes.topCell_of_emptySpace);
        var visibleCells = $(getColumn(drag_obj.table, col_index)).filter(':visible');
        $(visibleCells[visibleCells.length - 1]).removeClass(classes.bottomCell_of_emptySpace);

        DT.removeStylesForAdjacentColumns(col_index);
      });

      // updateReportJson(drag_obj.table, drag_obj.startIndex, drag_obj.movingColIndex);

      DT.rememberDrag(drag_obj.table, drag_obj.startIndex, col_index);
    }

    DT.isDragging = false;
  },

  isSuitableColumnToSwap: function(targetIndex, movingColIndex) {
    if (targetIndex != -1) {
      if (typeof movingColIndex === 'undefined' || targetIndex != movingColIndex) {  // check if equals the moving one
        var top_cell = $(getColumn(DT.dragObj.table, targetIndex)[0]);
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
  rememberDrag: function(table, startIndex, finishIndex) {
    if (DT.isLocally) {
      if (table.id && navigator.cookieEnabled) {
        var cookieName = 'DT-' + table.id;
        var prev = readCookie(cookieName);
        var new_val = '';
        if (prev) {
          new_val = prev + ',';
        }
        new_val += startIndex + '/' + finishIndex;
        createCookie(cookieName, new_val, DT.cookieDays);
      }
    } else {
      if (document.reportId) {
        var asm_names = [];
        forEach(table.rows[0].cells, function(cell) {
          var asm_name = $(cell).find('.' + classes.assemblyName).html();
          if (asm_name) {
            asm_names.push(asm_name);
          }
        });

        var asm_names_string = asm_names[0];
        for (var i = 1; i < asm_names.length; i++) {
            asm_names_string += ' ' + asm_names[i];
        }

        $.ajax({
          type: 'GET',
          url: '/reorder-report-columns',
          dataType: 'json',
          data: {
            reportId: document.reportId,
            assemblyNames: asm_names_string
          }
        });
      }
    }
  },

  replayDrags: function(table) {
    if (!table.id || !navigator.cookieEnabled)
      return;

    var drag_str = readCookie("dragtable-" + table.id);
    if (!drag_str)
      return;

    var drags = drag_str.split(',');

    for (var i = 0; i < drags.length; i++) {
      var pair = drags[i].split('/');
      if (pair.length != 2)
        continue;

      var startIndex = parseInt(pair[0]);
      var finishIndex = parseInt(pair[1]);
      if (isNaN(startIndex) || isNaN(finishIndex))
        continue;

      moveColumn(table, startIndex, finishIndex);
    }
  },
};

var DT = dragTable;

/* ******************************************************************
   Supporting functions: bundled here to avoid depending on a library
   ****************************************************************** */

// Cookie functions based on http://www.quirksmode.org/js/cookies.html
// Cookies won't work for local files.

var moveColumn = function(table, index, nextIndex) {
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
};

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

// clone an element, copying its style and class.
var fullCopy = function(elt, deep) {
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
};

var getColumn = function(table, index) {
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
};

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
