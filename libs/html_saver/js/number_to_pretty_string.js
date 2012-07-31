
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
        return '<span style="word-spacing:-1px;">' + num.toString() + '</span>';
    } else {
        return '<span style="word-spacing: -1px;">' +
                    num.toString().replace(/(\d)(?=(\d\d\d)+(?!\d))/g, '$1&nbsp;')
                    + '&nbsp;' + dimension +
               '</span>';
    }
}

