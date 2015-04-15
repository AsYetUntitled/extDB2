
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

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
		std::vector<std::string> args;

		FILE *fh = fopen ("/proc/self/cmdline", "r"); // /proc/self  :D
		if (!fh)
		{
			status = false;
		}
		else
		{
			char buffer[4096];
			while (fgets (buffer, 4096, fh))
			{
				args.push_back(buffer);
			};
		};
		fclose (fh);
		boost::program_options::options_description desc("Options");
		desc.add_options()
			("extDB2-var", boost::program_options::value<std::string>(), "extDB2 Variable")
			("extDB2-conf", boost::program_options::value<std::string>(), "extDB2 Config File")
			("extDB2-work", boost::program_options::value<std::string>(), "extDB2 Work Directory");
		boost::program_options::parsed_options options = boost::program_options::command_line_parser(args).options(desc).allow_unregistered().run();


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
					std::vector< std::string > args;
					args.reserve(nArgs - 1);

					if (nArgs == NULL)
					{
						status = false;
					}
					else
					{
						for (int i = 0; i < nArgs; i++)
						{
							std::string arg = CW2A(pszArgsW[i]);
							args.push_back(std::move(arg));
						};
					}
					boost::program_options::options_description desc("Options");
					desc.add_options()
						("extDB2-var", boost::program_options::value<std::string>(), "extDB2 Variable")
						("extDB2-conf", boost::program_options::value<std::string>(), "extDB2 Config File")
						("extDB2-work", boost::program_options::value<std::string>(), "extDB2 Work Directory");
					boost::program_options::parsed_options options = boost::program_options::command_line_parser(args).options(desc).allow_unregistered().run();

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
