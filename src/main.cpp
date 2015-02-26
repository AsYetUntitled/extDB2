#include <boost/filesystem.hpp>

#include "ext.h"

namespace
{
	Ext *extension;
};

#ifdef __GNUC__
	#include <dlfcn.h>
	// Code for GNU C compiler
	static void __attribute__((constructor))
	extension_init(void)
	{
		Dl_info dl_info;
		dladdr((void*)extension_init, &dl_info);
		extension = (new Ext(boost::filesystem::path (dl_info.dli_fname).string()));
	}

	static void __attribute__((destructor))
	extension_destroy(void)
	{
		extension->stop();
	}

	extern "C"
	{
		void RVExtension(char *output, int outputSize, const char *function); 
	};

	void RVExtension(char *output, int outputSize, const char *function)
	{
		outputSize -= 1;
		extension->callExtenion(output, outputSize, function);
	};

#elif _MSC_VER
	// Code for MSVC compiler
	//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers   // Now Defined VIA CMake Build System
	#include <windows.h>

	EXTERN_C IMAGE_DOS_HEADER __ImageBase;

	BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
	{
		switch (ul_reason_for_call)
		{
			case DLL_PROCESS_ATTACH:
				WCHAR path[MAX_PATH+1];
				GetModuleFileNameW ((HINSTANCE)&__ImageBase, path, (MAX_PATH + 1));
				extension = new Ext(boost::filesystem::path (path).string());
				break;
			case DLL_PROCESS_DETACH:
				extension->stop();
				break;
		}
		return TRUE;
	}

	extern "C"
	{
		__declspec(dllexport) void __stdcall RVExtension(char *output, int outputSize, const char *function); 
	};

	void __stdcall RVExtension(char *output, int outputSize, const char *function)
	{
		outputSize -= 1;
		extension->callExtenion(output,outputSize,function);
	};
#endif
