/**
 * wlmscpfs service for raccoon.
 */
const fs = require('fs');
const path = require("path");
const { spawn } = require('node:child_process');
const { exec } = require('child_process');
const { raccoonConfig } = require("../../config-class");
const { pluginsConfig } = require("../config");
const configData = pluginsConfig.wlmscpfs4raccoon;
const dicomFilePath = configData.storepath;
const maxRetryTimes = 3;
let retryTimes = 0;
let isShuttingDown = false;

module.exports.runDCM = async function () {
    executeDCM();
}

// 將wlmscpfs的MongoDB設定調整成與raccoon相同
function updateDcmqrscpMongoConfig() {
    let conn_string = `mongodb://${raccoonConfig.mongoDbConfig.hosts[0]}/?appname=wlmscpfs4raccoon`;
    let mongoDB_name = `${configData.dbName}`;
    let collection_name = `${configData.collectionName}`;
    let DICOM_STORE_ROOTPATH = `${path.resolve(raccoonConfig.dicomWebConfig.storeRootPath)}`;
    let _w = `conn_string=${conn_string}
mongoDB_name=${mongoDB_name}
collection_name=${collection_name}
DICOM_STORE_ROOTPATH=${DICOM_STORE_ROOTPATH}`;
    fs.writeFileSync("./plugins/wlmscpfs4raccoon/dcmtk/wlmscpfsMongoConfig.cfg", _w);
}

function checkProcessRunning(processName) {
    return new Promise((resolve, reject) => {
      let command = '';
      if (process.platform === 'win32') {
        command = `tasklist /FI "IMAGENAME eq ${processName}" /NH`;
      } else {
        command = `pgrep ${processName}`;
      }
  
      exec(command, (error, stdout) => {
        if (error) {
          if(process.platform === 'win32') {
            reject(error);
            return;
          } else {
            resolve(false);
          }
        }
  
        const output = stdout.toString().trim();
        const isRunning = process.platform === 'win32' ? output.includes(processName) : output !== '';
        console.log("checkprocessoutput:" + output);
        resolve(isRunning);
      });
    });
  }


let processName = '';
if (process.platform === 'win32') {
    processName = 'wlmscpfs.exe';
} else {
    processName = 'wlmscpfs';
}

async function cleanupDCM() {
    return checkProcessRunning(processName)
      .then((isRunning) => {
        if (isRunning) {
          console.log(`[wlmscpfs4raccoon] Closing ${processName}...`);
          return killProcess(processName);
        }
        else {
            console.log(`[wlmscpfs4raccoon] ${processName} is not running.`);
        }
      })
      .catch((error) => {
        console.error('[wlmscpfs4raccoon] Error:', error);
      });
}

async function executeDCM() {
    checkProcessRunning(processName)
    .then(async (isRunning) => {
        if (isRunning) {
            console.log(`[wlmscpfs4raccoon] ${processName} is running, killing it.`);
        } 
        await cleanupDCM();
        // Start the wlmscpfs.
        console.log("[wlmscpfs4raccoon] Starting wlmscpfs.");
        updateDcmqrscpMongoConfig();
        var dcmService;
        if (process.platform === "win32") { // windows
            dcmService = spawn(`./plugins/wlmscpfs4raccoon/dcmtk/wlmscpfs.exe`, [configData.port])
        }
        else { // linux maybe?
            dcmService = spawn(`./plugins/wlmscpfs4raccoon/dcmtk/wlmscpfs`, [configData.port])
        }
        dcmService.on("spawn", (data) => onDCMStart(data));
        dcmService.stdout.on('data', (data) => onDCMOutput(data));
        dcmService.on("close", (data) => onDCMExit(data));
    })
    .catch((error) => {
        console.error('[wlmscpfs4raccoon] Error:', error);
    });

}

function onDCMStart(data) {
    console.log(`[wlmscpfs4raccoon] wlmscpfs has started!`);
}

function onDCMOutput(data) {
    if (data.toString().startsWith("E:")) {
        console.error(`[wlmscpfs4raccoon] ${data}`);
    }
}

function onDCMExit(data) {
    console.error(`[wlmscpfs4raccoon] wlmscpfs exited, the message follows as below:\n${data}`);
    if (retryTimes < maxRetryTimes) {
        retryTimes += 1;
        if(!isShuttingDown) {
            console.log(`[wlmscpfs4raccoon] wlmscpfs exited, restarting.`);
            executeDCM();
        }
    }
    else {
        console.error(`[wlmscpfs4raccoon] Cannot Start wlmscpfs.`);
        process.exit(1);
    }
}

function killProcess(processName) {
    return new Promise((resolve, reject) => {
      let killCommand = '';
      if (process.platform === 'win32') {
        killCommand = `taskkill /F /IM ${processName}`;
      } else {
        killCommand = `pkill ${processName}`;
      }
  
      exec(killCommand, (error, stdout) => {
        if (error) {
          reject(error);
          return;
        }
  
        resolve(stdout.toString().trim());
      });
    });
}



var rl = require("readline").createInterface({
    input: process.stdin,
    output: process.stdout
  });

rl.on("SIGINT", function () {
process.emit("SIGINT");
});

process.on( "SIGINT", async function() {
    console.log( "\n[wlmscpfs4raccoon] Gracefully shutting down from SIGINT (Crtl-C)" );
    isShuttingDown = true;
    await cleanupDCM();
    process.exit();
});
process.on( "exit", async function() {
    console.log("[wlmscpfs4raccoon] Exiting");
    isShuttingDown = true;
    await cleanupDCM();
});