/*
 *
 *  Copyright (C) 1993-2022, OFFIS e.V.
 *  All rights reserved.  See COPYRIGHT file for details.
 *
 *  This software and supporting documentation were developed by
 *
 *    OFFIS e.V.
 *    R&D Division Health
 *    Escherweg 2
 *    D-26121 Oldenburg, Germany
 *
 *
 *  Module:  dcmqrdb
 *
 *  Author:  Marco Eichelberg
 *
 *  Purpose: classes DcmQueryRetrieveIndexDatabaseHandle,
 *                   DcmQueryRetrieveIndexDatabaseHandleFactory
 *
 */



#include "dcmtk/config/osconfig.h"    /* make sure OS specific configuration is included first */

BEGIN_EXTERN_C
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
END_EXTERN_C



#include "../../../mongo-c-driver-1.23.3/src/libmongoc/src/mongoc/mongoc.h"
#include "../../../mongo-c-driver-1.23.3/src/libbson/src/bson/bson.h"
#include "dcmtk/ofstd/ofstd.h"
#include "dcmtk/dcmqrdb/dcmqrdbs.h"
#include "dcmtk/dcmqrdb/dcmqrdbi.h"
#include "dcmtk/dcmqrdb/dcmqrcnf.h"
#include "dcmtk/dcmqrdb/dcmqropt.h"
#include "dcmtk/ofstd/ofstdinc.h"
#include "dcmtk/dcmqrdb/dcmqridx.h"
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcmatch.h"
#include <ctime>
#include <vector>

/* ========================= static data ========================= */

/**** The TbFindAttr table contains the description of tags (keys) supported
 **** by the DB Module.
 **** Tags described here have to be present in the Index Record file.
 **** The order is insignificant.
 ****
 **** Each element of this table is described by
 ****           The tag value
 ****           The level of this tag (from patient to image)
 ****           The Key Type (only UNIQUE_KEY values is used)
 ****
 **** This table and the IndexRecord structure should contain at least
 **** all Unique and Required keys.
 ***/

static const DB_FindAttr TbFindAttr [] = {
        DB_FindAttr( DCM_PatientBirthDate,                      PATIENT_LEVEL,  OPTIONAL_KEY ),
        DB_FindAttr( DCM_PatientSex,                            PATIENT_LEVEL,  OPTIONAL_KEY ),
        DB_FindAttr( DCM_PatientName,                           PATIENT_LEVEL,  REQUIRED_KEY ),
        DB_FindAttr( DCM_PatientID,                             PATIENT_LEVEL,  UNIQUE_KEY   ),
        DB_FindAttr( DCM_PatientBirthTime,                      PATIENT_LEVEL,  OPTIONAL_KEY ),
        DB_FindAttr( DCM_RETIRED_OtherPatientIDs,               PATIENT_LEVEL,  OPTIONAL_KEY ),
        DB_FindAttr( DCM_OtherPatientNames,                     PATIENT_LEVEL,  OPTIONAL_KEY ),
        DB_FindAttr( DCM_EthnicGroup,                           PATIENT_LEVEL,  OPTIONAL_KEY ),
        DB_FindAttr( DCM_PatientComments,                       PATIENT_LEVEL,  OPTIONAL_KEY ),
        DB_FindAttr( DCM_IssuerOfPatientID,                     PATIENT_LEVEL,  OPTIONAL_KEY ),
        DB_FindAttr( DCM_StudyDate,                             STUDY_LEVEL,    REQUIRED_KEY ),
        DB_FindAttr( DCM_StudyTime,                             STUDY_LEVEL,    REQUIRED_KEY ),
        DB_FindAttr( DCM_StudyID,                               STUDY_LEVEL,    REQUIRED_KEY ),
        DB_FindAttr( DCM_AccessionNumber,                       STUDY_LEVEL,    REQUIRED_KEY ),
        DB_FindAttr( DCM_ReferringPhysicianName,                STUDY_LEVEL,    OPTIONAL_KEY ),
        DB_FindAttr( DCM_StudyDescription,                      STUDY_LEVEL,    OPTIONAL_KEY ),
        DB_FindAttr( DCM_NameOfPhysiciansReadingStudy,          STUDY_LEVEL,    OPTIONAL_KEY ),
        DB_FindAttr( DCM_StudyInstanceUID,                      STUDY_LEVEL,    UNIQUE_KEY   ),
        DB_FindAttr( DCM_RETIRED_OtherStudyNumbers,             STUDY_LEVEL,    OPTIONAL_KEY ),
        DB_FindAttr( DCM_AdmittingDiagnosesDescription,         STUDY_LEVEL,    OPTIONAL_KEY ),
        DB_FindAttr( DCM_PatientAge,                            STUDY_LEVEL,    OPTIONAL_KEY ),
        DB_FindAttr( DCM_PatientSize,                           STUDY_LEVEL,    OPTIONAL_KEY ),
        DB_FindAttr( DCM_PatientWeight,                         STUDY_LEVEL,    OPTIONAL_KEY ),
        DB_FindAttr( DCM_Occupation,                            STUDY_LEVEL,    OPTIONAL_KEY ),
        DB_FindAttr( DCM_AdditionalPatientHistory,              STUDY_LEVEL,    OPTIONAL_KEY ),
        DB_FindAttr( DCM_SeriesNumber,                          SERIE_LEVEL,    REQUIRED_KEY ),
        DB_FindAttr( DCM_SeriesInstanceUID,                     SERIE_LEVEL,    UNIQUE_KEY   ),
        DB_FindAttr( DCM_Modality,                              SERIE_LEVEL,    OPTIONAL_KEY ),
        DB_FindAttr( DCM_InstanceNumber,                        IMAGE_LEVEL,    REQUIRED_KEY ),
        DB_FindAttr( DCM_SOPInstanceUID,                        IMAGE_LEVEL,    UNIQUE_KEY   ),

        DB_FindAttr(DCM_MediaStorageSOPClassUID,           IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_MediaStorageSOPInstanceUID,       IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_TransferSyntaxUID,                IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_ImplementationClassUID,           IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_ImplementationVersionName,        IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_DirectoryRecordType,              IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_ImageType,                        IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_AcquisitionDate,                  IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_ContentDate,                      IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_AcquisitionTime,                  IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_ContentTime,                      IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_Manufacturer,                     IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_ManufacturerModelName,            IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_PatientIdentityRemoved,           IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_DeidentificationMethod,           IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_ContrastBolusAgent,               IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_BodyPartExamined,                 IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_ScanOptions,                      IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_KVP,                              IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_DataCollectionDiameter,           IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_SoftwareVersions,                 IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_ReconstructionDiameter,           IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_GantryDetectorTilt,               IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_TableHeight,                      IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_RotationDirection,                IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_ExposureTime,                     IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_XRayTubeCurrent,                  IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_FilterType,                       IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_GeneratorPower,                   IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_FocalSpots,                       IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_ConvolutionKernel,                IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_PatientPosition,                  IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_ExposureModulationType,           IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_RETIRED_EstimatedDoseSaving,              IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_CTDIvol,                          IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_AcquisitionNumber,                IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_InstanceNumber,                   IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_PatientOrientation,               IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_ImagePositionPatient,             IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_ImageOrientationPatient,          IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_FrameOfReferenceUID,              IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_PositionReferenceIndicator,       IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_SliceLocation,                    IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_SamplesPerPixel,                  IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_PhotometricInterpretation,        IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_PlanarConfiguration,              IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_NumberOfFrames,                   IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_Rows,                             IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_Columns,                          IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_PixelSpacing,                     IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_PixelAspectRatio,                 IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_BitsAllocated,                    IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_BitsStored,                       IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_HighBit,                          IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_PixelRepresentation,              IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_WindowCenter,                     IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_WindowWidth,                      IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_RescaleIntercept,                 IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_RescaleSlope,                     IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_StorageMediaFileSetID,            IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_StorageMediaFileSetUID,           IMAGE_LEVEL,    OPTIONAL_KEY),
        DB_FindAttr(DCM_PixelData,                        IMAGE_LEVEL,    OPTIONAL_KEY)

  };

/**** The NbFindAttr variable contains the length of the TbFindAttr table
 ***/

static int NbFindAttr = ((sizeof (TbFindAttr)) / (sizeof (TbFindAttr [0])));

/* ========================= static functions ========================= */

std::string getCurrentTimeAsString()
{
    // 取得目前時間
    std::time_t currentTime = std::time(nullptr);

    // 轉換為目前時區
    std::tm* localTime = std::localtime(&currentTime);

    // 轉換成string
    std::ostringstream oss;
    oss << std::put_time(localTime, "[%Y/%m/%d %H:%M:%S]");
    return oss.str();
}

/*
* msg=訊息
* msgType--> E=ERROR
*            D=DEBUG
*            W=WARN
*           ""=INFO
*/
static void ConsoleLog(std::string msg, std::string msgType)
{
    if (strcmp(msgType.c_str(), "E") == 0)
    {
        std::cout << "[ERROR]";
    }
    else if (strcmp(msgType.c_str(), "D") == 0)
    {
        std::cout << "[DEBUG]";
    }
    else if (strcmp(msgType.c_str(), "W") == 0)
    {
        std::cout << "[WARN]";
    }
    else
    {
        std::cout << "[INFO]";
    }

    std::string currentTime = getCurrentTimeAsString();
    std::cout << currentTime;
    std::cout << " ";
    std::cout << msg << std::endl;
}

#pragma region MongoDB

/**
* MongoDB設定
*/

// Mongo 連線網址
std::string conn_string = "mongodb://127.0.0.1/?appname=dcmqrscp4raccoon";
std::string mongoDB_name = "raccoon_polka";
std::string collection_name = "dicom";
std::string DICOM_STORE_ROOTPATH = "D:/Programming/Nodejs/raccoon/files/";

#ifdef _WIN32 // Windows

#include <windows.h>

std::string getExecutablePath() {
    char exePath[MAX_PATH];
    DWORD pathLength = GetModuleFileName(NULL, exePath, MAX_PATH);

    if (pathLength == 0) {
        ConsoleLog("Failed to retrieve executable path.", "E");
        exit(1);
    }

    std::string exeDirectory = std::string(exePath);
    std::size_t lastSlashPos = exeDirectory.find_last_of("\\/");

    if (lastSlashPos == std::string::npos) {
        ConsoleLog("Invalid executable path.", "E");
        exit(1);
    }

    return exeDirectory.substr(0, lastSlashPos + 1);
}

#elif defined(__linux__) // Linux

#include <unistd.h>
#include <limits.h>

std::string getExecutablePath() {
    char exePath[PATH_MAX];
    ssize_t pathLength = readlink("/proc/self/exe", exePath, sizeof(exePath));

    if (pathLength == -1) {
        ConsoleLog("Failed to retrieve executable path.", "E");
        exit(1);
    }

    std::string exeDirectory = std::string(exePath, pathLength);
    std::size_t lastSlashPos = exeDirectory.find_last_of("\\/");

    if (lastSlashPos == std::string::npos) {
        ConsoleLog("Invalid executable path.", "E");
        exit(1);
    }

    return exeDirectory.substr(0, lastSlashPos + 1);
}

#else
#error Unsupported platform
#endif

void ReadMongoConfig()
{
    std::string exeDirectory = getExecutablePath();

    std::string configFilePath = exeDirectory + "dcmqrscpMongoConfig.cfg";

    std::ifstream config_file(configFilePath);
    if (!config_file.is_open()) 
    {
        ConsoleLog("MongoDB Config File Path:" + configFilePath, "");
        ConsoleLog("Failed to open mongo config file.", "E");
    }

    std::string line;
    while (std::getline(config_file, line)) 
    {
        size_t delimiter_pos = line.find('=');
        if (delimiter_pos != std::string::npos) 
        {
            std::string key = line.substr(0, delimiter_pos);
            std::string value = line.substr(delimiter_pos + 1);

            if (key == "conn_string")
            {
                conn_string = value;
            }
            else if (key == "mongoDB_name") 
            {
                mongoDB_name = value;
            }
            else if (key == "collection_name") 
            {
                collection_name = value;
            }
            else if (key == "DICOM_STORE_ROOTPATH") 
            {
                DICOM_STORE_ROOTPATH = value;
            }
        }
    }

    config_file.close();

    // 印出讀取後的變數值
    ConsoleLog("MongoDB conn_string:" + conn_string, "");
    ConsoleLog("MongoDB mongoDB_name:" + mongoDB_name, "");
    ConsoleLog("MongoDB collection_name:" + collection_name, "");
    ConsoleLog("MongoDB DICOM_STORE_ROOTPATH:" + DICOM_STORE_ROOTPATH, "");
}

/*
Visual C++ 編譯時，遇到「無法解析的外部符號」
解決的辦法如下：
首先，到dcmqrscp專案的「屬性」
並到「組態屬性」->「連結器」->「輸入」
其中的「其他相依性」則是告訴編譯器，你需要引用哪些 lib 檔
將mongo的兩個lib引入

DEBUG:
..\..\..\lib\Debug\dcmqrdb.lib;..\..\..\lib\Debug\dcmnet.lib;..\..\..\lib\Debug\dcmdata.lib;..\..\..\lib\Debug\oflog.lib;..\..\..\lib\Debug\ofstd.lib;..\..\..\mongo-c-driver-1.23.3\src\libbson\Debug\bson-1.0.lib;..\..\..\mongo-c-driver-1.23.3\src\libmongoc\Debug\mongoc-1.0.lib;iphlpapi.lib;ws2_32.lib;netapi32.lib;wsock32.lib;kernel32.lib;user32.lib;gdi32.lib;winspool.lib;shell32.lib;ole32.lib;oleaut32.lib;uuid.lib;comdlg32.lib;advapi32.lib
RELEASE:
..\..\..\lib\Release\dcmqrdb.lib;..\..\..\lib\Release\dcmnet.lib;..\..\..\lib\Release\dcmdata.lib;..\..\..\lib\Release\oflog.lib;..\..\..\lib\Release\ofstd.lib;..\..\..\mongo-c-driver-1.23.3\src\libbson\Release\bson-1.0.lib;..\..\..\mongo-c-driver-1.23.3\src\libmongoc\Release\mongoc-1.0.lib;iphlpapi.lib;ws2_32.lib;netapi32.lib;wsock32.lib;kernel32.lib;user32.lib;gdi32.lib;winspool.lib;shell32.lib;ole32.lib;oleaut32.lib;uuid.lib;comdlg32.lib;advapi32.lib

在dcmqrdb專案的「屬性」
「C/C++」->「一般」->「其他Include目錄」
改成這個，來加入MongoDB的Include目錄。
DEBUG:
D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\dcmtk-3.6.7\config\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\ofstd\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\oflog\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmdata\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmimgle\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmimage\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmjpeg\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmjpls\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmtls\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmnet\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmsr\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmsign\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmwlm\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmqrdb\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmpstat\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmrt\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmiod\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmfg\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmseg\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmtract\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmpmap\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmect\include;D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\mongo-c-driver-1.23.3\src\libmongoc\src;D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\mongo-c-driver-1.23.3\src\libmongoc\src\mongoc;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\mongo-c-driver-1.23.3\src\libmongoc\src;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\mongo-c-driver-1.23.3\src\libmongoc\src\mongoc;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\mongo-c-driver-1.23.3\src\libmongoc\..\..\src\common;D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\mongo-c-driver-1.23.3\src\libmongoc\..\..\src;D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\mongo-c-driver-1.23.3\src\libmongoc\..\..\src\common;D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\mongo-c-driver-1.23.3\src\libbson\src;D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\mongo-c-driver-1.23.3\src\libbson\src\bson;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\mongo-c-driver-1.23.3\src\libbson\src;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\mongo-c-driver-1.23.3\src\libbson\src\bson;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\mongo-c-driver-1.23.3\src\libbson\..\..\src\common;D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\mongo-c-driver-1.23.3\src\libbson\..\..\src;D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\mongo-c-driver-1.23.3\src\libbson\..\..\src\common;%(AdditionalIncludeDirectories)
RELEASE:
D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\dcmtk-3.6.7\config\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\ofstd\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\oflog\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmdata\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmimgle\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmimage\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmjpeg\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmjpls\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmtls\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmnet\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmsr\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmsign\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmwlm\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmqrdb\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmpstat\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmrt\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmiod\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmfg\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmseg\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmtract\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmpmap\include;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\dcmtk-3.6.7\dcmect\include;D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\mongo-c-driver-1.23.3\src\libmongoc\src;D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\mongo-c-driver-1.23.3\src\libmongoc\src\mongoc;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\mongo-c-driver-1.23.3\src\libmongoc\src;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\mongo-c-driver-1.23.3\src\libmongoc\src\mongoc;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\mongo-c-driver-1.23.3\src\libmongoc\..\..\src\common;D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\mongo-c-driver-1.23.3\src\libmongoc\..\..\src;D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\mongo-c-driver-1.23.3\src\libmongoc\..\..\src\common;D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\mongo-c-driver-1.23.3\src\libbson\src;D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\mongo-c-driver-1.23.3\src\libbson\src\bson;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\mongo-c-driver-1.23.3\src\libbson\src;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\mongo-c-driver-1.23.3\src\libbson\src\bson;D:\Programming\C++\dcmqrscp4raccoon\CMakeSource\mongo-c-driver-1.23.3\src\libbson\..\..\src\common;D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\mongo-c-driver-1.23.3\src\libbson\..\..\src;D:\Programming\C++\dcmqrscp4raccoon\dcmqrscp4raccoon\mongo-c-driver-1.23.3\src\libbson\..\..\src\common;%(AdditionalIncludeDirectories)

windows下需要SSIZE_T
加入以下來解決
#ifdef _WIN32
#include <basetsd.h>
typedef SSIZE_T ssize_t;
#endif // _WIN32
在bson-cmp.h加入這段

*/

/*
* 將十進位數字轉十六進位字串
*/
std::string int_to_hex(Uint16 i)
{
    std::stringstream stream;
    //stream << "0x"
    stream << ""
        << std::setfill('0') << std::setw(sizeof(Uint16) * 2)
        << std::hex << i;
    return stream.str();
}

/*
* C++版的字串替換Replace
*/
std::string ReplaceString(std::string subject, const std::string& search, const std::string& replace)
{
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return subject;
}

/************
 *      Initializes addresses in an IdxRecord
 */

static void DB_IdxInitRecord(IdxRecord* idx, int linksOnly)
{
    if (!linksOnly)
    {
        idx->param[RECORDIDX_PatientBirthDate].XTag = DCM_PatientBirthDate;
        idx->param[RECORDIDX_PatientBirthDate].ValueLength = DA_MAX_LENGTH;
        idx->PatientBirthDate[0] = '\0';
        idx->param[RECORDIDX_PatientSex].XTag = DCM_PatientSex;
        idx->param[RECORDIDX_PatientSex].ValueLength = CS_MAX_LENGTH;
        idx->PatientSex[0] = '\0';
        idx->param[RECORDIDX_PatientName].XTag = DCM_PatientName;
        idx->param[RECORDIDX_PatientName].ValueLength = PN_MAX_LENGTH;
        idx->PatientName[0] = '\0';
        idx->param[RECORDIDX_PatientID].XTag = DCM_PatientID;
        idx->param[RECORDIDX_PatientID].ValueLength = LO_MAX_LENGTH;
        idx->PatientID[0] = '\0';
        idx->param[RECORDIDX_PatientBirthTime].XTag = DCM_PatientBirthTime;
        idx->param[RECORDIDX_PatientBirthTime].ValueLength = TM_MAX_LENGTH;
        idx->PatientBirthTime[0] = '\0';
        idx->param[RECORDIDX_OtherPatientIDs].XTag = DCM_RETIRED_OtherPatientIDs;
        idx->param[RECORDIDX_OtherPatientIDs].ValueLength = LO_MAX_LENGTH;
        idx->OtherPatientIDs[0] = '\0';
        idx->param[RECORDIDX_OtherPatientNames].XTag = DCM_OtherPatientNames;
        idx->param[RECORDIDX_OtherPatientNames].ValueLength = PN_MAX_LENGTH;
        idx->OtherPatientNames[0] = '\0';
        idx->param[RECORDIDX_EthnicGroup].XTag = DCM_EthnicGroup;
        idx->param[RECORDIDX_EthnicGroup].ValueLength = SH_MAX_LENGTH;
        idx->EthnicGroup[0] = '\0';
        idx->param[RECORDIDX_StudyDate].XTag = DCM_StudyDate;
        idx->param[RECORDIDX_StudyDate].ValueLength = DA_MAX_LENGTH;
        idx->StudyDate[0] = '\0';
        idx->param[RECORDIDX_StudyTime].XTag = DCM_StudyTime;
        idx->param[RECORDIDX_StudyTime].ValueLength = TM_MAX_LENGTH;
        idx->StudyTime[0] = '\0';
        idx->param[RECORDIDX_StudyID].XTag = DCM_StudyID;
        idx->param[RECORDIDX_StudyID].ValueLength = CS_MAX_LENGTH;
        idx->StudyID[0] = '\0';
        idx->param[RECORDIDX_StudyDescription].XTag = DCM_StudyDescription;
        idx->param[RECORDIDX_StudyDescription].ValueLength = LO_MAX_LENGTH;
        idx->StudyDescription[0] = '\0';
        idx->param[RECORDIDX_NameOfPhysiciansReadingStudy].XTag = DCM_NameOfPhysiciansReadingStudy;
        idx->param[RECORDIDX_NameOfPhysiciansReadingStudy].ValueLength = PN_MAX_LENGTH;
        idx->NameOfPhysiciansReadingStudy[0] = '\0';
        idx->param[RECORDIDX_AccessionNumber].XTag = DCM_AccessionNumber;
        idx->param[RECORDIDX_AccessionNumber].ValueLength = CS_MAX_LENGTH;
        idx->AccessionNumber[0] = '\0';
        idx->param[RECORDIDX_ReferringPhysicianName].XTag = DCM_ReferringPhysicianName;
        idx->param[RECORDIDX_ReferringPhysicianName].ValueLength = PN_MAX_LENGTH;
        idx->ReferringPhysicianName[0] = '\0';
        idx->param[RECORDIDX_ProcedureDescription].XTag = DCM_StudyDescription;
        idx->param[RECORDIDX_ProcedureDescription].ValueLength = LO_MAX_LENGTH;
        idx->ProcedureDescription[0] = '\0';
        idx->param[RECORDIDX_AttendingPhysiciansName].XTag = DCM_NameOfPhysiciansReadingStudy;
        idx->param[RECORDIDX_AttendingPhysiciansName].ValueLength = PN_MAX_LENGTH;
        idx->AttendingPhysiciansName[0] = '\0';
        idx->param[RECORDIDX_StudyInstanceUID].XTag = DCM_StudyInstanceUID;
        idx->param[RECORDIDX_StudyInstanceUID].ValueLength = UI_MAX_LENGTH;
        idx->StudyInstanceUID[0] = '\0';
        idx->param[RECORDIDX_OtherStudyNumbers].XTag = DCM_RETIRED_OtherStudyNumbers;
        idx->param[RECORDIDX_OtherStudyNumbers].ValueLength = IS_MAX_LENGTH;
        idx->OtherStudyNumbers[0] = '\0';
        idx->param[RECORDIDX_AdmittingDiagnosesDescription].XTag = DCM_AdmittingDiagnosesDescription;
        idx->param[RECORDIDX_AdmittingDiagnosesDescription].ValueLength = LO_MAX_LENGTH;
        idx->AdmittingDiagnosesDescription[0] = '\0';
        idx->param[RECORDIDX_PatientAge].XTag = DCM_PatientAge;
        idx->param[RECORDIDX_PatientAge].ValueLength = AS_MAX_LENGTH;
        idx->PatientAge[0] = '\0';
        idx->param[RECORDIDX_PatientSize].XTag = DCM_PatientSize;
        idx->param[RECORDIDX_PatientSize].ValueLength = DS_MAX_LENGTH;
        idx->PatientSize[0] = '\0';
        idx->param[RECORDIDX_PatientWeight].XTag = DCM_PatientWeight;
        idx->param[RECORDIDX_PatientWeight].ValueLength = DS_MAX_LENGTH;
        idx->PatientWeight[0] = '\0';
        idx->param[RECORDIDX_Occupation].XTag = DCM_Occupation;
        idx->param[RECORDIDX_Occupation].ValueLength = SH_MAX_LENGTH;
        idx->Occupation[0] = '\0';
        idx->param[RECORDIDX_SeriesNumber].XTag = DCM_SeriesNumber;
        idx->param[RECORDIDX_SeriesNumber].ValueLength = IS_MAX_LENGTH;
        idx->SeriesNumber[0] = '\0';
        idx->param[RECORDIDX_SeriesInstanceUID].XTag = DCM_SeriesInstanceUID;
        idx->param[RECORDIDX_SeriesInstanceUID].ValueLength = UI_MAX_LENGTH;
        idx->SeriesInstanceUID[0] = '\0';
        idx->param[RECORDIDX_Modality].XTag = DCM_Modality;
        idx->param[RECORDIDX_Modality].ValueLength = CS_MAX_LENGTH;
        idx->ImageNumber[0] = '\0';
        idx->param[RECORDIDX_ImageNumber].XTag = DCM_InstanceNumber;
        idx->param[RECORDIDX_ImageNumber].ValueLength = IS_MAX_LENGTH;
        idx->ImageNumber[0] = '\0';
        idx->param[RECORDIDX_SOPInstanceUID].XTag = DCM_SOPInstanceUID;
        idx->param[RECORDIDX_SOPInstanceUID].ValueLength = UI_MAX_LENGTH;
        idx->SOPInstanceUID[0] = '\0';
        idx->param[RECORDIDX_SeriesDate].XTag = DCM_SeriesDate;
        idx->param[RECORDIDX_SeriesDate].ValueLength = DA_MAX_LENGTH;
        idx->SeriesDate[0] = '\0';
        idx->param[RECORDIDX_SeriesTime].XTag = DCM_SeriesTime;
        idx->param[RECORDIDX_SeriesTime].ValueLength = TM_MAX_LENGTH;
        idx->SeriesTime[0] = '\0';
        idx->param[RECORDIDX_SeriesDescription].XTag = DCM_SeriesDescription;
        idx->param[RECORDIDX_SeriesDescription].ValueLength = LO_MAX_LENGTH;
        idx->SeriesDescription[0] = '\0';
        idx->param[RECORDIDX_ProtocolName].XTag = DCM_ProtocolName;
        idx->param[RECORDIDX_ProtocolName].ValueLength = LO_MAX_LENGTH;
        idx->ProtocolName[0] = '\0';
        idx->param[RECORDIDX_OperatorsName].XTag = DCM_OperatorsName;
        idx->param[RECORDIDX_OperatorsName].ValueLength = PN_MAX_LENGTH;
        idx->OperatorsName[0] = '\0';
        idx->param[RECORDIDX_PerformingPhysicianName].XTag = DCM_PerformingPhysicianName;
        idx->param[RECORDIDX_PerformingPhysicianName].ValueLength = PN_MAX_LENGTH;
        idx->PerformingPhysicianName[0] = '\0';
        idx->param[RECORDIDX_PresentationLabel].XTag = DCM_ContentLabel;
        idx->param[RECORDIDX_PresentationLabel].ValueLength = CS_LABEL_MAX_LENGTH;
        idx->PresentationLabel[0] = '\0';
        idx->param[RECORDIDX_IssuerOfPatientID].XTag = DCM_IssuerOfPatientID;
        idx->param[RECORDIDX_IssuerOfPatientID].ValueLength = LO_MAX_LENGTH;
        idx->IssuerOfPatientID[0] = '\0';
        idx->param[RECORDIDX_SpecificCharacterSet].XTag = DCM_SpecificCharacterSet;
        idx->param[RECORDIDX_SpecificCharacterSet].ValueLength = CS_MAX_LENGTH * 8;
        idx->SpecificCharacterSet[0] = '\0';

        idx->param[RECORDIDX_FileMetaInformationVersion].XTag = DCM_FileMetaInformationVersion;
        idx->param[RECORDIDX_FileMetaInformationVersion].ValueLength = UR_MAX_LENGTH;
        idx->FileMetaInformationVersion[0] = '\0';
        idx->param[RECORDIDX_MediaStorageSOPClassUID].XTag = DCM_MediaStorageSOPClassUID;
        idx->param[RECORDIDX_MediaStorageSOPClassUID].ValueLength = UI_MAX_LENGTH;
        idx->MediaStorageSOPClassUID[0] = '\0';
        idx->param[RECORDIDX_MediaStorageSOPInstanceUID].XTag = DCM_MediaStorageSOPInstanceUID;
        idx->param[RECORDIDX_MediaStorageSOPInstanceUID].ValueLength = UI_MAX_LENGTH;
        idx->MediaStorageSOPInstanceUID[0] = '\0';
        idx->param[RECORDIDX_TransferSyntaxUID].XTag = DCM_TransferSyntaxUID;
        idx->param[RECORDIDX_TransferSyntaxUID].ValueLength = UI_MAX_LENGTH;
        idx->TransferSyntaxUID[0] = '\0';
        idx->param[RECORDIDX_ImplementationClassUID].XTag = DCM_ImplementationClassUID;
        idx->param[RECORDIDX_ImplementationClassUID].ValueLength = UI_MAX_LENGTH;
        idx->ImplementationClassUID[0] = '\0';
        idx->param[RECORDIDX_ImplementationVersionName].XTag = DCM_ImplementationVersionName;
        idx->param[RECORDIDX_ImplementationVersionName].ValueLength = SH_MAX_LENGTH;
        idx->ImplementationVersionName[0] = '\0';
        idx->param[RECORDIDX_DirectoryRecordType].XTag = DCM_DirectoryRecordType;
        idx->param[RECORDIDX_DirectoryRecordType].ValueLength = CS_MAX_LENGTH;
        idx->DirectoryRecordType[0] = '\0';
        idx->param[RECORDIDX_ImageType].XTag = DCM_ImageType;
        idx->param[RECORDIDX_ImageType].ValueLength = CS_MAX_LENGTH;
        idx->ImageType[0] = '\0';
        idx->param[RECORDIDX_AcquisitionDate].XTag = DCM_AcquisitionDate;
        idx->param[RECORDIDX_AcquisitionDate].ValueLength = DA_MAX_LENGTH;
        idx->AcquisitionDate[0] = '\0';
        idx->param[RECORDIDX_ContentDate].XTag = DCM_ContentDate;
        idx->param[RECORDIDX_ContentDate].ValueLength = DA_MAX_LENGTH;
        idx->ContentDate[0] = '\0';
        idx->param[RECORDIDX_AcquisitionTime].XTag = DCM_AcquisitionTime;
        idx->param[RECORDIDX_AcquisitionTime].ValueLength = TM_MAX_LENGTH;
        idx->AcquisitionTime[0] = '\0';
        idx->param[RECORDIDX_ContentTime].XTag = DCM_ContentTime;
        idx->param[RECORDIDX_ContentTime].ValueLength = TM_MAX_LENGTH;
        idx->ContentTime[0] = '\0';
        idx->param[RECORDIDX_Manufacturer].XTag = DCM_Manufacturer;
        idx->param[RECORDIDX_Manufacturer].ValueLength = LO_MAX_LENGTH;
        idx->Manufacturer[0] = '\0';
        idx->param[RECORDIDX_ManufacturerModelName].XTag = DCM_ManufacturerModelName;
        idx->param[RECORDIDX_ManufacturerModelName].ValueLength = LO_MAX_LENGTH;
        idx->ManufacturerModelName[0] = '\0';
        idx->param[RECORDIDX_PatientIdentityRemoved].XTag = DCM_PatientIdentityRemoved;
        idx->param[RECORDIDX_PatientIdentityRemoved].ValueLength = CS_MAX_LENGTH;
        idx->PatientIdentityRemoved[0] = '\0';
        idx->param[RECORDIDX_DeidentificationMethod].XTag = DCM_DeidentificationMethod;
        idx->param[RECORDIDX_DeidentificationMethod].ValueLength = LO_MAX_LENGTH;
        idx->DeidentificationMethod[0] = '\0';
        idx->param[RECORDIDX_ContrastBolusAgent].XTag = DCM_ContrastBolusAgent;
        idx->param[RECORDIDX_ContrastBolusAgent].ValueLength = LO_MAX_LENGTH;
        idx->ContrastBolusAgent[0] = '\0';
        idx->param[RECORDIDX_BodyPartExamined].XTag = DCM_BodyPartExamined;
        idx->param[RECORDIDX_BodyPartExamined].ValueLength = CS_MAX_LENGTH;
        idx->BodyPartExamined[0] = '\0';
        idx->param[RECORDIDX_ScanOptions].XTag = DCM_ScanOptions;
        idx->param[RECORDIDX_ScanOptions].ValueLength = CS_MAX_LENGTH;
        idx->ScanOptions[0] = '\0';
        idx->param[RECORDIDX_KVP].XTag = DCM_KVP;
        idx->param[RECORDIDX_KVP].ValueLength = DS_MAX_LENGTH;
        idx->KVP[0] = '\0';
        idx->param[RECORDIDX_DataCollectionDiameter].XTag = DCM_DataCollectionDiameter;
        idx->param[RECORDIDX_DataCollectionDiameter].ValueLength = DS_MAX_LENGTH;
        idx->DataCollectionDiameter[0] = '\0';
        idx->param[RECORDIDX_SoftwareVersions].XTag = DCM_SoftwareVersions;
        idx->param[RECORDIDX_SoftwareVersions].ValueLength = LO_MAX_LENGTH;
        idx->SoftwareVersions[0] = '\0';
        idx->param[RECORDIDX_ReconstructionDiameter].XTag = DCM_ReconstructionDiameter;
        idx->param[RECORDIDX_ReconstructionDiameter].ValueLength = DS_MAX_LENGTH;
        idx->ReconstructionDiameter[0] = '\0';
        idx->param[RECORDIDX_GantryDetectorTilt].XTag = DCM_GantryDetectorTilt;
        idx->param[RECORDIDX_GantryDetectorTilt].ValueLength = DS_MAX_LENGTH;
        idx->GantryDetectorTilt[0] = '\0';
        idx->param[RECORDIDX_TableHeight].XTag = DCM_TableHeight;
        idx->param[RECORDIDX_TableHeight].ValueLength = DS_MAX_LENGTH;
        idx->TableHeight[0] = '\0';
        idx->param[RECORDIDX_RotationDirection].XTag = DCM_RotationDirection;
        idx->param[RECORDIDX_RotationDirection].ValueLength = CS_MAX_LENGTH;
        idx->RotationDirection[0] = '\0';
        idx->param[RECORDIDX_ExposureTime].XTag = DCM_ExposureTime;
        idx->param[RECORDIDX_ExposureTime].ValueLength = IS_MAX_LENGTH;
        idx->ExposureTime[0] = '\0';
        idx->param[RECORDIDX_XRayTubeCurrent].XTag = DCM_XRayTubeCurrent;
        idx->param[RECORDIDX_XRayTubeCurrent].ValueLength = IS_MAX_LENGTH;
        idx->XRayTubeCurrent[0] = '\0';
        idx->param[RECORDIDX_FilterType].XTag = DCM_FilterType;
        idx->param[RECORDIDX_FilterType].ValueLength = SH_MAX_LENGTH;
        idx->FilterType[0] = '\0';
        idx->param[RECORDIDX_GeneratorPower].XTag = DCM_GeneratorPower;
        idx->param[RECORDIDX_GeneratorPower].ValueLength = IS_MAX_LENGTH;
        idx->GeneratorPower[0] = '\0';
        idx->param[RECORDIDX_FocalSpots].XTag = DCM_FocalSpots;
        idx->param[RECORDIDX_FocalSpots].ValueLength = DS_MAX_LENGTH;
        idx->FocalSpots[0] = '\0';
        idx->param[RECORDIDX_ConvolutionKernel].XTag = DCM_ConvolutionKernel;
        idx->param[RECORDIDX_ConvolutionKernel].ValueLength = SH_MAX_LENGTH;
        idx->ConvolutionKernel[0] = '\0';
        idx->param[RECORDIDX_PatientPosition].XTag = DCM_PatientPosition;
        idx->param[RECORDIDX_PatientPosition].ValueLength = SH_MAX_LENGTH;
        idx->PatientPosition[0] = '\0';
        idx->param[RECORDIDX_PatientPosition].XTag = DCM_PatientPosition;
        idx->param[RECORDIDX_PatientPosition].ValueLength = CS_MAX_LENGTH;
        idx->PatientPosition[0] = '\0';
        idx->param[RECORDIDX_ExposureModulationType].XTag = DCM_ExposureModulationType;
        idx->param[RECORDIDX_ExposureModulationType].ValueLength = CS_MAX_LENGTH;
        idx->ExposureModulationType[0] = '\0';
        idx->param[RECORDIDX_EstimatedDoseSaving].XTag = DCM_RETIRED_EstimatedDoseSaving;
        idx->param[RECORDIDX_EstimatedDoseSaving].ValueLength = FD_MAX_LENGTH;
        idx->EstimatedDoseSaving[0] = '\0';
        idx->param[RECORDIDX_CTDIvol].XTag = DCM_CTDIvol;
        idx->param[RECORDIDX_CTDIvol].ValueLength = FD_MAX_LENGTH;
        idx->CTDIvol[0] = '\0';
        idx->param[RECORDIDX_AcquisitionNumber].XTag = DCM_AcquisitionNumber;
        idx->param[RECORDIDX_AcquisitionNumber].ValueLength = IS_MAX_LENGTH;
        idx->AcquisitionNumber[0] = '\0';
        idx->param[RECORDIDX_InstanceNumber].XTag = DCM_InstanceNumber;
        idx->param[RECORDIDX_InstanceNumber].ValueLength = IS_MAX_LENGTH;
        idx->InstanceNumber[0] = '\0';
        idx->param[RECORDIDX_PatientOrientation].XTag = DCM_PatientOrientation;
        idx->param[RECORDIDX_PatientOrientation].ValueLength = IS_MAX_LENGTH;
        idx->PatientOrientation[0] = '\0';
        idx->param[RECORDIDX_ImagePositionPatient].XTag = DCM_ImagePositionPatient;
        idx->param[RECORDIDX_ImagePositionPatient].ValueLength = DS_MAX_LENGTH;
        idx->ImagePositionPatient[0] = '\0';
        idx->param[RECORDIDX_ImageOrientationPatient].XTag = DCM_ImageOrientationPatient;
        idx->param[RECORDIDX_ImageOrientationPatient].ValueLength = DS_MAX_LENGTH;
        idx->ImageOrientationPatient[0] = '\0';
        idx->param[RECORDIDX_FrameOfReferenceUID].XTag = DCM_FrameOfReferenceUID;
        idx->param[RECORDIDX_FrameOfReferenceUID].ValueLength = UI_MAX_LENGTH;
        idx->FrameOfReferenceUID[0] = '\0';
        idx->param[RECORDIDX_PositionReferenceIndicator].XTag = DCM_PositionReferenceIndicator;
        idx->param[RECORDIDX_PositionReferenceIndicator].ValueLength = LO_MAX_LENGTH;
        idx->PositionReferenceIndicator[0] = '\0';
        idx->param[RECORDIDX_SliceLocation].XTag = DCM_SliceLocation;
        idx->param[RECORDIDX_SliceLocation].ValueLength = DS_MAX_LENGTH;
        idx->SliceLocation[0] = '\0';
        idx->param[RECORDIDX_SamplesPerPixel].XTag = DCM_SamplesPerPixel;
        idx->param[RECORDIDX_SamplesPerPixel].ValueLength = US_MAX_LENGTH;
        idx->SamplesPerPixel[0] = '\0';
        idx->param[RECORDIDX_PhotometricInterpretation].XTag = DCM_PhotometricInterpretation;
        idx->param[RECORDIDX_PhotometricInterpretation].ValueLength = CS_MAX_LENGTH;
        idx->PhotometricInterpretation[0] = '\0';
        idx->param[RECORDIDX_PlanarConfiguration].XTag = DCM_PlanarConfiguration;
        idx->param[RECORDIDX_PlanarConfiguration].ValueLength = US_MAX_LENGTH;
        idx->PlanarConfiguration[0] = '\0';
        idx->param[RECORDIDX_NumberOfFrames].XTag = DCM_NumberOfFrames;
        idx->param[RECORDIDX_NumberOfFrames].ValueLength = IS_MAX_LENGTH;
        idx->NumberOfFrames[0] = '\0';
        idx->param[RECORDIDX_Rows].XTag = DCM_Rows;
        idx->param[RECORDIDX_Rows].ValueLength = US_MAX_LENGTH;
        idx->Rows[0] = '\0';
        idx->param[RECORDIDX_Columns].XTag = DCM_Columns;
        idx->param[RECORDIDX_Columns].ValueLength = US_MAX_LENGTH;
        idx->Columns[0] = '\0';
        idx->param[RECORDIDX_PixelSpacing].XTag = DCM_PixelSpacing;
        idx->param[RECORDIDX_PixelSpacing].ValueLength = DS_MAX_LENGTH;
        idx->PixelSpacing[0] = '\0';
        idx->param[RECORDIDX_PixelAspectRatio].XTag = DCM_PixelAspectRatio;
        idx->param[RECORDIDX_PixelAspectRatio].ValueLength = IS_MAX_LENGTH;
        idx->PixelAspectRatio[0] = '\0';
        idx->param[RECORDIDX_BitsAllocated].XTag = DCM_BitsAllocated;
        idx->param[RECORDIDX_BitsAllocated].ValueLength = US_MAX_LENGTH;
        idx->BitsAllocated[0] = '\0';
        idx->param[RECORDIDX_BitsStored].XTag = DCM_BitsStored;
        idx->param[RECORDIDX_BitsStored].ValueLength = US_MAX_LENGTH;
        idx->BitsStored[0] = '\0';
        idx->param[RECORDIDX_HighBit].XTag = DCM_HighBit;
        idx->param[RECORDIDX_HighBit].ValueLength = US_MAX_LENGTH;
        idx->HighBit[0] = '\0';
        idx->param[RECORDIDX_PixelRepresentation].XTag = DCM_PixelRepresentation;
        idx->param[RECORDIDX_PixelRepresentation].ValueLength = US_MAX_LENGTH;
        idx->PixelRepresentation[0] = '\0';
        idx->param[RECORDIDX_WindowCenter].XTag = DCM_WindowCenter;
        idx->param[RECORDIDX_WindowCenter].ValueLength = DS_MAX_LENGTH;
        idx->WindowCenter[0] = '\0';
        idx->param[RECORDIDX_WindowWidth].XTag = DCM_WindowWidth;
        idx->param[RECORDIDX_WindowWidth].ValueLength = DS_MAX_LENGTH;
        idx->WindowWidth[0] = '\0';
        idx->param[RECORDIDX_RescaleIntercept].XTag = DCM_RescaleIntercept;
        idx->param[RECORDIDX_RescaleIntercept].ValueLength = DS_MAX_LENGTH;
        idx->RescaleIntercept[0] = '\0';
        idx->param[RECORDIDX_RescaleSlope].XTag = DCM_RescaleSlope;
        idx->param[RECORDIDX_RescaleSlope].ValueLength = DS_MAX_LENGTH;
        idx->RescaleSlope[0] = '\0';
        idx->param[RECORDIDX_StorageMediaFileSetID].XTag = DCM_StorageMediaFileSetID;
        idx->param[RECORDIDX_StorageMediaFileSetID].ValueLength = SH_MAX_LENGTH;
        idx->StorageMediaFileSetID[0] = '\0';
        idx->param[RECORDIDX_StorageMediaFileSetUID].XTag = DCM_StorageMediaFileSetUID;
        idx->param[RECORDIDX_StorageMediaFileSetUID].ValueLength = UI_MAX_LENGTH;
        idx->StorageMediaFileSetUID[0] = '\0';
        idx->param[RECORDIDX_PixelData].XTag = DCM_PixelData;
        idx->param[RECORDIDX_PixelData].ValueLength = UR_MAX_LENGTH;
        idx->PixelData[0] = '\0';

    }
    idx->param[RECORDIDX_PatientBirthDate].PValueField = (char*)idx->PatientBirthDate;
    idx->param[RECORDIDX_PatientSex].PValueField = (char*)idx->PatientSex;
    idx->param[RECORDIDX_PatientName].PValueField = (char*)idx->PatientName;
    idx->param[RECORDIDX_PatientID].PValueField = (char*)idx->PatientID;
    idx->param[RECORDIDX_PatientBirthTime].PValueField = (char*)idx->PatientBirthTime;
    idx->param[RECORDIDX_OtherPatientIDs].PValueField = (char*)idx->OtherPatientIDs;
    idx->param[RECORDIDX_OtherPatientNames].PValueField = (char*)idx->OtherPatientNames;
    idx->param[RECORDIDX_EthnicGroup].PValueField = (char*)idx->EthnicGroup;
    idx->param[RECORDIDX_StudyDate].PValueField = (char*)idx->StudyDate;
    idx->param[RECORDIDX_StudyTime].PValueField = (char*)idx->StudyTime;
    idx->param[RECORDIDX_StudyID].PValueField = (char*)idx->StudyID;
    idx->param[RECORDIDX_StudyDescription].PValueField = (char*)idx->StudyDescription;
    idx->param[RECORDIDX_NameOfPhysiciansReadingStudy].PValueField = (char*)idx->NameOfPhysiciansReadingStudy;
    idx->param[RECORDIDX_AccessionNumber].PValueField = (char*)idx->AccessionNumber;
    idx->param[RECORDIDX_ReferringPhysicianName].PValueField = (char*)idx->ReferringPhysicianName;
    idx->param[RECORDIDX_ProcedureDescription].PValueField = (char*)idx->ProcedureDescription;
    idx->param[RECORDIDX_AttendingPhysiciansName].PValueField = (char*)idx->AttendingPhysiciansName;
    idx->param[RECORDIDX_StudyInstanceUID].PValueField = (char*)idx->StudyInstanceUID;
    idx->param[RECORDIDX_OtherStudyNumbers].PValueField = (char*)idx->OtherStudyNumbers;
    idx->param[RECORDIDX_AdmittingDiagnosesDescription].PValueField = (char*)idx->AdmittingDiagnosesDescription;
    idx->param[RECORDIDX_PatientAge].PValueField = (char*)idx->PatientAge;
    idx->param[RECORDIDX_PatientSize].PValueField = (char*)idx->PatientSize;
    idx->param[RECORDIDX_PatientWeight].PValueField = (char*)idx->PatientWeight;
    idx->param[RECORDIDX_Occupation].PValueField = (char*)idx->Occupation;
    idx->param[RECORDIDX_SeriesNumber].PValueField = (char*)idx->SeriesNumber;
    idx->param[RECORDIDX_SeriesInstanceUID].PValueField = (char*)idx->SeriesInstanceUID;
    idx->param[RECORDIDX_Modality].PValueField = (char*)idx->Modality;
    idx->param[RECORDIDX_ImageNumber].PValueField = (char*)idx->ImageNumber;
    idx->param[RECORDIDX_SOPInstanceUID].PValueField = (char*)idx->SOPInstanceUID;
    idx->param[RECORDIDX_SeriesDate].PValueField = (char*)idx->SeriesDate;
    idx->param[RECORDIDX_SeriesTime].PValueField = (char*)idx->SeriesTime;
    idx->param[RECORDIDX_SeriesDescription].PValueField = (char*)idx->SeriesDescription;
    idx->param[RECORDIDX_ProtocolName].PValueField = (char*)idx->ProtocolName;
    idx->param[RECORDIDX_OperatorsName].PValueField = (char*)idx->OperatorsName;
    idx->param[RECORDIDX_PerformingPhysicianName].PValueField = (char*)idx->PerformingPhysicianName;
    idx->param[RECORDIDX_PresentationLabel].PValueField = (char*)idx->PresentationLabel;
    idx->param[RECORDIDX_IssuerOfPatientID].PValueField = (char*)idx->IssuerOfPatientID;
    idx->param[RECORDIDX_SpecificCharacterSet].PValueField = (char*)idx->SpecificCharacterSet;

    idx->param[RECORDIDX_MediaStorageSOPClassUID].PValueField = (char*)idx->MediaStorageSOPClassUID;
    idx->param[RECORDIDX_MediaStorageSOPInstanceUID].PValueField = (char*)idx->MediaStorageSOPInstanceUID;
    idx->param[RECORDIDX_TransferSyntaxUID].PValueField = (char*)idx->TransferSyntaxUID;
    idx->param[RECORDIDX_ImplementationClassUID].PValueField = (char*)idx->ImplementationClassUID;
    idx->param[RECORDIDX_ImplementationVersionName].PValueField = (char*)idx->ImplementationVersionName;
    idx->param[RECORDIDX_DirectoryRecordType].PValueField = (char*)idx->DirectoryRecordType;
    idx->param[RECORDIDX_ImageType].PValueField = (char*)idx->ImageType;
    idx->param[RECORDIDX_AcquisitionDate].PValueField = (char*)idx->AcquisitionDate;
    idx->param[RECORDIDX_ContentDate].PValueField = (char*)idx->ContentDate;
    idx->param[RECORDIDX_AcquisitionTime].PValueField = (char*)idx->AcquisitionTime;
    idx->param[RECORDIDX_ContentTime].PValueField = (char*)idx->ContentTime;
    idx->param[RECORDIDX_Manufacturer].PValueField = (char*)idx->Manufacturer;
    idx->param[RECORDIDX_ManufacturerModelName].PValueField = (char*)idx->ManufacturerModelName;
    idx->param[RECORDIDX_PatientIdentityRemoved].PValueField = (char*)idx->PatientIdentityRemoved;
    idx->param[RECORDIDX_DeidentificationMethod].PValueField = (char*)idx->DeidentificationMethod;
    idx->param[RECORDIDX_ContrastBolusAgent].PValueField = (char*)idx->ContrastBolusAgent;
    idx->param[RECORDIDX_BodyPartExamined].PValueField = (char*)idx->BodyPartExamined;
    idx->param[RECORDIDX_ScanOptions].PValueField = (char*)idx->ScanOptions;
    idx->param[RECORDIDX_KVP].PValueField = (char*)idx->KVP;
    idx->param[RECORDIDX_DataCollectionDiameter].PValueField = (char*)idx->DataCollectionDiameter;
    idx->param[RECORDIDX_SoftwareVersions].PValueField = (char*)idx->SoftwareVersions;
    idx->param[RECORDIDX_ReconstructionDiameter].PValueField = (char*)idx->ReconstructionDiameter;
    idx->param[RECORDIDX_GantryDetectorTilt].PValueField = (char*)idx->GantryDetectorTilt;
    idx->param[RECORDIDX_TableHeight].PValueField = (char*)idx->TableHeight;
    idx->param[RECORDIDX_RotationDirection].PValueField = (char*)idx->RotationDirection;
    idx->param[RECORDIDX_ExposureTime].PValueField = (char*)idx->ExposureTime;
    idx->param[RECORDIDX_XRayTubeCurrent].PValueField = (char*)idx->XRayTubeCurrent;
    idx->param[RECORDIDX_FilterType].PValueField = (char*)idx->FilterType;
    idx->param[RECORDIDX_GeneratorPower].PValueField = (char*)idx->GeneratorPower;
    idx->param[RECORDIDX_FocalSpots].PValueField = (char*)idx->FocalSpots;
    idx->param[RECORDIDX_ConvolutionKernel].PValueField = (char*)idx->ConvolutionKernel;
    idx->param[RECORDIDX_PatientPosition].PValueField = (char*)idx->PatientPosition;
    idx->param[RECORDIDX_ExposureModulationType].PValueField = (char*)idx->ExposureModulationType;
    idx->param[RECORDIDX_EstimatedDoseSaving].PValueField = (char*)idx->EstimatedDoseSaving;
    idx->param[RECORDIDX_CTDIvol].PValueField = (char*)idx->CTDIvol;
    idx->param[RECORDIDX_AcquisitionNumber].PValueField = (char*)idx->AcquisitionNumber;
    idx->param[RECORDIDX_InstanceNumber].PValueField = (char*)idx->InstanceNumber;
    idx->param[RECORDIDX_PatientOrientation].PValueField = (char*)idx->PatientOrientation;
    idx->param[RECORDIDX_ImagePositionPatient].PValueField = (char*)idx->ImagePositionPatient;
    idx->param[RECORDIDX_ImageOrientationPatient].PValueField = (char*)idx->ImageOrientationPatient;
    idx->param[RECORDIDX_FrameOfReferenceUID].PValueField = (char*)idx->FrameOfReferenceUID;
    idx->param[RECORDIDX_PositionReferenceIndicator].PValueField = (char*)idx->PositionReferenceIndicator;
    idx->param[RECORDIDX_SliceLocation].PValueField = (char*)idx->SliceLocation;
    idx->param[RECORDIDX_SamplesPerPixel].PValueField = (char*)idx->SamplesPerPixel;
    idx->param[RECORDIDX_PhotometricInterpretation].PValueField = (char*)idx->PhotometricInterpretation;
    idx->param[RECORDIDX_PlanarConfiguration].PValueField = (char*)idx->PlanarConfiguration;
    idx->param[RECORDIDX_NumberOfFrames].PValueField = (char*)idx->NumberOfFrames;
    idx->param[RECORDIDX_Rows].PValueField = (char*)idx->Rows;
    idx->param[RECORDIDX_Columns].PValueField = (char*)idx->Columns;
    idx->param[RECORDIDX_PixelSpacing].PValueField = (char*)idx->PixelSpacing;
    idx->param[RECORDIDX_PixelAspectRatio].PValueField = (char*)idx->PixelAspectRatio;
    idx->param[RECORDIDX_BitsAllocated].PValueField = (char*)idx->BitsAllocated;
    idx->param[RECORDIDX_BitsStored].PValueField = (char*)idx->BitsStored;
    idx->param[RECORDIDX_HighBit].PValueField = (char*)idx->HighBit;
    idx->param[RECORDIDX_PixelRepresentation].PValueField = (char*)idx->PixelRepresentation;
    idx->param[RECORDIDX_WindowCenter].PValueField = (char*)idx->WindowCenter;
    idx->param[RECORDIDX_WindowWidth].PValueField = (char*)idx->WindowWidth;
    idx->param[RECORDIDX_RescaleIntercept].PValueField = (char*)idx->RescaleIntercept;
    idx->param[RECORDIDX_RescaleSlope].PValueField = (char*)idx->RescaleSlope;
    idx->param[RECORDIDX_StorageMediaFileSetID].PValueField = (char*)idx->StorageMediaFileSetID;
    idx->param[RECORDIDX_StorageMediaFileSetUID].PValueField = (char*)idx->StorageMediaFileSetUID;
    idx->param[RECORDIDX_PixelData].PValueField = (char*)idx->PixelData;
}

/************
**      Search if an Index Record has already been found
*/
static int DB_UIDAlreadyFound(
    DB_Private_Handle* phandle,
    IdxRecord* idxRec
)
{
    DB_UidList* plist;

    for (plist = phandle->uidList; plist; plist = plist->next) {
        if (((int)phandle->queryLevel >= PATIENT_LEVEL)
            && (strcmp(plist->patient, (char*)idxRec->PatientID) != 0)
            )
            continue;
        if (((int)phandle->queryLevel >= STUDY_LEVEL)
            && (strcmp(plist->study, (char*)idxRec->StudyInstanceUID) != 0)
            )
            continue;
        if (((int)phandle->queryLevel >= SERIE_LEVEL)
            && (strcmp(plist->serie, (char*)idxRec->SeriesInstanceUID) != 0)
            )
            continue;
        if (((int)phandle->queryLevel >= IMAGE_LEVEL)
            && (strcmp(plist->image, (char*)idxRec->SOPInstanceUID) != 0)
            )
            continue;
        return (OFTrue);
    }
    return (OFFalse);
}


void GetParamInBson(const bson_t* i_bson, bson_iter_t& iter, std::string tag, char& dest)
{
    if (strcmp(bson_iter_key(&iter), tag.c_str()) == 0)
    {
        //std::cout << "param type=" << bson_iter_type(&iter) << std::endl;
        const uint8_t* param_bson_data;
        uint32_t param_bson_data_size;
        bson_t* param_bson;
        bson_iter_document(&iter, &param_bson_data_size, &param_bson_data);
        param_bson = bson_new_from_data(param_bson_data, param_bson_data_size);
        bson_iter_t param_iter;
        if (bson_iter_init(&param_iter, param_bson))
        {
            std::string vrType = "UI";
            while (bson_iter_next(&param_iter))
            {
                if (strcmp(bson_iter_key(&param_iter), "vr") == 0)
                {
                    vrType = bson_iter_utf8(&param_iter, 0);
                }
                if (strcmp(bson_iter_key(&param_iter), "BulkDataURI") == 0)
                {
                    strcpy(&dest, bson_iter_utf8(&param_iter, 0));
                }
                if (strcmp(bson_iter_key(&param_iter), "Value") == 0)
                {
                    const bson_value_t* array_value;
                    bson_iter_t array_iter;
                    if (BSON_ITER_HOLDS_ARRAY(&param_iter) && bson_iter_recurse(&param_iter, &array_iter)) 
                    {
                        while (bson_iter_next(&array_iter)) 
                        {
                            if (strcmp(vrType.c_str(), "PN") == 0)
                            {
                                if (BSON_ITER_HOLDS_DOCUMENT(&array_iter)) 
                                {
                                    const uint8_t* array_data;
                                    uint32_t array_len;
                                    bson_iter_document(&array_iter, &array_len, &array_data);
                                    bson_t* array_document = bson_new_from_data(array_data, array_len);
                                    bson_iter_t value_iter;
                                    if (bson_iter_init_find(&value_iter, array_document, "Alphabetic") && BSON_ITER_HOLDS_UTF8(&value_iter)) 
                                    {
                                        const char* alphabetic_value = bson_iter_utf8(&value_iter, NULL);
                                        strcpy(&dest, bson_iter_utf8(&value_iter, 0));
                                        break;
                                    }
                                    bson_destroy(array_document);
                                }
                            }
                            else
                            {
                                if (BSON_ITER_HOLDS_UTF8(&array_iter))
                                {
                                    strcpy(&dest, bson_iter_utf8(&array_iter, 0));
                                }
                                else if (BSON_ITER_HOLDS_INT(&array_iter))
                                {
                                    strcpy(&dest, std::to_string(bson_iter_int32(&array_iter)).c_str());
                                }
                                else if (BSON_ITER_HOLDS_DATE_TIME(&array_iter))
                                {
                                    int64_t timestamp = bson_iter_date_time(&array_iter);
                                    time_t time = timestamp / 1000; // 將毫秒轉換為秒

                                    struct tm* tm_info;
                                    char buffer[26];
                                    tm_info = localtime(&time);
                                    strftime(buffer, sizeof(buffer), "%Y%m%d", tm_info);

                                    std::string formattedDate(buffer);
                                    strcpy(&dest, formattedDate.c_str());
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
}

/*
* 從資料庫讀取所需的bson資料並轉換成IdxRecord
*/
IdxRecord* bson_to_idx_record(const bson_t* i_bson, IdxRecord& theRec)
{
    DB_IdxInitRecord(&theRec, 0);

    bson_iter_t iter;
    if (bson_iter_init(&iter, i_bson)) {
        while (bson_iter_next(&iter)) {
            if (strcmp(bson_iter_key(&iter), "instancePath") == 0)
            {
                std::string filename = std::string(DICOM_STORE_ROOTPATH) + '/' + std::string(bson_iter_utf8(&iter, 0));
                ConsoleLog("Filename:" + filename, "");
                strcpy(theRec.filename, filename.c_str());
            }
            GetParamInBson(i_bson,iter,"00080016",*theRec.SOPClassUID); //(strcmp(bson_iter_key(&iter), "SOPClassUID") == 0) ? strcpy(theRec.SOPClassUID, bson_iter_utf8(&iter, 0)) : "";
            //(strcmp(bson_iter_key(&iter), "RecordedDate") == 0) ? theRec.RecordedDate = bson_iter_double(&iter) : 0.0;
            //(strcmp(bson_iter_key(&iter), "ImageSize") == 0) ? theRec.ImageSize = bson_iter_int32(&iter) : 0;
            GetParamInBson(i_bson, iter, "00100030", *theRec.PatientBirthDate); //(strcmp(bson_iter_key(&iter), "PatientBirthDate") == 0) ? strcpy(theRec.PatientBirthDate, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00100040", *theRec.PatientSex); //(strcmp(bson_iter_key(&iter), "PatientSex") == 0) ? strcpy(theRec.PatientSex, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00100010", *theRec.PatientName); //(strcmp(bson_iter_key(&iter), "PatientName") == 0) ? strcpy(theRec.PatientName, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00100020", *theRec.PatientID); //(strcmp(bson_iter_key(&iter), "PatientID") == 0) ? strcpy(theRec.PatientID, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00100032", *theRec.PatientBirthTime); //(strcmp(bson_iter_key(&iter), "PatientBirthTime") == 0) ? strcpy(theRec.PatientBirthTime, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00101000", *theRec.OtherPatientIDs); //(strcmp(bson_iter_key(&iter), "OtherPatientIDs") == 0) ? strcpy(theRec.OtherPatientIDs, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00100040", *theRec.PatientSex); (strcmp(bson_iter_key(&iter), "OtherPatientNames") == 0) ? strcpy(theRec.OtherPatientNames, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00102160", *theRec.EthnicGroup); //(strcmp(bson_iter_key(&iter), "EthnicGroup") == 0) ? strcpy(theRec.EthnicGroup, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00080020", *theRec.StudyDate); //(strcmp(bson_iter_key(&iter), "StudyDate") == 0) ? strcpy(theRec.StudyDate, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00080030", *theRec.StudyTime); //(strcmp(bson_iter_key(&iter), "StudyTime") == 0) ? strcpy(theRec.StudyTime, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00200010", *theRec.StudyID); //(strcmp(bson_iter_key(&iter), "StudyID") == 0) ? strcpy(theRec.StudyID, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00081030", *theRec.StudyDescription); //(strcmp(bson_iter_key(&iter), "StudyDescription") == 0) ? strcpy(theRec.StudyDescription, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00081060", *theRec.NameOfPhysiciansReadingStudy); //(strcmp(bson_iter_key(&iter), "NameOfPhysiciansReadingStudy") == 0) ? strcpy(theRec.NameOfPhysiciansReadingStudy, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00080050", *theRec.AccessionNumber); //(strcmp(bson_iter_key(&iter), "AccessionNumber") == 0) ? strcpy(theRec.AccessionNumber, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00080090", *theRec.ReferringPhysicianName); //(strcmp(bson_iter_key(&iter), "ReferringPhysicianName") == 0) ? strcpy(theRec.ReferringPhysicianName, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00321060", *theRec.ProcedureDescription); //(strcmp(bson_iter_key(&iter), "ProcedureDescription") == 0) ? strcpy(theRec.ProcedureDescription, bson_iter_utf8(&iter, 0)) : "";
            //GetParamInBson(i_bson, iter, "00100040", *theRec.AttendingPhysiciansName); //(strcmp(bson_iter_key(&iter), "AttendingPhysiciansName") == 0) ? strcpy(theRec.AttendingPhysiciansName, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "0020000D", *theRec.StudyInstanceUID); //(strcmp(bson_iter_key(&iter), "StudyInstanceUID") == 0) ? strcpy(theRec.StudyInstanceUID, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00201070", *theRec.OtherStudyNumbers); //(strcmp(bson_iter_key(&iter), "OtherStudyNumbers") == 0) ? strcpy(theRec.OtherStudyNumbers, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00081080", *theRec.AdmittingDiagnosesDescription); //(strcmp(bson_iter_key(&iter), "AdmittingDiagnosesDescription") == 0) ? strcpy(theRec.AdmittingDiagnosesDescription, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00101010", *theRec.PatientAge); //(strcmp(bson_iter_key(&iter), "PatientAge") == 0) ? strcpy(theRec.PatientAge, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00100020", *theRec.PatientSize); //(strcmp(bson_iter_key(&iter), "PatientSize") == 0) ? strcpy(theRec.PatientSize, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00101030", *theRec.PatientWeight); //(strcmp(bson_iter_key(&iter), "PatientWeight") == 0) ? strcpy(theRec.PatientWeight, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00102180", *theRec.Occupation); //(strcmp(bson_iter_key(&iter), "Occupation") == 0) ? strcpy(theRec.Occupation, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00200011", *theRec.SeriesNumber); //(strcmp(bson_iter_key(&iter), "SeriesNumber") == 0) ? strcpy(theRec.SeriesNumber, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "0020000E", *theRec.SeriesInstanceUID); //(strcmp(bson_iter_key(&iter), "SeriesInstanceUID") == 0) ? strcpy(theRec.SeriesInstanceUID, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00080060", *theRec.Modality); //(strcmp(bson_iter_key(&iter), "Modality") == 0) ? strcpy(theRec.Modality, bson_iter_utf8(&iter, 0)) : "";
            //GetParamInBson(i_bson, iter, "00100040", *theRec.ImageNumber); //(strcmp(bson_iter_key(&iter), "ImageNumber") == 0) ? strcpy(theRec.ImageNumber, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00080018", *theRec.SOPInstanceUID); //(strcmp(bson_iter_key(&iter), "SOPInstanceUID") == 0) ? strcpy(theRec.SOPInstanceUID, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00080021", *theRec.SeriesDate); //(strcmp(bson_iter_key(&iter), "SeriesDate") == 0) ? strcpy(theRec.SeriesDate, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00080031", *theRec.SeriesTime); //(strcmp(bson_iter_key(&iter), "SeriesTime") == 0) ? strcpy(theRec.SeriesTime, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "0008103E", *theRec.SeriesDescription); //(strcmp(bson_iter_key(&iter), "SeriesDescription") == 0) ? strcpy(theRec.SeriesDescription, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00181030", *theRec.ProtocolName); //(strcmp(bson_iter_key(&iter), "ProtocolName") == 0) ? strcpy(theRec.ProtocolName, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00081070", *theRec.OperatorsName); //(strcmp(bson_iter_key(&iter), "OperatorsName") == 0) ? strcpy(theRec.OperatorsName, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00081050", *theRec.PerformingPhysicianName); //(strcmp(bson_iter_key(&iter), "PerformingPhysicianName") == 0) ? strcpy(theRec.PerformingPhysicianName, bson_iter_utf8(&iter, 0)) : "";
            //GetParamInBson(i_bson, iter, "00100040", *theRec.PresentationLabel); //(strcmp(bson_iter_key(&iter), "PresentationLabel") == 0) ? strcpy(theRec.PresentationLabel, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00100021", *theRec.IssuerOfPatientID); //(strcmp(bson_iter_key(&iter), "IssuerOfPatientID") == 0) ? strcpy(theRec.IssuerOfPatientID, bson_iter_utf8(&iter, 0)) : "";
            //GetParamInBson(i_bson, iter, "00100040", *theRec.InstanceDescription); //(strcmp(bson_iter_key(&iter), "InstanceDescription") == 0) ? strcpy(theRec.InstanceDescription, bson_iter_utf8(&iter, 0)) : "";
            GetParamInBson(i_bson, iter, "00080005", *theRec.SpecificCharacterSet); //(strcmp(bson_iter_key(&iter), "SpecificCharacterSet") == 0) ? strcpy(theRec.SpecificCharacterSet, bson_iter_utf8(&iter, 0)) : "";

            GetParamInBson(i_bson, iter, "00020001", *theRec.FileMetaInformationVersion);
            GetParamInBson(i_bson, iter, "00020002", *theRec.MediaStorageSOPClassUID);
            GetParamInBson(i_bson, iter, "00020003", *theRec.MediaStorageSOPInstanceUID);
            GetParamInBson(i_bson, iter, "00020010", *theRec.TransferSyntaxUID);
            GetParamInBson(i_bson, iter, "00020012", *theRec.ImplementationClassUID);
            GetParamInBson(i_bson, iter, "00020013", *theRec.ImplementationVersionName);
            GetParamInBson(i_bson, iter, "00041230", *theRec.DirectoryRecordType);
            GetParamInBson(i_bson, iter, "00080008", *theRec.ImageType);
            GetParamInBson(i_bson, iter, "00080022", *theRec.AcquisitionDate);
            GetParamInBson(i_bson, iter, "00080023", *theRec.ContentDate);
            GetParamInBson(i_bson, iter, "00080032", *theRec.AcquisitionTime);
            GetParamInBson(i_bson, iter, "00080033", *theRec.ContentTime);
            GetParamInBson(i_bson, iter, "00080070", *theRec.Manufacturer);
            GetParamInBson(i_bson, iter, "00080080", *theRec.ManufacturerModelName);
            GetParamInBson(i_bson, iter, "00120063", *theRec.PatientIdentityRemoved);
            GetParamInBson(i_bson, iter, "00120064", *theRec.DeidentificationMethod);
            GetParamInBson(i_bson, iter, "00180010", *theRec.ContrastBolusAgent);
            GetParamInBson(i_bson, iter, "00180015", *theRec.BodyPartExamined);
            GetParamInBson(i_bson, iter, "00180021", *theRec.ScanOptions);
            GetParamInBson(i_bson, iter, "00180060", *theRec.KVP);
            GetParamInBson(i_bson, iter, "00189315", *theRec.DataCollectionDiameter);
            GetParamInBson(i_bson, iter, "00181020", *theRec.SoftwareVersions);
            GetParamInBson(i_bson, iter, "00181151", *theRec.ReconstructionDiameter);
            GetParamInBson(i_bson, iter, "00181190", *theRec.GantryDetectorTilt);
            GetParamInBson(i_bson, iter, "00181130", *theRec.TableHeight);
            GetParamInBson(i_bson, iter, "00181140", *theRec.RotationDirection);
            GetParamInBson(i_bson, iter, "00181150", *theRec.ExposureTime);
            GetParamInBson(i_bson, iter, "00181151", *theRec.XRayTubeCurrent);
            GetParamInBson(i_bson, iter, "00181160", *theRec.FilterType);
            GetParamInBson(i_bson, iter, "00181170", *theRec.GeneratorPower);
            GetParamInBson(i_bson, iter, "00181190", *theRec.FocalSpots);
            GetParamInBson(i_bson, iter, "00181210", *theRec.ConvolutionKernel);
            GetParamInBson(i_bson, iter, "00185100", *theRec.PatientPosition);
            GetParamInBson(i_bson, iter, "00189323", *theRec.ExposureModulationType);
            GetParamInBson(i_bson, iter, "00189324", *theRec.EstimatedDoseSaving);
            GetParamInBson(i_bson, iter, "00189345", *theRec.CTDIvol);
            GetParamInBson(i_bson, iter, "00200012", *theRec.AcquisitionNumber);
            GetParamInBson(i_bson, iter, "00200013", *theRec.InstanceNumber);
            GetParamInBson(i_bson, iter, "00200020", *theRec.PatientOrientation);
            GetParamInBson(i_bson, iter, "00200032", *theRec.ImagePositionPatient);
            GetParamInBson(i_bson, iter, "00200037", *theRec.ImageOrientationPatient);
            GetParamInBson(i_bson, iter, "00200052", *theRec.FrameOfReferenceUID);
            GetParamInBson(i_bson, iter, "00201040", *theRec.PositionReferenceIndicator);
            GetParamInBson(i_bson, iter, "00201041", *theRec.SliceLocation);
            GetParamInBson(i_bson, iter, "00280002", *theRec.SamplesPerPixel);
            GetParamInBson(i_bson, iter, "00280004", *theRec.PhotometricInterpretation);
            GetParamInBson(i_bson, iter, "00280006", *theRec.PlanarConfiguration);
            GetParamInBson(i_bson, iter, "00280008", *theRec.NumberOfFrames);
            GetParamInBson(i_bson, iter, "00280010", *theRec.Rows);
            GetParamInBson(i_bson, iter, "00280011", *theRec.Columns);
            GetParamInBson(i_bson, iter, "00280030", *theRec.PixelSpacing);
            GetParamInBson(i_bson, iter, "00280034", *theRec.PixelAspectRatio);
            GetParamInBson(i_bson, iter, "00280100", *theRec.BitsAllocated);
            GetParamInBson(i_bson, iter, "00280101", *theRec.BitsStored);
            GetParamInBson(i_bson, iter, "00280102", *theRec.HighBit);
            GetParamInBson(i_bson, iter, "00280103", *theRec.PixelRepresentation);
            GetParamInBson(i_bson, iter, "00281050", *theRec.WindowCenter);
            GetParamInBson(i_bson, iter, "00281051", *theRec.WindowWidth);
            GetParamInBson(i_bson, iter, "00281052", *theRec.RescaleIntercept);
            GetParamInBson(i_bson, iter, "00281053", *theRec.RescaleSlope);
            GetParamInBson(i_bson, iter, "00880130", *theRec.StorageMediaFileSetID);
            GetParamInBson(i_bson, iter, "00880140", *theRec.StorageMediaFileSetUID);
            GetParamInBson(i_bson, iter, "7FE00010", *theRec.PixelData);

            //printf("Found a field named: %s\nvalue:%s\n", bson_iter_key(&iter), bson_iter_utf8(&iter, 0));
        }
    }
    DB_IdxInitRecord(&theRec, 1);
    return &theRec;
}

/*
* 取得MongoDB的Query bson.
*/
bson_t* mongoDBGetQuery(DB_Private_Handle* phandle)
{
    DB_ElementList* plist;

    // 將query轉bson
    bson_t* query;
    query = bson_new();
    for (plist = phandle->findRequestList; plist; plist = plist->next)
    {
        if (plist->elem.PValueField.ptr.p != NULL)
        {
            std::string xtag = int_to_hex(plist->elem.XTag.key[0]) + int_to_hex(plist->elem.XTag.key[1]);
            transform(xtag.begin(), xtag.end(), xtag.begin(), ::toupper);
            std::string queryKey = xtag + ".Value.0";

            std::string queryValue = plist->elem.PValueField.ptr.p;

            ConsoleLog("QueryKey[" + queryKey + "]", "");
            ConsoleLog("Xtag[" + xtag + "]", "");

            if (strcmp(xtag.c_str(), "0020000D") == 0)
            {
                BSON_APPEND_UTF8(query, queryKey.c_str(), queryValue.c_str());
            }
            else if (strcmp(xtag.c_str(), "0020000E") == 0)
            {
                BSON_APPEND_UTF8(query, queryKey.c_str(), queryValue.c_str());
            }
            else
            {
                queryValue = "^" + queryValue;
                queryValue = ReplaceString(queryValue, "*", ".*");
                queryValue = ReplaceString(queryValue, "?", ".");
                if (strcmp(xtag.c_str(), "00100010") == 0)
                {
                    std::string queryKey2 = xtag + ".Value.0.Alphabetic";
                    bson_append_regex(query, queryKey2.c_str(), -1, queryValue.c_str(), "");
                }
                else
                {
                    bson_append_regex(query, queryKey.c_str(), -1, queryValue.c_str(), "");
                }
            }
            ConsoleLog("QueryValue[" + queryValue + "]", "");
        }
    }

    return query;
}

/*
* 從MongoDB利用C-Find的參數找尋資料
*/
OFBool DcmQueryRetrieveIndexDatabaseHandle::mongoDBFindRecord(DB_Private_Handle* phandle, IdxRecord& idxRec, DB_LEVEL level, DB_LEVEL infLevel, int* match, CharsetConsideringMatcher& dbmatch)
{
    int                 i;
    DcmTagKey   XTag;
    DB_ElementList* plist;
    DB_LEVEL    XTagLevel = PATIENT_LEVEL; // DB_GetTagLevel() will set this correctly
    OFBool foundAnything = OFFalse;
    ConsoleLog("Start MongoDB C-FIND", "");

    // MongoDB連接
    mongoc_client_t* mongoClient;
    mongoc_uri_t* uri;
    bson_error_t mongoError;

    mongoc_init();
    uri = mongoc_uri_new_with_error(conn_string.c_str(), &mongoError);
    if (!uri) {
        ConsoleLog("failed to parse URI:" + conn_string, "E");
        ConsoleLog("error message:" + std::string(mongoError.message), "E");
    }

    mongoClient = mongoc_client_new_from_uri(uri);
    if (mongoClient)
    {
        bson_t* query;
        query = mongoDBGetQuery(phandle);

        mongoc_collection_t* collection;
        collection = mongoc_client_get_collection(mongoClient, mongoDB_name.c_str(), collection_name.c_str());
        mongoc_cursor_t* cursor;
        const bson_t* resultBson;
        resultBson = bson_new();
        char* str;
        cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);
        while (mongoc_cursor_next(cursor, &resultBson)) {
            str = bson_as_canonical_extended_json(resultBson, NULL);
            bson_free(str);

            // Convert Bson to IdxRecord.
            bson_to_idx_record(resultBson, idxRec);

            if (DB_UIDAlreadyFound(handle_, &idxRec))
            {
                continue;
            }
            else
            {
                foundAnything = OFTrue;
                break;
            }
        }
        ConsoleLog("End MongoDB C-FIND", "");

        // 釋放記憶體
        bson_destroy(query);
        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(collection);
    }
    else
    {
        ConsoleLog("MongoDB Client Error", "E");
    }

    // 釋放記憶體
    mongoc_uri_destroy(uri);
    mongoc_client_destroy(mongoClient);

    mongoc_cleanup();

    return foundAnything;
}


static void mongoDBFindRecordsForMove(DB_Private_Handle* handle_, DB_ElementList* plist, DB_CounterList* pidxlist, DB_CounterList* lastidxlist, DcmQueryRetrieveDatabaseStatus* status)
{
    // Find and get Records from mongodb
    // MongoDB連接
    mongoc_client_t* mongoClient;
    mongoc_database_t* db;
    mongoc_uri_t* uri;
    bson_error_t mongoError;

    mongoc_init();
    uri = mongoc_uri_new_with_error(conn_string.c_str(), &mongoError);
    if (!uri) {
        fprintf(stderr,
            "failed to parse URI: %s\n"
            "error message:       %s\n",
            conn_string.c_str(),
            mongoError.message);
        std::cout << "mongodb uri error" << "" << std::endl;
    }

    mongoClient = mongoc_client_new_from_uri(uri);
    if (mongoClient)
    {
        // 將query轉bson
        bson_t* query;
        query = mongoDBGetQuery(handle_);

        mongoc_collection_t* collection;
        collection = mongoc_client_get_collection(mongoClient, mongoDB_name.c_str(), collection_name.c_str());
        mongoc_cursor_t* cursor;
        const bson_t* resultBson;
        resultBson = bson_new();
        char* str;
        cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);
        while (mongoc_cursor_next(cursor, &resultBson)) {
            str = bson_as_canonical_extended_json(resultBson, NULL);
            bson_free(str);

            pidxlist = (DB_CounterList*)malloc(sizeof(DB_CounterList));
            if (pidxlist == NULL) {
                status->setStatus(STATUS_FIND_Refused_OutOfResources);
                //return (QR_EC_IndexDatabaseError);
            }

            pidxlist->rec = new IdxRecord();
            bson_to_idx_record(resultBson, *pidxlist->rec);

            pidxlist->next = NULL;
            handle_->NumberRemainOperations++;
            if (handle_->moveCounterList == NULL)
            {
                lastidxlist = pidxlist;
                handle_->moveCounterList = lastidxlist;
            }
            else {
                lastidxlist->next = pidxlist;
                lastidxlist = pidxlist;
            }
        }
        std::cout << "ending mongodb find" << "" << std::endl;

        // 釋放記憶體
        bson_destroy(query);
        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(collection);
    }
    else
    {
        std::cout << "mongodb client error" << "" << std::endl;
    }
    // 釋放記憶體
    mongoc_uri_destroy(uri);
    mongoc_client_destroy(mongoClient);

    mongoc_cleanup();
}

#pragma endregion

static char *DB_strdup(const char* str)
{
    if (str == NULL) return NULL;
    size_t buflen = strlen(str)+1;
    char* s = (char*)malloc(buflen);
    OFStandard::strlcpy(s, str, buflen);
    return s;
}

/************
**      Add UID in Index Record to the UID found list
 */

static void DB_UIDAddFound (
                DB_Private_Handle       *phandle,
                IdxRecord               *idxRec
                )
{
    DB_UidList *plist ;

    plist = (DB_UidList *) malloc (sizeof (DB_UidList)) ;
    if (plist == NULL) {
        DCMQRDB_ERROR("DB_UIDAddFound: out of memory");
        return;
    }
    plist->next = phandle->uidList ;
    plist->patient = NULL ;
    plist->study = NULL ;
    plist->serie = NULL ;
    plist->image = NULL ;

    if ((int)phandle->queryLevel >= PATIENT_LEVEL)
        plist->patient = DB_strdup ((char *) idxRec->PatientID) ;
    if ((int)phandle->queryLevel >= STUDY_LEVEL)
        plist->study = DB_strdup ((char *) idxRec->StudyInstanceUID) ;
    if ((int)phandle->queryLevel >= SERIE_LEVEL)
        plist->serie = DB_strdup ((char *) idxRec->SeriesInstanceUID) ;
    if ((int)phandle->queryLevel >= IMAGE_LEVEL)
        plist->image = DB_strdup ((char *) idxRec->SOPInstanceUID) ;

    phandle->uidList = plist ;
}


/******************************
 *      Seek to a file position and do error checking
 *
 * Motivation:
 * We have had situations during demonstrations where size of the DB index file
 * has exploded.  It seems that a record is being written to a position
 * way past the end of file.
 * This seek function does some sanity error checking to try to identify
 * the problem.
 */
static long DB_lseek(int fildes, long offset, int whence)
{
    long pos;
    long curpos;
    long endpos;

    /*
    ** we should not be seeking to an offset < 0
    */
    if (offset < 0) {
        DCMQRDB_ERROR("*** DB ALERT: attempt to seek before beginning of file");
    }

    /* get the current position */
    curpos = lseek(fildes, 0, SEEK_CUR);
    if (curpos < 0) {
        DCMQRDB_ERROR("DB_lseek: cannot get current position: " << OFStandard::getLastSystemErrorCode().message());
        return curpos;
    }
    /* get the end of file position */
    endpos = lseek(fildes, 0, SEEK_END);
    if (endpos < 0) {
        DCMQRDB_ERROR("DB_lseek: cannot get end of file position: " << OFStandard::getLastSystemErrorCode().message());
        return endpos;
    }

    /* return to current position */
    curpos = lseek(fildes, curpos, SEEK_SET);
    if (curpos < 0) {
        DCMQRDB_ERROR("DB_lseek: cannot reset current position: " << OFStandard::getLastSystemErrorCode().message());
        return curpos;
    }

    /* do the requested seek */
    pos = lseek(fildes, offset, whence);
    if (pos < 0) {
        DCMQRDB_ERROR("DB_lseek: cannot seek to " << offset << ": " << OFStandard::getLastSystemErrorCode().message());
        return pos;
    }

    /*
    ** print an alert if we are seeking to far
    ** what is the limit? We don't expect the index file to be
    ** larger than 32Mb
    */
    const long maxFileSize = 33554432;
    if (pos > maxFileSize) {
        DCMQRDB_ERROR("*** DB ALERT: attempt to seek beyond " << maxFileSize << " bytes");
    }

    /* print an alert if we are seeking beyond the end of file.
     * ignore when file is empty or contains only the version information.
     */
    if ((endpos > DBHEADERSIZE) && (pos > endpos)) {
        DCMQRDB_ERROR("*** DB ALERT: attempt to seek beyond end of file" << OFendl
            << "              offset=" << offset << " filesize=" << endpos);
    }

    return pos;
}

/******************************
 *      Read an Index record
 */

OFCondition DcmQueryRetrieveIndexDatabaseHandle::DB_IdxRead (int idx, IdxRecord *idxRec)
{

    /*** Goto the right index in file
    **/

    DB_lseek (handle_ -> pidx, OFstatic_cast(long, DBHEADERSIZE + SIZEOF_STUDYDESC + idx * SIZEOF_IDXRECORD), SEEK_SET) ;

    /*** Read the record
    **/

    if (read (handle_ -> pidx, (char *) idxRec, SIZEOF_IDXRECORD) != SIZEOF_IDXRECORD)
        return (QR_EC_IndexDatabaseError) ;

    DB_lseek (handle_ -> pidx, OFstatic_cast(long, DBHEADERSIZE), SEEK_SET) ;

    /*** Initialize record links
    **/

    DB_IdxInitRecord (idxRec, 1) ;
    return EC_Normal ;
}

/******************************
 *      Add an Index record
 *      Returns the index allocated for this record
 */

static OFCondition DB_IdxAdd(DB_Private_Handle* phandle, int* idx, IdxRecord* idxRec)
{
    IdxRecord   rec;
    OFCondition cond = EC_Normal;

    /*** Find free place for the record
    *** A place is free if filename is empty
    **/

    *idx = 0;

    DB_lseek(phandle->pidx, OFstatic_cast(long, DBHEADERSIZE + SIZEOF_STUDYDESC), SEEK_SET);
    while (read(phandle->pidx, (char*)&rec, SIZEOF_IDXRECORD) == SIZEOF_IDXRECORD) {
        if (rec.filename[0] == '\0')
            break;
        (*idx)++;
    }

    /*** We have either found a free place or we are at the end of file. **/


    DB_lseek(phandle->pidx, OFstatic_cast(long, DBHEADERSIZE + SIZEOF_STUDYDESC + (*idx) * SIZEOF_IDXRECORD), SEEK_SET);

    if (write(phandle->pidx, (char*)idxRec, SIZEOF_IDXRECORD) != SIZEOF_IDXRECORD)
        cond = QR_EC_IndexDatabaseError;
    else
        cond = EC_Normal;

    DB_lseek(phandle->pidx, OFstatic_cast(long, DBHEADERSIZE), SEEK_SET);

    return cond;
}

/******************************
 *      Change the StudyDescRecord
 */

OFCondition DcmQueryRetrieveIndexDatabaseHandle::DB_StudyDescChange(StudyDescRecord *pStudyDesc)
{
    OFCondition cond = EC_Normal;
    DB_lseek (handle_ -> pidx, OFstatic_cast(long, DBHEADERSIZE), SEEK_SET) ;
    if (write (handle_ -> pidx, (char *) pStudyDesc, SIZEOF_STUDYDESC) != SIZEOF_STUDYDESC)
        cond = QR_EC_IndexDatabaseError;
    DB_lseek (handle_ -> pidx, OFstatic_cast(long, DBHEADERSIZE), SEEK_SET) ;
    return cond ;
}

/******************************
 *      Init an Index record loop
 */

OFCondition DcmQueryRetrieveIndexDatabaseHandle::DB_IdxInitLoop(int *idx)
{
    DB_lseek (handle_ -> pidx, OFstatic_cast(long, DBHEADERSIZE + SIZEOF_STUDYDESC), SEEK_SET) ;
    *idx = -1 ;
    return EC_Normal ;
}

/******************************
 *      Get next Index record
 *      On return, idx is initialized with the index of the record read
 */

OFCondition DcmQueryRetrieveIndexDatabaseHandle::DB_IdxGetNext(int *idx, IdxRecord *idxRec)
{

    (*idx)++ ;
    DB_lseek (handle_ -> pidx, OFstatic_cast(long, DBHEADERSIZE + SIZEOF_STUDYDESC + OFstatic_cast(long, *idx) * SIZEOF_IDXRECORD), SEEK_SET) ;
    while (read (handle_ -> pidx, (char *) idxRec, SIZEOF_IDXRECORD) == SIZEOF_IDXRECORD) {
        if (idxRec -> filename [0] != '\0') {
            DB_IdxInitRecord (idxRec, 1) ;

            return EC_Normal ;
        }
        (*idx)++ ;
    }

    DB_lseek (handle_ -> pidx, OFstatic_cast(long, DBHEADERSIZE), SEEK_SET) ;

    return QR_EC_IndexDatabaseError ;
}


/******************************
 *      Get next Index record
 *      On return, idx is initialized with the index of the record read
 */

OFCondition DcmQueryRetrieveIndexDatabaseHandle::DB_GetStudyDesc (StudyDescRecord *pStudyDesc)
{

    DB_lseek (handle_ -> pidx, OFstatic_cast(long, DBHEADERSIZE), SEEK_SET) ;
    if ( read (handle_ -> pidx, (char *) pStudyDesc, SIZEOF_STUDYDESC) == SIZEOF_STUDYDESC )
        return EC_Normal ;

    DB_lseek (handle_ -> pidx, OFstatic_cast(long, DBHEADERSIZE), SEEK_SET) ;

    return QR_EC_IndexDatabaseError ;
}


/******************************
 *      Remove an Index record
 *      Just put a record with filename == ""
 */

OFCondition DcmQueryRetrieveIndexDatabaseHandle::DB_IdxRemove(int idx)
{
    IdxRecord   rec ;
    OFCondition cond = EC_Normal;

    DB_lseek (handle_ -> pidx, OFstatic_cast(long, DBHEADERSIZE + SIZEOF_STUDYDESC + OFstatic_cast(long, idx) * SIZEOF_IDXRECORD), SEEK_SET) ;
    DB_IdxInitRecord (&rec, 0) ;

    rec. filename [0] = '\0' ;
    if (write (handle_ -> pidx, (char *) &rec, SIZEOF_IDXRECORD) == SIZEOF_IDXRECORD)
        cond = EC_Normal ;
    else
        cond = QR_EC_IndexDatabaseError ;

    DB_lseek (handle_ -> pidx, OFstatic_cast(long, DBHEADERSIZE), SEEK_SET) ;

    return cond ;
}

OFCondition DcmQueryRetrieveIndexDatabaseHandle::DB_lock(OFBool exclusive)
{
    int lockmode;

    if (exclusive) {
        lockmode = LOCK_EX;     /* exclusive lock */
    } else {
        lockmode = LOCK_SH;     /* shared lock */
    }
    if (dcmtk_flock(handle_->pidx, lockmode) < 0) {
        dcmtk_plockerr("DB_lock");
        return QR_EC_IndexDatabaseError;
    }
    return EC_Normal;
}

OFCondition DcmQueryRetrieveIndexDatabaseHandle::DB_unlock()
{
    if (dcmtk_flock(handle_->pidx, LOCK_UN) < 0) {
        dcmtk_plockerr("DB_unlock");
        return QR_EC_IndexDatabaseError;
    }
    return EC_Normal;
}

/*******************
 *    Free an element List
 */

static OFCondition DB_FreeUidList (DB_UidList *lst)
{
    while (lst != NULL) {
        if (lst -> patient)
            free (lst -> patient);
        if (lst -> study)
            free (lst -> study);
        if (lst -> serie)
            free (lst -> serie);
        if (lst -> image)
            free (lst -> image);
        DB_UidList *curlst = lst;
        lst = lst->next;
        free (curlst);
    }
    return EC_Normal;
}


/*******************
 *    Free a UID List
 */

static OFCondition DB_FreeElementList (DB_ElementList *lst)
{
    if (lst == NULL) return EC_Normal;

    OFCondition cond = DB_FreeElementList (lst -> next);
    if (lst->elem.PValueField != NULL) {
        free ((char *) lst -> elem. PValueField);
    }
    delete lst;
    return (cond);
}

/*******************
 *    Is the specified tag supported
 */

static int DB_TagSupported (DcmTagKey tag)
{
    int i;

    for (i = 0; i < NbFindAttr; i++)
        if (TbFindAttr[i]. tag == tag)
            return (OFTrue);

    return (OFFalse);

}

/*******************
 *    Get UID tag of a specified level
 */

static OFCondition DB_GetUIDTag (DB_LEVEL level, DcmTagKey *tag)
{
    int i;

    for (i = 0; i < NbFindAttr; i++)
        if ((TbFindAttr[i]. level == level) && (TbFindAttr[i]. keyAttr == UNIQUE_KEY))
            break;

    if (i < NbFindAttr) {
        *tag = TbFindAttr[i].tag;
        return (EC_Normal);
    }
    else
    return (QR_EC_IndexDatabaseError);

}

/*******************
 *    Get tag level of a specified tag
 */

static OFCondition DB_GetTagLevel (DcmTagKey tag, DB_LEVEL *level)
{
    int i;

    for (i = 0; i < NbFindAttr; i++)
        if (TbFindAttr[i]. tag == tag)
            break;

    if (i < NbFindAttr) {
        *level = TbFindAttr[i]. level;
        return (EC_Normal);
    }
    else
    return (QR_EC_IndexDatabaseError);
}

/*******************
 *    Get tag key attribute of a specified tag
 */

static OFCondition DB_GetTagKeyAttr (DcmTagKey tag, DB_KEY_TYPE *keyAttr)
{
    int i;

    for (i = 0; i < NbFindAttr; i++)
        if (TbFindAttr[i]. tag == tag)
            break;

    if (i < NbFindAttr) {
        *keyAttr = TbFindAttr[i]. keyAttr;
        return (EC_Normal);
    }
    else
    return (QR_EC_IndexDatabaseError);
}

/***********************
 *    Duplicate a DICOM element
 *    dst space is supposed provided by the caller
 */

static void DB_DuplicateElement (DB_SmallDcmElmt *src, DB_SmallDcmElmt *dst)
{
    memset( (char*)dst, 0, sizeof (DB_SmallDcmElmt));
    dst -> XTag = src -> XTag;
    dst -> ValueLength = src -> ValueLength;

    if (src -> ValueLength == 0)
        dst -> PValueField = NULL;
    else {
        dst -> PValueField = (char *)malloc ((int) src -> ValueLength+1);
        memset(dst->PValueField, 0, (size_t)(src->ValueLength+1));
        if (dst->PValueField != NULL) {
            memcpy (dst -> PValueField,  src -> PValueField,
                (size_t) src -> ValueLength);
        } else {
            DCMQRDB_ERROR("DB_DuplicateElement: out of memory");
        }
    }
}


/***********************
 *    Compare two ImagesofStudyArray elements
 */

extern "C" int DB_Compare(const void *ve1, const void *ve2)
{
    ImagesofStudyArray *e1 = (ImagesofStudyArray *)ve1;
    ImagesofStudyArray *e2 = (ImagesofStudyArray *)ve2;
    if ( e1 -> RecordedDate > e2 -> RecordedDate )
        return (1);
    else
    if ( e1 -> RecordedDate == e2 -> RecordedDate )
        return (0);
    else
        return (-1);

}


/* ==================================================================== */

DcmQueryRetrieveDatabaseHandle::~DcmQueryRetrieveDatabaseHandle()
{
}

/* ========================= FIND ========================= */

// helper function to print 'ASCII' instead of an empty string for the value of
// Specific Character Set
static const char* characterSetName( const OFString& charset )
{
    if (charset.empty())
        return "ASCII";
    return charset.c_str();
}

class DcmQueryRetrieveIndexDatabaseHandle::CharsetConsideringMatcher
{
public:

    // Constructor, remember references to the find request character set and converter
    CharsetConsideringMatcher(DB_Private_Handle& handle)
    : findRequestCharacterSet(handle.findRequestCharacterSet)
    , findRequestConverter(handle.findRequestConverter)
    , candidateCharacterSet()
    , candidateConverter()
    , isFindRequestConversionNecessary(isConversionToUTF8Necessary(findRequestCharacterSet))
    , isCandidateConversionNecessary()
    , isConversionNecessary()
    {

    }

    // read access to the candidate's character set value
    const OFString& getCandidateCharacterSet() const
    {
        return candidateCharacterSet;
    }

    // prepare character set conversion for specific index record
    void setRecord(IdxRecord& idxRec)
    {
        // copy value of specific character set of the entry, since the converter
        // would need it as an OFString anyway.
        candidateCharacterSet.assign(idxRec.param[RECORDIDX_SpecificCharacterSet].PValueField,
                                     idxRec.param[RECORDIDX_SpecificCharacterSet].ValueLength);
        // test if conversion is potentially necessary since the character sets differ
        if (findRequestCharacterSet != candidateCharacterSet) {
            // determine if the candidate is compatible to UTF-8 or must be converted
            isCandidateConversionNecessary = isConversionToUTF8Necessary(candidateCharacterSet);
            // if it must be converted, clear the converter if it was previously initialized,
            // but for a different character set
            if (isCandidateConversionNecessary && candidateConverter &&
                candidateConverter.getSourceCharacterSet() != candidateCharacterSet) {
                candidateConverter.clear();
            }
            // even if the character sets differ, they may both be compatible to UTF-8,
            // in which case conversion is still not necessary
            isConversionNecessary = isCandidateConversionNecessary || isFindRequestConversionNecessary;
        } else {
            // conversion is not necessary
            isConversionNecessary = OFFalse;
        }
    }

    // Try to match Two DB_ElementList elements
    // The first one is the query key, the second one the candidate
    // from the database entry.
    // Returns OFTrue if both values match, OFFalse otherwise
    OFBool operator()(DB_ElementList* query, DB_SmallDcmElmt* candidate)
    {
        // Universal matching is applied if the query value is empty:
        // always return OFTrue
        if (!query->elem.ValueLength)
            return OFTrue;

        (void)findRequestConverter;
        OFString buffer;
        const char* pQuery = query->elem.PValueField;
        const char* pQueryEnd = pQuery + query->elem.ValueLength;
        const char* pCandidate = candidate->PValueField;
        const char* pCandidateEnd = pCandidate + candidate->ValueLength;

        DcmVR vr = DcmTag(query->elem.XTag).getVR();
        if (isConversionNecessary && vr.isAffectedBySpecificCharacterSet()) {
#ifdef DCMTK_ENABLE_CHARSET_CONVERSION
            // convert query, if it isn't UTF-8 or ASCII already
            if (isFindRequestConversionNecessary) {
                // does a value already exist in the cache?
                if (!query->utf8Value) {
                    // fill the cache if it doesn't
                    query->utf8Value = OFString();
                    // initialize the converter, if this is the first
                    // time we need it
                    OFCondition cond = EC_Normal;
                    if (!findRequestConverter)
                        cond = findRequestConverter.selectCharacterSet(findRequestCharacterSet);
                    if (cond.good()) {
                        // covert the string and cache the result, using the
                        // specific delimitation characters for this VR
                        cond = findRequestConverter.convertString(
                            query->elem.PValueField,
                            query->elem.ValueLength,
                            *query->utf8Value,
                            vr.getDelimiterChars()
                        );
                    }
                    if (cond.bad()) {
                        DCMQRDB_WARN("Character set conversion of the query key failed with the following error: '" << cond.text()
                            << "', will compare values that use different (incompatible) character sets: \""
                            << characterSetName(findRequestCharacterSet) << "\" and \"" << characterSetName(candidateCharacterSet) << '"');
                        // put the original value in the cache, since retrying the conversion on the next encounter does not make sense
                        // (it would only fail again).
                        query->utf8Value = OFString(query->elem.PValueField, query->elem.ValueLength);
                    }
                }
                // use the value from the cache for the following match
                // operations
                pQuery = query->utf8Value->c_str();
                pQueryEnd = pQuery + query->utf8Value->size();
            }
            // convert the candidate, if it isn't already UTF-8 or ASCII
            if (isCandidateConversionNecessary) {
                // initialize the converter, if this is the first time
                // we need it for this entry
                OFCondition cond = EC_Normal;
                if (!candidateConverter)
                    cond = candidateConverter.selectCharacterSet(candidateCharacterSet);
                if (cond.good()) {
                    // convert the string using the local buffer and the
                    // specific delimitation characters for this VR
                    cond = candidateConverter.convertString(
                        candidate->PValueField,
                        candidate->ValueLength,
                        buffer,
                        vr.getDelimiterChars()
                    );
                }
                if (cond.good()) {
                    // assign the buffer contents to the value being used
                    // in the following match operations
                    pCandidate = buffer.c_str();
                    pCandidateEnd = pCandidate + buffer.size();
                } else {
                    DCMQRDB_WARN("Character set conversion of the candidate failed with the following error: '" << cond.text()
                        << "', will compare values that use different (incompatible) character sets: \""
                        << characterSetName(findRequestCharacterSet) << "\" and \"" << characterSetName(candidateCharacterSet) << '"');
                }
            }
#else
            DCMQRDB_WARN("Character set conversion is not available, comparing values that use different (incompatible) character sets: \""
                << characterSetName(findRequestCharacterSet) << "\" and \"" << characterSetName(candidateCharacterSet) << '"');
#endif
        }

        // remove leading and trailing spaces before matching
        if (vr.isaString()) {
            OFStandard::trimString(pQuery, pQueryEnd);
            OFStandard::trimString(pCandidate, pCandidateEnd);
        }

        // use DcmAttributeMatching to perform the appropriate matching function
        // for the given VR
        return DcmAttributeMatching( vr )( pQuery, pQueryEnd - pQuery,
                                           pCandidate, pCandidateEnd - pCandidate );
    }

private:
    const OFString& findRequestCharacterSet;
    DcmSpecificCharacterSet& findRequestConverter;
    OFString candidateCharacterSet;
    DcmSpecificCharacterSet candidateConverter;
    const OFBool isFindRequestConversionNecessary;
    OFBool isCandidateConversionNecessary;
    OFBool isConversionNecessary;
};

OFBool DcmQueryRetrieveIndexDatabaseHandle::isConversionToUTF8Necessary(const OFString& characterSet)
{
    // empty      -> ASCII, subset of UTF-8
    // ISO_IR 6   -> ASCII, subset of UTF-8
    // ISO_IR 192 -> UTF-8
    return !characterSet.empty() &&
           characterSet != "ISO_IR 192" &&
           characterSet != "ISO_IR 6"
    ;
}

OFBool DcmQueryRetrieveIndexDatabaseHandle::isConversionNecessary(const OFString& sourceCharacterSet,
                                                                  const OFString& destinationCharacterSet)
{
    // conversion is unnecessary if both are the same character set or if
    // the destination is UTF-8 and the source is compatible to UTF-8
    // (i.e. ASCII).
    return sourceCharacterSet != destinationCharacterSet &&
    (
        destinationCharacterSet != "ISO_IR 192" ||
        isConversionToUTF8Necessary(sourceCharacterSet)
    );
}

/************
**      Create the response list in specified handle,
**      using informations found in an index record.
**      Old response list is supposed freed
**/

void DcmQueryRetrieveIndexDatabaseHandle::makeResponseList (
                DB_Private_Handle       *phandle,
                IdxRecord               *idxRec
                )
{
    int i ;
    DB_ElementList *pRequestList = NULL;
    DB_ElementList *plist = NULL;
    DB_ElementList *last = NULL;

    phandle->findResponseList = NULL ;

    /*** For each element in Request identifier
    **/
    ConsoleLog("Adding file to response:" + std::string(idxRec->filename), "");
    for (pRequestList = phandle->findRequestList ; pRequestList ; pRequestList = pRequestList->next) {

        /*** Find Corresponding Tag in index record
        **/

        for (i = 0; i < NBPARAMETERS; i++) 
        {
            if (idxRec->param[i].XTag.key[0] == pRequestList->elem.XTag.key[0] && idxRec->param[i].XTag.key[1] == pRequestList->elem.XTag.key[1])
            {
                break;
            }
        }
        /*** If Tag not found, skip the element
        **/

        if (i >= NBPARAMETERS)
            continue ;

        /*** Append index record element to response list
        **/

        plist = new DB_ElementList ;
        if (plist == NULL) {
            DCMQRDB_ERROR("makeResponseList: out of memory");
            return;
        }
        DB_DuplicateElement(&idxRec->param[i], &plist->elem);

        if (phandle->findResponseList == NULL) {
            phandle->findResponseList = last = plist ;
        }
        else {
            last->next = plist ;
            last = plist ;
        }

    }

    /** Specific Character Set stuff
    **/

    if (idxRec->param[RECORDIDX_SpecificCharacterSet].ValueLength) {
        plist = new DB_ElementList ;
        if (plist == NULL) {
            DCMQRDB_ERROR("makeResponseList: out of memory");
            return;
        }

        DB_DuplicateElement(&idxRec->param[RECORDIDX_SpecificCharacterSet], &plist->elem);

        if (phandle->findResponseList == NULL) {
            phandle->findResponseList = last = plist ;
        }
        else {
            last->next = plist ;
            last = plist ;
        }
    }
}



/************
**      Test a Find Request List
**      Returns EC_Normal if OK, else returns QR_EC_IndexDatabaseError
 */

OFCondition DcmQueryRetrieveIndexDatabaseHandle::testFindRequestList (
                DB_ElementList  *findRequestList,
                DB_LEVEL        queryLevel,
                DB_LEVEL        infLevel,
                DB_LEVEL        lowestLevel
                )
{
    DB_ElementList *plist ;
    DB_LEVEL    XTagLevel = PATIENT_LEVEL; // DB_GetTagLevel() will set this correctly
    DB_KEY_TYPE XTagType  = OPTIONAL_KEY;  // DB_GetTagKeyAttr() will set this
    int level ;

    /**** Query level must be at least the infLevel
    ***/

    if (queryLevel < infLevel) {
        DCMQRDB_INFO("Level incompatible with Information Model (level " << queryLevel << ")");
        return QR_EC_IndexDatabaseError ;
    }

    if (queryLevel > lowestLevel) {
        DCMQRDB_DEBUG("Level incompatible with Information Model (level " << queryLevel << ")");
        return QR_EC_IndexDatabaseError ;
    }

    for (level = PATIENT_LEVEL ; level <= IMAGE_LEVEL ; level++) {

        /**** Manage exception due to StudyRoot Information Model :
        **** In this information model, queries may include Patient attributes
        **** but only if they are made at the study level
        ***/

        if ((level == PATIENT_LEVEL) && (infLevel == STUDY_LEVEL)) {
            /** In Study Root Information Model, accept only Patient Tags
            ** if the Query Level is the Study level
            */

            int atLeastOneKeyFound = OFFalse ;
            for (plist = findRequestList ; plist ; plist = plist->next) {
                DB_GetTagLevel (plist->elem. XTag, &XTagLevel) ;
                if (XTagLevel != level)
                    continue ;
                atLeastOneKeyFound = OFTrue ;
            }
            if (atLeastOneKeyFound && (queryLevel != STUDY_LEVEL)) {
                DCMQRDB_DEBUG("Key found in Study Root Information Model (level " << level << ")");
                return QR_EC_IndexDatabaseError ;
            }
        }

        /**** If current level is above the QueryLevel
        ***/

        else if (level < queryLevel) {

            /** For this level, only unique keys are allowed
            ** Parse the request list elements referring to
            ** this level.
            ** Check that only unique key attr are provided
            */

            int uniqueKeyFound = OFFalse ;
            for (plist = findRequestList ; plist ; plist = plist->next) {
                DB_GetTagLevel (plist->elem. XTag, &XTagLevel) ;
                if (XTagLevel != level)
                    continue ;
                DB_GetTagKeyAttr (plist->elem. XTag, &XTagType) ;
                if (XTagType != UNIQUE_KEY) {
                    DCMQRDB_DEBUG("Non Unique Key found (level " << level << ")");
                    return QR_EC_IndexDatabaseError ;
                }
                else if (plist->elem.ValueLength == 0) {
                    DCMQRDB_DEBUG("Unique Key value is empty (level " << level << ")");
                    return QR_EC_IndexDatabaseError ;
                }
                else if (uniqueKeyFound) {
                    DCMQRDB_DEBUG("More than one Unique Key found (level " << level << ")");
                    return QR_EC_IndexDatabaseError ;
                }
                else
                    uniqueKeyFound = OFTrue ;
            }
            if (! uniqueKeyFound) {
                DCMQRDB_DEBUG("No Unique Key found (level " << level << ")");
                return QR_EC_IndexDatabaseError ;
            }
        }

        /**** If current level is the QueryLevel
        ***/

        else if (level == queryLevel) {

            /** For this level, all keys are allowed
            ** Parse the request list elements referring to
            ** this level.
            ** Check that at least one key is provided
            */

            int atLeastOneKeyFound = OFFalse ;
            for (plist = findRequestList ; plist ; plist = plist->next) {
                DB_GetTagLevel (plist->elem. XTag, &XTagLevel) ;
                if (XTagLevel != level)
                    continue ;
                atLeastOneKeyFound = OFTrue ;
            }
            if (! atLeastOneKeyFound) {
                DCMQRDB_DEBUG("No Key found at query level (level " << level << ")");
                return QR_EC_IndexDatabaseError ;
            }
        }

        /**** If current level beyond the QueryLevel
        ***/

        else if (level > queryLevel) {

            /** For this level, no key is allowed
            ** Parse the request list elements referring to
            ** this level.
            ** Check that no key is provided
            */

            int atLeastOneKeyFound = OFFalse ;
            for (plist = findRequestList ; plist ; plist = plist->next) {
                DB_GetTagLevel (plist->elem. XTag, &XTagLevel) ;
                if (XTagLevel != level)
                    continue ;
                atLeastOneKeyFound = OFTrue ;
            }
            if (atLeastOneKeyFound) {
                DCMQRDB_DEBUG("Key found beyond query level (level " << level << ")");
                return QR_EC_IndexDatabaseError ;
            }
        }

    }
    return EC_Normal ;
}



/************
**      Hierarchical Search Algorithm
**      Returns OFTrue if matching is OK, else returns OFFalse
 */

OFCondition DcmQueryRetrieveIndexDatabaseHandle::hierarchicalCompare (
                DB_Private_Handle       *phandle,
                IdxRecord               *idxRec,
                DB_LEVEL                level,
                DB_LEVEL                infLevel,
                int                     *match,
                CharsetConsideringMatcher& dbmatch)
{
    int                 i ;
    DcmTagKey   XTag ;
    DB_ElementList *plist ;
    DB_LEVEL    XTagLevel  = PATIENT_LEVEL; // DB_GetTagLevel() will set this correctly

    /**** If current level is above the QueryLevel
    ***/

    if (level < phandle->queryLevel) {

        /** Get UID Tag for current level
         */

        DB_GetUIDTag (level, &XTag) ;

        /** Find Element with this XTag in Identifier list
         */

        for (plist = phandle->findRequestList ; plist ; plist = plist->next)
            if (plist->elem. XTag == XTag)
                break ;

        /** Element not found
         */

        if (plist == NULL) {
            *match = OFFalse ;
            DCMQRDB_WARN("hierarchicalCompare : No UID Key found at level " << (int) level);
            return QR_EC_IndexDatabaseError ;
        }

        /** Find element with the same XTag in index record
         */

        for (i = 0 ; i < NBPARAMETERS ; i++)
            if (idxRec->param [i]. XTag == XTag)
                break ;

        /** Compare with Single value matching
        ** If Match fails, return OFFalse
        */

        if (!dbmatch(plist, &idxRec->param[i])) {
            *match = OFFalse ;
            return EC_Normal ;
        }

        /** Match succeeded.
        ** Try at next level
        */

        return hierarchicalCompare (phandle, idxRec, (DB_LEVEL)(level + 1), infLevel, match, dbmatch) ;
    }

    /**** If current level is the QueryLevel
    ***/
    
    else if (level == phandle->queryLevel) {

        /*** For each element in Identifier list
        **/

        for (plist = phandle->findRequestList ; plist ; plist = plist->next) {

            /** Get the Tag level of this element
             */

            DB_GetTagLevel (plist->elem. XTag, &XTagLevel) ;

            /** If we are in the Study Root Information Model exception
            ** we must accept patients keys at the study level
            */

            if (  (XTagLevel == PATIENT_LEVEL)
                  && (phandle->queryLevel == STUDY_LEVEL)
                  && (infLevel == STUDY_LEVEL)
                ) ;

            /** In other cases, only keys at the current level are
            ** taken into account. So skip this element.
            */

            else if (XTagLevel != level)
                continue ;

            /** Find element with the same XTag in index record
             */

            for (i = 0 ; i < NBPARAMETERS ; i++)
                if (idxRec->param [i]. XTag == plist->elem. XTag)
                    break ;

            /** Compare with appropriate Matching.
            ** If Match fails, return OFFalse
            */


            if (!dbmatch(plist, &idxRec->param[i])) {
                *match = OFFalse ;
                return EC_Normal ;
            }
        }

        /*** If we are here, all matches succeeded at the current level.
        *** Perhaps check that we have tried at least one match ??
        **/

        *match = OFTrue ;
        return EC_Normal ;

    }
    return QR_EC_IndexDatabaseError;
}





/********************
**      Start find in Database
**/

OFCondition DcmQueryRetrieveIndexDatabaseHandle::startFindRequest(
                const char      *SOPClassUID,
                DcmDataset      *findRequestIdentifiers,
                DcmQueryRetrieveDatabaseStatus  *status)
{

    ReadMongoConfig();

    DB_SmallDcmElmt     elem ;
    DB_ElementList      *plist = NULL;
    DB_ElementList      *last = NULL;
    int                 MatchFound ;
    IdxRecord           idxRec ;
    DB_LEVEL            qLevel = PATIENT_LEVEL; // highest legal level for a query in the current model
    DB_LEVEL            lLevel = IMAGE_LEVEL;   // lowest legal level for a query in the current model

    OFCondition         cond = EC_Normal;
    OFBool qrLevelFound = OFFalse;

    /**** Is SOPClassUID supported ?
    ***/

    if (strcmp( SOPClassUID, UID_FINDPatientRootQueryRetrieveInformationModel) == 0)
        handle_->rootLevel = PATIENT_ROOT ;
    else if (strcmp( SOPClassUID, UID_FINDStudyRootQueryRetrieveInformationModel) == 0)
        handle_->rootLevel = STUDY_ROOT ;
#ifndef NO_PATIENTSTUDYONLY_SUPPORT
    else if (strcmp( SOPClassUID, UID_RETIRED_FINDPatientStudyOnlyQueryRetrieveInformationModel) == 0)
        handle_->rootLevel = PATIENT_STUDY ;
#endif
    else {
        status->setStatus(STATUS_FIND_Refused_SOPClassNotSupported);
        return (QR_EC_IndexDatabaseError) ;
    }

    /**** Parse Identifiers in the Dicom Object
    **** Find Query Level and construct a list
    **** of query identifiers
    ***/

    if (findRequestIdentifiers->findAndGetOFStringArray(DCM_SpecificCharacterSet, handle_->findRequestCharacterSet).bad())
        handle_->findRequestCharacterSet.clear();
    if (handle_->findRequestConverter && handle_->findRequestConverter.getSourceCharacterSet() != handle_->findRequestCharacterSet)
        handle_->findRequestConverter.clear();

    handle_->findRequestList = NULL ;

    int elemCount = OFstatic_cast(int, findRequestIdentifiers->card());
    for (int elemIndex=0; elemIndex<elemCount; elemIndex++) {

        DcmElement* dcelem = findRequestIdentifiers->getElement(elemIndex);

        elem.XTag = dcelem->getTag().getXTag();
        if (elem.XTag == DCM_QueryRetrieveLevel || DB_TagSupported(elem.XTag)) {
            elem.ValueLength = dcelem->getLength();
            if (elem.ValueLength == 0) {
                elem.PValueField = NULL ;
            } else if ((elem.PValueField = OFstatic_cast(char*, malloc(OFstatic_cast(size_t, elem.ValueLength+1)))) == NULL) {
                status->setStatus(STATUS_FIND_Refused_OutOfResources);
                return (QR_EC_IndexDatabaseError) ;
            } else {
                /* only char string type tags are supported at the moment */
                char *s = NULL;
                dcelem->getString(s);
                /* the available space is always elem.ValueLength+1 */
                OFStandard::strlcpy(elem.PValueField, s, elem.ValueLength+1);
            }
            /** If element is the Query Level, store it in handle
             */

            if (elem.XTag == DCM_QueryRetrieveLevel && elem.PValueField) {
                char *pc ;
                char level [50] ;

                strncpy(level, (char*)elem.PValueField,
                        (elem.ValueLength<50)? (size_t)(elem.ValueLength) : 49) ;

                /*** Skip this two lines if you want strict comparison
                **/

                for (pc = level ; *pc ; pc++)
                    *pc = ((*pc >= 'a') && (*pc <= 'z')) ? 'A' - 'a' + *pc : *pc ;

                if (strncmp (level, PATIENT_LEVEL_STRING,
                             strlen (PATIENT_LEVEL_STRING)) == 0)
                    handle_->queryLevel = PATIENT_LEVEL ;
                else if (strncmp (level, STUDY_LEVEL_STRING,
                                  strlen (STUDY_LEVEL_STRING)) == 0)
                    handle_->queryLevel = STUDY_LEVEL ;
                else if (strncmp (level, SERIE_LEVEL_STRING,
                                  strlen (SERIE_LEVEL_STRING)) == 0)
                    handle_->queryLevel = SERIE_LEVEL ;
                else if (strncmp (level, IMAGE_LEVEL_STRING,
                                  strlen (IMAGE_LEVEL_STRING)) == 0)
                    handle_->queryLevel = IMAGE_LEVEL ;
                else {
                    if (elem. PValueField)
                        free (elem. PValueField) ;
#ifdef DEBUG
                    DCMQRDB_DEBUG("DB_startFindRequest () : Illegal query level (" << level << ")");
#endif
                    status->setStatus(STATUS_FIND_Failed_UnableToProcess);
                    return (QR_EC_IndexDatabaseError) ;
                }
                qrLevelFound = OFTrue;
            } else {
                /** Else it is a query identifier.
                ** Append it to our RequestList if it is supported
                */
                if (DB_TagSupported (elem. XTag)) {

                    plist = new DB_ElementList ;
                    if (plist == NULL) {
                        status->setStatus(STATUS_FIND_Refused_OutOfResources);
                        return (QR_EC_IndexDatabaseError) ;
                    }
                    DB_DuplicateElement (&elem, &(plist->elem)) ;
                    if (handle_->findRequestList == NULL) {
                        handle_->findRequestList = last = plist ;
                    } else {
                        last->next = plist ;
                        last = plist ;
                    }
                }
            }

            if ( elem. PValueField ) {
                free (elem. PValueField) ;
            }
        }
    }

    if (!qrLevelFound) {
        /* The Query/Retrieve Level is missing */
        status->setStatus(STATUS_FIND_Error_DataSetDoesNotMatchSOPClass);
        DCMQRDB_WARN("DB_startFindRequest(): missing Query/Retrieve Level");
        handle_->idxCounter = -1 ;
        DB_FreeElementList (handle_->findRequestList) ;
        handle_->findRequestList = NULL ;
        return (QR_EC_IndexDatabaseError) ;
    }

    switch (handle_->rootLevel)
    {
      case PATIENT_ROOT :
        qLevel = PATIENT_LEVEL ;
        lLevel = IMAGE_LEVEL ;
        break ;
      case STUDY_ROOT :
        qLevel = STUDY_LEVEL ;
        lLevel = IMAGE_LEVEL ;
        break ;
      case PATIENT_STUDY:
        qLevel = PATIENT_LEVEL ;
        lLevel = STUDY_LEVEL ;
        break ;
    }

    /**** Test the consistency of the request list
    ***/

    if (doCheckFindIdentifier) {
        cond = testFindRequestList (handle_->findRequestList, handle_->queryLevel, qLevel, lLevel) ;
        if (cond != EC_Normal) {
            handle_->idxCounter = -1 ;
            DB_FreeElementList (handle_->findRequestList) ;
            handle_->findRequestList = NULL ;
#ifdef DEBUG
            DCMQRDB_DEBUG("DB_startFindRequest () : STATUS_FIND_Error_DataSetDoesNotMatchSOPClass - Invalid RequestList");
#endif
            status->setStatus(STATUS_FIND_Error_DataSetDoesNotMatchSOPClass);
            return (cond) ;
        }
    }

    // 開始從資料庫找尋資料
    /**** Goto the beginning of Index File
    **** Then find the first matching image
    ***/

    DB_lock(OFFalse);

    DB_IdxInitLoop (&(handle_->idxCounter)) ;
    MatchFound = OFFalse ;
    cond = EC_Normal;

    CharsetConsideringMatcher dbmatch(*handle_);
    MatchFound = mongoDBFindRecord(handle_, idxRec, qLevel, qLevel, &MatchFound, dbmatch);
    dbmatch.setRecord(idxRec);
    std::cout << "Find Request Filename:" << idxRec.filename << "\n";
    

    /*
    while (1) {

        // Exit loop if read error (or end of file)
        

        if (DB_IdxGetNext(&(handle_->idxCounter), &idxRec) != EC_Normal)
            break ;

        // Exit loop if error or matching OK
        

        dbmatch.setRecord(idxRec);
        cond = hierarchicalCompare (handle_, &idxRec, qLevel, qLevel, &MatchFound, dbmatch) ;
        if (cond != EC_Normal)
            break;
        if (MatchFound)
            break;
    }
    */

    /**** If an error occurred in Matching function
    ****    return a failed status
    ***/

    if (cond != EC_Normal) {
        handle_->idxCounter = -1 ;
        DB_FreeElementList (handle_->findRequestList) ;
        handle_->findRequestList = NULL ;
#ifdef DEBUG
        DCMQRDB_DEBUG("DB_startFindRequest () : STATUS_FIND_Failed_UnableToProcess");
#endif
        status->setStatus(STATUS_FIND_Failed_UnableToProcess);

        DB_unlock();

        return (cond) ;
    }


    /**** If a matching image has been found,
    ****         add index record to UID found list
    ****    prepare Response List in handle
    ****    return status is pending
    ***/

    if (MatchFound) {
        DB_UIDAddFound (handle_, &idxRec) ;
        makeResponseList (handle_, &idxRec) ;
#ifdef DEBUG
        DCMQRDB_DEBUG("DB_startFindRequest () : STATUS_Pending");
#endif
        status->setStatus(STATUS_Pending);
        return (EC_Normal) ;
    }

    /**** else no matching image has been found,
    ****    free query identifiers list
    ****    status is success
    ***/

    else {
        handle_->idxCounter = -1 ;
        DB_FreeElementList (handle_->findRequestList) ;
        handle_->findRequestList = NULL ;
#ifdef DEBUG
        DCMQRDB_DEBUG("DB_startFindRequest () : STATUS_Success");
#endif
        status->setStatus(STATUS_Success);

        DB_unlock();

        return (EC_Normal) ;
    }

}

/********************
**      Get next find response in Database
 */

OFCondition DcmQueryRetrieveIndexDatabaseHandle::nextFindResponse (
                DcmDataset      **findResponseIdentifiers,
                DcmQueryRetrieveDatabaseStatus  *status,
#ifdef DCMTK_ENABLE_CHARSET_CONVERSION
                const DcmQueryRetrieveCharacterSetOptions& characterSetOptions)
#else
                const DcmQueryRetrieveCharacterSetOptions& /* characterSetOptions */)
#endif
{

    DB_ElementList      *plist = NULL;
    int                 MatchFound = OFFalse;
    IdxRecord           idxRec ;
    DB_LEVEL            qLevel = PATIENT_LEVEL;
    const char          *queryLevelString = NULL;
    OFCondition         cond = EC_Normal;

    if (handle_->findResponseList == NULL) {
#ifdef DEBUG
        DCMQRDB_DEBUG("DB_nextFindResponse () : STATUS_Success");
#endif
        *findResponseIdentifiers = NULL ;
        status->setStatus(STATUS_Success);

        DB_unlock();

        return (EC_Normal) ;
    }

    /***** Create the response (findResponseIdentifiers) using
    ***** the last find done and saved in handle findResponseList
    ****/

    *findResponseIdentifiers = new DcmDataset ;
    if ( *findResponseIdentifiers != NULL ) {

        /*** Put responses
        **/

        for ( plist = handle_->findResponseList ; plist != NULL ; plist = plist->next ) {
            DcmTag t(plist->elem.XTag);
            DcmElement *dce = DcmItem::newDicomElement(t);
            if (dce == NULL) {
                status->setStatus(STATUS_FIND_Refused_OutOfResources);
                return QR_EC_IndexDatabaseError;
            }
            if (plist->elem.PValueField != NULL &&
                strlen(plist->elem.PValueField) > 0) {
                OFCondition ec = dce->putString(plist->elem.PValueField);
                if (ec != EC_Normal) {
                    std::cout << "error code>>" << ec.text() << std::endl;
                    std::cout << "tag>>" << plist->elem.XTag.key[0] << "," << plist->elem.XTag.key[1] << std::endl;
                    std::cout << "val>>" << plist->elem.PValueField << std::endl;
                    DCMQRDB_WARN("dbfind: DB_nextFindResponse: cannot put()");
                    status->setStatus(STATUS_FIND_Failed_UnableToProcess);
                    return QR_EC_IndexDatabaseError;
                }
            }
            OFCondition ec = (*findResponseIdentifiers)->insert(dce, OFTrue /*replaceOld*/);
            if (ec != EC_Normal) {
                DCMQRDB_WARN("dbfind: DB_nextFindResponse: cannot insert()");
                status->setStatus(STATUS_FIND_Failed_UnableToProcess);
                return QR_EC_IndexDatabaseError;
            }
        }

        /*** Append the Query level
        **/

        switch (handle_->queryLevel) {
        case PATIENT_LEVEL :
            queryLevelString = PATIENT_LEVEL_STRING ;
            break ;
        case STUDY_LEVEL :
            queryLevelString = STUDY_LEVEL_STRING ;
            break ;
        case SERIE_LEVEL :
            queryLevelString = SERIE_LEVEL_STRING ;
            break ;
        case IMAGE_LEVEL :
            queryLevelString = IMAGE_LEVEL_STRING ;
            break ;
        }
        DU_putStringDOElement(*findResponseIdentifiers,
                              DCM_QueryRetrieveLevel, queryLevelString);

#ifdef DCMTK_ENABLE_CHARSET_CONVERSION
        OFString specificCharacterSet;
        if ((*findResponseIdentifiers)->findAndGetOFStringArray(DCM_SpecificCharacterSet, specificCharacterSet).bad())
            specificCharacterSet.clear();

        const OFString* destinationCharacterSet = NULL;
        const OFString* fallbackCharacterSet = NULL;

        if (characterSetOptions.flags & DcmQueryRetrieveCharacterSetOptions::Override) {
            destinationCharacterSet = &characterSetOptions.characterSet;
            if (
                (characterSetOptions.flags & DcmQueryRetrieveCharacterSetOptions::Fallback) &&
                characterSetOptions.characterSet != handle_->findRequestCharacterSet
            ) {
                fallbackCharacterSet = &handle_->findRequestCharacterSet;
            }
        } else {
            destinationCharacterSet = &handle_->findRequestCharacterSet;
            if (
                (characterSetOptions.flags & DcmQueryRetrieveCharacterSetOptions::Fallback) &&
                characterSetOptions.characterSet != handle_->findRequestCharacterSet
            ) {
                fallbackCharacterSet = &characterSetOptions.characterSet;
            }
        }

        if (isConversionNecessary(specificCharacterSet, *destinationCharacterSet)) {
            OFCondition charset_status = (*findResponseIdentifiers)->convertCharacterSet(
                specificCharacterSet,
                *destinationCharacterSet,
                characterSetOptions.conversionFlags,
                OFTrue);
            if (charset_status.bad()) {
                DCMQRDB_WARN("Converting response from character set \""
                    << characterSetName(specificCharacterSet)
                    << "\" to character set \""
                    << characterSetName(*destinationCharacterSet)
                    << "\" failed, (error message: " << charset_status.text() << ')');
                if (fallbackCharacterSet && isConversionNecessary(specificCharacterSet, *fallbackCharacterSet)) {
                    DCMQRDB_INFO("Trying to convert response from character set \""
                        << characterSetName(specificCharacterSet)
                        << "\" to fall-back character set \""
                        << characterSetName(*fallbackCharacterSet) << "\" instead");
                    charset_status = (*findResponseIdentifiers)->convertCharacterSet(
                        specificCharacterSet,
                        *fallbackCharacterSet,
                        characterSetOptions.conversionFlags,
                        OFTrue);
                    if (charset_status.bad()) {
                        DCMQRDB_WARN("Converting response from character set \""
                            << characterSetName(specificCharacterSet)
                            << "\" to character set \""
                            << characterSetName(*fallbackCharacterSet)
                            << "\" failed, (error message: " << charset_status.text() << ')');
                    } else {
                        DCMQRDB_INFO("Successfully converted response from character set \""
                            << characterSetName(specificCharacterSet)
                            << "\" to character set \""
                            << characterSetName(*fallbackCharacterSet) << "\"");
                    }
                } else if (fallbackCharacterSet) {
                    DCMQRDB_INFO("Conversion to fall-back character set \""
                        << characterSetName(*fallbackCharacterSet)
                        << "\" is not necessary, since the original character set is compatible");
                }
            } else {
                DCMQRDB_INFO("Successfully converted response from character set \""
                    << characterSetName(specificCharacterSet)
                    << "\" to character set \""
                    << characterSetName(*destinationCharacterSet)
                    << "\"");
            }
        }
#endif

#ifdef DEBUG
        DCMQRDB_DEBUG("DB: findResponseIdentifiers:" << OFendl
            << DcmObject::PrintHelper(**findResponseIdentifiers));
#endif
    } else {

        DB_unlock();

        return (QR_EC_IndexDatabaseError) ;
    }

    switch (handle_->rootLevel) {
    case PATIENT_ROOT : qLevel = PATIENT_LEVEL ;        break ;
    case STUDY_ROOT :   qLevel = STUDY_LEVEL ;          break ;
    case PATIENT_STUDY: qLevel = PATIENT_LEVEL ;        break ;
    }

    /***** Free the last response...
    ****/

    DB_FreeElementList (handle_->findResponseList) ;
    handle_->findResponseList = NULL ;

    /***** ... and find the next one
    ****/

    MatchFound = OFFalse ;
    cond = EC_Normal ;

    CharsetConsideringMatcher dbmatch(*handle_);
    MatchFound = mongoDBFindRecord(handle_, idxRec, qLevel, qLevel, &MatchFound, dbmatch);


    /**** If an error occurred in Matching function
    ****    return status is pending
    ***/

    if (cond != EC_Normal) {
        handle_->idxCounter = -1 ;
        DB_FreeElementList (handle_->findRequestList) ;
        handle_->findRequestList = NULL ;
#ifdef DEBUG
        DCMQRDB_DEBUG("DB_nextFindResponse () : STATUS_FIND_Failed_UnableToProcess");
#endif
        status->setStatus(STATUS_FIND_Failed_UnableToProcess);

        DB_unlock();

        return (cond) ;
    }

    /**** If a matching image has been found
    ****    add index records UIDs in found UID list
    ****    prepare Response List in handle
    ***/

    ConsoleLog("Next Match Found = " + std::to_string(MatchFound), "");
    if (MatchFound) {
        DB_UIDAddFound (handle_, &idxRec) ;
        makeResponseList (handle_, &idxRec) ;
#ifdef DEBUG
        DCMQRDB_DEBUG("DB_nextFindResponse () : STATUS_Pending");
#endif
        status->setStatus(STATUS_Pending);
        return (EC_Normal) ;
    }

    /**** else no matching image has been found,
    ****    free query identifiers list
    **** Response list is null, so next call will return STATUS_Success
    ***/

    else {
        handle_->idxCounter = -1 ;
        DB_FreeElementList (handle_->findRequestList) ;
        handle_->findRequestList = NULL ;
        DB_FreeUidList (handle_->uidList) ;
        handle_->uidList = NULL ;
    }

#ifdef DEBUG
    DCMQRDB_DEBUG("DB_nextFindResponse () : STATUS_Pending");
#endif
    status->setStatus(STATUS_Pending);
    return (EC_Normal) ;
}

/********************
**      Cancel find request
 */

OFCondition DcmQueryRetrieveIndexDatabaseHandle::cancelFindRequest (DcmQueryRetrieveDatabaseStatus *status)
{

    handle_->idxCounter = -1 ;
    DB_FreeElementList (handle_->findRequestList) ;
    handle_->findRequestList = NULL ;
    DB_FreeElementList (handle_->findResponseList) ;
    handle_->findResponseList = NULL ;
    DB_FreeUidList (handle_->uidList) ;
    handle_->uidList = NULL ;

    status->setStatus(STATUS_FIND_Cancel_MatchingTerminatedDueToCancelRequest);

    DB_unlock();

    return (EC_Normal) ;
}

/* ========================= MOVE ========================= */

/************
 *      Test a Move Request List
 *      Returns EC_Normal if OK, else returns QR_EC_IndexDatabaseError
 */

OFCondition DcmQueryRetrieveIndexDatabaseHandle::testMoveRequestList (
                DB_ElementList  *findRequestList,
                DB_LEVEL        queryLevel,
                DB_LEVEL        infLevel,
                DB_LEVEL        lowestLevel
                )
{
    DB_ElementList *plist ;
    DB_LEVEL    XTagLevel = PATIENT_LEVEL; // DB_GetTagLevel() will set this correctly
    DB_KEY_TYPE XTagType  = OPTIONAL_KEY;  // DB_GetTagKeyAttr() will set this
    int level ;

    /**** Query level must be at least the infLevel
    ***/

    if (queryLevel < infLevel) {
        DCMQRDB_DEBUG("Level incompatible with Information Model (level " << (int)queryLevel << ")");
        return QR_EC_IndexDatabaseError ;
    }

    if (queryLevel > lowestLevel) {
        DCMQRDB_DEBUG("Level incompatible with Information Model (level " << (int)queryLevel << ")");
        return QR_EC_IndexDatabaseError ;
    }

    for (level = PATIENT_LEVEL ; level <= IMAGE_LEVEL ; level++) {

        /**** Manage exception due to StudyRoot Information Model :
        **** In this information model, move may not include any
        **** Patient attributes.
        ***/

        if ((level == PATIENT_LEVEL) && (infLevel == STUDY_LEVEL)) {

            /** In Study Root Information Model, do not accept any
            ** Patient Tag
            */

            int atLeastOneKeyFound = OFFalse ;
            for (plist = findRequestList ; plist ; plist = plist->next) {
                DB_GetTagLevel (plist->elem. XTag, &XTagLevel) ;
                if (XTagLevel != level)
                    continue ;
                atLeastOneKeyFound = OFTrue ;
            }
            if (atLeastOneKeyFound) {
                DCMQRDB_DEBUG("Key found in Study Root Information Model (level " << level << ")");
                return QR_EC_IndexDatabaseError ;
            }
        }

        /**** If current level is above or equal to the QueryLevel
        ***/

        else if (level <= queryLevel) {

            /** For these levels, only unique keys are allowed
            ** Parse the request list elements referring to
            ** this level.
            ** Check that only unique key attr are provided
            */

            int uniqueKeyFound = OFFalse ;
            for (plist = findRequestList ; plist ; plist = plist->next) {
                DB_GetTagLevel (plist->elem. XTag, &XTagLevel) ;
                if (XTagLevel != level)
                    continue ;
                DB_GetTagKeyAttr (plist->elem. XTag, &XTagType) ;
                if (XTagType != UNIQUE_KEY) {
                    DCMQRDB_DEBUG("Non Unique Key found (level " << level << ")");
                    std::cout << "E:" << "Non Unique Key found (level " << level << ")" << std::endl;
                    return QR_EC_IndexDatabaseError ;
                }
                else if (plist->elem.ValueLength == 0) {
                    DCMQRDB_DEBUG("Unique Key value is empty (level " << level << ")");
                    std::cout << "E:" << "Unique Key value is empty (level " << level << ")" << std::endl;
                    return QR_EC_IndexDatabaseError ;
                }
                else if (uniqueKeyFound) {
                    DCMQRDB_DEBUG("More than one Unique Key found (level " << level << ")");
                    std::cout << "E:" << "More than one Unique Key found (level " << level << ")" << std::endl;
                    return QR_EC_IndexDatabaseError ;
                }
                else
                    uniqueKeyFound = OFTrue ;
            }
            if (! uniqueKeyFound) {
                DCMQRDB_DEBUG("No Unique Key found (level " << level << ")");
                return QR_EC_IndexDatabaseError ;
            }
        }

        /**** If current level beyond the QueryLevel
        ***/

        else if (level > queryLevel) {

            /** For this level, no key is allowed
            ** Parse the request list elements referring to
            ** this level.
            ** Check that no key is provided
            */

            int atLeastOneKeyFound = OFFalse ;
            for (plist = findRequestList ; plist ; plist = plist->next) {
                DB_GetTagLevel (plist->elem. XTag, &XTagLevel) ;
                if (XTagLevel != level)
                    continue ;
                atLeastOneKeyFound = OFTrue ;
            }
            if (atLeastOneKeyFound) {
                DCMQRDB_DEBUG("Key found beyond query level (level " << level << ")");
                return QR_EC_IndexDatabaseError ;
            }
        }

    }
    return EC_Normal ;
}


OFCondition DcmQueryRetrieveIndexDatabaseHandle::startMoveRequest(
        const char      *SOPClassUID,
        DcmDataset      *moveRequestIdentifiers,
        DcmQueryRetrieveDatabaseStatus  *status)
{
    ReadMongoConfig();
    DB_SmallDcmElmt     elem ;
    DB_ElementList      *plist = NULL;
    DB_ElementList      *last = NULL;
    DB_CounterList      *pidxlist = NULL;
    DB_CounterList      *lastidxlist = NULL;
    int                 MatchFound = OFFalse;
    IdxRecord           idxRec ;
    DB_LEVEL            qLevel = PATIENT_LEVEL; // highest legal level for a query in the current model
    DB_LEVEL            lLevel = IMAGE_LEVEL;   // lowest legal level for a query in the current model
    OFCondition         cond = EC_Normal;
    OFBool qrLevelFound = OFFalse;

    /**** Is SOPClassUID supported ?
    ***/

    if (strcmp( SOPClassUID, UID_MOVEPatientRootQueryRetrieveInformationModel) == 0)
        handle_->rootLevel = PATIENT_ROOT ;
    else if (strcmp( SOPClassUID, UID_MOVEStudyRootQueryRetrieveInformationModel) == 0)
        handle_->rootLevel = STUDY_ROOT ;
#ifndef NO_PATIENTSTUDYONLY_SUPPORT
    else if (strcmp( SOPClassUID, UID_RETIRED_MOVEPatientStudyOnlyQueryRetrieveInformationModel) == 0)
        handle_->rootLevel = PATIENT_STUDY ;
#endif
#ifndef NO_GET_SUPPORT
    /* experimental support for GET */
    else if (strcmp( SOPClassUID, UID_GETPatientRootQueryRetrieveInformationModel) == 0)
        handle_->rootLevel = PATIENT_ROOT ;
    else if (strcmp( SOPClassUID, UID_GETStudyRootQueryRetrieveInformationModel) == 0)
        handle_->rootLevel = STUDY_ROOT ;
#ifndef NO_PATIENTSTUDYONLY_SUPPORT
    else if (strcmp( SOPClassUID, UID_RETIRED_GETPatientStudyOnlyQueryRetrieveInformationModel) == 0)
        handle_->rootLevel = PATIENT_STUDY ;
#endif
#endif

    else {
        status->setStatus(STATUS_MOVE_Refused_SOPClassNotSupported);
        return (QR_EC_IndexDatabaseError) ;
    }


    /**** Parse Identifiers in the Dicom Object
    **** Find Query Level and construct a list
    **** of query identifiers
    ***/

    int elemCount = (int)(moveRequestIdentifiers->card());
    for (int elemIndex=0; elemIndex<elemCount; elemIndex++) {

        DcmElement* dcelem = moveRequestIdentifiers->getElement(elemIndex);

        elem.XTag = dcelem->getTag().getXTag();
        if (elem.XTag == DCM_QueryRetrieveLevel || DB_TagSupported(elem.XTag)) {
            elem.ValueLength = dcelem->getLength();
            if (elem.ValueLength == 0) {
                elem.PValueField = NULL ;
            } else if ((elem.PValueField = (char*)malloc((size_t)(elem.ValueLength+1))) == NULL) {
                status->setStatus(STATUS_MOVE_Failed_UnableToProcess);
                return (QR_EC_IndexDatabaseError) ;
            } else {
                /* only char string type tags are supported at the moment */
                char *s = NULL;
                dcelem->getString(s);
                /* the available space is always elem.ValueLength+1 */
                OFStandard::strlcpy(elem.PValueField, s, elem.ValueLength+1);
            }

            /** If element is the Query Level, store it in handle
             */

            if (elem. XTag == DCM_QueryRetrieveLevel && elem.PValueField) {
                char *pc ;
                char level [50] ;

                strncpy (level, (char *) elem. PValueField, (size_t)((elem. ValueLength < 50) ? elem. ValueLength : 49)) ;

                /*** Skip this two lines if you want strict comparison
                **/

                for (pc = level ; *pc ; pc++)
                    *pc = ((*pc >= 'a') && (*pc <= 'z')) ? 'A' - 'a' + *pc : *pc ;

                if (strncmp (level, PATIENT_LEVEL_STRING,
                             strlen (PATIENT_LEVEL_STRING)) == 0)
                    handle_->queryLevel = PATIENT_LEVEL ;
                else if (strncmp (level, STUDY_LEVEL_STRING,
                                  strlen (STUDY_LEVEL_STRING)) == 0)
                    handle_->queryLevel = STUDY_LEVEL ;
                else if (strncmp (level, SERIE_LEVEL_STRING,
                                  strlen (SERIE_LEVEL_STRING)) == 0)
                    handle_->queryLevel = SERIE_LEVEL ;
                else if (strncmp (level, IMAGE_LEVEL_STRING,
                                  strlen (IMAGE_LEVEL_STRING)) == 0)
                    handle_->queryLevel = IMAGE_LEVEL ;
                else {
#ifdef DEBUG
                    DCMQRDB_DEBUG("DB_startMoveRequest : STATUS_MOVE_Failed_UnableToProcess");
#endif
                    status->setStatus(STATUS_MOVE_Failed_UnableToProcess);
                    return (QR_EC_IndexDatabaseError) ;
                }
                qrLevelFound = OFTrue;
            } else {
                /** Else it is a query identifier
                ** Append it to our RequestList
                */
                if (! DB_TagSupported (elem. XTag))
                    continue ;

                plist = new DB_ElementList ;
                if (plist == NULL) {
                    status->setStatus(STATUS_FIND_Refused_OutOfResources);
                    return (QR_EC_IndexDatabaseError) ;
                }
                DB_DuplicateElement (&elem, & (plist->elem)) ;
                if (handle_->findRequestList == NULL) {
                    handle_->findRequestList = last = plist ;
                } else {
                    last->next = plist ;
                    last = plist ;
                }
            }

            if ( elem. PValueField ) {
                free (elem. PValueField) ;
            }
        }
    }

    if (!qrLevelFound) {
        /* The Query/Retrieve Level is missing */
        status->setStatus(STATUS_MOVE_Error_DataSetDoesNotMatchSOPClass);
        DCMQRDB_WARN("DB_startMoveRequest(): missing Query/Retrieve Level");
        ConsoleLog("Request is missing Query/Retrieve Level", "E");
        std::cout << "Error:" << "DB_startMoveRequest(): missing Query/Retrieve Level" << std::endl;
        handle_->idxCounter = -1 ;
        DB_FreeElementList (handle_->findRequestList) ;
        handle_->findRequestList = NULL ;
        return (QR_EC_IndexDatabaseError) ;
    }

    switch (handle_->rootLevel)
    {
      case PATIENT_ROOT :
        qLevel = PATIENT_LEVEL ;
        lLevel = IMAGE_LEVEL ;
        break ;
      case STUDY_ROOT :
        qLevel = STUDY_LEVEL ;
        lLevel = IMAGE_LEVEL ;
        break ;
      case PATIENT_STUDY:
        qLevel = PATIENT_LEVEL ;
        lLevel = STUDY_LEVEL ;
        break ;
    }

    /**** Test the consistency of the request list
    ***/

    if (doCheckMoveIdentifier) {
        cond = testMoveRequestList (handle_->findRequestList,
                                    handle_->queryLevel, qLevel, lLevel) ;
        if (cond != EC_Normal) {
            handle_->idxCounter = -1 ;
            DB_FreeElementList (handle_->findRequestList) ;
            handle_->findRequestList = NULL ;
#ifdef DEBUG
            DCMQRDB_DEBUG("DB_startMoveRequest () : STATUS_MOVE_Error_DataSetDoesNotMatchSOPClass - Invalid RequestList");
#endif
            status->setStatus(STATUS_MOVE_Error_DataSetDoesNotMatchSOPClass);
            return (cond) ;
        }
    }

    // 開始從資料庫尋找

    MatchFound = OFFalse ;
    handle_->moveCounterList = NULL ;
    handle_->NumberRemainOperations = 0 ;

    /**** Find matching images
    ***/

    DB_lock(OFFalse);

    CharsetConsideringMatcher dbmatch(*handle_);

    mongoDBFindRecordsForMove(handle_, plist, pidxlist, lastidxlist, status);

    DB_FreeElementList (handle_->findRequestList) ;
    handle_->findRequestList = NULL ;

    /**** If a matching image has been found,
    ****    status is pending
    ***/

    if ( handle_->NumberRemainOperations > 0 ) {
#ifdef DEBUG
        DCMQRDB_DEBUG("DB_startMoveRequest : STATUS_Pending");
#endif
        status->setStatus(STATUS_Pending);
        return (EC_Normal) ;
    }

    /**** else no matching image has been found,
    ****    free query identifiers list
    ****    status is success
    ***/

    else {
        handle_->idxCounter = -1 ;
#ifdef DEBUG
        DCMQRDB_DEBUG("DB_startMoveRequest : STATUS_Success");
#endif
        status->setStatus(STATUS_Success);

        DB_unlock();

        return (EC_Normal) ;
    }


}

OFCondition DcmQueryRetrieveIndexDatabaseHandle::nextMoveResponse(
    char *SOPClassUID,
    size_t SOPClassUIDSize,
    char *SOPInstanceUID,
    size_t SOPInstanceUIDSize,
    char *imageFileName,
    size_t imageFileNameSize,
    unsigned short *numberOfRemainingSubOperations,
    DcmQueryRetrieveDatabaseStatus *status)
{
    IdxRecord           *idxRec ;
    DB_CounterList              *nextlist ;

    /**** If all matching images have been retrieved,
    ****    status is success
    ***/

    if ( handle_->NumberRemainOperations <= 0 ) {
        status->setStatus(STATUS_Success);

        DB_unlock();

        return (EC_Normal) ;
    }

    /**** Goto the next matching image number of Index File
    ***/

    idxRec = handle_->moveCounterList->rec;
    /*if (DB_IdxRead(handle_->moveCounterList->idxCounter, &idxRec) != EC_Normal) {
#ifdef DEBUG
        DCMQRDB_DEBUG("DB_nextMoveResponse : STATUS_MOVE_Failed_UnableToProcess");
#endif
        status->setStatus(STATUS_MOVE_Failed_UnableToProcess);

        DB_unlock();

        return (QR_EC_IndexDatabaseError) ;
    }*/

    OFStandard::strlcpy(SOPClassUID, (char *) idxRec-> SOPClassUID, SOPClassUIDSize) ;
    OFStandard::strlcpy(SOPInstanceUID, (char *) idxRec-> SOPInstanceUID, SOPInstanceUIDSize) ;
    OFStandard::strlcpy(imageFileName, (char *) idxRec-> filename, imageFileNameSize) ;

    *numberOfRemainingSubOperations = OFstatic_cast(unsigned short, (--handle_->NumberRemainOperations));

    nextlist = handle_->moveCounterList->next ;
    free (handle_->moveCounterList) ;
    handle_->moveCounterList = nextlist ;
    status->setStatus(STATUS_Pending);
#ifdef DEBUG
    DCMQRDB_DEBUG("DB_nextMoveResponse : STATUS_Pending");
#endif
    return (EC_Normal) ;
}



OFCondition DcmQueryRetrieveIndexDatabaseHandle::cancelMoveRequest (DcmQueryRetrieveDatabaseStatus *status)
{
    DB_CounterList *plist ;

    while (handle_->moveCounterList) {
        plist  = handle_->moveCounterList ;
        handle_->moveCounterList = handle_->moveCounterList->next ;
        free (plist) ;
    }

    status->setStatus(STATUS_MOVE_Cancel_SubOperationsTerminatedDueToCancelIndication);

    DB_unlock();

    return (EC_Normal) ;
}


/* ========================= STORE ========================= */


void DcmQueryRetrieveIndexDatabaseHandle::enableQuotaSystem(OFBool enable)
{
    quotaSystemEnabled = enable;
}


/*
** Image file deleting
*/


OFCondition DcmQueryRetrieveIndexDatabaseHandle::deleteImageFile(char* imgFile)
{
    if (!quotaSystemEnabled) {
      DCMQRDB_WARN("file delete operations are disabled, keeping file: " << imgFile << " despite duplicate SOP Instance UID");
      return EC_Normal;
    } else {
      DCMQRDB_WARN("Deleting file: " << imgFile << " due to quota or duplicate SOP instance UID");
    }

#ifdef LOCK_IMAGE_FILES
    int lockfd;
#ifdef O_BINARY
    lockfd = open(imgFile, O_RDWR | O_BINARY, 0666);    /* obtain file descriptor */
#else
    lockfd = open(imgFile, O_RDWR, 0666);   /* obtain file descriptor */
#endif
    if (lockfd < 0) {
      DCMQRDB_WARN("DB ERROR: cannot open image file for deleting: " << imgFile);
      return QR_EC_IndexDatabaseError;
    }
    if (dcmtk_flock(lockfd, LOCK_EX) < 0) { /* exclusive lock (blocking) */
      DCMQRDB_WARN("DB ERROR: cannot lock image file for deleting: " << imgFile);
      dcmtk_plockerr("DB ERROR");
    }
#endif

    if (unlink(imgFile) < 0) {
        /* delete file */
        DCMQRDB_ERROR("DB ERROR: cannot delete image file: " << imgFile << OFendl
            << "QR_EC_IndexDatabaseError: " << OFStandard::getLastSystemErrorCode().message());
    }

#ifdef LOCK_IMAGE_FILES
    if (dcmtk_flock(lockfd, LOCK_UN) < 0) { /* unlock */
        DCMQRDB_WARN("DB ERROR: cannot unlock image file for deleting: " << imgFile);
        dcmtk_plockerr("DB ERROR");
     }
    close(lockfd);              /* release file descriptor */
#endif

    return EC_Normal;
}


/*************************
**   Delete oldest study in database
 */

int DcmQueryRetrieveIndexDatabaseHandle::deleteOldestStudy(StudyDescRecord *pStudyDesc)
{

    int oldestStudy ;
    double OldestDate ;
    int s ;
    size_t n ;
    int idx = 0 ;
    IdxRecord idxRec ;

    oldestStudy = 0 ;
    OldestDate = 0.0 ;

#ifdef DEBUG
    DCMQRDB_DEBUG("deleteOldestStudy");
#endif

    for ( s = 0 ; s < handle_ -> maxStudiesAllowed ; s++ ) {
        if ( ( pStudyDesc[s]. NumberofRegistratedImages != 0 ) &&
            ( ( OldestDate == 0.0 ) || ( pStudyDesc[s]. LastRecordedDate < OldestDate ) ) ) {
            OldestDate = pStudyDesc[s]. LastRecordedDate ;
            oldestStudy = s ;
        }
    }

#ifdef DEBUG
    DCMQRDB_DEBUG("deleteOldestStudy oldestStudy = " << oldestStudy);
#endif

    n = strlen(pStudyDesc[oldestStudy].StudyInstanceUID) ;
    while ( DB_IdxRead (idx, &idxRec) == EC_Normal ) {

    if ( ! ( strncmp(idxRec. StudyInstanceUID, pStudyDesc[oldestStudy].StudyInstanceUID, n) ) ) {
        DB_IdxRemove (idx) ;
        deleteImageFile(idxRec.filename);
    }
    idx++ ;
    }

    pStudyDesc[oldestStudy].NumberofRegistratedImages = 0 ;
    pStudyDesc[oldestStudy].StudySize = 0 ;
    return(oldestStudy) ;
}




/*************************
**   Delete oldest images in database
 */

OFCondition DcmQueryRetrieveIndexDatabaseHandle::deleteOldestImages(StudyDescRecord *pStudyDesc, int StudyNum, char *StudyUID, long RequiredSize)
{

    ImagesofStudyArray *StudyArray ;
    IdxRecord idxRec ;
    int nbimages = 0 , s = 0;
    size_t n ;
    long DeletedSize ;

#ifdef DEBUG
    DCMQRDB_DEBUG("deleteOldestImages RequiredSize = " << RequiredSize);
#endif
    n = strlen(StudyUID) ;
    StudyArray = (ImagesofStudyArray *)malloc(MAX_NUMBER_OF_IMAGES * sizeof(ImagesofStudyArray)) ;

    if (StudyArray == NULL) {
        DCMQRDB_WARN("deleteOldestImages: out of memory");
        return QR_EC_IndexDatabaseError;
    }

    /** Find all images having the same StudyUID
     */

    DB_IdxInitLoop (&(handle_ -> idxCounter)) ;
    while ( DB_IdxGetNext(&(handle_ -> idxCounter), &idxRec) == EC_Normal ) {
    if ( ! ( strncmp(idxRec. StudyInstanceUID, StudyUID, n) ) ) {

        StudyArray[nbimages]. idxCounter = handle_ -> idxCounter ;
        StudyArray[nbimages]. RecordedDate = idxRec. RecordedDate ;
        StudyArray[nbimages++]. ImageSize = idxRec. ImageSize ;
    }
    }

    /** Sort the StudyArray in order to have the oldest images first
     */
    qsort((char *)StudyArray, nbimages, sizeof(ImagesofStudyArray), DB_Compare) ;

#ifdef DEBUG
    {
        int i ;
        DCMQRDB_DEBUG("deleteOldestImages : Sorted images ref array");
        for (i = 0 ; i < nbimages ; i++)
            DCMQRDB_DEBUG("[" << STD_NAMESPACE setw(2) << i << "] :   Size " << StudyArray[i].ImageSize
                << "   Date " << STD_NAMESPACE setw(20) << STD_NAMESPACE setprecision(3) << StudyArray[i].RecordedDate
                << "   Ref " << StudyArray[i].idxCounter);
        DCMQRDB_DEBUG("deleteOldestImages : end of ref array");
    }
#endif

    s = 0 ;
    DeletedSize = 0 ;

    while ( DeletedSize < RequiredSize ) {

    IdxRecord idxRemoveRec ;
    DB_IdxRead (StudyArray[s]. idxCounter, &idxRemoveRec) ;
#ifdef DEBUG
    DCMQRDB_DEBUG("Removing file : " << idxRemoveRec. filename);
#endif
    deleteImageFile(idxRemoveRec.filename);

    DB_IdxRemove (StudyArray[s]. idxCounter) ;
    pStudyDesc[StudyNum].NumberofRegistratedImages -= 1 ;
    pStudyDesc[StudyNum].StudySize -= StudyArray[s]. ImageSize ;
    DeletedSize += StudyArray[s++]. ImageSize ;
    }

#ifdef DEBUG
    DCMQRDB_DEBUG("deleteOldestImages DeletedSize = " << (int)DeletedSize);
#endif
    free(StudyArray) ;
    return( EC_Normal ) ;

}



/*************************
 *   Verify if study UID already exists
 *   If the study UID exists, its index in the study descriptor is returned.
 *   If the study UID does not exist, the index of the first unused descriptor entry is returned.
 *   If no entries are free, maxStudiesAllowed is returned.
 */

int DcmQueryRetrieveIndexDatabaseHandle::matchStudyUIDInStudyDesc (StudyDescRecord *pStudyDesc, char *StudyUID, int maxStudiesAllowed)
{
    int s = 0 ;
    while  (s < maxStudiesAllowed)
    {
      if ((pStudyDesc[s].NumberofRegistratedImages > 0) && (0 == strcmp(pStudyDesc[s].StudyInstanceUID, StudyUID))) break;
      s++ ;
    }
    if (s==maxStudiesAllowed) // study uid does not exist, look for free descriptor
    {
      s=0;
      while  (s < maxStudiesAllowed)
      {
        if (pStudyDesc[s].NumberofRegistratedImages == 0) break;
        s++ ;
      }
    }
    return s;
}


/*************************
**  Check up storage rights in Study Desk record
 */

OFCondition DcmQueryRetrieveIndexDatabaseHandle::checkupinStudyDesc(StudyDescRecord *pStudyDesc, char *StudyUID, long imageSize)
{
    int         s ;
    long        RequiredSize ;

    s = matchStudyUIDInStudyDesc (pStudyDesc, StudyUID,
                     (int)(handle_ -> maxStudiesAllowed)) ;

    /** If Study already exists
     */

    if ( pStudyDesc[s]. NumberofRegistratedImages != 0 ) {

#ifdef DEBUG
    DCMQRDB_DEBUG("checkupinStudyDesc: study already exists : " << s) ;
#endif
    if ( OFstatic_cast(size_t, pStudyDesc[s]. StudySize) + imageSize  >
         OFstatic_cast(size_t, handle_ -> maxBytesPerStudy) )
    {
        if ( imageSize > handle_ -> maxBytesPerStudy ) {
#ifdef DEBUG
            DCMQRDB_DEBUG("checkupinStudyDesc: imageSize = " << imageSize << " too large");
#endif
            return ( QR_EC_IndexDatabaseError ) ;
        }

        RequiredSize = imageSize -
            ( handle_ -> maxBytesPerStudy - pStudyDesc[s]. StudySize ) ;
        deleteOldestImages(pStudyDesc, s, StudyUID, RequiredSize) ;
    }


    }
    else {
#ifdef DEBUG
    DCMQRDB_DEBUG("checkupinStudyDesc: study doesn't already exist");
#endif
    if ( imageSize > handle_ -> maxBytesPerStudy ) {
#ifdef DEBUG
        DCMQRDB_DEBUG("checkupinStudyDesc: imageSize = " << imageSize << " too large");
#endif
        return ( QR_EC_IndexDatabaseError ) ;
    }
    if ( s > ( handle_ -> maxStudiesAllowed - 1 ) )
        s = deleteOldestStudy(pStudyDesc) ;

    }

    pStudyDesc[s]. StudySize += imageSize ;
#ifdef DEBUG
    DCMQRDB_DEBUG("checkupinStudyDesc: ~~~~~~~~ StudySize = " << pStudyDesc[s]. StudySize);
#endif

    /* we only have second accuracy */
    pStudyDesc[s]. LastRecordedDate =  (double) time(NULL);

    pStudyDesc[s]. NumberofRegistratedImages++ ;
    OFStandard::strlcpy(pStudyDesc[s].StudyInstanceUID, StudyUID, UI_MAX_LENGTH+1) ;

    if ( DB_StudyDescChange (pStudyDesc) == EC_Normal)
        return ( EC_Normal ) ;
    else
        return ( QR_EC_IndexDatabaseError ) ;
}

/*
 * If the image is already stored remove it from the database.
 * hewett - Nov. 1, 93
 */
OFCondition DcmQueryRetrieveIndexDatabaseHandle::removeDuplicateImage(
    const char* SOPInstanceUID, const char* StudyInstanceUID,
    StudyDescRecord* pStudyDesc, const char* newImageFileName)
{

    int idx = 0;
    IdxRecord idxRec;
    int studyIdx = 0;

    studyIdx = matchStudyUIDInStudyDesc(pStudyDesc, (char*)StudyInstanceUID,
        (int)(handle_->maxStudiesAllowed));

    if (pStudyDesc[studyIdx].NumberofRegistratedImages == 0) {
        /* no study images, cannot be any old images */
        return EC_Normal;
    }

    while (DB_IdxRead(idx, &idxRec) == EC_Normal) {

        if (strcmp(idxRec.SOPInstanceUID, SOPInstanceUID) == 0) {

#ifdef DEBUG
            DCMQRDB_DEBUG("--- Removing Existing DB Image Record: " << idxRec.filename);
#endif
            /* remove the idx record  */
            DB_IdxRemove(idx);
            /* only remove the image file if it is different than that
             * being entered into the database.
             */
            if (strcmp(idxRec.filename, newImageFileName) != 0) {
                deleteImageFile(idxRec.filename);
            }
            /* update the study info */
            pStudyDesc[studyIdx].NumberofRegistratedImages--;
            pStudyDesc[studyIdx].StudySize -= idxRec.ImageSize;
        }
        idx++;
    }
    /* the study record should be written to file later */
    return EC_Normal;
}


/*************************
**  Add data from imageFileName to database
* 在檔案儲存完成後，將dicom資料從檔案紀錄到資料庫。
 */

OFCondition DcmQueryRetrieveIndexDatabaseHandle::storeRequest (
    const char  *SOPClassUID,
    const char  * /*SOPInstanceUID*/,
    const char  *imageFileName,
    DcmQueryRetrieveDatabaseStatus *status,
    OFBool      isNew)
{
    // We only need dcmqrscp to store the file, not index.dat since we already had raccoon to do the store things.
    return EC_Normal;
}

/*
** Prune invalid DB records.
*/

OFCondition DcmQueryRetrieveIndexDatabaseHandle::pruneInvalidRecords()
{
    int idx = 0;
    IdxRecord idxRec ;
    StudyDescRecord *pStudyDesc;

    DB_lock(OFTrue);

    pStudyDesc = (StudyDescRecord *)malloc (SIZEOF_STUDYDESC) ;
    if (pStudyDesc == NULL) {
      DCMQRDB_WARN("DB_pruneInvalidRecords: out of memory");
      DB_unlock();
      return (QR_EC_IndexDatabaseError) ;
    }

    for (int i = 0 ; i < handle_ -> maxStudiesAllowed ; i++ )
      pStudyDesc[i]. NumberofRegistratedImages = 0 ;

    DB_GetStudyDesc(pStudyDesc) ;

    while (DB_IdxRead(idx, &idxRec) == EC_Normal)
    {
      if (access(idxRec.filename, R_OK) < 0)
      {
#ifdef DEBUG
        DCMQRDB_DEBUG("*** Pruning Invalid DB Image Record: " << idxRec.filename);
#endif
        /* update the study info */
        int studyIdx = matchStudyUIDInStudyDesc(pStudyDesc, idxRec.StudyInstanceUID, (int)(handle_->maxStudiesAllowed)) ;
        if (studyIdx < handle_->maxStudiesAllowed)
        {
          if (pStudyDesc[studyIdx].NumberofRegistratedImages > 0)
          {
            pStudyDesc[studyIdx].NumberofRegistratedImages--;
          } else {
            pStudyDesc[studyIdx].NumberofRegistratedImages = 0;
            pStudyDesc[studyIdx].StudySize = 0;
            pStudyDesc[studyIdx].StudyInstanceUID[0] = '\0';
          }
          if (pStudyDesc[studyIdx].StudySize > idxRec.ImageSize)
          {
            pStudyDesc[studyIdx].StudySize -= idxRec.ImageSize;
          }
        }

        /* remove the idx record  */
        DB_IdxRemove (idx);
      }
      idx++;
    }

    DB_StudyDescChange (pStudyDesc);
    DB_unlock();
    free (pStudyDesc) ;
    return EC_Normal;
}


/* ========================= INDEX ========================= */


/************************
 *      Dump an index file
 */

void DcmQueryRetrieveIndexDatabaseHandle::printIndexFile (char *storeArea)
{
    int i ;
    int j ;
    IdxRecord           idxRec ;
    StudyDescRecord     *pStudyDesc;

    OFCondition result;
    DcmQueryRetrieveIndexDatabaseHandle handle(storeArea, -1, -1, result);
    if (result.bad()) return;

    pStudyDesc = (StudyDescRecord *)malloc (SIZEOF_STUDYDESC) ;
    if (pStudyDesc == NULL) {
        DCMQRDB_ERROR("printIndexFile: out of memory");
        return;
    }

    handle.DB_lock(OFFalse);

    handle.DB_GetStudyDesc(pStudyDesc);

    for (i=0; i<handle.handle_->maxStudiesAllowed; i++) {
        if (pStudyDesc[i].NumberofRegistratedImages != 0 ) {
            COUT << "******************************************************" << OFendl
                << "STUDY DESCRIPTOR: " << i << OFendl
                << "  Study UID: " << pStudyDesc[i].StudyInstanceUID << OFendl
                << "  StudySize: " << pStudyDesc[i].StudySize << OFendl
                << "  LastRecDate: " << pStudyDesc[i].LastRecordedDate << OFendl
                << "  NumOfImages: " << pStudyDesc[i].NumberofRegistratedImages << OFendl;
        }
    }

    handle.DB_IdxInitLoop (&j) ;
    while (1) {
        if (handle.DB_IdxGetNext(&j, &idxRec) != EC_Normal)
            break ;

        COUT << "*******************************************************" << OFendl;
        COUT << "RECORD NUMBER: " << j << OFendl << "  Status: ";
        if (idxRec.hstat == DVIF_objectIsNotNew)
            COUT << "is NOT new" << OFendl;
        else
            COUT << "is new" << OFendl;
        COUT << "  Filename: " << idxRec.filename << OFendl
             << "  ImageSize: " << idxRec.ImageSize << OFendl
             << "  RecordedDate: " << idxRec.RecordedDate << OFendl;
        for (i = 0 ; i < NBPARAMETERS ; i++) {  /* new definition */
            DB_SmallDcmElmt *se = idxRec.param + i;
            const char* value = "";
            if (se->PValueField != NULL) value = se->PValueField;
            DcmTag tag(se->XTag);
            COUT << "    " << tag.getTagName() << ": \"" << value << "\"" << OFendl;
        }
        COUT << "  InstanceDescription: \"" << idxRec.InstanceDescription << "\"" << OFendl;
    }
    COUT << "*******************************************************" << OFendl
         << "RECORDS IN THIS INDEXFILE: " << j << OFendl;

    handle.DB_unlock();

}

/************************
 *      Search in index file for SOP Class UID and SOP Instance UID. Used for the storage commitment server
 */

OFBool DcmQueryRetrieveIndexDatabaseHandle::findSOPInstance(const char *storeArea, const OFString &sopClassUID,const OFString &sopInstanceUID)
{
    int j ;
    IdxRecord           idxRec ;

    OFCondition result;
    OFBool Found = OFFalse;

    if (sopClassUID.empty() || sopInstanceUID.empty()) return Found;

    DcmQueryRetrieveIndexDatabaseHandle handle(storeArea, -1, -1, result);
    if (result.bad()) return Found;

    handle.DB_lock(OFFalse);

    handle.DB_IdxInitLoop (&j) ;
    while (1) {
        if (handle.DB_IdxGetNext(&j, &idxRec) != EC_Normal)
            break ;
        if (sopClassUID.compare(idxRec.SOPClassUID)==0 && sopInstanceUID.compare(idxRec.SOPInstanceUID)==0)
        {
            Found=OFTrue;
            break;
        }
    }
    handle.DB_unlock();
    return Found;
}


/* ========================= UTILS ========================= */


const char *DcmQueryRetrieveIndexDatabaseHandle::getStorageArea() const
{
    return handle_->storageArea;
}

const char *DcmQueryRetrieveIndexDatabaseHandle::getIndexFilename() const
{
    return handle_->indexFilename;
}

void DcmQueryRetrieveIndexDatabaseHandle::setIdentifierChecking(OFBool checkFind, OFBool checkMove)
{
    doCheckFindIdentifier = checkFind;
    doCheckMoveIdentifier = checkMove;
}


/***********************
 *      Creates a handle
 */

DcmQueryRetrieveIndexDatabaseHandle::DcmQueryRetrieveIndexDatabaseHandle(
    const char *storageArea,
    long maxStudiesPerStorageArea,
    long maxBytesPerStudy,
    OFCondition& result)
: handle_(NULL)
, quotaSystemEnabled(OFTrue)
, doCheckFindIdentifier(OFFalse)
, doCheckMoveIdentifier(OFFalse)
, fnamecreator()
{

    handle_ = new DB_Private_Handle;

#ifdef DEBUG
    DCMQRDB_DEBUG("DB_createHandle () : Handle created for " << storageArea);
    DCMQRDB_DEBUG("                     maxStudiesPerStorageArea: " << maxStudiesPerStorageArea
            << " maxBytesPerStudy: " << maxBytesPerStudy);
#endif

    /* check maximum number of studies for valid value */
    if (maxStudiesPerStorageArea < 0) {
        maxStudiesPerStorageArea = DB_UpperMaxStudies;
    }
    else if (maxStudiesPerStorageArea > DB_UpperMaxStudies) {
        DCMQRDB_WARN("maxStudiesPerStorageArea too large" << OFendl
            << "        setting to " << DB_UpperMaxStudies);
        maxStudiesPerStorageArea = DB_UpperMaxStudies;
    }
    /* check maximum study size for valid value value */
    if (maxBytesPerStudy < 0) {
        maxBytesPerStudy = DB_UpperMaxBytesPerStudy;
    }
    else if (maxBytesPerStudy > DB_UpperMaxBytesPerStudy) {
        DCMQRDB_WARN("maxBytesPerStudy too large" << OFendl
            << "        setting to " << DB_UpperMaxBytesPerStudy);
        maxBytesPerStudy = DB_UpperMaxBytesPerStudy;
    }

    if (handle_) {
        sprintf (handle_ -> storageArea,"%s", storageArea);
        sprintf (handle_ -> indexFilename,"%s%c%s", storageArea, PATH_SEPARATOR, DBINDEXFILE);

        /* create index file if it does not already exist */
        FILE* f = fopen(handle_->indexFilename, "ab");
        if (f == NULL) {
            DCMQRDB_ERROR(handle_->indexFilename << ": " << OFStandard::getLastSystemErrorCode().message());
            result = QR_EC_IndexDatabaseError;
            return;
        }
        fclose(f);

        /* open fd of index file */
#ifdef O_BINARY
        handle_ -> pidx = open(handle_ -> indexFilename, O_RDWR | O_BINARY );
#else
        handle_ -> pidx = open(handle_ -> indexFilename, O_RDWR );
#endif
        if ( handle_ -> pidx == (-1) )
        {
           result = QR_EC_IndexDatabaseError;
           return;
        }
        else
        {
            result = DB_lock(OFTrue);
            if ( result.bad() )
                return;

            // test whether the file contains more than zero bytes
            if ( DB_lseek( handle_ -> pidx, 0L, SEEK_END ) > 0 )
            {
                DB_lseek( handle_ -> pidx, 0L, SEEK_SET );
                // allocate HEADERSIZE + 1 bytes and fill it with zeros,
                // ensuring whatever is read is terminated with a NUL byte
                char header[DBHEADERSIZE+1] = {};
                // 0 is an invalid version, no matter what
                unsigned int version = 0;
                if
                (
                    read( handle_ -> pidx, header, DBHEADERSIZE ) != DBHEADERSIZE ||
                    strncmp( header, DBMAGIC, strlen(DBMAGIC) ) != 0              ||
                    sscanf( header + strlen(DBMAGIC), "%x", &version ) != 1       ||
                    version != DBVERSION
                )
                {
                    DB_unlock();
                    if ( version )
                        DCMQRDB_ERROR(handle_->indexFilename << ": invalid/unsupported QRDB database version " << version);
                    else
                        DCMQRDB_ERROR(handle_->indexFilename << ": unknown/legacy QRDB database file format");
                    result = QR_EC_IndexDatabaseError;
                    return;
                }
            }
            else
            {
                // write magic word and version number to the buffer
                // then write it to the file
                char header[DBHEADERSIZE + 1];
                sprintf( header, DBMAGIC "%.2X", DBVERSION );
                if ( write( handle_ -> pidx, header, DBHEADERSIZE ) != DBHEADERSIZE )
                {
                    DCMQRDB_ERROR(handle_->indexFilename << ": " << OFStandard::getLastSystemErrorCode().message());
                    DB_unlock();
                    result = QR_EC_IndexDatabaseError;
                    return;
                }
            }

            DB_unlock();

            handle_ -> idxCounter = -1;
            handle_ -> findRequestList = NULL;
            handle_ -> findResponseList = NULL;
            handle_ -> maxBytesPerStudy = maxBytesPerStudy;
            handle_ -> maxStudiesAllowed = maxStudiesPerStorageArea;
            handle_ -> uidList = NULL;
            result = EC_Normal;
            return;
        }
    }
    else
    {
        result = QR_EC_IndexDatabaseError;
        return;
    }
}

/***********************
 *      Destroys a handle
 */

DcmQueryRetrieveIndexDatabaseHandle::~DcmQueryRetrieveIndexDatabaseHandle()
{
    if (handle_)
    {
#ifndef _WIN32
      /* should not be necessary because we are closing the file handle anyway.
       * On Unix systems this does no harm, but on Windows the unlock fails
       * if the file was not locked before
       * and this gives an unnecessary error message on stderr.
       */
      DB_unlock();
#endif
      close( handle_ -> pidx);

      /* Free lists */
      DB_FreeElementList (handle_ -> findRequestList);
      DB_FreeElementList (handle_ -> findResponseList);
      DB_FreeUidList (handle_ -> uidList);

      delete handle_;
    }
}

/**********************************
 *      Provides a storage filename
 */

OFCondition DcmQueryRetrieveIndexDatabaseHandle::makeNewStoreFileName(
                const char      *SOPClassUID,
                const char      * /* SOPInstanceUID */ ,
                char            *newImageFileName,
                size_t          newImageFileNameLen)
{

    OFString filename;
    char prefix[12];

    const char *m = dcmSOPClassUIDToModality(SOPClassUID);
    if (m==NULL) m = "XX";
    sprintf(prefix, "%s_", m);
    // unsigned int seed = fnamecreator.hashString(SOPInstanceUID);
    unsigned int seed = (unsigned int)time(NULL);
    newImageFileName[0]=0; // return empty string in case of error
    if (! fnamecreator.makeFilename(seed, handle_->storageArea, prefix, ".dcm", filename))
        return QR_EC_IndexDatabaseError;

    OFStandard::strlcpy(newImageFileName, filename.c_str(), newImageFileNameLen);
    return EC_Normal;
}


OFCondition DcmQueryRetrieveIndexDatabaseHandle::instanceReviewed(int idx)
{
    // acquire shared lock and read record at index position
    OFCondition result = DB_lock(OFFalse);
    if (result.bad()) return result;
    IdxRecord record;
    result = DB_IdxRead(idx, &record);
    DB_unlock();

    if (result.good() && (record.hstat != DVIF_objectIsNotNew))
    {
      // acquire exclusive lock and update flag
      result = DB_lock(OFTrue);
      if (result.bad()) return result;

      record.hstat = DVIF_objectIsNotNew;
      DB_lseek(handle_->pidx, OFstatic_cast(long, DBHEADERSIZE + SIZEOF_STUDYDESC + idx * SIZEOF_IDXRECORD), SEEK_SET);
      if (write(handle_->pidx, OFreinterpret_cast(char *, &record), SIZEOF_IDXRECORD) != SIZEOF_IDXRECORD)
          result = QR_EC_IndexDatabaseError;
      DB_lseek(handle_->pidx, OFstatic_cast(long, DBHEADERSIZE), SEEK_SET);
      DB_unlock();
    }

    return result;
}


/***********************
 *    Default constructors for struct IdxRecord and DB_SSmallDcmElmt
 */

IdxRecord::IdxRecord()
: RecordedDate(0.0)
, ImageSize(0)
, hstat(DVIF_objectIsNotNew)
{
}

DB_SmallDcmElmt::DB_SmallDcmElmt()
: PValueField(NULL)
, ValueLength(0)
, XTag()
{
}


DcmQueryRetrieveIndexDatabaseHandleFactory::DcmQueryRetrieveIndexDatabaseHandleFactory(const DcmQueryRetrieveConfig *config)
: DcmQueryRetrieveDatabaseHandleFactory()
, config_(config)
{
}

DcmQueryRetrieveIndexDatabaseHandleFactory::~DcmQueryRetrieveIndexDatabaseHandleFactory()
{
}

DcmQueryRetrieveDatabaseHandle *DcmQueryRetrieveIndexDatabaseHandleFactory::createDBHandle(
    const char * /* callingAETitle */,
    const char *calledAETitle,
    OFCondition& result) const
{
  return new DcmQueryRetrieveIndexDatabaseHandle(
    config_->getStorageArea(calledAETitle),
    config_->getMaxStudies(calledAETitle),
    config_->getMaxBytesPerStudy(calledAETitle), result);
}
