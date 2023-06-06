/**
 * dcmqrscp service for raccoon.
 */
const fs = require('fs');
const path = require("path");
const { spawn } = require('node:child_process');
const { moveDicomFilesToTempDir, uploadDicomFilesInTempDir, deleteTempDir } = require("./stowUploader");
const { raccoonConfig } = require("../../config-class");
const { pluginsConfig } = require("../config");
const configData = pluginsConfig.dcm4raccoon;
const dicomFilePath = configData.storepath;
const maxRetryTimes = 3;
let retryTimes = 0;
let isChecking = false;

module.exports.runDCM = async function () {
    executeDCM();
}

// 將dcmqrscp的MongoDB設定調整成與raccoon相同
function updateDcmqrscpMongoConfig() {
    let conn_string = `mongodb://${raccoonConfig.mongoDbConfig.hosts[0]}/?appname=dcmqrscp4raccoon`;
    let mongoDB_name = `${raccoonConfig.mongoDbConfig.dbName}`;
    let collection_name = `dicom`;
    let DICOM_STORE_ROOTPATH = `${path.resolve(raccoonConfig.dicomWebConfig.storeRootPath)}`;
    let _w = `conn_string=${conn_string}
mongoDB_name=${mongoDB_name}
collection_name=${collection_name}
DICOM_STORE_ROOTPATH=${DICOM_STORE_ROOTPATH}`;
    fs.writeFileSync("./plugins/dcm4raccoon/dcmtk/dcmqrscpMongoConfig.cfg", _w);
}

// 等待sleep
function sleep(ms) {
    return new Promise((resolve) => {
        setTimeout(resolve, ms);
    });
}

// 每秒執行檢查是否資料夾內有檔案如果有就上傳
async function onEverySec() {
    // 執行程序
    isChecking = true;
    // 創建暫存目錄，如果不存在的話
    if (!fs.existsSync(dicomFilePath)) {
        console.log("creating folder:" + dicomFilePath);
        fs.mkdirSync(dicomFilePath);
    }
    await uploadDicomFilesInTempDir();
    // 刪除暫存目錄及其中的所有文件
    deleteTempDir();
    await sleep(1000);
    //console.log("one sec passed.");
    await onEverySec();
}

async function executeDCM() {
    if (!isChecking) {
        onEverySec();
    }

    // Start the dcmqrscp.
    console.log("[dcm4raccoon] Starting dcmqrscp.");
    updateDcmqrscpMongoConfig();
    var dcmService;
	if (process.platform === "win32") { // windows
		dcmService = spawn(`./plugins/dcm4raccoon/dcmtk/dcmqrscp.exe`, ["-c", path.resolve("./plugins/dcm4raccoon/dcmqrscp.cfg"), configData.port])
	}
	else { // linux maybe?
		dcmService = spawn(`./plugins/dcm4raccoon/dcmtk/dcmqrscp`, ["-c", path.resolve("./plugins/dcm4raccoon/dcmqrscp.cfg"), configData.port])
	}
    dcmService.on("spawn", (data) => onDCMStart(data));
    dcmService.stdout.on('data', (data) => onDCMOutput(data));
    dcmService.on("close", (data) => onDCMExit(data));
}

function onDCMStart(data) {
    console.log(`[dcm4raccoon] dcmqrscp has started!`);
}

function onDCMOutput(data) {
    if (data.toString().startsWith("E:")) {
        console.error(`[dcm4raccoon] ${data}`);
    }
}

function onDCMExit(data) {
    console.error(`[dcm4raccoon] Storescp exited, the message follows as below:\n${data}`);
    if (retryTimes < maxRetryTimes) {
        retryTimes += 1;
        console.log(`[dcm4raccoon] dcmqrscp exited, restarting.`);
        executeDCM();
    }
    else {
        console.error(`[dcm4raccoon] Cannot Start dcmqrscp.`);
        process.exit(1);
    }
}
