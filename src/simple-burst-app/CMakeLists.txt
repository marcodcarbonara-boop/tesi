build_lib(
  LIBNAME simple-burst-app
  SOURCE_FILES
    burst-buffered-application.cc
  HEADER_FILES
   burst-buffered-application.h
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${libinternet}
    ${libapplications}
)