/*
 *
 *  Copyright (C) 1993-2018, OFFIS e.V.
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
 *  Author:  Marco Eichelberg / Didier Lemoine / Gilles Mevel
 *
 *  Purpose: enums and structures used for the database index file
 *
 */

#ifndef DATAPRIV_H
#define DATAPRIV_H

#include "dcmtk/config/osconfig.h"    /* make sure OS specific configuration is included first */

#include "dcmtk/ofstd/ofoption.h"
#include "dcmtk/dcmnet/dicom.h"
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dcspchrs.h"
#include "dcmtk/dcmqrdb/dcmqrdbi.h"

BEGIN_EXTERN_C
#ifdef HAVE_IO_H
#define access my_access    // Workaround to make Visual C++ Compiler happy!
#include <io.h>
#undef access
#endif
END_EXTERN_C

// include this file in doxygen documentation

/** @file dcmqridx.h
 *  @brief type definitions and constants for the database index file
 */

/** types of query keys
 */
enum DB_KEY_TYPE
{
    /// unique key
    UNIQUE_KEY,
    /// required key
    REQUIRED_KEY,
    /// optional key
    OPTIONAL_KEY
};

/** query models
 */
enum DB_QUERY_CLASS
{
    /// patient root Q/R model
    PATIENT_ROOT,
    /// study root Q/R model
    STUDY_ROOT,
    /// patient/study only Q/R model
    PATIENT_STUDY
};

/** Level Strings
 */

#define PATIENT_LEVEL_STRING    "PATIENT"
#define STUDY_LEVEL_STRING      "STUDY"
#define SERIE_LEVEL_STRING      "SERIES"
#define IMAGE_LEVEL_STRING      "IMAGE"

/*
** Maximum size of things to put in db records.
** Some values will have vm>1 thus these maximums are
** intended to leave enough space for most common uses.
*/
#define AE_MAX_LENGTH   128     /* Application Entity    */
#define AS_MAX_LENGTH   32      /* Age String            */
#define AT_MAX_LENGTH   32      /* Attribute Tag         */
#define CS_MAX_LENGTH   128     /* Code String           */
#define DA_MAX_LENGTH   80      /* Date                  */
#define DS_MAX_LENGTH   128     /* Decimal String        */
#define DT_MAX_LENGTH   208     /* Date Time             */
#define FL_MAX_LENGTH   32      /* FLoating point single */
#define FD_MAX_LENGTH   64      /* Floating point Double */
#define IS_MAX_LENGTH   96      /* Integer String        */
#define LO_MAX_LENGTH   256     /* Long String           */
#define LT_MAX_LENGTH   40960   /* Long Text             */
#define PN_MAX_LENGTH   256     /* Person Name           */
#define SH_MAX_LENGTH   64      /* Short String          */
#define SL_MAX_LENGTH   32      /* Signed Long           */
#define SS_MAX_LENGTH   16      /* Signed Short          */
#define ST_MAX_LENGTH   4096    /* Short Text            */
#define TM_MAX_LENGTH   128     /* Time                  */
#define UI_MAX_LENGTH   64      /* Unique Identifier     */
#define UL_MAX_LENGTH   32      /* Unsigned Long         */
#define UR_MAX_LENGTH   4096   /* UR             */
#define US_MAX_LENGTH   16      /* Unsigned Short        */
#define CS_LABEL_MAX_LENGTH 16  /* Code String - Presentation Label */
#define DESCRIPTION_MAX_LENGTH 128  /* Not related to any particular DICOM attribute */

#define DBC_MAXSTRING           256

#define MAX_MAX_STUDIES         DB_UpperMaxStudies
#define MAX_NUMBER_OF_IMAGES    10000
#define SIZEOF_IDXRECORD        (sizeof (IdxRecord))
#define SIZEOF_STUDYDESC        (sizeof (StudyDescRecord) * MAX_MAX_STUDIES)

/* ENSURE THAT DBVERSION IS INCREMENTED WHENEVER ONE OF THESE STRUCTS IS MODIFIED */

struct DCMTK_DCMQRDB_EXPORT DB_SerializedTagKey
{
    inline DB_SerializedTagKey() {}
    inline DB_SerializedTagKey(const DcmTagKey& rhs) { *this = rhs; }
    inline DB_SerializedTagKey& operator=(const DcmTagKey& tk) { key[0] = tk.getGroup(); key[1] = tk.getElement(); return *this; }
    inline operator DcmTagKey() const { return DcmTagKey( key[0], key[1] ); }
    inline bool operator==(const DB_SerializedTagKey& rhs) const { return key[0] == rhs.key[0] && key[1] == rhs.key[1]; }
    Uint16 key[2];
};

/* ENSURE THAT DBVERSION IS INCREMENTED WHENEVER ONE OF THESE STRUCTS IS MODIFIED */

struct DCMTK_DCMQRDB_EXPORT DB_SerializedCharPtr
{
    inline DB_SerializedCharPtr(char* p) { ptr.p = p; }
    inline DB_SerializedCharPtr& operator=(char* p) { ptr.p = p; return *this; }
    inline operator char*() const { return ptr.p; }
    union
    {
        char* p;
        Uint64 placeholder;
    } ptr ;
};

/* ENSURE THAT DBVERSION IS INCREMENTED WHENEVER ONE OF THESE STRUCTS IS MODIFIED */

/** this class provides a primitive interface for handling a flat DICOM element,
 *  similar to DcmElement, but only for use within the database module
 */
struct DCMTK_DCMQRDB_EXPORT DB_SmallDcmElmt
{
public:
    /// default constructor
    DB_SmallDcmElmt();

    /// pointer to the value field
    DB_SerializedCharPtr PValueField ;

    /// value length in bytes
    Uint32 ValueLength ;

    /// attribute tag
    DB_SerializedTagKey XTag ;

private:
    /** private undefined copy constructor
     * @param copy documented to avoid doxygen warning
     */
    DB_SmallDcmElmt(const DB_SmallDcmElmt& copy);
    /** private undefined copy assignment operator
     * @param copy documented to avoid doxygen warning
     */
    DB_SmallDcmElmt& operator=(const DB_SmallDcmElmt& copy);
};

/* ENSURE THAT DBVERSION IS INCREMENTED WHENEVER ONE OF THESE STRUCTS IS MODIFIED */

/** this class provides a primitive interface for handling a list of flat DICOM elements,
 *  similar to DcmItem, but only for use within the database module
 */
struct DCMTK_DCMQRDB_EXPORT DB_ElementList
{
    /// default constructor
    DB_ElementList(): elem(), next(NULL), utf8Value() {}

    /// current list element
    DB_SmallDcmElmt elem ;

    /// pointer to next in list
    struct DB_ElementList *next ;

    /// UTF-8 cache
    OFoptional<OFString> utf8Value ;

private:
    /** private undefined copy constructor
     * @param copy documented to avoid doxygen warning
     */
    DB_ElementList(const DB_ElementList& copy);
    /** private undefined copy assignment operator
     * @param copy documented to avoid doxygen warning
     */
    DB_ElementList& operator=(const DB_ElementList& copy);
};

/* ENSURE THAT DBVERSION IS INCREMENTED WHENEVER ONE OF THESE STRUCTS IS MODIFIED */

struct DCMTK_DCMQRDB_EXPORT DB_UidList
{
    char *patient ;
    char *study ;
    char *serie ;
    char *image ;
    struct DB_UidList *next ;
};

/* ENSURE THAT DBVERSION IS INCREMENTED WHENEVER ONE OF THESE STRUCTS IS MODIFIED */

struct DCMTK_DCMQRDB_EXPORT DB_CounterList
{
    IdxRecord *rec;
    int idxCounter ;
    struct DB_CounterList *next ;
};

/* ENSURE THAT DBVERSION IS INCREMENTED WHENEVER ONE OF THESE STRUCTS IS MODIFIED */

struct DCMTK_DCMQRDB_EXPORT DB_FindAttr
{
    DcmTagKey tag ;
    DB_LEVEL level ;
    DB_KEY_TYPE keyAttr ;

    /* to passify some C++ compilers */
    DB_FindAttr(const DcmTagKey& t, DB_LEVEL l, DB_KEY_TYPE kt)
        : tag(t), level(l), keyAttr(kt) { }
};

/* ENSURE THAT DBVERSION IS INCREMENTED WHENEVER ONE OF THESE STRUCTS IS MODIFIED */

struct DCMTK_DCMQRDB_EXPORT DB_Private_Handle
{
    int pidx ;
    OFString findRequestCharacterSet ;
    DcmSpecificCharacterSet findRequestConverter ;
    DB_ElementList *findRequestList ;
    DB_ElementList *findResponseList ;
    DB_LEVEL queryLevel ;
    char indexFilename[DBC_MAXSTRING+1] ;
    char storageArea[DBC_MAXSTRING+1] ;
    long maxBytesPerStudy ;
    long maxStudiesAllowed ;
    int idxCounter ;
    DB_CounterList *moveCounterList ;
    int NumberRemainOperations ;
    DB_QUERY_CLASS rootLevel ;
    DB_UidList *uidList ;

    DB_Private_Handle()
    : pidx(0)
    , findRequestCharacterSet()
    , findRequestConverter()
    , findRequestList(NULL)
    , findResponseList(NULL)
    , queryLevel(STUDY_LEVEL)
//  , indexFilename()
//  , storageArea()
    , maxBytesPerStudy(0)
    , maxStudiesAllowed(0)
    , idxCounter(0)
    , moveCounterList(NULL)
    , NumberRemainOperations(0)
    , rootLevel(STUDY_ROOT)
    , uidList(NULL)
    {
    }
};

/* ENSURE THAT DBVERSION IS INCREMENTED WHENEVER ONE OF THESE STRUCTS IS MODIFIED */

/** this struct defines the structure of each "Study Record" in the index.dat
 *  file maintained by this module. A Study Record is a direct binary copy
 *  of an instance of this struct.
 */
struct DCMTK_DCMQRDB_EXPORT StudyDescRecord
{
    /// Study Instance UID of the study described by this record
    char StudyInstanceUID [UI_MAX_LENGTH+1] ;

    /// combined size (in bytes) of all images of this study in the database
    Uint32 StudySize ;

    /// timestamp for last update of this study. Format: output of time(2) converted to double.
    double LastRecordedDate ;

    /// number of images of this study in the database
    Uint32 NumberofRegistratedImages ;
};

/* ENSURE THAT DBVERSION IS INCREMENTED WHENEVER ONE OF THESE STRUCTS IS MODIFIED */

struct DCMTK_DCMQRDB_EXPORT ImagesofStudyArray
{
    Uint32 idxCounter ;
    double RecordedDate ;
    Uint32 ImageSize ;
};


/* the following constants define which array element
 * of the param[] array in the IdxRecord structure
 * is linked to which value field in the IdxRecord.
 * numbers must be continuous, starting with 0.
 *
 * The constant NBPARAMETERS must contain the number
 * of array elements to be referenced in param[]
 * (= highest index +1)
 */

#define RECORDIDX_PatientBirthDate                0
#define RECORDIDX_PatientSex                      1
#define RECORDIDX_PatientName                     2
#define RECORDIDX_PatientID                       3
#define RECORDIDX_PatientBirthTime                4
#define RECORDIDX_OtherPatientIDs                 5
#define RECORDIDX_OtherPatientNames               6
#define RECORDIDX_EthnicGroup                     7
#define RECORDIDX_StudyDate                       8
#define RECORDIDX_StudyTime                       9
#define RECORDIDX_StudyID                        10
#define RECORDIDX_StudyDescription               11
#define RECORDIDX_NameOfPhysiciansReadingStudy   12
#define RECORDIDX_AccessionNumber                13
#define RECORDIDX_ReferringPhysicianName         14
#define RECORDIDX_ProcedureDescription           15
#define RECORDIDX_AttendingPhysiciansName        16
#define RECORDIDX_StudyInstanceUID               17
#define RECORDIDX_OtherStudyNumbers              18
#define RECORDIDX_AdmittingDiagnosesDescription  19
#define RECORDIDX_PatientAge                     20
#define RECORDIDX_PatientSize                    21
#define RECORDIDX_PatientWeight                  22
#define RECORDIDX_Occupation                     23
#define RECORDIDX_SeriesNumber                   24
#define RECORDIDX_SeriesInstanceUID              25
#define RECORDIDX_Modality                       26
#define RECORDIDX_ImageNumber                    27
#define RECORDIDX_SOPInstanceUID                 28
#define RECORDIDX_SeriesDate                     29
#define RECORDIDX_SeriesTime                     30
#define RECORDIDX_SeriesDescription              31
#define RECORDIDX_ProtocolName                   32
#define RECORDIDX_OperatorsName                  33
#define RECORDIDX_PerformingPhysicianName        34
#define RECORDIDX_PresentationLabel              35
#define RECORDIDX_IssuerOfPatientID              36
#define RECORDIDX_SpecificCharacterSet           37

#define RECORDIDX_FileMetaInformationVersion 38
#define RECORDIDX_MediaStorageSOPClassUID 39
#define RECORDIDX_MediaStorageSOPInstanceUID 40
#define RECORDIDX_TransferSyntaxUID 41
#define RECORDIDX_ImplementationClassUID 42
#define RECORDIDX_ImplementationVersionName 43
#define RECORDIDX_DirectoryRecordType 44
#define RECORDIDX_ImageType 45
#define RECORDIDX_AcquisitionDate 46
#define RECORDIDX_ContentDate 47
#define RECORDIDX_AcquisitionTime 48
#define RECORDIDX_ContentTime 49
#define RECORDIDX_Manufacturer 50
#define RECORDIDX_ManufacturerModelName 51
#define RECORDIDX_PatientIdentityRemoved 52
#define RECORDIDX_DeidentificationMethod 53
#define RECORDIDX_ContrastBolusAgent 54
#define RECORDIDX_BodyPartExamined 55
#define RECORDIDX_ScanOptions 56
#define RECORDIDX_KVP 57
#define RECORDIDX_DataCollectionDiameter 58
#define RECORDIDX_SoftwareVersions 59
#define RECORDIDX_ReconstructionDiameter 60
#define RECORDIDX_GantryDetectorTilt 61
#define RECORDIDX_TableHeight 62
#define RECORDIDX_RotationDirection 63
#define RECORDIDX_ExposureTime 64
#define RECORDIDX_XRayTubeCurrent 65
#define RECORDIDX_FilterType 66
#define RECORDIDX_GeneratorPower 67
#define RECORDIDX_FocalSpots 68
#define RECORDIDX_ConvolutionKernel 69
#define RECORDIDX_PatientPosition 70
#define RECORDIDX_ExposureModulationType 71
#define RECORDIDX_EstimatedDoseSaving 72
#define RECORDIDX_CTDIvol 73
#define RECORDIDX_AcquisitionNumber 74
#define RECORDIDX_InstanceNumber 75
#define RECORDIDX_PatientOrientation 76
#define RECORDIDX_ImagePositionPatient 77
#define RECORDIDX_ImageOrientationPatient 78
#define RECORDIDX_FrameOfReferenceUID 79
#define RECORDIDX_PositionReferenceIndicator 80
#define RECORDIDX_SliceLocation 81
#define RECORDIDX_SamplesPerPixel 82
#define RECORDIDX_PhotometricInterpretation 83
#define RECORDIDX_PlanarConfiguration 84
#define RECORDIDX_NumberOfFrames 85
#define RECORDIDX_Rows 86
#define RECORDIDX_Columns 87
#define RECORDIDX_PixelSpacing 88
#define RECORDIDX_PixelAspectRatio 89
#define RECORDIDX_BitsAllocated 90
#define RECORDIDX_BitsStored 91
#define RECORDIDX_HighBit 92
#define RECORDIDX_PixelRepresentation 93
#define RECORDIDX_WindowCenter 94
#define RECORDIDX_WindowWidth 95
#define RECORDIDX_RescaleIntercept 96
#define RECORDIDX_RescaleSlope 97
#define RECORDIDX_StorageMediaFileSetID 98
#define RECORDIDX_StorageMediaFileSetUID 99
#define RECORDIDX_PixelData 100

#define NBPARAMETERS                             101

/* ENSURE THAT DBVERSION IS INCREMENTED WHENEVER ONE OF THESE STRUCTS IS MODIFIED */

/** this class manages an instance entry of the index file.
 *  Each instance/image record within the index.dat file is
 *  a direct (binary) copy of this structure.
 */
struct DCMTK_DCMQRDB_EXPORT IdxRecord
{
    /// default constructor
    IdxRecord();
    /* undefined */ IdxRecord(const IdxRecord& copy);

    char    filename                        [DBC_MAXSTRING+1] ;
    char    SOPClassUID                     [UI_MAX_LENGTH+1] ;
    double  RecordedDate ;
    Uint32  ImageSize ;

    DB_SmallDcmElmt param                   [NBPARAMETERS] ;

    char    PatientBirthDate                [DA_MAX_LENGTH+1] ;
    char    PatientSex                      [CS_MAX_LENGTH+1] ;
    char    PatientName                     [PN_MAX_LENGTH+1] ;
    char    PatientID                       [LO_MAX_LENGTH+1] ;
    char    PatientBirthTime                [TM_MAX_LENGTH+1] ;
    char    OtherPatientIDs                 [LO_MAX_LENGTH+1] ;
    char    OtherPatientNames               [PN_MAX_LENGTH+1] ;
    char    EthnicGroup                     [SH_MAX_LENGTH+1] ;

    char    StudyDate                       [DA_MAX_LENGTH+1] ;
    char    StudyTime                       [TM_MAX_LENGTH+1] ;
    char    StudyID                         [CS_MAX_LENGTH+1] ;
    char    StudyDescription                [LO_MAX_LENGTH+1] ;
    char    NameOfPhysiciansReadingStudy    [PN_MAX_LENGTH+1] ;

    char    AccessionNumber                 [CS_MAX_LENGTH+1] ;
    char    ReferringPhysicianName          [PN_MAX_LENGTH+1] ;
    char    ProcedureDescription            [LO_MAX_LENGTH+1] ;
    char    AttendingPhysiciansName         [PN_MAX_LENGTH+1] ;
    char    StudyInstanceUID                [UI_MAX_LENGTH+1] ;
    char    OtherStudyNumbers               [IS_MAX_LENGTH+1] ;
    char    AdmittingDiagnosesDescription   [LO_MAX_LENGTH+1] ;
    char    PatientAge                      [AS_MAX_LENGTH+1] ;
    char    PatientSize                     [DS_MAX_LENGTH+1] ;
    char    PatientWeight                   [DS_MAX_LENGTH+1] ;
    char    Occupation                      [SH_MAX_LENGTH+1] ;

    char    SeriesNumber                    [IS_MAX_LENGTH+1] ;
    char    SeriesInstanceUID               [UI_MAX_LENGTH+1] ;
    char    Modality                        [CS_MAX_LENGTH+1] ;

    char    ImageNumber                     [IS_MAX_LENGTH+1] ;
    char    SOPInstanceUID                  [UI_MAX_LENGTH+1] ;

    char    SeriesDate                      [DA_MAX_LENGTH+1] ;
    char    SeriesTime                      [TM_MAX_LENGTH+1] ;
    char    SeriesDescription               [LO_MAX_LENGTH+1] ;
    char    ProtocolName                    [LO_MAX_LENGTH+1] ;
    char    OperatorsName                   [PN_MAX_LENGTH+1] ;
    char    PerformingPhysicianName         [PN_MAX_LENGTH+1] ;
    char    PresentationLabel               [CS_LABEL_MAX_LENGTH+1] ;
    char    IssuerOfPatientID               [LO_MAX_LENGTH+1] ;

    char    hstat;

    // Not related to any particular DICOM attribute !
    char    InstanceDescription             [DESCRIPTION_MAX_LENGTH+1] ;

    // Specific Character Set, support for VM ~ 8 (depending on the
    // actual length of the used DTs)
    char    SpecificCharacterSet            [CS_MAX_LENGTH*8+1] ;

    char FileMetaInformationVersion[UR_MAX_LENGTH + 1];
    char MediaStorageSOPClassUID[UI_MAX_LENGTH + 1];
    char MediaStorageSOPInstanceUID[UI_MAX_LENGTH + 1];
    char TransferSyntaxUID[UI_MAX_LENGTH + 1];
    char ImplementationClassUID[UI_MAX_LENGTH + 1];
    char ImplementationVersionName[SH_MAX_LENGTH + 1];
    char DirectoryRecordType[CS_MAX_LENGTH + 1];
    char ImageType[CS_MAX_LENGTH + 1];
    char AcquisitionDate[DA_MAX_LENGTH + 1];
    char ContentDate[DA_MAX_LENGTH + 1];
    char AcquisitionTime[TM_MAX_LENGTH + 1];
    char ContentTime[TM_MAX_LENGTH + 1];
    char Manufacturer[LO_MAX_LENGTH + 1];
    char ManufacturerModelName[LO_MAX_LENGTH + 1];
    char PatientIdentityRemoved[CS_MAX_LENGTH + 1];
    char DeidentificationMethod[LO_MAX_LENGTH + 1];
    char ContrastBolusAgent[LO_MAX_LENGTH + 1];
    char BodyPartExamined[CS_MAX_LENGTH + 1];
    char ScanOptions[CS_MAX_LENGTH + 1];
    char KVP[DS_MAX_LENGTH + 1];
    char DataCollectionDiameter[DS_MAX_LENGTH + 1];
    char SoftwareVersions[LO_MAX_LENGTH + 1];
    char ReconstructionDiameter[DS_MAX_LENGTH + 1];
    char GantryDetectorTilt[DS_MAX_LENGTH + 1];
    char TableHeight[DS_MAX_LENGTH + 1];
    char RotationDirection[CS_MAX_LENGTH + 1];
    char ExposureTime[IS_MAX_LENGTH + 1];
    char XRayTubeCurrent[IS_MAX_LENGTH + 1];
    char FilterType[SH_MAX_LENGTH + 1];
    char GeneratorPower[IS_MAX_LENGTH + 1];
    char FocalSpots[DS_MAX_LENGTH + 1];
    char ConvolutionKernel[SH_MAX_LENGTH + 1];
    char PatientPosition[SH_MAX_LENGTH + 1];
    char ExposureModulationType[CS_MAX_LENGTH + 1];
    char EstimatedDoseSaving[FD_MAX_LENGTH + 1];
    char CTDIvol[FD_MAX_LENGTH + 1];
    char AcquisitionNumber[IS_MAX_LENGTH + 1];
    char InstanceNumber[IS_MAX_LENGTH + 1];
    char PatientOrientation[IS_MAX_LENGTH + 1];
    char ImagePositionPatient[DS_MAX_LENGTH + 1];
    char ImageOrientationPatient[DS_MAX_LENGTH + 1];
    char FrameOfReferenceUID[UI_MAX_LENGTH + 1];
    char PositionReferenceIndicator[LO_MAX_LENGTH + 1];
    char SliceLocation[DS_MAX_LENGTH + 1];
    char SamplesPerPixel[US_MAX_LENGTH + 1];
    char PhotometricInterpretation[CS_MAX_LENGTH + 1];
    char PlanarConfiguration[US_MAX_LENGTH + 1];
    char NumberOfFrames[IS_MAX_LENGTH + 1];
    char Rows[US_MAX_LENGTH + 1];
    char Columns[US_MAX_LENGTH + 1];
    char PixelSpacing[DS_MAX_LENGTH + 1];
    char PixelAspectRatio[IS_MAX_LENGTH + 1];
    char BitsAllocated[US_MAX_LENGTH + 1];
    char BitsStored[US_MAX_LENGTH + 1];
    char HighBit[US_MAX_LENGTH + 1];
    char PixelRepresentation[US_MAX_LENGTH + 1];
    char WindowCenter[DS_MAX_LENGTH + 1];
    char WindowWidth[DS_MAX_LENGTH + 1];
    char RescaleIntercept[DS_MAX_LENGTH + 1];
    char RescaleSlope[DS_MAX_LENGTH + 1];
    char StorageMediaFileSetID[SH_MAX_LENGTH + 1];
    char StorageMediaFileSetUID[UI_MAX_LENGTH + 1];
    char PixelData[UR_MAX_LENGTH + 1];

private:
    /* undefined */ IdxRecord& operator=(const IdxRecord& copy);
};


#endif
