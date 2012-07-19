
function toPrettyString(num) {
    if (num < 10000) {
        return num.toString();
    } else {
        return num.toString().replace(/(\d)(?=(\d\d\d)+(?!\d))/g,
            '<span style="word-spacing:-2px;">$1 </span>');
    }
}