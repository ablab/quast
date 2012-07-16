

function toPrettyString(num) {
    if (num < 10000) {
        return num.toString();
    } else {
        return num.toString().replace(/(\d)(?=(\d\d\d)+(?!\d))/g,
            '$1<span style="font-size: 0.7em"> </span>');
    }
}