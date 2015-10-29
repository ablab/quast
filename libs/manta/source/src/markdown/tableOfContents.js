/// derived from stackoverflow post by Ates Goral
///
window.onload = function () {
    var toc = "<h2>Table of Contents</h2>";
    var level = 0;
    var isFirst = 1;
    var maxLevel = 3;
    
    document.body.innerHTML =
        document.body.innerHTML.replace(
            /<h([\d])[^>]+>([^<]+)<\/h([\d])>/gi,
            function (str, openLevel, titleText, closeLevel) {
                if (openLevel != closeLevel) {
                    return str;
                }
                
                // skip h1 headers:
                if (openLevel == 1) { return str; }

                retval = "";
                if (isFirst == 1)
                {
                	isFirst = 0;
                	/// put the toc above the first non-h1 header:
                	retval += "<div id='toc'></div>\n";
                }
                
                if (openLevel > maxLevel) { return str; }
                
                if (openLevel > level) {
                    toc += (new Array(openLevel - level + 1)).join("<ul>");
                } else if (openLevel < level) {
                    toc += (new Array(level - openLevel + 1)).join("</ul>");
                }

                level = parseInt(openLevel);

                var anchor = titleText.replace(/ /g, "_");
                toc += "<li><a href=\"#" + anchor + "\">" + titleText
                    + "</a></li>";
                
                return retval + "<h" + openLevel + "><a name=\"" + anchor + "\"></a>"
                    + titleText + "</h" + closeLevel + ">";
            }
        );

    if (level) {
        toc += (new Array(level + 1)).join("</ul>");
    }

    document.getElementById("toc").innerHTML += toc;
};
