build_lib(
  LIBNAME simple-5g-system
  SOURCE_FILES
    helper/simple-5g-handover-helper.cc
    helper/simple-5g-net-device-helper.cc
    model/simple-5g-channel.cc
    model/simple-5g-net-device.cc
  HEADER_FILES
    helper/simple-5g-handover-helper.h
    helper/simple-5g-net-device-helper.h
    model/simple-5g-channel.h
    model/simple-5g-net-device.h
  LIBRARIES_TO_LINK ${libinternet}
)
