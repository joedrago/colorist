Module['onRuntimeInitialized'] = function() {
    if(Module['ready']) {
        setTimeout(function() {
            Module['ready']();
        }, 0);
    }
    Module.onExecuteFinished = null;
};

Module.coloristLog = function(section, indent, text) {
    console.log("["+section+":"+indent+"] " + text);
}

Module.coloristError = function(text) {
    console.log("ERROR: " + text);
}

Module.execute = function(args, cb)
{
    var strArr = ["colorist"];
    strArr = strArr.concat(args);
    var ptrArr = Module._malloc(strArr.length * 4);
    var tofree = [ptrArr];
    for (var i = 0; i < strArr.length; i++) {
        var len = strArr[i].length + 1;
        var ptr = Module._malloc(len);
        tofree.push(ptr);
        Module.stringToUTF8(strArr[i], ptr, len);
        Module.setValue(ptrArr + i * 4, ptr, "i32");
    }
    Module.onExecuteFinished = function() {
        for(var i = 0; i < tofree.length; i++) {
            Module._free(tofree[i]);
        }
        Module.onExecuteFinished = null;
        if(cb) {
            setTimeout(function() {
                cb();
            }, 0);
        }
    }
    Module._execute(strArr.length, ptrArr);
}
