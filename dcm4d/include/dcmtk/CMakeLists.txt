# declare installation files
install(DIRECTORY dcmtk/dcm4d DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/dcmtk" COMPONENT include FILES_MATCHING PATTERN "*.h")
