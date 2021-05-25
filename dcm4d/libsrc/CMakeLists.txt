# create library from source files
DCMTK_ADD_LIBRARY(dcm4d reader)

DCMTK_TARGET_LINK_MODULES(dcm4d ofstd oflog dcmdata)
DCMTK_TARGET_LINK_LIBRARIES(dcm4d)
