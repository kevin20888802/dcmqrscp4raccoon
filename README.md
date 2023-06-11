# Raccoon-dicom Plugin: dcmqrscp4raccoon
Raccoon-dicom插件: dcmqrscp4raccoon 是一個使用NodeJS開發的插件，

旨在擴展Raccoon-dicom醫學影像伺服器(https://github.com/Chinlinlee/raccoon-dicom) 的功能。

該插件使用經過修改過後的dcmtk的dcmqrscp，使其儲存的方式從原先的二進制檔案資料庫，改成儲存到Raccoon-dicom醫學影像伺服器使用的MongoDB相同的資料庫，最終透過插件的方式與Raccoon-dicom醫學影像伺服器整合。

該插件使得原本只支援dicomWeb協定的Raccoon-dicom伺服器能夠同時支援DIMSE協定的C-STORE、C-FIND和C-MOVE等操作。

## 安裝方法
按照以下步驟安裝dcmqrscp4raccoon插件：

1. 將 /Export/raccoon-dicom_plugin/ 資料夾中的所有檔案和資料夾複製到Raccoon-dicom伺服器的根目錄下。

2. 根據需要修改 plugin/config.js 和 plugin/dcm4raccoon/dcmqrscp.cfg 配置檔案。請注意，在 dcmqrscp.cfg 檔案中的 AETable 需要使用IP位址而不是主機名稱。建議採用與原始的dcmtk的dcmqrscp相同的配置方法。

3. 儲存對配置檔案所做的更改。


## 先決條件
使用dcmqrscp4raccoon插件需要滿足以下先決條件：

- Raccoon-dicom伺服器

## 使用方法
1. 執行以下命令啟動Raccoon-dicom伺服器插件：

>> node server.js

2. dcmqrscp4raccoon插件現在會為Raccoon-dicom伺服器啟用DIMSE協定支援，從而使其能夠執行C-STORE、C-FIND和C-MOVE操作。

## 測試環境
dcmqrscp4raccoon插件已在以下環境中成功測試：

- Windows 11
- Ubuntu 23.04
