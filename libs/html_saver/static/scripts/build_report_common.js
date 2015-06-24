
function recoverOrderFromCookies() {
    if (!navigator.cookieEnabled)
        return null;

    var order_string = readCookie("order");
    if (!order_string)
        return null;

    var order = [];
    var fail = false;
    forEach(order_string.split(' '), function(val) {
        val = parseInt(val);
        if (isNaN(val))
            fail = true;
        else
            order.push(val);
    });

    if (fail)
        return null;

    return order;
}


function readJson(what) {
    var result;
    try {
        result = JSON.parse($('#' + what + '-json').html());
    } catch (e) {
        result = null;
    }
    return result;
}
