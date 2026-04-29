find_package(PkgConfig)

PKG_CHECK_MODULES(PC_GR_AEGIR gnuradio-Aegir)

FIND_PATH(
    GR_AEGIR_INCLUDE_DIRS
    NAMES gnuradio/Aegir/api.h
    HINTS $ENV{AEGIR_DIR}/include
        ${PC_AEGIR_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    GR_AEGIR_LIBRARIES
    NAMES gnuradio-Aegir
    HINTS $ENV{AEGIR_DIR}/lib
        ${PC_AEGIR_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
          )

include("${CMAKE_CURRENT_LIST_DIR}/gnuradio-AegirTarget.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GR_AEGIR DEFAULT_MSG GR_AEGIR_LIBRARIES GR_AEGIR_INCLUDE_DIRS)
MARK_AS_ADVANCED(GR_AEGIR_LIBRARIES GR_AEGIR_INCLUDE_DIRS)
