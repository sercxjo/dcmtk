/// Recursive DICOM directory lister

#include <dcmtk/dcm4d/reader.h>
#include <dcmtk/ofstd/ofconsol.h>

int main (int argn, char **argv)
{
  SeriesSet srs;

  if (argn < 2)  srs.scanDirectory(OFFilename("."), false /* no save path */, true /* recursive */);
  else for (int i=1; i<argn; i++)  srs.scanDirectory(OFFilename(argv[i]), argn>2, true);

  COUT << srs.size() << "\n";
  for (auto sr:srs) COUT << sr->slices.size() << ' ';
  COUT << '\n';
  unsigned long long i=1;
  for (auto sr:srs) {
    COUT << "SeriesInstanceUID=" << sr->SeriesInstanceUID << '\n';
    for (auto sl:sr->slices)  COUT << sl.FileName << '\n';
    i++;
  }

  return 0;
}
