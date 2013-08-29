INCLUDE(FindPackageHandleStandardArgs)

if(${HERMES_VERSION} STREQUAL "HERMES2D")
  if(WIN64)
    FIND_LIBRARY(HERMES_LIBRARY NAMES hermes2d_64 hermes2d_64-debug PATHS ${HERMES_DIRECTORY} /usr/lib /usr/local/lib NO_DEFAULT_PATH)
  else()
    FIND_LIBRARY(HERMES_LIBRARY NAMES hermes2d hermes2d-debug PATHS ${HERMES_DIRECTORY} /usr/lib /usr/local/lib NO_DEFAULT_PATH)
  endif()
  
  FIND_PATH(HERMES_INCLUDE hermes2d.h ${HERMES2D_INCLUDE_PATH} /usr/include/hermes2d /usr/local/include/hermes2d)
	FIND_PACKAGE_HANDLE_STANDARD_ARGS(HERMES DEFAULT_MSG HERMES_LIBRARY HERMES_INCLUDE)
endif(${HERMES_VERSION} STREQUAL "HERMES2D")
