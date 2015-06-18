# - Find JEMalloc
# Find the native JEMalloc includes and library
#
#  JEMalloc_INCLUDE_DIR - where to find JEMalloc.h, etc.
#  JEMalloc_LIBRARY   - List of libraries when using JEMalloc.
#  JEMalloc_FOUND       - True if JEMalloc found.

find_path(JEMalloc_INCLUDE_DIR jemalloc/jemalloc.h NO_DEFAULT_PATH PATHS
  C:/local/GitHub/jemalloc-arma/jemalloc-3.6.0/include
  /usr/include
  /opt/local/include
  /usr/local/include
)

set(JEMalloc_NAMES jemalloc)

find_library(JEMalloc_LIBRARY NO_DEFAULT_PATH
  NAMES ${JEMalloc_NAMES} PATHS
  C:/local/GitHub/jemalloc-arma/jemalloc-3.6.0/lib/vc2013/x86
  /lib
  /usr/lib
  /usr/local/lib
)

if (JEMalloc_INCLUDE_DIR AND JEMalloc_LIBRARY)
  set(JEMalloc_FOUND TRUE)
else ()
  set(JEMalloc_FOUND FALSE)
endif ()

if (JEMalloc_FOUND)
  message(STATUS "Found JEMalloc: ${JEMalloc_LIBRARY}")
else ()
  message(STATUS "Not Found JEMalloc: ${JEMalloc_LIBRARY}")
  if (JEMalloc_FIND_REQUIRED)
    message(STATUS "Looked for JEMalloc libraries named ${JEMalloc_NAMES}.")
    message(FATAL_ERROR "Could NOT find JEMalloc library")
  endif ()
endif ()

mark_as_advanced(
  JEMalloc_LIBRARY
  JEMalloc_INCLUDE_DIR
  )
