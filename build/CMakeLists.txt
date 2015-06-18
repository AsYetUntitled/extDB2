#Change this if you need to target a specific CMake version

cmake_minimum_required(VERSION 3.2)
cmake_policy(SET CMP0015 NEW)

set(CMAKE_USER_MAKE_RULES_OVERRIDE
   ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/c_flag_overrides.cmake)
set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX
   ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/cxx_flag_overrides.cmake)

project(extDB2)

set(CMAKE_SUPPRESS_REGENERATION 1)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Choose the type of build, options are: Debug Release
RelWithDebInfo MinSizeRel."
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_CONFIGURATION_TYPES Release CACHE TYPE INTERNAL FORCE)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules" ${CMAKE_MODULE_PATH})

#Check for 32bit vs. 64bit system
if (CMAKE_SIZEOF_VOID_P EQUAL 8) #64bit
	message(FATAL_ERROR "Warning: You are on a 64-bit system, make sure you are pulling in the appropriate 32-bit libraries.")
elseif (CMAKE_SIZEOF_VOID_P EQUAL 4) #32bit
	message(STATUS "You are on a 32-bit system.")
else()
	message(STATUS "CMake wasn't able to identify if you are on 32-bit or 64-bit...")
endif()


SET(SOURCES
	../src/memory_allocator.cpp
	../src/sanitize.cpp
	../src/ext.cpp
	../src/backends/belogscanner.cpp
	../src/backends/http.cpp
	../src/backends/rcon.cpp
	../src/backends/remoteserver.cpp
	../src/backends/steam.cpp
	../src/protocols/http_raw.cpp
	../src/protocols/log.cpp
	../src/protocols/misc.cpp
	../src/protocols/rcon.cpp
	../src/protocols/steam.cpp
	../src/protocols/steam_v2.cpp
	../src/protocols/sql_custom.cpp
	../src/protocols/sql_custom_v2.cpp
	../src/protocols/sql_raw.cpp
	../src/protocols/sql_raw_v2.cpp
)


SET(COMPILE_EXTENSION TRUE CACHE BOOL "Compile extDB2")
SET(COMPILE_TEST_APPLICATION FALSE CACHE BOOL "Compiles a test application of extDB2.")
SET(COMPILE_RCON_APPLICATION FALSE CACHE BOOL "Compiles extDB2 Rcon.")
SET(COMPILE_SANITIZE_APPLICATION FALSE CACHE BOOL "Compiles extDB2 Sanitize (testing application).")

SET(DEBUG_LOGGING FALSE CACHE BOOL "Enable Debug Logging.")
SET(DEBUG_TESTING FALSE CACHE BOOL "Enable Extra Console Output.")


if (DEBUG_LOGGING)
	add_definitions(-DDEBUG_LOGGING)
	if(${WIN32})
		SET(RESOURCE_FILE ../src/dll-debug.rc)
	endif()
	message(STATUS "Debug Logging is enabled.")
else()
	if(${WIN32})
		SET(RESOURCE_FILE ../src/dll.rc)
	endif()
endif()
if (DEBUG_TESTING)
	add_definitions(-DDEBUG_TESTING)
	message(STATUS "Testing flag is enabled.")
endif()


if (COMPILE_TEST_APPLICATION)
	set(EXECUTABLE_NAME "extDB2-test")
	add_executable(${EXECUTABLE_NAME} ${SOURCES})
	add_definitions(-DTEST_APP)
	message(STATUS "Building Test Application")
elseif (COMPILE_RCON_APPLICATION)
	SET(SOURCES ../src/backends/rcon.cpp) # Override Sources
	set(EXECUTABLE_NAME "extDB2-rcon")
	add_executable(${EXECUTABLE_NAME} ${SOURCES})
	add_definitions(-DRCON_APP)
	message(STATUS "Building Rcon Applicaton")
elseif (COMPILE_SANITIZE_APPLICATION)
	SET(SOURCES ../src/sanitize.cpp) # Override Sources
	set(EXECUTABLE_NAME "extDB2-sanitize")
	add_executable(${EXECUTABLE_NAME} ${SOURCES})
	add_definitions(-DSANITIZE_APP)
	message(STATUS "Building Sanitize Applicaton")
else()
	LIST(APPEND SOURCES ${RESOURCE_FILE})
	LIST(APPEND SOURCES ../src/main.cpp)
	set(EXECUTABLE_NAME "extDB2")
	add_library (${EXECUTABLE_NAME} SHARED ${SOURCES})
	message(STATUS "Building Test Application")
endif()


# -----------------------
# START STATIC LIBRARIES
# -----------------------
if(${UNIX})
     set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()

# JEMalloc
if(${WIN32})
	add_definitions(-D JEMALLOC_LAZY_LOCK)
  add_definitions(-D USE_STATIC)
endif()
include(FindJEMalloc)
if(JEMalloc_FOUND)
	include_directories(${JEMalloc_INCLUDE_DIR})
else()
	message(FATAL_ERROR "\nJEMalloc not found\n Please Set JEMalloc_INCLUDE_DIR & JEMalloc_LIBRARIES")
endif()

# BOOST
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)
if(${WIN32})
	add_definitions(-D _WIN32_WINNT=0x0600)
endif()
if (COMPILE_EXTENSION)
	find_package(Boost REQUIRED COMPONENTS chrono date_time filesystem random regex system thread)
else()
	find_package(Boost REQUIRED COMPONENTS chrono date_time filesystem program_options random regex system thread)
endif()
if(Boost_FOUND)
	include_directories(${Boost_INCLUDE_DIRS})
else()
	message(FATAL_ERROR "\nBOOST not found\n Please Set BOOST_ROOT & BOOST_LIBRARYDIR")
endif()

# POCO
if(${WIN32})
	add_definitions(-D POCO_NO_UNWINDOWS)
endif()
include(FindPOCO)
if(POCO_FOUND)
	include_directories(${POCO_INCLUDE_DIR} ${POCO_Crypto_INCLUDE_DIR} ${POCO_Data_INCLUDE_DIR})
	include_directories(${POCO_Data_SQLite_INCLUDE_DIR} ${POCO_Data_MYSQL_INCLUDE_DIR} ${POCO_Net_INCLUDE_DIR} ${POCO_Util_INCLUDE_DIR})
else()
	message(FATAL_ERROR "\nPOCO not found\n Please Set POCO_ROOT")
endif()

# MySQL
include(FindMySQL)
if(MYSQL_FOUND)
	include_directories(${MYSQL_INCLUDE_DIR})
else()
	message(FATAL_ERROR "\nMySQL not found")
endif()

if(${UNIX})
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".so")
endif()
# -----------------------
# END STATIC LIBRARIES
# -----------------------


if(${WIN32})
	# Windows
	target_link_libraries(${EXECUTABLE_NAME} ${Boost_LIBRARIES}  ${POCO_LIBRARIES} ${MYSQL_LIBRARY} ${TBB_MALLOC_LIBRARY} Iphlpapi)
	add_definitions(-DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN)
	SET_TARGET_PROPERTIES(${EXECUTABLE_NAME} PROPERTIES LINK_FLAGS " /MANIFEST:NO /ERRORREPORT:NONE")
else()
	# Linux
	target_link_libraries(${EXECUTABLE_NAME} -Wl,-Bstatic ${Boost_LIBRARIES} ${POCO_LIBRARIES} ${MYSQL_LIBRARY}  -Wl,-Bdynamic -ldl -pthread -lz ${TBB_MALLOC_LIBRARY})
	set(CMAKE_CXX_FLAGS "-std=c++0x -static-libstdc++ -static-libgcc ${CMAKE_CXX_FLAGS}")

	if (NOT((COMPILE_TEST_APPLICATION) OR (COMPILE_RCON_APPLICATION) OR (COMPILE_TEST_SANITIZE_APPLICATION)))
		ADD_CUSTOM_COMMAND(
			TARGET ${EXECUTABLE_NAME}
			POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E rename libextDB2.so extDB2.so
		)
	endif()
endif()

mark_as_advanced(CMAKE_INSTALL_PREFIX)
mark_as_advanced(CMAKE_CONFIGURATION_TYPES)



message(STATUS "C_FLAGS:  =${CMAKE_C_FLAGS}")
message(STATUS "CMAKE_C_FLAGS_DEBUG:=${CMAKE_C_FLAGS_DEBUG}")
message(STATUS "CMAKE_C_FLAGS_RELEASE:=${CMAKE_C_FLAGS_RELEASE}")
message(STATUS "CMAKE_C_FLAGS_MINSIZEREL:=${CMAKE_C_FLAGS_MINSIZEREL}")
message(STATUS "CMAKE_C_FLAGS_RELWITHDEBINFO:=${CMAKE_C_FLAGS_RELWITHDEBINFO}")
message(STATUS "")
message(STATUS "")
message(STATUS "CXX_FLAGS:=${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG:=${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE:=${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "CMAKE_CXX_FLAGS_MINSIZEREL:=${CMAKE_CXX_FLAGS_MINSIZEREL}")
message(STATUS "CMAKE_CXX_FLAGS_RELWITHDEBINFO:=${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
