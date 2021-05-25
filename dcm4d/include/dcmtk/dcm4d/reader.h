/**
  \brief Single pass parallel DICOM reader.

  A set of classes for optimized fast loading and sorting of files in one pass without repeated reads.
  It is possible as a complete or the basic only tags loading.

*/

#include <valarray>
#include <numeric>
#include <string>
#include <set>

#include <dcmtk/ofstd/offile.h>
#include <dcmtk/ofstd/ofdefine.h>
#include <dcmtk/dcmdata/dctk.h>

#ifdef dcm4d_EXPORTS
#define DCMTK_DCM4D_EXPORT DCMTK_DECL_EXPORT
#else
#define DCMTK_DCM4D_EXPORT DCMTK_DECL_IMPORT
#endif

/**
  \brief Vector algebra elements
*/
struct Vector3D: std::valarray<double> {
  using valarray::valarray;
  Vector3D(): valarray(3) {}
  Vector3D(const std::valarray<double> &val): valarray(val) {} //< for Visual Studio compiler
};

inline Vector3D cross(const Vector3D& a, const Vector3D& b)
{
  return a.cshift(1)*b.cshift(2) - a.cshift(2)*b.cshift(1);
}

/// ∑ vᵢ²
inline double sqr(const std::valarray<double>& v)
{
  return (v*v).sum();
}

/**
  \brief One slice (one file) info

  This structure contains flags need for sorting.
  A derived class can contain other tags and data obtained when loading the file.

  TODO: mammography sorting
*/

struct DCMTK_DCM4D_EXPORT SliceInfo {
    long AcquisitionNumber=std::numeric_limits<long>::max(); ///< The maximal value std::numeric_limits<long>::max() means the tag is missing
    long TemporalPosition=std::numeric_limits<long>::max();
    long InstanceNumber=std::numeric_limits<long>::max();
    unsigned long FileSize=0;
    std::string Laterality;  ///< ImageLaterality, (Series)Laterality. For paired body parts: R or L, U - unpaired, B - both left and right
    std::string ViewPosition; ///< or from ViewCodeSequence, For MG: ViewPosition=CC/CodeValue=R-10242/CodeMeaning=cranio-caudal
                              ///<  or MLO/R-10226/medio-lateral oblique
    std::string SOPInstanceUID;
    OFFilename FileName;
    Vector3D ImagePositionPatient;
    Vector3D Orientation[3];

    double GantryTilt=0;
    double SliceLocation=std::numeric_limits<double>::max();
    std::valarray<double> PixelSpacing={1.,1.};
    enum {SpacingInPatient, SpacingAtDetector, SpacingUnknown} SpacingType=SpacingUnknown;
    long Rows=0, Columns=0;
    long NumberOfFrames=1;
    bool HasImagePositionPatient=false; ///< ImagePositionPatient exists in the DICOM file as 3 coordinates
    bool HasOrientation=false;

    bool operator<(const SliceInfo& b) const; /// For ordering slices in the image

    bool fill(DcmItem* d1, DcmItem* d2=nullptr);

    static std::string stringTag(DcmTagKey tag, DcmItem* d1, DcmItem* d2=nullptr, bool subSearch=true);
    static bool getTag(double& r, DcmTagKey tag, DcmItem* d1, DcmItem* d2 = nullptr);
    static bool getTag(long& r, DcmTagKey tag, DcmItem* d1, DcmItem* d2 = nullptr, bool subSearch=true);
    static bool getTag(std::valarray<double>& r, OFIStringStream& istr);
    static bool getTag(std::valarray<double>& r, DcmTagKey tag, DcmItem* d1, DcmItem* d2 = nullptr);
    static bool getTag(Vector3D (&r)[3], DcmTagKey tag, DcmItem* d1, DcmItem* d2 = nullptr);
};


/**
  \brief Return type of GetSeries, describes a logical group of files interpreted later as series or 2d...4d image.

  Files grouped into a single 3D or 3D+t block are described by an instance
  of this class. Relevant descriptive properties can be used to provide
  the application user with meaningful choices.
*/

struct DCMTK_DCM4D_EXPORT SeriesInfo
{
    std::string PatientID;
    std::string IssuerOfPatientID;
    std::string PatientName;
    std::string PatientBirthDate;

    std::string StudyDate;
    std::string StudyID;
    std::string StudyInstanceUID;

    long SeriesNumber=std::numeric_limits<long>::max();
    std::string SeriesTime;
    std::string SeriesInstanceUID;
    std::string Modality;
    std::string SOPClassUID;

    bool fill(DcmItem* d1, DcmItem* d2=nullptr);
    bool operator<(const SeriesInfo& b) const; ///< For ordering series

    std::set<SliceInfo> slices;
};

struct CompareSeriesPtr
{
    bool operator()(const std::shared_ptr<SeriesInfo>& a, const std::shared_ptr<SeriesInfo>& b) const
    {
        return *a < *b;
    }
};

struct DCMTK_DCM4D_EXPORT SeriesSet: std::set<std::shared_ptr<SeriesInfo>, CompareSeriesPtr> {

    void scanDirectory(const OFFilename& dir, bool withPath, bool recurse);
};
