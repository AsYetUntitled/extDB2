
#include <boost/algorithm/string.hpp>
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
		bool status = true;
		std::unordered_map<std::string, std::string> options;

		FILE *fh = fopen ("/proc/self/cmdline", "r"); // /proc/self  :D
		if (!fh)
		{
			status = false;
		}
		else
		{
			std::size_t found;
   			char *arg = 0;
   			size_t size = 0;
   			while(getdelim(&arg, &size, 0, fh) != -1)
   			{
      			std::string argument_str(arg);
      			boost::erase_all(argument_str, "\"");
				if (argument_str.size() >= 12)
				{
					found = argument_str.find("-extDB2_VAR=");
					if (found == 0)
					{
						options["VAR"] = argument_str.substr(12);
					}
					else
					{
						found = argument_str.find("-extDB2_WORK=");
						if (found == 0)
						{
							options["WORK"] = argument_str.substr(13);
						}
					}
				}
   			}
   			free(arg);
		};
		fclose(fh);

		Dl_info dl_info;
		dladdr((void*)extension_init, &dl_info);
		extension = new Ext(boost::filesystem::path (dl_info.dli_fname).string(), options, status);
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
					bool status = true;
					int nArgs;
					LPWSTR *pszArgsW = CommandLineToArgvW(GetCommandLineW(), &nArgs);
					std::unordered_map<std::string, std::string> options;

					if (nArgs == NULL)
					{
						status = false;
					}
					else
					{
						std::size_t found;
						std::string argument_str;
						for (int i = 0; i < nArgs; i++)
						{
							argument_str = CW2A(pszArgsW[i]);
							boost::erase_all(argument_str, "\"");
							if (argument_str.size() >= 12)
							{
								found = argument_str.find("-extDB2_VAR=");
								if (found == 0)
								{
									options["VAR"] = argument_str.substr(12);
								}
								else
								{
									found = argument_str.find("-extDB2_WORK=");
									if (found == 0)
									{
										options["WORK"] = argument_str.substr(13);
									}
								}
							}
						}
					}

					WCHAR path[MAX_PATH + 1];
					GetModuleFileNameW((HINSTANCE)&__ImageBase, path, (MAX_PATH + 1));
					extension = new Ext(boost::filesystem::path(path).string(), options, status);
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
