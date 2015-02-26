#-------------------------------------------------------------------
# This file is part of the CMake build system for OGRE
#     (Object-oriented Graphics Rendering Engine)
# For the latest info, see http://www.ogre3d.org/
#
# The contents of this file are placed in the public domain. Feel
# free to make use of it in any way you like.
#-------------------------------------------------------------------

# - Try to find POCO libraries
# Once done, this will define
#
#  POCO_FOUND - system has POCO
#  POCO_INCLUDE_DIRS - the POCO include directories 
#  POCO_LIBRARIES - link these to use POCO

include(FindPkgMacros)

set(POCO_LIBRARIES "")

findpkg_begin(POCO)


# Get path, convert backslashes as ${ENV_${var}}
getenv_path(POCO_HOME)
getenv_path(POCO_ROOT)
getenv_path(POCO_BASE)

# construct search paths
set(POCO_PREFIX_PATH 
  ${POCO_HOME} ${ENV_POCO_HOME} 
  ${POCO_ROOT} ${ENV_POCO_ROOT}
  ${POCO_BASE} ${ENV_POCO_BASE}
)
# redo search if prefix path changed
clear_if_changed(POCO_PREFIX_PATH
  POCO_LIBRARY_REL
  POCO_INCLUDE_DIR
)

create_search_paths(POCO)
set(POCO_INC_SEARCH_PATH ${POCO_INC_SEARCH_PATH} ${POCO_PREFIX_PATH})

set(POCO_LIBRARY_NAMES PocoFoundationmt)

find_path(POCO_INCLUDE_DIR NAMES Poco/Foundation.h HINTS ${POCO_INC_SEARCH_PATH} ${POCO_PKGC_INCLUDE_DIRS} PATH_SUFFIXES Foundation/include)
find_library(POCO_LIBRARY_REL NAMES ${POCO_LIBRARY_NAMES} HINTS ${POCO_LIB_SEARCH_PATH} ${POCO_PKGC_LIBRARY_DIRS} PATH_SUFFIXES Linux/i686)
make_library_set(POCO_LIBRARY)

findpkg_finish(POCO)

# Look for Poco's Foundation package
findpkg_begin(POCO_Foundation)
set(POCO_FOUNDATION_NAMES PocoFoundationmt PocoFoundation)
find_path(POCO_Foundation_INCLUDE_DIR NAMES Poco/Foundation.h HINTS ${POCO_INCLUDE_DIR} ${POCO_INC_SEARCH_PATH} ${POCO_PKGC_INCLUDE_DIRS} PATH_SUFFIXES Foundation/include .)
find_library(POCO_Foundation_LIBRARY_REL NAMES ${POCO_FOUNDATION_NAMES} HINTS ${POCO_LIB_SEARCH_PATH} ${POCO_PKGC_LIBRARY_DIRS} PATH_SUFFIXES Linux/i686)
make_library_set(POCO_Foundation_LIBRARY)
list(APPEND POCO_LIBRARIES ${POCO_Foundation_LIBRARY})
findpkg_finish(POCO_Foundation)

# Look for Poco's Crypto package
findpkg_begin(POCO_Crypto)
set(POCO_CRYPTO_LIBRARY_NAMES PocoCryptomt PocoCrypto)
find_path(POCO_Crypto_INCLUDE_DIR NAMES Poco/Crypto/Crypto.h HINTS ${POCO_INCLUDE_DIR} ${POCO_INC_SEARCH_PATH} ${POCO_PKGC_INCLUDE_DIRS} PATH_SUFFIXES Crypto/include .)
find_library(POCO_Crypto_LIBRARY_REL NAMES ${POCO_CRYPTO_LIBRARY_NAMES} HINTS ${POCO_LIB_SEARCH_PATH} ${POCO_PKGC_LIBRARY_DIRS} PATH_SUFFIXES Linux/i686)
make_library_set(POCO_Crypto_LIBRARY)
list(APPEND POCO_LIBRARIES ${POCO_Crypto_LIBRARY})
findpkg_finish(POCO_Crypto)

# Look for Poco's Data package
findpkg_begin(POCO_Data)
set(POCO_DATA_LIBRARY_NAMES PocoDatamt PocoData)
find_path(POCO_Data_INCLUDE_DIR NAMES Poco/Data/Data.h HINTS ${POCO_INCLUDE_DIR} ${POCO_INC_SEARCH_PATH} ${POCO_PKGC_INCLUDE_DIRS} PATH_SUFFIXES Data/include)
find_library(POCO_Data_LIBRARY_REL NAMES ${POCO_DATA_LIBRARY_NAMES} HINTS ${POCO_LIB_SEARCH_PATH} ${POCO_PKGC_LIBRARY_DIRS} PATH_SUFFIXES Linux/i686)
make_library_set(POCO_Data_LIBRARY)
list(APPEND POCO_LIBRARIES ${POCO_Data_LIBRARY})
findpkg_finish(POCO_Data)

# Look for Poco's ODBC package
#findpkg_begin(POCO_Data_ODBC)
#set(POCO_Data_ODBC_LIBRARY_NAMES PocoDataODBCmt PocoODBC PocoDataODBC)
#find_path(POCO_Data_ODBC_INCLUDE_DIR NAMES Poco/Data/ODBC/ODBC.h HINTS ${POCO_INCLUDE_DIR} ${POCO_INC_SEARCH_PATH} ${POCO_PKGC_INCLUDE_DIRS} PATH_SUFFIXES Data/ODBC/include)
#find_library(POCO_Data_ODBC_LIBRARY_REL NAMES ${POCO_Data_ODBC_LIBRARY_NAMES} HINTS ${POCO_LIB_SEARCH_PATH} ${POCO_PKGC_LIBRARY_DIRS} PATH_SUFFIXES Linux/i686)
#make_library_set(POCO_Data_ODBC_LIBRARY)
#list(APPEND POCO_LIBRARIES ${POCO_Data_ODBC_LIBRARY})
#findpkg_finish(POCO_Data_ODBC)

# Look for Poco's SQLite package
findpkg_begin(POCO_Data_SQLite)
set(POCO_Data_SQLite_LIBRARY_NAMES PocoDataSQLitemt PocoSQLite PocoDataSQLite)
find_path(POCO_Data_SQLite_INCLUDE_DIR NAMES Poco/Data/SQLite/SQLite.h HINTS ${POCO_INCLUDE_DIR} ${POCO_INC_SEARCH_PATH} ${POCO_PKGC_INCLUDE_DIRS} PATH_SUFFIXES Data/SQLite/include)
find_library(POCO_Data_SQLite_LIBRARY_REL NAMES ${POCO_Data_SQLite_LIBRARY_NAMES} HINTS ${POCO_LIB_SEARCH_PATH} ${POCO_PKGC_LIBRARY_DIRS} PATH_SUFFIXES Linux/i686)
make_library_set(POCO_Data_SQLite_LIBRARY)
list(APPEND POCO_LIBRARIES ${POCO_Data_SQLite_LIBRARY})
findpkg_finish(POCO_Data_SQLite)

# Look for Poco's MYSQL package
findpkg_begin(POCO_Data_MYSQL)
set(POCO_Data_MYSQL_LIBRARY_NAMES PocoDataMySQLmt PocoMySQL PocoDataMySQL)
find_path(POCO_Data_MYSQL_INCLUDE_DIR NAMES Poco/Data/MySQL/MySQL.h HINTS ${POCO_INCLUDE_DIR} ${POCO_INC_SEARCH_PATH} ${POCO_PKGC_INCLUDE_DIRS} PATH_SUFFIXES Data/MySQL/include)
find_library(POCO_Data_MYSQL_LIBRARY_REL NAMES ${POCO_Data_MYSQL_LIBRARY_NAMES} HINTS ${POCO_LIB_SEARCH_PATH} ${POCO_PKGC_LIBRARY_DIRS} PATH_SUFFIXES Linux/i686)
make_library_set(POCO_Data_MYSQL_LIBRARY)
list(APPEND POCO_LIBRARIES ${POCO_Data_MYSQL_LIBRARY})
findpkg_finish(POCO_Data_MYSQL)

# Look for Poco's Net package
findpkg_begin(POCO_Net)
set(POCO_Net_LIBRARY_NAMES PocoNetmt PocoNet)
find_path(POCO_Net_INCLUDE_DIR NAMES Poco/Net/Net.h HINTS ${POCO_INCLUDE_DIR} ${POCO_INC_SEARCH_PATH} ${POCO_PKGC_INCLUDE_DIRS} PATH_SUFFIXES Net/include)
find_library(POCO_Net_LIBRARY_REL NAMES ${POCO_Net_LIBRARY_NAMES} HINTS ${POCO_LIB_SEARCH_PATH} ${POCO_PKGC_LIBRARY_DIRS} PATH_SUFFIXES Linux/i686)
make_library_set(POCO_Net_LIBRARY)
list(APPEND POCO_LIBRARIES ${POCO_Net_LIBRARY})
findpkg_finish(POCO_Net)

# Look for Poco's Util package
findpkg_begin(POCO_Util)
set(POCO_Util_LIBRARY_NAMES PocoUtilmt PocoUtil)
find_path(POCO_Util_INCLUDE_DIR NAMES Poco/Util/Util.h HINTS ${POCO_INCLUDE_DIR} ${POCO_INC_SEARCH_PATH} ${POCO_PKGC_INCLUDE_DIRS} PATH_SUFFIXES Util/include)
find_library(POCO_Util_LIBRARY_REL NAMES ${POCO_Util_LIBRARY_NAMES} HINTS ${POCO_LIB_SEARCH_PATH} ${POCO_PKGC_LIBRARY_DIRS} PATH_SUFFIXES Linux/i686)
make_library_set(POCO_Util_LIBRARY)
list(APPEND POCO_LIBRARIES ${POCO_Util_LIBRARY})
findpkg_finish(POCO_Util)


# Look for Poco's XML package
findpkg_begin(POCO_XML)
set(POCO_XML_LIBRARY_NAMES PocoXMLmt PocoXML)
find_path(POCO_XML_INCLUDE_DIR NAMES Poco/XML/XML.h HINTS ${POCO_INCLUDE_DIR} ${POCO_INC_SEARCH_PATH} ${POCO_PKGC_INCLUDE_DIRS} PATH_SUFFIXES XML/include)
find_library(POCO_XML_LIBRARY_REL NAMES ${POCO_XML_LIBRARY_NAMES} HINTS ${POCO_LIB_SEARCH_PATH} ${POCO_PKGC_LIBRARY_DIRS} PATH_SUFFIXES Linux/i686)
make_library_set(POCO_XML_LIBRARY)
list(APPEND POCO_LIBRARIES ${POCO_XML_LIBRARY})
findpkg_finish(POCO_XML)

IF (POCO_Foundation_INCLUDE_DIR)
	SET(POCO_FOUND TRUE)
ENDIF (POCO_Foundation_INCLUDE_DIR)

set (POCO_LIBRARYDIR ${POCO_ROOT}/lib)
if(NOT DEFINED POCO_LIBRARYDIR)
	set (POCO_LIBRARYDIR ${POCO_ROOT}/lib)
endif(NOT DEFINED POCO_LIBRARYDIR)

# Damn Linux Users Getting Confused, thinking this is a problem & ignoring lines with error on them
if(NOT ${WIN32})
MARK_AS_ADVANCED(
	POCO_ROOT
)
endif()