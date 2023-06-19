/*
 *
 *  Copyright (C) 1996-2020, OFFIS e.V.
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
 *  Module:  dcmwlm
 *
 *  Author:  Thomas Wilkens, Jan Schlamelcher
 *
 *  Purpose: Class for managing file system interaction.
 *
 */

 // ----------------------------------------------------------------------------

#include "dcmtk/config/osconfig.h"

#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/ofstd/ofconsol.h"
#include "dcmtk/ofstd/ofstd.h"
#include "dcmtk/ofstd/ofdate.h"
#include "dcmtk/ofstd/oftime.h"
#include "dcmtk/ofstd/oftypes.h"
#include "dcmtk/ofstd/offilsys.h"
#include "dcmtk/dcmnet/dicom.h"
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcitem.h"
#include "dcmtk/dcmdata/dcvrda.h"
#include "dcmtk/dcmdata/dcvrtm.h"
#include "dcmtk/dcmdata/dcmatch.h"
#include "dcmtk/dcmwlm/wltypdef.h"
#include "dcmtk/dcmwlm/wlds.h"
#include "dcmtk/dcmdata/dctk.h"
#include <stdio.h>
#include <stdlib.h>

#include "dcmtk/dcmwlm/wlfsim.h"

// ----------------------------------------------------------------------------

class WlmFileSystemInteractionManager::MatchingKeys
{
    MatchingKeys(int)
    {
        keys.push_back(OFMake_pair(DCM_PatientName, OFTrue));
        keys.push_back(OFMake_pair(DCM_ResponsiblePerson, OFTrue));
        keys.push_back(OFMake_pair(DCM_ResponsiblePersonRole, OFTrue));
        keys.push_back(OFMake_pair(DCM_PatientID, OFFalse));
        keys.push_back(OFMake_pair(DCM_AccessionNumber, OFTrue));
        keys.push_back(OFMake_pair(DCM_RequestedProcedureID, OFTrue));
        keys.push_back(OFMake_pair(DCM_ReferringPhysicianName, OFTrue));
        keys.push_back(OFMake_pair(DCM_PatientSex, OFTrue));
        keys.push_back(OFMake_pair(DCM_RequestingPhysician, OFTrue));
        keys.push_back(OFMake_pair(DCM_AdmissionID, OFTrue));
        keys.push_back(OFMake_pair(DCM_RequestedProcedurePriority, OFTrue));
        keys.push_back(OFMake_pair(DCM_PatientBirthDate, OFTrue));
        keys.push_back(OFMake_pair(DCM_IssuerOfPatientID, OFTrue));
        combinedKeys.push_back(OFMake_pair(DCM_StudyDate, DCM_StudyTime));
        sequenceKeys.push_back(OFMake_pair(DCM_ScheduledProcedureStepSequence, MatchingKeys()));
        sequenceKeys.back().second.keys.push_back(OFMake_pair(DCM_ScheduledStationAETitle, OFFalse));
        sequenceKeys.back().second.keys.push_back(OFMake_pair(DCM_Modality, OFFalse));
        sequenceKeys.back().second.keys.push_back(OFMake_pair(DCM_ScheduledPerformingPhysicianName, OFTrue));
        sequenceKeys.back().second.combinedKeys.push_back(OFMake_pair(DCM_ScheduledProcedureStepStartDate, DCM_ScheduledProcedureStepStartTime));
    }

public:
    static const MatchingKeys root;
    MatchingKeys() {}
    OFVector<OFPair<DcmTagKey, OFBool> >       keys;
    OFVector<OFPair<DcmTagKey, DcmTagKey> >    combinedKeys;
    OFVector<OFPair<DcmTagKey, MatchingKeys> > sequenceKeys;
};

const WlmFileSystemInteractionManager::MatchingKeys WlmFileSystemInteractionManager::MatchingKeys::root(0);

// ----------------------------------------------------------------------------

WlmFileSystemInteractionManager::WlmFileSystemInteractionManager()
    : dfPath()
    , enableRejectionOfIncompleteWlFiles(OFTrue)
    , calledApplicationEntityTitle()
    , matchingRecords()
{

}

// ----------------------------------------------------------------------------

WlmFileSystemInteractionManager::~WlmFileSystemInteractionManager()
// Date         : July 11, 2002
// Author       : Thomas Wilkens
// Task         : Destructor.
// Parameters   : none.
// Return Value : none.
{
}

// ----------------------------------------------------------------------------

void WlmFileSystemInteractionManager::SetEnableRejectionOfIncompleteWlFiles(OFBool value)
// Date         : May 3, 2005
// Author       : Thomas Wilkens
// Task         : Set value in member variable.
// Parameters   : value - [in] The value to set.
// Return Value : none.
{
    enableRejectionOfIncompleteWlFiles = value;
}

// ----------------------------------------------------------------------------

OFCondition WlmFileSystemInteractionManager::ConnectToFileSystem(const OFString& dfPathv)
// Date         : July 11, 2002
// Author       : Thomas Wilkens
// Task         : Connects to the worklist file system database.
// Parameters   : dfPathv - [in] Path to worklist file system database.
// Return Value : Indicates if the connection could be established or not.
{
    // check parameter
    if (dfPathv.empty())
    {
        DCMWLM_ERROR("Invalid parameters, cannot connect to worklist file system database");
        return(WLM_EC_CannotConnectToDataSource);
    }

    // copy value
    dfPath = dfPathv;

    // check if the specified path is existent and accessible for reading
    if (!OFStandard::dirExists(dfPath) || !OFStandard::isReadable(dfPath))
        return(WLM_EC_CannotConnectToDataSource);
    else
        return(EC_Normal);
}

// ----------------------------------------------------------------------------

OFCondition WlmFileSystemInteractionManager::DisconnectFromFileSystem()
// Date         : July 11, 2002
// Author       : Thomas Wilkens
// Task         : Disconnects from the worklist file system database.
// Parameters   : none.
// Return Value : Indicates if the connection was disconnected successfully.
{
    return(EC_Normal);
}

#pragma region ConsoleLog

std::string getCurrentTimeAsStringFSIM()
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
static void ConsoleLogFSIM(std::string msg, std::string msgType)
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

    std::string currentTime = getCurrentTimeAsStringFSIM();
    std::cout << currentTime;
    std::cout << " ";
    std::cout << msg << std::endl;
}

#pragma endregion
#pragma region MongoDB
/**
* MongoDB設定
*/

// Mongo 連線網址
std::string conn_stringFSIM = "mongodb://127.0.0.1/?appname=dcmqrscp4raccoon";
std::string mongoDB_nameFSIM = "raccoon_polka";
std::string collection_nameFSIM = "dicom";

#ifdef _WIN32 // Windows

#include <windows.h>

std::string getExecutablePathFSIM() {
    char exePath[MAX_PATH];
    DWORD pathLength = GetModuleFileName(NULL, exePath, MAX_PATH);

    if (pathLength == 0) {
        ConsoleLogFSIM("Failed to retrieve executable path.", "E");
        exit(1);
    }

    std::string exeDirectory = std::string(exePath);
    std::size_t lastSlashPos = exeDirectory.find_last_of("\\/");

    if (lastSlashPos == std::string::npos) {
        ConsoleLogFSIM("Invalid executable path.", "E");
        exit(1);
    }

    return exeDirectory.substr(0, lastSlashPos + 1);
}

#elif defined(__linux__) // Linux

#include <unistd.h>
#include <limits.h>

std::string getExecutablePathFSIM() {
    char exePath[PATH_MAX];
    ssize_t pathLength = readlink("/proc/self/exe", exePath, sizeof(exePath));

    if (pathLength == -1) {
        ConsoleLogFSIM("Failed to retrieve executable path.", "E");
        exit(1);
    }

    std::string exeDirectory = std::string(exePath, pathLength);
    std::size_t lastSlashPos = exeDirectory.find_last_of("\\/");

    if (lastSlashPos == std::string::npos) {
        ConsoleLogFSIM("Invalid executable path.", "E");
        exit(1);
    }

    return exeDirectory.substr(0, lastSlashPos + 1);
}

#else
#error Unsupported platform
#endif

void ReadMongoConfigFSIM()
{
    std::string exeDirectory = getExecutablePathFSIM();
    std::string configFilePath = exeDirectory + "wlmscpfsMongoConfig.cfg";

    std::ifstream config_file(configFilePath);

    if (!config_file.is_open()) {
        ConsoleLogFSIM("MongoDB Config File Path:" + configFilePath, "");
        ConsoleLogFSIM("Failed to open mongo config file.", "E");
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
                conn_stringFSIM = value;
            }
            else if (key == "mongoDB_name")
            {
                mongoDB_nameFSIM = value;
            }
            else if (key == "collection_name")
            {
                collection_nameFSIM = value;
            }
        }
    }
    ConsoleLogFSIM("MongoDB conn_string:" + conn_stringFSIM, "");
    ConsoleLogFSIM("MongoDB mongoDB_name:" + mongoDB_nameFSIM, "");
    ConsoleLogFSIM("MongoDB collection_name:" + collection_nameFSIM, "");

    config_file.close();
}
#pragma endregion

// ----------------------------------------------------------------------------
/*
* 檢查資料庫是否存在該aetitle的任一筆資料。
*/
OFBool WlmFileSystemInteractionManager::IsCalledApplicationEntityTitleSupported(const OFString& calledApplicationEntityTitlev)
// Date         : July 11, 2002
// Author       : Thomas Wilkens
// Task         : Checks if the given called application entity title is supported. If this is the case,
//                OFTrue will be returned, else OFFalse.
// Parameters   : calledApplicationEntityTitlev - [in] The application entity title which shall be checked
//                                                for support. Valid pointer expected.
// Return Value : OFTrue  - The called application entity title is supported.
//                OFFalse - The called application entity title is not supported or it is not given.
{
    // copy value
    calledApplicationEntityTitle = calledApplicationEntityTitlev;
    ConsoleLogFSIM("Checking any file with AETtitle " + std::string(calledApplicationEntityTitlev.c_str()), "");
    ReadMongoConfigFSIM();
    // MongoDB連接
    mongoc_client_t* mongoClient;
    mongoc_database_t* db;
    mongoc_uri_t* uri;
    bson_error_t mongoError;

    mongoc_init();
    uri = mongoc_uri_new_with_error(conn_stringFSIM.c_str(), &mongoError);
    if (!uri) {
        ConsoleLogFSIM("failed to parse URI:" + conn_stringFSIM, "E");
        ConsoleLogFSIM("error message:" + std::string(mongoError.message), "E");
    }

    mongoClient = mongoc_client_new_from_uri(uri);
    if (mongoClient)
    {
        bson_t* query;
        query = bson_new();
        std::string queryKey = "AETitle";
        std::string queryValue = std::string(calledApplicationEntityTitle.c_str());
        ConsoleLogFSIM("QueryKey[" + queryKey + "]  ,  " + "QueryValue[" + queryValue + "]", "");
        bson_append_utf8(query, queryKey.c_str(), -1, queryValue.c_str(), -1);

        mongoc_collection_t* collection;
        collection = mongoc_client_get_collection(mongoClient, mongoDB_nameFSIM.c_str(), collection_nameFSIM.c_str());
        mongoc_cursor_t* cursor;
        const bson_t* resultBson;
        resultBson = bson_new();

        cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);
        //mongoc_cursor_set_limit(cursor, 2);
        if (mongoc_cursor_next(cursor, &resultBson)) {
            ConsoleLogFSIM("Called AETitle " + std::string(calledApplicationEntityTitle.c_str()) + " Exists", "");
            return (OFTrue);
        }
    }
    ConsoleLogFSIM("Called AETitle " + std::string(calledApplicationEntityTitle.c_str()) + " Did Not Exist", "");
    return (OFFalse);
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
std::string queryValueAppendRegex(std::string q)
{
    std::string queryValue = q;
    queryValue = "^" + queryValue;
    queryValue = ReplaceString(queryValue, "*", ".*");
    queryValue = ReplaceString(queryValue, "?", ".");
    return queryValue;
}
std::string DcmElementToString(DcmElement* query)
{
    char* queryValue;
    if (query->isaString())
    {
        OFString _s;
        query->getOFString(_s, 0);
        queryValue = strdup(_s.c_str());
    }
    return std::string(queryValue);
}
bson_t* WlmFileSystemInteractionManager::GetFindQuery(DcmDataset searchMask)
{
    bson_t* query;
    query = bson_new();

    // 取得MongoDB查詢

    // 先限制AETitle
    bson_append_utf8(query, "AETitle", -1, calledApplicationEntityTitle.c_str(), -1);

    MatchingKeys matchingKeys = MatchingKeys::root;
    for (OFVector<OFPair<DcmTagKey, OFBool> >::const_iterator it = matchingKeys.keys.begin(); it != matchingKeys.keys.end(); ++it)
    {
        const OFPair<DcmTagKey, OFBool>& key = *it;
        DcmElement* queryDCM = OFnullptr;
        if (searchMask.findAndGetElement(key.first, queryDCM, OFFalse).good() && queryDCM)
        {
            std::string queryKey = "" + std::string(key.first.toString().c_str()) + "";
            queryKey += ".value";
            std::string queryValue = queryValueAppendRegex(DcmElementToString(queryDCM));
            //ConsoleLogFSIM("QueryKey[" + queryKey + "]  ,  " + "QueryValue[" + queryValue + "]", "");
            bson_append_regex(query, queryKey.c_str(), -1, queryValue.c_str(), "");
        }
    }

    // remove this bloated version of the code if C++11 ever becomes a requirement of DCMTK
    for (OFVector<OFPair<DcmTagKey, DcmTagKey> >::const_iterator it = matchingKeys.combinedKeys.begin(); it != matchingKeys.combinedKeys.end(); ++it)
    {
        const OFPair<DcmTagKey, DcmTagKey>& combinedKey = *it;
        DcmElement* queryDCM = OFnullptr;
        if (searchMask.findAndGetElement(combinedKey.first, queryDCM, OFFalse).good() && queryDCM && !queryDCM->isUniversalMatch())
        {
            std::string queryKey = "" + std::string(combinedKey.first.toString().c_str()) + "";
            queryKey += ".value";
            std::string queryValue = queryValueAppendRegex(DcmElementToString(queryDCM));
            //ConsoleLogFSIM("QueryKey[" + queryKey + "]  ,  " + "QueryValue[" + queryValue + "]", "");
            bson_append_regex(query, queryKey.c_str(), -1, queryValue.c_str(), "");

            DcmElement* secondQueryDCM = OFnullptr;
            if (searchMask.findAndGetElement(combinedKey.second, secondQueryDCM, OFFalse).good() && secondQueryDCM && !secondQueryDCM->isUniversalMatch())
            {
                std::string queryKey = "" + std::string(combinedKey.second.toString().c_str()) + "";
                queryKey += ".value";
                std::string queryValue = queryValueAppendRegex(DcmElementToString(secondQueryDCM));
                //ConsoleLogFSIM("QueryKey[" + queryKey + "]  ,  " + "QueryValue[" + queryValue + "]", "");
                bson_append_regex(query, queryKey.c_str(), -1, queryValue.c_str(), "");
            }
        }
        else if (searchMask.findAndGetElement(combinedKey.second, queryDCM, OFFalse).good() && queryDCM && !queryDCM->isUniversalMatch())
        {
            std::string queryKey = "" + std::string(combinedKey.second.toString().c_str()) + "";
            queryKey += ".value";
            std::string queryValue = queryValueAppendRegex(DcmElementToString(queryDCM));
            //ConsoleLogFSIM("QueryKey[" + queryKey + "]  ,  " + "QueryValue[" + queryValue + "]", "");
            bson_append_regex(query, queryKey.c_str(), -1, queryValue.c_str(), "");
        }
    }

    // seq
    for (OFVector<OFPair<DcmTagKey, MatchingKeys> >::const_iterator it = matchingKeys.sequenceKeys.begin(); it != matchingKeys.sequenceKeys.end(); ++it)
    {
        const OFPair<DcmTagKey, MatchingKeys>& sequenceKey = *it;
        DcmElement* queryDCM = OFnullptr;
        if (searchMask.findAndGetElement(sequenceKey.first, queryDCM, OFFalse).good() && queryDCM && queryDCM->ident() == EVR_SQ && !IsUniversalMatch(OFstatic_cast(DcmSequenceOfItems&, *queryDCM), sequenceKey.second))
        {
            std::string queryKey = "" + std::string(sequenceKey.first.toString().c_str()) + "";
            queryKey += ".value";
            std::string queryValue = queryValueAppendRegex(DcmElementToString(queryDCM));
            //ConsoleLogFSIM("QueryKey[" + queryKey + "]  ,  " + "QueryValue[" + queryValue + "]", "");
            bson_append_regex(query, queryKey.c_str(), -1, queryValue.c_str(), "");
        }
    }
    return query;
}

// ----------------------------------------------------------------------------

/*
* dcmtk原先 worklist 搜尋所有檔案並檢查他們是否符合的function
*/
size_t WlmFileSystemInteractionManager::DetermineMatchingRecords(DcmDataset* searchMask)
{
    assert(searchMask);
    matchingRecords.clear();
    OFdirectory_iterator it(dfPath / calledApplicationEntityTitle);

    // 1.   使用FindNextWorklistFile依序查找檔案
    if (FindNextWorklistFile(it) != OFdirectory_iterator())
    {
        // 2.   MatchWorklistFile檢查檔案是否符合，會先將檔案使用OFshared_ptr<DcmDataset>( file.getAndRemoveDataset() )轉換成DcmDataset之後檢查，
        //      如果符合，會將該檔案push_back到matchingRecords。
        MatchWorklistFile(*searchMask, *it);

        // 3.   重複 1以及2 的步驟
        while (FindNextWorklistFile(++it) != OFdirectory_iterator())
            MatchWorklistFile(*searchMask, *it);
    }
    else DCMWLM_INFO("<no files found>");
    return matchingRecords.size();
}

// ----------------------------------------------------------------------------

void WlmFileSystemInteractionManager::MatchWorklistFile(DcmDataset& searchMask,
    const OFpath& worklistFile)
{
    // read information from worklist file
    DcmFileFormat file;
    OFCondition status = file.loadFile(worklistFile);
    if (status.bad())
    {
        DCMWLM_WARN("Could not read worklist file " << worklistFile << ", file will be ignored: " << status.text());
        return;
    }
    // extract the data set from worklist file, if any
    // storing it into an OFshared_ptr ensures it will be freed in the end not matter what
    if (OFshared_ptr<DcmDataset> pDataset = OFshared_ptr<DcmDataset>(file.getAndRemoveDataset()))
    {
        if (enableRejectionOfIncompleteWlFiles)
        {
            DCMWLM_INFO("Checking whether worklist file " << worklistFile << " is complete");
            // in case option --enable-file-reject is set, we have to check if the current
            // .wl-file meets certain conditions; in detail, the file's dataset has to be
            // checked whether it contains all necessary return type 1 attributes and contains
            // information in all these attributes; if this is condition is not met, the
            // .wl-file shall be rejected
            if (!DatasetIsComplete(pDataset.get()))
            {
                DCMWLM_WARN("Worklist file " << worklistFile << " is incomplete, file will be ignored");
                return;
            }
        }
        // check if the current dataset matches the matching key attribute values
        if (DatasetMatchesSearchMask(*pDataset, searchMask, MatchingKeys::root))
        {
            DCMWLM_INFO("Information from worklist file " << worklistFile << " matches query");
            // insert the matching dataset into matchingRecords
            matchingRecords.push_back(pDataset);
        }
        else DCMWLM_INFO("Information from worklist file " << worklistFile << " does not match query");
    }
    else DCMWLM_WARN("Worklist file " << worklistFile << " is empty, file will be ignored");
}

// ----------------------------------------------------------------------------

unsigned long WlmFileSystemInteractionManager::GetNumberOfSequenceItemsForMatchingRecord(DcmTagKey sequenceTag, WlmSuperiorSequenceInfoType* superiorSequenceArray, unsigned long numOfSuperiorSequences, unsigned long idx)
// Date         : January 6, 2004
// Author       : Thomas Wilkens
// Task         : For the matching record that is identified through idx, this function returns the number
//                of items that are contained in the sequence element that is referred to by sequenceTag.
//                In case this sequence element is itself contained in a certain item of another superior
//                sequence, superiorSequenceArray contains information about where to find the correct
//                sequence element.
// Parameters   : sequenceTag            - [in] The tag of the sequence element for which the number of items
//                                              shall be determined.
//                superiorSequenceArray  - [in] Array which contains information about superior sequence elements
//                                              the given sequence element is contained in.
//                numOfSuperiorSequences - [in] The number of elements in the above array.
//                idx                    - [in] Identifies the record from which the number of sequence items
//                                              shall be determined.
// Return Value : The number of items that are contained in the sequence element that is referred to by
//                sequenceTag and that can be found in sequence items which are specified in superiorSequenceArray.
{
    OFCondition cond;
    DcmSequenceOfItems* sequenceElement = NULL, * tmp = NULL;
    unsigned long i;

    // initialize result variable
    unsigned long numOfItems = 0;

    // if the sequence in question is not contained in another sequence
    if (numOfSuperiorSequences == 0)
    {
        // simply find and get this sequence in the matching dataset (on the main level)
        cond = matchingRecords[idx]->findAndGetSequence(sequenceTag, sequenceElement, OFFalse);
    }
    else
    {
        // if it is contained in (an)other sequence(s), find it
        cond = matchingRecords[idx]->findAndGetSequence(superiorSequenceArray[0].sequenceTag, sequenceElement, OFFalse);
        for (i = 1; i < numOfSuperiorSequences && cond.good(); i++)
        {
            cond = sequenceElement->getItem(superiorSequenceArray[i - 1].currentItem)->findAndGetSequence(superiorSequenceArray[i].sequenceTag, tmp, OFFalse);
            if (cond.good())
                sequenceElement = tmp;
        }

        if (cond.good())
        {
            cond = sequenceElement->getItem(superiorSequenceArray[numOfSuperiorSequences - 1].currentItem)->findAndGetSequence(sequenceTag, tmp, OFFalse);
            if (cond.good())
                sequenceElement = tmp;
        }
    }

    // determine number of items for the sequence in question
    if (cond.good())
        numOfItems = sequenceElement->card();

    // return result
    return(numOfItems);
}

// ----------------------------------------------------------------------------

void WlmFileSystemInteractionManager::GetAttributeValueForMatchingRecord(DcmTagKey tag, WlmSuperiorSequenceInfoType* superiorSequenceArray, unsigned long numOfSuperiorSequences, unsigned long idx, char*& value)
// Date         : July 11, 2002
// Author       : Thomas Wilkens
// Task         : This function determines an attribute value of a matching record
//                and returns this value in a newly created string to the caller.
// Parameters   : tag                    - [in] Attribute tag. Specifies which attribute's value shall be returned.
//                superiorSequenceArray  - [in] Array which contains information about superior sequence elements
//                                              the given element is contained in.
//                numOfSuperiorSequences - [in] The number of elements in the above array.
//                idx                    - [in] Identifies the record from which the attribute value shall be retrieved.
//                value                  - [out] Pointer to a newly created string that contains the requested value.
//                                               If value was not found an emtpy string will be returned.
// Return Value : none.
{
    OFCondition cond;
    DcmSequenceOfItems* sequenceElement = NULL, * tmp = NULL;
    unsigned long i;
    const char* val = "";
    Uint16 v;
    size_t len = 0;

    // if the element in question is not contained in another sequence
    if (numOfSuperiorSequences == 0)
    {
        // simply find and get this element in the matching dataset (on the main level);
        // here, we have to distinguish two cases: attribute PregnancyStatus has to be re-
        // trieved as a Uint16 value (also note for this case, that this attribute can only
        // occur on the main level, it will never be contained in a sequence), all other
        // attributes have to be retrieved as strings
        if (tag == DCM_PregnancyStatus)
        {
            cond = matchingRecords[idx]->findAndGetUint16(tag, v, 0, OFFalse);
            if (cond.good())
            {
                len = 20;
                value = new char[len];
                sprintf(value, "%u", v);
            }
            else
            {
                len = 2;
                value = new char[len];
                OFStandard::strlcpy(value, "4", len);           // a value of "4" in attribute PregnancyStatus means "unknown" in DICOM
            }
        }
        else
        {
            std::cout << "idx=" << idx << std::endl;
            cond = matchingRecords[idx]->findAndGetString(tag, val, OFFalse);
            std::cout << tag.getGroup() << "," << tag.getElement() << std::endl;
            if (cond.good() && val != NULL)
            {
                len = strlen(val) + 1;
                value = new char[len];
                OFStandard::strlcpy(value, val, len);
            }
            else
            {
                len = 1;
                value = new char[len];
                OFStandard::strlcpy(value, "", len);
            }
        }
    }
    else
    {
        // if it is contained in (an)other sequence(s), go through all corresponding sequence items
        cond = matchingRecords[idx]->findAndGetSequence(superiorSequenceArray[0].sequenceTag, sequenceElement, OFFalse);
        for (i = 1; i < numOfSuperiorSequences && cond.good(); i++)
        {
            cond = sequenceElement->getItem(superiorSequenceArray[i - 1].currentItem)->findAndGetSequence(superiorSequenceArray[i].sequenceTag, tmp, OFFalse);
            if (cond.good())
                sequenceElement = tmp;
        }

        // now sequenceElement points to the sequence element in which the attribute
        // in question can be found; retrieve a value for this attribute (note that
        // all attributes in sequences can actually be retrieved as strings)
        if (cond.good())
        {
            cond = sequenceElement->getItem(superiorSequenceArray[numOfSuperiorSequences - 1].currentItem)->findAndGetString(tag, val, OFFalse);
            if (cond.good() && val != NULL)
            {
                len = strlen(val) + 1;
                value = new char[len];
                OFStandard::strlcpy(value, val, len);
            }
            else
            {
                len = 1;
                value = new char[len];
                OFStandard::strlcpy(value, "", len);
            }
        }
        else
        {
            len = 1;
            value = new char[len];
            OFStandard::strlcpy(value, "", len);
        }
    }
}

// ----------------------------------------------------------------------------

void WlmFileSystemInteractionManager::ClearMatchingRecords()
{
    matchingRecords.clear();
}

// ----------------------------------------------------------------------------

OFdirectory_iterator& WlmFileSystemInteractionManager::FindNextWorklistFile(OFdirectory_iterator& it)
{
    for (; it != OFdirectory_iterator() && ".wl" != it->path().extension(); ++it);
    return it;
}

// ----------------------------------------------------------------------------

OFBool WlmFileSystemInteractionManager::DatasetIsComplete(DcmDataset* dataset)
// Date         : May 2, 2005
// Author       : Thomas Wilkens
// Task         : This function checks if the given dataset (which represents the information from a
//                worklist file) contains all necessary return type 1 information. According to the
//                DICOM standard part 4 annex K, the following attributes are type 1 attributes in
//                C-Find RSP messages:
//                      Attribute                             Tag      Return Key Type
//                  ScheduledProcedureStepSequence        (0040,0100)        1
//                   > ScheduledStationAETitle            (0040,0001)        1
//                   > ScheduledProcedureStepStartDate    (0040,0002)        1
//                   > ScheduledProcedureStepStartTime    (0040,0003)        1
//                   > Modality                           (0008,0060)        1
//                   > ScheduledProcedureStepDescription  (0040,0007)        1C (The ScheduledProcedureStepDescription (0040,0007) or the ScheduledProtocolCodeSequence (0040,0008) or both shall be supported by the SCP; we actually support both, so we have to check if at least one of the two attributes contains valid information.)
//                   > ScheduledProtocolCodeSequence      (0040,0008)        1C (see abobve)
//                   > > CodeValue                        (0008,0100)        1
//                   > > CodingSchemeDesignator           (0008,0102)        1
//                   > ScheduledProcedureStepID           (0040,0009)        1
//                  RequestedProcedureID                  (0040,1001)        1
//                  RequestedProcedureDescription         (0032,1060)        1C (The RequestedProcedureDescription (0032,1060) or the RequestedProcedureCodeSequence (0032,1064) or both shall be supported by the SCP; we actually support both, so we have to check if at least one of the two attributes contains valid information.)
//                  RequestedProcedureCodeSequence        (0032,1064)        1C (see abobve)
//                   > > CodeValue                        (0008,0100)        1
//                   > > CodingSchemeDesignator           (0008,0102)        1
//                  StudyInstanceUID                      (0020,000D)        1
//                  ReferencedStudySequence               (0008,1110)        2
//                   > ReferencedSOPClassUID              (0008,1150)        1C (Required if a sequence item is present)
//                   > ReferencedSOPInstanceUID           (0008,1155)        1C (Required if a sequence item is present)
//                  ReferencedPatientSequence             (0008,1120)        2
//                   > ReferencedSOPClassUID              (0008,1150)        1C (Required if a sequence item is present)
//                   > ReferencedSOPInstanceUID           (0008,1155)        1C (Required if a sequence item is present)
//                  PatientName                           (0010,0010)        1
//                  PatientID                             (0010,0020)        1
// Parameters   : dataset - [in] The dataset of the worklist file which is currently examined.
// Return Value : OFTrue in case the given dataset contains all necessary return type 1 information,
//                OFFalse otherwise.
{
    DcmElement* scheduledProcedureStepSequence = NULL;

    // intialize returnValue
    OFBool complete = OFTrue;

    DCMWLM_DEBUG("Checking whether dataset is complete ...");

    // the dataset is considered to be incomplete...
    // ...if the ScheduledProcedureStepSequence is missing or
    // ...if the ScheduledProcedureStepSequence does not have exactly one item
    if (dataset->findAndGetElement(DCM_ScheduledProcedureStepSequence, scheduledProcedureStepSequence).bad() || ((DcmSequenceOfItems*)scheduledProcedureStepSequence)->card() != 1)
    {
        DCMWLM_DEBUG("- ScheduledProcedureStepSequence " << DCM_ScheduledProcedureStepSequence << " is missing or does not have exactly one item");
        complete = OFFalse;
    }
    else
    {
        // so the ScheduledProcedureStepSequence is existent and has exactly one item;
        // get this one and only item from the ScheduledProcedureStepSequence
        DcmItem* scheduledProcedureStepSequenceItem = ((DcmSequenceOfItems*)scheduledProcedureStepSequence)->getItem(0);

        // the dataset is considered to be incomplete...
        // ...if ScheduledStationAETitle is missing or empty in the ScheduledProcedureStepSequence, or
        // ...if ScheduledProcedureStepStartDate is missing or empty in the ScheduledProcedureStepSequence, or
        // ...if ScheduledProcedureStepStartTime is missing or empty in the ScheduledProcedureStepSequence, or
        // ...if Modality is missing or empty in the ScheduledProcedureStepSequence, or
        // ...if ScheduledProcedureStepID is missing or empty in the ScheduledProcedureStepSequence, or
        // ...if RequestedProcedureID is missing or empty in the dataset, or
        // ...if StudyInstanceUID is missing or empty in the dataset, or
        // ...if PatientName is missing or empty in the dataset, or
        // ...if PatientID is missing or empty in the dataset, or
        // ...if inside the ScheduledProcedureStepSequence, no information can be retrieved from
        //    the ScheduledProcedureStepDescription attribute and (at the same time) also no
        //    complete information can be retrieved from the ScheduledProtocolCodeSequence attribute, or
        // ...if inside the dataset, no information can be retrieved from the RequestedProcedureDescription
        //    attribute and (at the same time) also no complete information can be retrieved from the
        //    RequestedProcedureCodeSequence attribute, or
        // ...if inside the dataset, the ReferencedStudySequence is either absent or it is existent and non-empty but incomplete, or
        // ...if inside the dataset, the ReferencedPatientSequence is either absent or it is existent and non-empty but incomplete
        if (AttributeIsAbsentOrEmpty(DCM_ScheduledStationAETitle, scheduledProcedureStepSequenceItem) ||
            AttributeIsAbsentOrEmpty(DCM_ScheduledProcedureStepStartDate, scheduledProcedureStepSequenceItem) ||
            AttributeIsAbsentOrEmpty(DCM_ScheduledProcedureStepStartTime, scheduledProcedureStepSequenceItem) ||
            AttributeIsAbsentOrEmpty(DCM_Modality, scheduledProcedureStepSequenceItem) ||
            AttributeIsAbsentOrEmpty(DCM_ScheduledProcedureStepID, scheduledProcedureStepSequenceItem) ||
            AttributeIsAbsentOrEmpty(DCM_RequestedProcedureID, dataset) ||
            AttributeIsAbsentOrEmpty(DCM_StudyInstanceUID, dataset) ||
            AttributeIsAbsentOrEmpty(DCM_PatientName, dataset) ||
            AttributeIsAbsentOrEmpty(DCM_PatientID, dataset) ||
            DescriptionAndCodeSequenceAttributesAreIncomplete(DCM_ScheduledProcedureStepDescription, DCM_ScheduledProtocolCodeSequence, scheduledProcedureStepSequenceItem) ||
            DescriptionAndCodeSequenceAttributesAreIncomplete(DCM_RequestedProcedureDescription, DCM_RequestedProcedureCodeSequence, dataset) ||
            ReferencedStudyOrPatientSequenceIsAbsentOrExistentButNonEmptyAndIncomplete(DCM_ReferencedStudySequence, dataset) ||
            ReferencedStudyOrPatientSequenceIsAbsentOrExistentButNonEmptyAndIncomplete(DCM_ReferencedPatientSequence, dataset))
            complete = OFFalse;
    }
    // return result
    return(complete);
}

// ----------------------------------------------------------------------------

OFBool WlmFileSystemInteractionManager::ReferencedStudyOrPatientSequenceIsAbsentOrExistentButNonEmptyAndIncomplete(DcmTagKey sequenceTagKey, DcmItem* dset)
// Date         : May 4, 2005
// Author       : Thomas Wilkens
// Task         : This function checks if the specified sequence attribute is absent or existent but non-empty
//                and incomplete in the given dataset.
// Parameters   : sequenceTagKey - [in] The sequence attribute which shall be checked.
//                dset           - [in] The dataset in which the attribute is contained.
// Return Value : OFTrue in case the sequence attribute is absent (and cannot be added to the dataset)
//                or existent but non-empty and incomplete, OFFalse otherwise.
{
    DcmElement* sequence = NULL;
    OFBool result;

    // check whether the type 2 sequence attribute is absent
    if (dset->findAndGetElement(sequenceTagKey, sequence).bad())
    {
        DCMWLM_DEBUG("- " << DcmTag(sequenceTagKey).getTagName() << " " << sequenceTagKey << " is missing");
        // try to add it to the dataset and return OFFalse if successful
        if (dset->insertEmptyElement(sequenceTagKey).good())
        {
            DCMWLM_WARN("Added missing type 2 sequence attribute " << sequenceTagKey << " to the current record");
            result = OFFalse;
        }
        else
            result = OFTrue;
    }
    else
    {
        // if the sequence attribute is existent but empty, we want to return OFFalse
        // (note that the sequence is actually type 2, so being empty is ok)
        if (((DcmSequenceOfItems*)sequence)->card() == 0)
            result = OFFalse;
        else
        {
            // if the sequence attribute is existent and non-empty, we need
            // to check every item in the sequence for completeness
            result = OFFalse;
            for (unsigned long i = 0; i < ((DcmSequenceOfItems*)sequence)->card() && !result; i++)
            {
                if (AttributeIsAbsentOrEmpty(DCM_ReferencedSOPClassUID, ((DcmSequenceOfItems*)sequence)->getItem(i)) ||
                    AttributeIsAbsentOrEmpty(DCM_ReferencedSOPInstanceUID, ((DcmSequenceOfItems*)sequence)->getItem(i)))
                    result = OFTrue;
            }
            if (result)
                DCMWLM_DEBUG("- " << DcmTag(sequenceTagKey).getTagName() << " " << sequenceTagKey << " is incomplete");
        }
    }

    // return result
    return(result);
}

// ----------------------------------------------------------------------------

OFBool WlmFileSystemInteractionManager::DescriptionAndCodeSequenceAttributesAreIncomplete(DcmTagKey descriptionTagKey, DcmTagKey codeSequenceTagKey, DcmItem* dset)
// Date         : April 18, 2017
// Author       : Michael Onken
// Task         : This method ensures that either code or description is set to a non-empty value,
//                and at the same time none of the attributes is present with a zero-length value.
//                If one of these requirements are not met, then OFTrue is returned, otherwise OFFalse.
// Parameters   : descriptionTagKey  - [in] The description attribute which shall be checked.
//                codeSequenceTagKey - [in] The codeSequence attribute which shall be checked.
//                dset               - [in] The dataset in which the attributes are contained.
// Return Value : OFFalse (i.e. no error regarding the standard) in case at least
//                one of both attributes has a non-empty, valid value, and none
//                is set to an empty value. OFTrue otherwise.
{
    // check if the code sequence attribute is complete,
    // i.e. if complete information can be retrieved from this attribute

    // if the attribute is not existent or has no items, we consider it incomplete
    DcmSequenceOfItems* seq = NULL;
    if (dset->findAndGetSequence(codeSequenceTagKey, seq).good())
    {
        if (seq->card() == 0)
        {
            DCMWLM_DEBUG("- " << DcmTag(codeSequenceTagKey).getTagName() << " " << codeSequenceTagKey << " is empty");
            return OFTrue;
        }
        else if (seq->card() > 0)
        {
            // if it is existent and has items, check every item for completeness
            for (unsigned long i = 0; i < seq->card(); i++)
            {
                if (AttributeIsAbsentOrEmpty(DCM_CodeValue, seq->getItem(i)) ||
                    AttributeIsAbsentOrEmpty(DCM_CodingSchemeDesignator, seq->getItem(i)))
                {
                    DCMWLM_DEBUG("- " << DcmTag(codeSequenceTagKey).getTagName() << " " << codeSequenceTagKey << " has incomplete or empty code entries");
                    return OFTrue;
                }
            }
        }
    }
    // check whether description is valid. If sequence does exists, the attribute
    // must exist with a valid value.
    OFString description;
    if (dset->findAndGetOFStringArray(descriptionTagKey, description).bad())
    {
        if (!seq)
        {
            DCMWLM_DEBUG("- " << DcmTag(codeSequenceTagKey).getTagName() << " " << codeSequenceTagKey
                << " and " << DcmTag(descriptionTagKey) << " " << descriptionTagKey << " are both not set");
            return OFTrue;
        }
    }
    // in any case, description must not exist with empty value in the dataset
    else if (description.empty())
    {
        DCMWLM_DEBUG("- " << DcmTag(descriptionTagKey).getTagName() << " " << descriptionTagKey << " is empty");
        return OFTrue;
    }

    return OFFalse;
}

// ----------------------------------------------------------------------------

OFBool WlmFileSystemInteractionManager::AttributeIsAbsentOrEmpty(DcmTagKey elemTagKey, DcmItem* dset)
// Date         : May 4, 2005
// Author       : Thomas Wilkens
// Task         : This function checks if the specified attribute is absent or contains an empty value in the given dataset.
// Parameters   : elemTagKey - [in] The attribute which shall be checked.
//                dset       - [in] The dataset in which the attribute is contained.
// Return Value : OFTrue in case the attribute is absent or contains an empty value, OFFalse otherwise.
{
    DcmElement* elem = NULL;

    if (dset->findAndGetElement(elemTagKey, elem).bad() || elem->getLength() == 0)
    {
        DCMWLM_DEBUG("- " << DcmTag(elemTagKey).getTagName() << " " << elemTagKey << " is missing or empty");
        return(OFTrue);
    }
    else
        return(OFFalse);
}


OFBool WlmFileSystemInteractionManager::IsUniversalMatch(DcmSequenceOfItems& query,
    const MatchingKeys& matchingKeys,
    const OFBool normalize,
    const OFBool normalizeWildCards)
{
    DcmItem* pQueryItem = OFstatic_cast(DcmItem*, query.nextInContainer(OFnullptr));
    if (pQueryItem) {
#ifdef HAVE_CXX11
        for (auto& key : matchingKeys.keys)
        {
#else
        // remove this bloated version of the code if C++11 ever becomes a requirement of DCMTK
        for (OFVector<OFPair<DcmTagKey, OFBool> >::const_iterator it = matchingKeys.keys.begin(); it != matchingKeys.keys.end(); ++it)
        {
            const OFPair<DcmTagKey, OFBool>& key = *it;
#endif
            DcmElement* query_elem = OFnullptr;
            if (pQueryItem->findAndGetElement(key.first, query_elem, OFFalse).good() && query_elem && !query_elem->isUniversalMatch(normalize, normalizeWildCards && key.second))
                return OFFalse;
        }

#ifdef HAVE_CXX11
        for (auto& combinedKey : matchingKeys.combinedKeys)
        {
#else
        // remove this bloated version of the code if C++11 ever becomes a requirement of DCMTK
        for (OFVector<OFPair<DcmTagKey, DcmTagKey> >::const_iterator it = matchingKeys.combinedKeys.begin(); it != matchingKeys.combinedKeys.end(); ++it)
        {
            const OFPair<DcmTagKey, DcmTagKey>& combinedKey = *it;
#endif
            DcmElement* query_elem = OFnullptr;
            if (pQueryItem->findAndGetElement(combinedKey.first, query_elem, OFFalse).good() && query_elem && !query_elem->isUniversalMatch(normalize, normalizeWildCards))
                return OFFalse;
            else if (pQueryItem->findAndGetElement(combinedKey.second, query_elem, OFFalse).good() && query_elem && !query_elem->isUniversalMatch(normalize, normalizeWildCards))
                return OFFalse;
        }

        // sequence matching
#ifdef HAVE_CXX11
        for (auto& sequenceKey : matchingKeys.sequenceKeys)
        {
#else
      // remove this bloated version of the code if C++11 ever becomes a requirement of DCMTK
        for (OFVector<OFPair<DcmTagKey, MatchingKeys> >::const_iterator it = matchingKeys.sequenceKeys.begin(); it != matchingKeys.sequenceKeys.end(); ++it)
        {
            const OFPair<DcmTagKey, MatchingKeys>& sequenceKey = *it;
#endif
            DcmElement* query_elem = OFnullptr;
            if (pQueryItem->findAndGetElement(sequenceKey.first, query_elem, OFFalse).good() && query_elem && query_elem->ident() == EVR_SQ && !IsUniversalMatch(OFstatic_cast(DcmSequenceOfItems&, *query_elem), sequenceKey.second, normalize, normalizeWildCards))
                return OFFalse;
        }
        }
    return OFTrue;
        }

OFBool WlmFileSystemInteractionManager::MatchSequences(DcmSequenceOfItems & candidate,
    DcmSequenceOfItems & query,
    const MatchingKeys & matchingKeys)
{
    for (DcmObject* pQueryItem = query.nextInContainer(OFnullptr); pQueryItem; pQueryItem = query.nextInContainer(pQueryItem))
        for (DcmObject* pCandidateItem = candidate.nextInContainer(OFnullptr); pCandidateItem; pCandidateItem = candidate.nextInContainer(pCandidateItem))
            if (DatasetMatchesSearchMask(*OFstatic_cast(DcmItem*, pCandidateItem), *OFstatic_cast(DcmItem*, pQueryItem), matchingKeys))
                return OFTrue;
    return OFFalse;
}



// ----------------------------------------------------------------------------

OFBool WlmFileSystemInteractionManager::DatasetMatchesSearchMask(DcmItem & dataset, DcmItem & searchMask,
    const MatchingKeys & matchingKeys)
{
#ifdef HAVE_CXX11
    for (auto& key : matchingKeys.keys)
    {
#else
    // remove this bloated version of the code if C++11 ever becomes a requirement of DCMTK
    for (OFVector<OFPair<DcmTagKey, OFBool> >::const_iterator it = matchingKeys.keys.begin(); it != matchingKeys.keys.end(); ++it)
    {
        const OFPair<DcmTagKey, OFBool>& key = *it;
#endif
        DcmElement* query = OFnullptr;
        if (searchMask.findAndGetElement(key.first, query, OFFalse).good() && query && !query->isUniversalMatch())
        {
            DcmElement* candidate = OFnullptr;
            if (dataset.findAndGetElement(key.first, candidate, OFFalse).bad() || !candidate || !query->matches(*candidate, key.second))
                return OFFalse;
        }
    }

#ifdef HAVE_CXX11
    for (auto& combinedKey : matchingKeys.combinedKeys)
    {
#else
    // remove this bloated version of the code if C++11 ever becomes a requirement of DCMTK
    for (OFVector<OFPair<DcmTagKey, DcmTagKey> >::const_iterator it = matchingKeys.combinedKeys.begin(); it != matchingKeys.combinedKeys.end(); ++it)
    {
        const OFPair<DcmTagKey, DcmTagKey>& combinedKey = *it;
#endif
        DcmElement* query = OFnullptr;
        if (searchMask.findAndGetElement(combinedKey.first, query, OFFalse).good() && query && !query->isUniversalMatch())
        {
            DcmElement* candidate = OFnullptr;
            if (dataset.findAndGetElement(combinedKey.first, candidate, OFFalse).bad() || !candidate)
                return OFFalse;
            DcmElement* secondQuery = OFnullptr;
            if (searchMask.findAndGetElement(combinedKey.second, secondQuery, OFFalse).good() && secondQuery && !secondQuery->isUniversalMatch())
            {
                DcmElement* secondCandidate = OFnullptr;
                if (dataset.findAndGetElement(combinedKey.second, secondCandidate, OFFalse).bad() || !secondCandidate || !query->combinationMatches(*secondQuery, *candidate, *secondCandidate))
                    return OFFalse;
            }
            else if (!query->matches(*candidate))
            {
                return OFFalse;
            }
        }
        else if (searchMask.findAndGetElement(combinedKey.second, query, OFFalse).good() && query && !query->isUniversalMatch())
        {
            DcmElement* candidate = OFnullptr;
            if (dataset.findAndGetElement(combinedKey.second, candidate, OFFalse).bad() || !candidate || !query->matches(*candidate))
                return OFFalse;
        }
    }

    // sequence matching

#ifdef HAVE_CXX11
    for (auto& sequenceKey : matchingKeys.sequenceKeys)
    {
#else
  // remove this bloated version of the code if C++11 ever becomes a requirement of DCMTK
    for (OFVector<OFPair<DcmTagKey, MatchingKeys> >::const_iterator it = matchingKeys.sequenceKeys.begin(); it != matchingKeys.sequenceKeys.end(); ++it)
    {
        const OFPair<DcmTagKey, MatchingKeys>& sequenceKey = *it;
#endif
        DcmElement* query = OFnullptr;
        if (searchMask.findAndGetElement(sequenceKey.first, query, OFFalse).good() && query && query->ident() == EVR_SQ && !IsUniversalMatch(OFstatic_cast(DcmSequenceOfItems&, *query), sequenceKey.second))
        {
            DcmElement* candidate = OFnullptr;
            if (dataset.findAndGetElement(sequenceKey.first, candidate, OFFalse).bad() || !candidate || candidate->ident() != EVR_SQ || !MatchSequences(OFstatic_cast(DcmSequenceOfItems&, *candidate), OFstatic_cast(DcmSequenceOfItems&, *query), sequenceKey.second))
                return OFFalse;
        }
    }
    return OFTrue;
    }
