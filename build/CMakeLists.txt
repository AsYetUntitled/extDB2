#Change this if you need to target a specific CMake version

cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0015 NEW)

set(CMAKE_USER_MAKE_RULES_OVERRIDE
   ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/c_flag_overrides.cmake)
set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX
   ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/cxx_flag_overrides.cmake)
project(extDB2)

set(CMAKE_SUPPRESS_REGENERATION 1)
SET(CMAKE_BUILD_TYPE "Release")
set(CMAKE_CONFIGURATION_TYPES Release CACHE TYPE  
INTERNAL FORCE )
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules" ${CMAKE_MODULE_PATH})

#Check for 32bit vs. 64bit system
if (CMAKE_SIZEOF_VOID_P EQUAL 8) #64bit
	message(STATUS "Warning: You are on a 64-bit system, make sure you are pulling in the appropriate 32-bit libraries.")
elseif (CMAKE_SIZEOF_VOID_P EQUAL 4) #32bit
	message(STATUS "You are on a 32-bit system.")
else()
	message(STATUS "CMake wasn't able to identify if you are on 32-bit or 64-bit...")
endif()

# Logging defaults to OFF
SET(DEBUG_LOGGING FALSE CACHE BOOL "Enables or disables Debug Logging.")
# Compile test application defaults to OFF
SET(COMPILE_TEST_APPLICATION FALSE CACHE BOOL "Compiles a test application of extDB2.")
# Testing defaults to OFF
SET(TESTING FALSE CACHE BOOL "Sets the testing flag for extDB2.")
# Test RCON defaults to OFF
SET(COMPILE_RCON_APPLICATION FALSE CACHE BOOL "Enables or disables testing of RCON.")
# Test sanitize defaults to OFF
SET(COMPILE_TEST_SANITIZE_APPLICATION FALSE CACHE BOOL "Enables or disables testing of sanitization.")


SET(SOURCES
	../src/memory_allocator.cpp
	../src/ext.cpp
	../src/sanitize.cpp
	../src/rconworker.cpp
	../src/redisworker.cpp
	../src/remoteserver.cpp
	../src/steamworker.cpp
	../src/protocols/abstract_protocol.cpp
	../src/protocols/redis_raw.cpp
	../src/protocols/sql_custom.cpp
	../src/protocols/sql_raw.cpp
	../src/protocols/log.cpp
	../src/protocols/misc.cpp
	../src/protocols/rcon.cpp
	../src/protocols/steam.cpp
)

if (COMPILE_TEST_APPLICATION)
	set(EXECUTABLE_NAME "extDB2-test")
	add_executable(${EXECUTABLE_NAME} ${SOURCES})
	add_definitions(-DTEST_APP)
	message(STATUS "The extDB2 test application will be compiled.")
elseif (COMPILE_TEST_SANITIZE_APPLICATION)
	SET(SOURCES ../src/sanitize.cpp) # Override Sources
	set(EXECUTABLE_NAME "extDB2-sanitize")
	add_executable(${EXECUTABLE_NAME} ${SOURCES})
	add_definitions(-DTEST_SANITIZE_APP)
	message(STATUS "Sanitization testing is enabled.")	
elseif (COMPILE_RCON_APPLICATION)
	SET(SOURCES ../src/rconworker.cpp) # Override Sources
	set(EXECUTABLE_NAME "extDB2-rcon")
	add_executable(${EXECUTABLE_NAME} ${SOURCES})
	add_definitions(-DRCON_APP)
	message(STATUS "RCON testing is enabled.")
else()
	LIST(APPEND SOURCES ../src/main.cpp)  # Add main.cpp for library build
	set(EXECUTABLE_NAME "extDB2")
	add_library (${EXECUTABLE_NAME} SHARED ${SOURCES})
endif()

if (DEBUG_LOGGING)
	add_definitions(-DDEBUG_LOGGING)
	message(STATUS "Debug Logging is enabled.")
else()
	message(STATUS "Debug Logging is disabled.")
endif()

if (TESTING)
	add_definitions(-DTESTING)
	message(STATUS "Testing flag is enabled.")
endif()

if(${WIN32})
else()
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()


# BOOST
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)
if(${WIN32})
	add_definitions(-D _WIN32_WINNT=0x0600)
endif()

find_package(Boost REQUIRED COMPONENTS chrono date_time filesystem program_options random regex system thread)
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
	message(FATAL_ERROR "\nMYSQL not found")
endif()

if(${WIN32})
else()
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".so")
endif()

# Look for Intel Threading Building Blocks (TBB)
include(FindTBB)
if(TBB_FOUND)
	include_directories(${TBB_INCLUDE_DIRS})
else()
	message(FATAL_ERROR "\nIntel TBB not found\n Please Set TBB_ROOT")
endif()



if(${WIN32})
	# Windows
	target_link_libraries(${EXECUTABLE_NAME} ${Boost_LIBRARIES}  ${POCO_LIBRARIES} ${MYSQL_LIBRARY} ${TBB_MALLOC_LIBRARY})
	
	add_definitions(-DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN)
	SET_TARGET_PROPERTIES(${EXECUTABLE_NAME} PROPERTIES LINK_FLAGS " /MANIFEST:NO /ERRORREPORT:NONE")
else()
	# Linux 
	target_link_libraries(${EXECUTABLE_NAME} -Wl,-Bstatic ${Boost_LIBRARIES} ${POCO_LIBRARIES} ${MYSQL_LIBRARY}  -Wl,-Bdynamic -ldl -pthread ${TBB_MALLOC_LIBRARY})
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
