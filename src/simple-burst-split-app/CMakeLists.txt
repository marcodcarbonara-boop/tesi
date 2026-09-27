build_lib(
  LIBNAME simple-burst-split-app
  SOURCE_FILES
    burst-buffered-splitter-application.cc
  HEADER_FILES
    burst-buffered-splitter-application.h
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${libinternet}
    ${libapplications}
)