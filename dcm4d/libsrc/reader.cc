#include <dcmtk/dcm4d/reader.h>

#include <valarray>
#include <numeric>
#include <string>
#include <set>
#include <atomic>
#include <thread>

#include <dcmtk/ofstd/offile.h>
#include <dcmtk/ofstd/ofstream.h>
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/ofstd/ofconsol.h>

/**
  \brief Single pass parallel DICOM reader.

  A set of classes for optimized fast loading and sorting of files in one pass without repeated reads.
  It is possible as a complete or the basic only tags loading.

*/

bool SliceInfo::fill(DcmItem* d1, DcmItem* d2)
{
  SOPInstanceUID= stringTag(DCM_SOPInstanceUID, d1, d2, false);
  if (SOPInstanceUID.empty())  SOPInstanceUID= stringTag(DCM_ReferencedSOPInstanceUIDInFile, d1, d2, false);
  if (SOPInstanceUID.empty())  SOPInstanceUID= stringTag(DCM_MediaStorageSOPInstanceUID, d1, d2, false);
  if (SOPInstanceUID.empty())  return false;
  Laterality = stringTag(DCM_ImageLaterality, d1, d2, false);
  if (Laterality.empty())  Laterality = stringTag(DCM_Laterality, d1, d2, false);
  ViewPosition = stringTag(DCM_ViewPosition, d1, d2, false);
  if (ViewPosition.empty()) {  // Determine position reading ViewCodeSequence
    DcmItem *seq=0;
    if (d1->findAndGetSequenceItem(DCM_ViewCodeSequence, seq).good()) {
      auto code = stringTag(DCM_CodeValue, seq, nullptr, true);
      if (code == "R-10242")  ViewPosition= "CC";
      else if (code == "R-10224")  ViewPosition= "ML";
      else if (code == "R-10226")  ViewPosition= "MLO";
    }
  }
  getTag(InstanceNumber, DCM_InstanceNumber, d1, d2);
  getTag(AcquisitionNumber, DCM_AcquisitionNumber, d1, d2);
  getTag(TemporalPosition, DCM_TemporalPositionIndex, d1, d2);
  HasImagePositionPatient= getTag(ImagePositionPatient, DCM_ImagePositionPatient, d1, d2);
  HasOrientation= getTag(Orientation, DCM_ImageOrientationPatient, d1, d2);
  getTag(GantryTilt, DCM_GantryDetectorTilt, d1, d2);
  getTag(SliceLocation, DCM_SliceLocation, d1, d2);
  getTag(Rows, DCM_Rows, d1, d2);
  getTag(Columns, DCM_Columns, d1, d2);
  getTag(NumberOfFrames, DCM_NumberOfFrames, d1, d2);
  if (getTag(PixelSpacing, DCM_PixelSpacing, d1, d2))  SpacingType= SpacingInPatient;
  else if(getTag(PixelSpacing, DCM_ImagerPixelSpacing, d1, d2))  SpacingType= SpacingAtDetector;
  return true;
}

std::string SliceInfo::stringTag(DcmTagKey tag, DcmItem* d1, DcmItem* d2, bool subSearch)
{
  std::string r;
  d1->findAndGetOFStringArray(tag, r, subSearch);
  if (r.empty() && d2)  d2->findAndGetOFStringArray(tag, r, subSearch);
  return r;
}
bool SliceInfo::getTag(double& r, DcmTagKey tag, DcmItem* d1, DcmItem* d2)
{
  bool success=false;
  auto v = OFStandard::atof(stringTag(tag, d1, d2).c_str(), &success);
  if (success) r = v;
  return success;
}
bool SliceInfo::getTag(long& r, DcmTagKey tag, DcmItem* d1, DcmItem* d2, bool subSearch)
{
  try {
    r = std::stol(stringTag(tag, d1, d2, subSearch));
  } catch (std::exception&) {
    return false;
  }
  return true;
}

bool SliceInfo::getTag(std::valarray<double>& r, OFIStringStream& istr)
{
  bool successful = false;
  std::string coordinate;
  for (auto it=std::begin(r); it!=std::end(r); ++it) {
    if (!std::getline(istr, coordinate, '\\'))  return false;
    *it = OFStandard::atof(coordinate.c_str(), &successful);
    if (!successful) return false;
  }
  return true;
}
bool SliceInfo::getTag(std::valarray<double>& r, DcmTagKey tag, DcmItem* d1, DcmItem* d2)
{
  OFString s{stringTag(tag, d1, d2)};
  OFIStringStream istr{s};
  return getTag(r, istr);
}
bool SliceInfo::getTag(Vector3D (&r)[3], DcmTagKey tag, DcmItem* d1, DcmItem* d2)
{
  OFString s{stringTag(tag, d1, d2)};
  OFIStringStream istr{s};
  if (!getTag(r[0], istr) || !getTag(r[1], istr))  return false;
  r[2] = cross(r[0], r[1]);
  return true;
}

bool SeriesInfo::fill(DcmItem* d1, DcmItem* d2)
{
  SeriesInstanceUID= SliceInfo::stringTag(DCM_SeriesInstanceUID, d1, d2, false);
  StudyInstanceUID= SliceInfo::stringTag(DCM_StudyInstanceUID, d1, d2, false);
  SliceInfo::getTag(SeriesNumber, DCM_SeriesNumber, d1, d2, false);
  if (SeriesInstanceUID.empty() && StudyInstanceUID.empty())  return false;
  PatientID= SliceInfo::stringTag(DCM_PatientID, d2, d1, false);
  IssuerOfPatientID= SliceInfo::stringTag(DCM_IssuerOfPatientID, d2, d1, false);
  PatientName= SliceInfo::stringTag(DCM_PatientName, d1, d2, false);
  PatientBirthDate= SliceInfo::stringTag(DCM_PatientBirthDate, d1, d2, false);
  StudyDate= SliceInfo::stringTag(DCM_StudyDate, d1, d2, false);
  StudyID= SliceInfo::stringTag(DCM_StudyID, d1, d2, false);
  SeriesTime= SliceInfo::stringTag(DCM_SeriesTime, d1, d2, false);
  Modality= SliceInfo::stringTag(DCM_Modality, d1, d2, false);
  SOPClassUID= SliceInfo::stringTag(DCM_SOPClassUID, d1, d2, false);
  return true;
}

bool SliceInfo::operator<(const SliceInfo& b) const
{
  // This method MUST accept missing location and position information (and all else, too)
  // because we cannot rely on anything
  // (restriction on the sentence before: we have to provide consistent sorting, so we
  // rely on the minimum information all DICOM files need to provide: SOP Instance UID, or at least the filename).

  /* We CAN expect a group of equal
     - series instance uid
     - image orientation
     - pixel spacing or imager pixel spacing
     - slice thickness
     - gantry tilt
     But if there is no more than 3 same oriented slices, it is a mutioriented series, which we save as one block,
     so we will additionally split series after loading when it size is known.
     Number of rows/columns we can adapt during volume loading
  */

  // See if we have Image Position and Orientation which is equal and some other attributes are not very different
  if ((HasImagePositionPatient && HasOrientation) != (b.HasImagePositionPatient && b.HasOrientation))
    return (HasImagePositionPatient && HasOrientation) < (b.HasImagePositionPatient && b.HasOrientation);
  if ( HasImagePositionPatient && HasOrientation
    && fabs(GantryTilt - b.GantryTilt) <= 10
    && SpacingType == b.SpacingType && sqr(PixelSpacing - b.PixelSpacing) <= 0.1 * sqr(PixelSpacing)
    && sqr(Orientation[0] - b.Orientation[0]) <= 0.00000001
    && sqr(Orientation[1] - b.Orientation[1]) <= 0.00000001 ) {

    // Compute the distance from world origin (0,0,0) ALONG THE MEAN of the two NORMALS of the slices
    Vector3D normal= Orientation[2] + b.Orientation[2];

    Vector3D displacement= b.ImagePositionPatient - ImagePositionPatient;

    double dist = (normal*displacement).sum();

    // Test that we need to check more properties to distinguish slices
    if (fabs(dist) > 0.0001)  return dist > 0;
  }
  if (!HasImagePositionPatient && !b.HasImagePositionPatient && !Laterality.empty() && !b.Laterality.empty()) { // for paired body parts
    if (ViewPosition != b.ViewPosition)  return ViewPosition < b.ViewPosition; // "CC" < "ML[O]"
    if (Laterality != b.Laterality)  return Laterality > b.Laterality; // "R" > "L"
  }
  if (fabs(SliceLocation - b.SliceLocation) > 0.0001)  return SliceLocation < b.SliceLocation;
  // Try to sort by Acquisition Number
  if (AcquisitionNumber != b.AcquisitionNumber)  return AcquisitionNumber < b.AcquisitionNumber;
  // Neither position nor acquisition number are good for sorting, so check more
  // Let's try temporal index
  if (TemporalPosition != b.TemporalPosition)  return TemporalPosition < b.TemporalPosition;
  if (InstanceNumber != b.InstanceNumber)  return InstanceNumber < b.InstanceNumber;

  // LAST RESORT: all valuable information for sorting is missing or same.
  // Sort by some meaningless but unique identifiers and file names to satisfy the sort function
  if (SOPInstanceUID != b.SOPInstanceUID)  return SOPInstanceUID < b.SOPInstanceUID;
#if (defined(WIDE_CHAR_FILE_IO_FUNCTIONS) || defined(WIDE_CHAR_MAIN_FUNCTION)) && defined(_WIN32)
  if (auto x= FileName.getWideCharPointer()==b.FileName.getWideCharPointer()
     ? 0
     : FileName.getWideCharPointer()==0
       ? -1
       : b.FileName.getWideCharPointer()==0 ? 1 : wcscmp(FileName.getWideCharPointer(), b.FileName.getWideCharPointer()))  return x < 0;
#endif
  return strcmp(FileName.getCharPointer(), b.FileName.getCharPointer()) < 0;
}

bool SeriesInfo::operator<(const SeriesInfo& b) const
{
  if (PatientID != b.PatientID && IssuerOfPatientID != b.IssuerOfPatientID) {
     if (PatientName != b.PatientName)  return PatientName < b.PatientName;
     if (PatientBirthDate != b.PatientBirthDate)  return PatientBirthDate < b.PatientBirthDate;
     if (IssuerOfPatientID != b.IssuerOfPatientID)  return IssuerOfPatientID < b.IssuerOfPatientID;
     return PatientID < b.PatientID;
  }
  if (StudyInstanceUID != b.StudyInstanceUID) {
    if (StudyDate != b.StudyDate)  return StudyDate < b.StudyDate;
    if (StudyID != b.StudyID)  return StudyID < b.StudyID;
    return StudyInstanceUID < b.StudyInstanceUID;
  }
  if (SeriesInstanceUID != b.SeriesInstanceUID) {
    if (slices.size()==1 && b.slices.size()==1) {
      const auto &x= *slices.begin(), &y= *b.slices.begin();
      if((!x.HasImagePositionPatient && !x.Laterality.empty()) || (!y.HasImagePositionPatient && !y.Laterality.empty())) { // for paired body parts (MG)
        if (x.ViewPosition != y.ViewPosition)  return x.ViewPosition < y.ViewPosition; // "CC" < "ML[O]"
        if (x.Laterality != y.Laterality)  return x.Laterality > y.Laterality; // "R" > "L"
      }
    }
    if (SeriesNumber != b.SeriesNumber)  return SeriesNumber < b.SeriesNumber;
    if (SeriesTime != b.SeriesTime)  return SeriesTime < b.SeriesTime;
    if (Modality != b.Modality) return Modality < b.Modality;
    if (SOPClassUID != b.SOPClassUID) return SOPClassUID < b.SOPClassUID;
    return SeriesInstanceUID < b.SeriesInstanceUID;
  }
  if (SeriesInstanceUID.empty() && slices.size() && b.slices.size()) {
      const auto &x= *slices.begin(), &y= *b.slices.begin();
  }
  return false;
}

void SeriesSet::scanDirectory(const OFFilename& dir, bool withPath, bool recurse)
{
  OFList<OFFilename> files;

  OFStandard::searchDirectoryRecursively(withPath? dir : OFFilename(), files, OFFilename()/*pattern*/,
                                         withPath? OFFilename() : dir/*prefix*/, recurse);

  for (auto fn: files) {
    OFFilename fp = fn;
    if (!withPath) OFStandard::combineDirAndFilename(fp, dir, fn, OFTrue /*allowEmptyDirName*/);
    DcmFileFormat ff;
    ff.loadFile(fp); // 600ms (4154 files with 4044068500 bytes)
    SliceInfo sl;
    if (!sl.fill(ff.getDataset(), ff.getMetaInfo())) {
      continue; // 165ms
    }
    sl.FileName = fn;
    auto sr = std::make_shared<SeriesInfo>();
    if (!sr->fill(ff.getDataset(), ff.getMetaInfo()))  continue; // 11ms
    sr->slices.emplace(sl);
    auto it = find(sr);
    if (it == end()) {
      it = emplace_hint(it, sr);
    } else {
      (*it)->slices.emplace(sl);
    }
  }
}
