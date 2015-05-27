
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include "ext.h"


Ext *extension;


#ifdef __GNUC__
	#include <dlfcn.h>
	// Code for GNU C compiler
	static void __attribute__((constructor))
	extension_init(void)
	{
		std::unordered_map<std::string, std::string> options;

		FILE *fh = fopen ("/proc/self/cmdline", "r"); // /proc/self  :D
		if (fh != NULL)
		{
			char *arg = 0;
			size_t size = 0;
			while(getdelim(&arg, &size, 0, fh) != -1)
			{
				std::string argument_str(arg);
				boost::erase_all(argument_str, "\"");
				if (boost::algorithm::istarts_with(argument_str, "-extDB2_VAR="))
				{
					options["VAR"] = argument_str.substr(argument_str.find("=") + 1);
				}
				else if (boost::algorithm::istarts_with(argument_str, "-extDB2_WORK="))
				{
					options["WORK"] = argument_str.substr(argument_str.find("=") + 1);
				}
				else if (boost::algorithm::istarts_with(argument_str, "-bepath="))
				{
					options["BEPATH"] = argument_str.substr(argument_str.find("=") + 1);
				}
			}
			free(arg);
		};
		fclose(fh);

		Dl_info dl_info;
		dladdr((void*)extension_init, &dl_info);
		extension = new Ext(boost::filesystem::path (dl_info.dli_fname).string(), options);
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
		extension->callExtension(output, outputSize, function);
	};


#elif _MSC_VER
	// Code for MSVC compiler
	//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers   // Now Defined VIA CMake Build System
	#include <windows.h>
	#include <atlstr.h>
	#include <shellapi.h>

	EXTERN_C IMAGE_DOS_HEADER __ImageBase;

	BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
	{
		switch (ul_reason_for_call)
		{
			case DLL_PROCESS_ATTACH:
				{
					int nArgs;
					LPWSTR *pszArgsW = CommandLineToArgvW(GetCommandLineW(), &nArgs);
					std::unordered_map<std::string, std::string> options;

					if (nArgs != NULL)
					{
						std::string argument_str;
						for (int i = 0; i < nArgs; i++)
						{
							argument_str = CW2A(pszArgsW[i]);
							boost::erase_all(argument_str, "\"");
							if (boost::algorithm::istarts_with(argument_str, "-extDB2_VAR="))
							{
								options["VAR"] = argument_str.substr(argument_str.find("=") + 1);
							}
							else if (boost::algorithm::istarts_with(argument_str, "-extDB2_WORK="))
							{
								options["WORK"] = argument_str.substr(argument_str.find("=") + 1);
							}
							else if (boost::algorithm::istarts_with(argument_str, "-bepath="))
							{
								options["BEPATH"] = argument_str.substr(argument_str.find("=") + 1);
							}
						}
					}

					WCHAR path[MAX_PATH + 1];
					GetModuleFileNameW((HINSTANCE)&__ImageBase, path, (MAX_PATH + 1));
					extension = new Ext(boost::filesystem::path(path).string(), options);
				}
				break;
			case DLL_PROCESS_DETACH:
				extension->stop();
				break;
		}
		return true;
	}

	extern "C"
	{
		__declspec(dllexport) void __stdcall RVExtension(char *output, int outputSize, const char *function); 
	};

	void __stdcall RVExtension(char *output, int outputSize, const char *function)
	{
		outputSize -= 1;
		extension->callExtension(output, outputSize, function);
	};
#endif
