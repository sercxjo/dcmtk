# declare executables
foreach(PROGRAM dcmlist dcmrlist)
  DCMTK_ADD_EXECUTABLE(${PROGRAM} ${PROGRAM})

  # make sure executables are linked to the corresponding libraries
  DCMTK_TARGET_LINK_MODULES(${PROGRAM} dcm4d dcmdata oflog ofstd)
endforeach()
