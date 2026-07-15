build_lib(
  LIBNAME simple-sat-system
  SOURCE_FILES
    helper/simple-sat-net-device-helper.cc
    model/simple-sat-channel.cc
    model/simple-sat-net-device.cc
  HEADER_FILES
    helper/simple-sat-net-device-helper.h
    model/simple-sat-channel.h
    model/simple-sat-net-device.h
  LIBRARIES_TO_LINK ${libinternet}
)
