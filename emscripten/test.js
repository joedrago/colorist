var colorist = require("./colorist");
colorist['onRuntimeInitialized'] = function() {
    go();
};

colorist.coloristLog = function(section, indent, text) {
    console.log("["+section+":"+indent+"] " + text);
}
colorist.coloristError = function(text) {
    console.log("ERROR: " + text);
}

function callColorist(args)
{
    var strArr = ["colorist"];
    strArr = strArr.concat(args);
    var ptrArr = colorist._malloc(strArr.length * 4);
    var tofree = [ptrArr];
    for (var i = 0; i < strArr.length; i++) {
        var len = strArr[i].length + 1;
        var ptr = colorist._malloc(len);
        tofree.push(ptr);
        colorist.stringToUTF8(strArr[i], ptr, len);
        colorist.setValue(ptrArr + i * 4, ptr, "i32");
    }
    var rst = colorist._execute(strArr.length, ptrArr);
    for(var i = 0; i < tofree.length; i++) {
        colorist._free(tofree[i]);
    }
}

function go()
{
    var fs = require("fs");

    colorist.FS.writeFile("/orange.jpg", fs.readFileSync("orange.jpg"));
    callColorist("report /orange.jpg /orange.html".split(" "));
    fs.writeFileSync("orange.html", colorist.FS.readFile("/orange.html"));
    colorist.FS.unlink("/orange.jpg");
    colorist.FS.unlink("/orange.html");
}
