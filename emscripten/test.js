var colorist = require("./colorist");
colorist.ready = function() {
    go();
}

function go()
{
    var fs = require("fs");

    colorist.FS.writeFile("/orange.jpg", fs.readFileSync("orange.jpg"));
    colorist.execute("report /orange.jpg /orange.html".split(" "));
    fs.writeFileSync("orange.html", colorist.FS.readFile("/orange.html"));
    colorist.FS.unlink("/orange.jpg");
    colorist.FS.unlink("/orange.html");
}
