if(MSVC)
	set(CMAKE_CXX_FLAGS_DEBUG_INIT "/D _WINDLL /MTd /Zi /WX- /Oy- /EHsc /GS- /arch:SSE2 /fp:precise /Od /RTC1 /D_DEBUG")
	set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT       "/MT /WX- /O1 /Oy- /EHsc /GS- /arch:SSE2 /fp:precise /D NDEBUG")
	set(CMAKE_CXX_FLAGS_RELEASE_INIT          "/MT /WX- /O2 /Oy- /EHsc /GS- /arch:SSE2 /fp:precise /D NDEBUG")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT   "/MT /Zi /O2 /Oy- /EHsc /GS-  /arch:SSE2 /fp:precise /D NDEBUG")
else()
	set(CMAKE_CXX_FLAGS_DEBUG_INIT 			  "-march=i686 -msse2 -msse3 -fPIC -m32 -O2")
	set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT       "-march=i686 -msse2 -msse3 -fPIC -m32 -O2")
	set(CMAKE_CXX_FLAGS_RELEASE_INIT          "-march=i686 -msse2 -msse3 -fPIC -m32 -O2")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT   "-march=i686 -msse2 -msse3 -fPIC -m32 -O2")
endif()