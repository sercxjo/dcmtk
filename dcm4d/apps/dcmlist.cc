#include <dcmtk/dcm4d/reader.h>
#include <iostream>

void listDirectory(const OFFilename& dir, bool withPath, bool recurse)
{
  SeriesSet srs;
  srs.scanDirectory(dir, withPath, recurse);

  std::cerr << srs.size() << "\n";
  for (auto sr:srs) std::cerr << sr->slices.size() << ' ';
  std::cerr << '\n';
  unsigned long long i=1;
  for (auto sr:srs) {
    OFOStringStream outname;
    outname << dir << PATH_SEPARATOR << "{1B7B7B67-5793-4FC4-" << std::hex << std::setw(4) << std::setfill('0') << (i>>48) << '-'
            << std::setw(12) << (i&0xffffffffffff) << "}" << PATH_SEPARATOR;
    OFStandard::createDirectory(outname.str(), dir);
    outname << "FileList.txt";
    std::cerr << outname.str() << '\n';
    std::ofstream out(outname.str());
    for (auto sl:sr->slices)  out << sl.FileName << '\n';
    i++;
  }
}

int main (int argn, char **argv)
{
  if (argn < 2)  listDirectory(OFFilename("."), false, false);
  else for (int i=1; i<argn; i++)  listDirectory(OFFilename(argv[i]), false, false);

  return 0;
}
