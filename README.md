# Raccoon-dicom Plugin: dcmqrscp4raccoon
Raccoon-dicom插件: dcmqrscp4raccoon 是一個使用NodeJS開發的插件，

旨在擴展Raccoon-dicom醫學影像伺服器(https://github.com/Chinlinlee/raccoon-dicom) 的功能。

該插件使用經過修改過後的dcmtk的dcmqrscp，使其儲存的方式從原先的二進制檔案資料庫，改成儲存到Raccoon-dicom醫學影像伺服器使用的MongoDB相同的資料庫，最終透過插件的方式與Raccoon-dicom醫學影像伺服器整合。

該插件使得原本的Raccoon-dicom伺服器能夠以dcmtk的方式，同時支援DIMSE協定的C-STORE、C-FIND和C-MOVE等操作。

## 先決條件
使用dcmqrscp4raccoon插件需要滿足以下先決條件：

- 部署好[Raccoon-dicom伺服器](https://github.com/Chinlinlee/raccoon-dicom)

## 安裝方法
按照以下步驟安裝dcmqrscp4raccoon插件：

1. 於此專案的根目錄中找到 `Export/raccoon-dicom_plugin/` 資料夾

2. 將`Export/raccoon-dicom_plugin/`中的所有檔案和資料夾複製到[Raccoon-dicom伺服器](https://github.com/Chinlinlee/raccoon-dicom)的`plugins`目錄下。

3. 根據需要修改 `plugins/config.js` 和 `plugins/dcm4raccoon/dcmqrscp.cfg` 配置檔案。
    #### ⚠️ 請注意，在 dcmqrscp.cfg 檔案中的 AETable 需要使用IP位址而不是主機名稱。建議採用與原始的dcmtk的dcmqrscp相同的配置方法。

4. 儲存對配置檔案所做的更改。

## 使用方法
#### ⚠️ 請先停止正在運行中的[Raccoon-dicom伺服器](https://github.com/Chinlinlee/raccoon-dicom)

1. 執行以下命令啟動dcmqrscp4raccoon插件：
```bash
# 你可以把Raccoon-dicom伺服器的安裝根目錄匯出為環境變數
cd %Raccoon-dicom_HOME% # Windows
cd $Raccoon-dicom_HOME # Linux
npm install request
node plugins/dcm4raccoon/index.js
```

2. dcmqrscp4raccoon插件現在會為Raccoon-dicom伺服器啟用DIMSE協定支援，從而使其能夠執行C-STORE、C-FIND和C-MOVE操作。

3. 重起啟動Raccoon-dicom伺服器 (您可以參考以下指令)
```bash
# 請在新的terminal視窗中(又稱CMD或命令提示字元)操作
cd %Raccoon-dicom_HOME%
npm install
node server.js
```

## 測試環境
dcmqrscp4raccoon插件已在以下環境中成功測試：

- Windows 11
- Ubuntu 23.04

