const fs = require('fs');
const path = require('path');
const MongoClient = require('mongodb').MongoClient;

// 讀取cfg檔案內容
function readConfigFile() {
  try {
    const configFile = fs.readFileSync('./wlmdump2mongodb.cfg', 'utf8');
    const lines = configFile.split('\n');
    const config = {};

    lines.forEach((line) => {
      const [key, value] = line.split('=');
      if (key && value) {
        config[key.trim()] = value.trim();
      }
    });

    return config;
  } catch (error) {
    console.error('讀取配置檔失敗:', error);
    return null;
  }
}

// 連接至MongoDB資料庫
async function connectToMongoDB(connString, dbName) {
  try {
    const client = await MongoClient.connect(connString, { useUnifiedTopology: true });
    const db = client.db(dbName);
    console.log('連接至MongoDB成功');
    return { client, db };
  } catch (error) {
    console.error('連接至MongoDB失敗:', error);
    return null;
  }
}

// 創建集合
async function createCollection(db, collectionName) {
  try {
    await db.createCollection(collectionName);
    console.log('創建集合:', collectionName);
  } catch (error) {
    console.error('創建集合時發生錯誤:', error);
  }
}

// 儲存資料到 MongoDB
async function saveKeysToMongoDB(db, collectionName, fileName, AETitle, keys) {
  try {
    const collection = db.collection(collectionName);
    const document = {
      fileName,
      AETitle,
      ...keys
    };
    const result = await collection.insertOne(document);
    console.log(`儲存資料成功: ${result.insertedId}`);
  } catch (error) {
    console.error('儲存資料到 MongoDB 時發生錯誤:', error);
  }
}

// 掃描檔案並儲存資料到 MongoDB
async function scanAndSaveFiles(db, collectionName, directory, folderName) {
  const files = fs.readdirSync(directory, { withFileTypes: true });

  for (const file of files) {
    const filePath = path.join(directory, file.name);

    if (file.isFile() && path.extname(file.name) === '.dump') {
      console.log('處理檔案:', filePath);

      const keys = {};

      const lines = fs.readFileSync(filePath, 'utf-8').split('\n');

      for (const line of lines) {
        const matches = line.match(/(\([^)]+\))\s+(\S+)\s+(.+)/);

        if (matches && matches.length === 4) {
          const key = matches[1];
          const vr = matches[2];
          const value = matches[3].trim(); // 移除前後空格

          keys[key] = {
            vr,
            value
          };
        }
      }

      await saveKeysToMongoDB(db, collectionName, file.name, folderName, keys);

      console.log('完成處理檔案:', filePath);
    } else if (file.isDirectory()) {
      const subDirectory = path.join(directory, file.name);
      await scanAndSaveFiles(db, collectionName, subDirectory, file.name);
    }
  }
}

// 主程式
async function main() {
  const config = readConfigFile();
  if (!config) {
    return;
  }

  const { conn_string, mongoDB_name, collection_name } = config;
  const { client, db } = await connectToMongoDB(conn_string, mongoDB_name);

  if (db) {
    await createCollection(db, collection_name);
    await scanAndSaveFiles(db, collection_name, './wlistdb/', '');
    client.close();
  }
}

// 執行主程式
main();
