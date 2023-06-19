const fs = require("fs");
const { pluginsConfig } = require("../config");
const { runDCM } = require("./executeDcm");
const configData = pluginsConfig.wlmscpfs4raccoon;
const dicomFilePath = configData.storepath;

module.exports = function(req, res) {
}    

if(configData.enable){
    // Start services.
    runDCM();
}