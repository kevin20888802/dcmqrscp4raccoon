const fs = require("fs");
const { pluginsConfig } = require("../config");
const { runDCM } = require("./executeDcm");
const configData = pluginsConfig.dcm4raccoon;
const dicomFilePath = configData.storepath;

module.exports = function(req, res) {
}    

if(configData.enable){
    // Create folder for dicom files.
    if(!fs.existsSync(dicomFilePath)) {
        console.log(`[dcm4raccoon] File temp folder (${dicomFilePath}) not exist, creating.`);
        fs.mkdirSync(dicomFilePath);
    }
    fs.chmodSync(dicomFilePath,0777);
    
    // Start services.
    runDCM();
}