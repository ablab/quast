
function toPrettyString(num) {
    if (num < 10000) {
        return num.toString();
    } else {
        return '<span style="word-spacing: -1px;">' +
                    num.toString().replace(/(\d)(?=(\d\d\d)+(?!\d))/g,'$1&nbsp;') +
               '</span>';
    }
}


function toPrettyStringWithDimencion(num, dimension) {
    if (num < 10000) {
        return '<span style="word-spacing:-1px;">' + num.toString() + '&nbsp;' + dimension + '</span>';
    } else {
        return '<span style="word-spacing: -1px;">' +
                    num.toString().replace(/(\d)(?=(\d\d\d)+(?!\d))/g, '$1&nbsp;')
                    + '&nbsp;' + dimension +
               '</span>';
    }
}

function myToFixed(num) {
    if (num % 1 != 0) {
        return num.toFixed(1);
    } else {
        return num.toFixed(0);
    }
}

function getBpTickFormatter(maxY) {
    return function(val, axis) {
        var res;

        if (val == 0) {
            res = 0;

        } else if (val >= 1000000) {
            res = val / 1000000;
            res = myToFixed(res);

            if (val > maxY + 1 || val + axis.tickSize >= 1000000000) {
                res = res + ' Mbp';
            }
        } else if (val >= 1000) {
            res = val / 1000;
            res = myToFixed(res);

            if (val > maxY + 1 || val + axis.tickSize >= 1000000) {
                res = res + ' Kbp';
            }
        } else if (val >= 1) {
            res = myToFixed(val);

            if (val > maxY + 1 || val + axis.tickSize >= 1000) {
                res = res + ' bp';
            }
        }
        return '<span style="word-spacing: -1px;">' + res + '</span>';
    }
}

function getContigNumberTickFormatter(maxX) {
    return function (val, axis) {
        if (typeof axis.tickSize == 'number' && val > maxX - axis.tickSize) {
            var valStr = val.toString();
            var lastDigit = valStr[valStr.length-1];
            var beforeLastDigit = valStr[valStr.length-2];

            var res = val + "'th";

            if (lastDigit == '1' && beforeLastDigit != '1') {
                res = val + "'st";
            } else if (lastDigit == '2' && beforeLastDigit != '1') {
                res = val + "'nd";
            } else if (lastDigit == '3' && beforeLastDigit != '1') {
                res = val + "'rd";
            }
            return "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" + res + "&nbsp;contig";
        }
        return val;
    }
}