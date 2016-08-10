// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "azure_c_shared_utility/gballoc.h"

#include "module.h"
#include "message.h"
#include "azure_c_shared_utility/iot_logging.h"
#include "azure_c_shared_utility/base64.h"

#ifdef UNDER_TEST
#define SafeArrayCreateVector myTest_SafeArrayCreateVector
#define SafeArrayPutElement   myTest_SafeArrayPutElement
#define SafeArrayDestroy      myTest_SafeArrayDestroy
#define SafeArrayCreate       myTest_SafeArrayCreate
#define SafeArrayAccessData   myTest_SafeArrayAccessData
#define SafeArrayUnaccessData myTest_SafeArrayUnaccessData
#endif

#include "dotnetcore.h"
#include "coreclrhost.h"
	
static const char* AzureIotGatewayAsssemblyName = "Microsoft.Azure.IoT.Gateway";
static const char* ModuleManagerTypeName = "Microsoft.Azure.IoT.Gateway.GatewayHost";
static const char* ModuleManagerCreateMethodName = "Create";
static const char* ModuleManagerReceiveMethodName = "Receive";
static const char* ModuleManagerDestroyMethodName = "Destroy";
// TODO
static const int InvalidCoreClr = -1;
static const int FailedToLoadCoreClr = -1;
// TODO: Header?
#if defined(__WIN32__)
static const char* PathSeparator = ";";
static const char* DirectorySeparator = "\\";
#else
static const char* PathSeparator = ":";
static const char* DirectorySeparator = "/";
#endif

static const char* serverGcVar = "CORECLR_SERVER_GC";

#if defined(__linux__)
#define symlinkEntrypointExecutable "/proc/self/exe"
#elif !defined(__APPLE__)
#define symlinkEntrypointExecutable "/proc/curproc/exe"
#endif

struct DOTNETCORE_HOST_HANDLE_DATA
{
	DOTNETCORE_HOST_HANDLE_DATA()
	{

	};

    MESSAGE_BUS_HANDLE          bus;

	void*                       hostHandle;
	unsigned int                domainId;
	module_manager_create_ptr   moduleManagerCreateDelegate;
	module_manager_destroy_ptr  moduleManagerDestroyDelegate;
	module_manager_receive_ptr  moduleManagerReceiveDelegate;

private:
	DOTNET_HOST_HANDLE_DATA& operator=(const DOTNET_HOST_HANDLE_DATA&);
};

typedef (int)(*module_manager_create_ptr)(DOTNETCORE_HOST_HANDLE_DATA* moduleHandle, DOTNETCORE_HOST_CONFIG* configHandle)
typedef (int)(*module_manager_receive_ptr)(DOTNETCORE_HOST_HANDLE_DATA* moduleHandle, MESSAGE_HANDLE messageHandle)
typedef (int)(*module_manager_destroy_ptr)(DOTNETCORE_HOST_HANDLE_DATA* moduleHandle)

static void* g_singletonLibraryHandle;
static void* g_singletonHostHandle;
static unsigned int g_domainId;

#if defined(__WIN32__)
LPCWSTR ConvertUtf8ToUtf16LEWithErrorLogging(const char* utf8)
{
    int chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, NULL, 0);
    LPWSTR* buffer = malloc(chars * 2);
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, buffer, chars))
    {
        int error = GetLastError();
        LogError(, "MultiByteToWideChar failed w/hr 0x8007%04lx%8l\n", error);
        free(buffer);
        return NULL;
    }

    return buffer;
}

const char* ConvertUtf16ToUtf8LEWithErrorLogging(LPCWSTR utf16)
{
    int bytes = WideCharToMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS, utf16, -1, NULL, 0, NULL);
    char* buffer = malloc(bytes);
    if (WideCharToMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS, utf16, -1, buffer, bytes, NULL))
    {
        int error = GetLastError();
        LogError(, "WideCharToMultiByte failed w/hr 0x8007%04lx%8l\n", error);
        free(buffer);
        return NULL;
    }

    return buffer;
}
#endif

void* LoadCoreClrWithErrorLogging(const char* path)
#if defined(__WIN32__)
{
	HMODULE result = NULL;

    LPCWSTR pathUtf16 = ConvertUtf8ToUtf16LEWithErrorLogging(oath);
    if(pathUtf16 != NULL)
    {
        result = LoadLibraryW(path);
        if( result == NULL)
        {
            int error = GetLastError();
            LogError(, "LoadLibrary failed to open the %s w/hr 0x8007%04lx%8l\n", path, error);
        }
    }

    return result;
}
#else
{
	void* result = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (result == nullptr)
	{
        char* error = dlerror();
        LogError(, "dlopen failed to open the %s with error %s\n", coreClrDll, error);
	}
	
	return result;
}
#endif // WIN32

bool GetEntrypointExecutableAbsolutePath(std::string& entrypointExecutable)
#if defined(__WIN32__)
{
    bool result = false;
    entrypointExecutable.clear();
    int chars = GetModuleFileNameW(NULL, NULL, 0);
    LPWSTR pathUtf16 = malloc(chars * 2);
    if (GetModuleFileNameW(NULL, pathUtf16, pathUtf16))
    {
        int error = GetLastError();
        LogError(, "GetModuleFileNameW(NULL) failed w/hr 0x8007%04lx%8l\n", error);
    }
    else
    {
        const char* path = ConvertUtf16ToUtf8LEWithErrorLogging(pathUtf16);
        entrypointExecutable.assign(path);
        free(path);
        free(pathUtf16);
    }

    return result;
}
#else
{
// from https://github.com/dotnet/coreclr/blob/master/src/coreclr/hosts/unixcoreruncommon/coreruncommon.cpp
    bool result = false;
    
    entrypointExecutable.clear();

    // Get path to the executable for the current process using
    // platform specific means.
#if defined(__linux__) || (defined(__NetBSD__) && !defined(KERN_PROC_PATHNAME))
    // On Linux, fetch the entry point EXE absolute path, inclusive of filename.
    char exe[PATH_MAX];
    ssize_t res = readlink(symlinkEntrypointExecutable, exe, PATH_MAX - 1);
    if (res != -1)
    {
        exe[res] = '\0';
        entrypointExecutable.assign(exe);
        result = true;
    }
    else
    {
        result = false;
    }
#elif defined(__APPLE__)
    
    // On Mac, we ask the OS for the absolute path to the entrypoint executable
    uint32_t lenActualPath = 0;
    if (_NSGetExecutablePath(nullptr, &lenActualPath) == -1)
    {
        // OSX has placed the actual path length in lenActualPath,
        // so re-attempt the operation
        std::string resizedPath(lenActualPath, '\0');
        char *pResizedPath = const_cast<char *>(resizedPath.c_str());
        if (_NSGetExecutablePath(pResizedPath, &lenActualPath) == 0)
        {
            entrypointExecutable.assign(pResizedPath);
            result = true;
        }
    }
#elif defined (__FreeBSD__)
    static const int name[] = {
        CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1
    };
    char path[PATH_MAX];
    size_t len;

    len = sizeof(path);
    if (sysctl(name, 4, path, &len, nullptr, 0) == 0)
    {
        entrypointExecutable.assign(path);
        result = true;
    }
    else
    {
        // ENOMEM
        result = false;
    }
#elif defined(__NetBSD__) && defined(KERN_PROC_PATHNAME)
    static const int name[] = {
        CTL_KERN, KERN_PROC_ARGS, -1, KERN_PROC_PATHNAME,
    };
    char path[MAXPATHLEN];
    size_t len;

    len = sizeof(path);
    if (sysctl(name, __arraycount(name), path, &len, NULL, 0) != -1)
    {
        entrypointExecutable.assign(path);
        result = true;
    }
    else
    {
        result = false;
    }
#else
    // On non-Mac OS, return the symlink that will be resolved by GetAbsolutePath
    // to fetch the entrypoint EXE absolute path, inclusive of filename.
    entrypointExecutable.assign(symlinkEntrypointExecutable);
    result = true;
#endif 

    return result;
}
#endif // !WIN32

void AddFilesFromDirectoryToTpaList(const char* directory, std::string& tpaList)
#if defined(__WIN32__)
{
     const wchar_t * const tpaExtensions[] = {
                L".ni.dll",      // Probe for .ni.dll first so that it's preferred if ni and il coexist in the same dir
                L".dll",
                L".ni.exe",
                L".exe",
                };
    const int countExtensions = 4;

    // from https://github.com/dotnet/coreclr/blob/master/src/coreclr/hosts/corerun/corerun.cpp
    
    // const wchar_t* targetPath, wchar_t** rgTPAExtensions, int countExtensions)

    LPCWSTR targetPath = ConvertUtf8ToUtf16LEWithErrorLogging(directory);
    std::wstring assemblyPath = new std::wstring();
    const size_t dirLength = wcslen(targetPath);

    for (int iExtension = 0; iExtension < countExtensions; iExtension++)
    {
        assemblyPath.clear();
        assemblyPath.assign(targetPath, dirLength);
        assemblyPath.append(tpaExtensions[iExtension]);

        WIN32_FIND_DATA data;
        HANDLE findHandle = WszFindFirstFile(assemblyPath.c_str(), &data);

        if (findHandle != INVALID_HANDLE_VALUE) {
            do {
                if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    // It seems that CoreCLR doesn't always use the first instance of an assembly on the TPA list (ni's may be preferred
                    // over il, even if they appear later). So, only include the first instance of a simple assembly name to allow
                    // users the opportunity to override Framework assemblies by placing dlls in %CORE_LIBRARIES%

                    // ToLower for case-insensitive comparisons
                    wchar_t* fileNameChar = data.cFileName;
                    while (*fileNameChar)
                    {
                        *fileNameChar = towlower(*fileNameChar);
                        fileNameChar++;
                    }

                    // Remove extension
                    wchar_t fileNameWithoutExtension[MAX_PATH_FNAME];
                    wcscpy_s(fileNameWithoutExtension, MAX_PATH_FNAME, data.cFileName);

                    RemoveExtensionAndNi(fileNameWithoutExtension);

                    // Add to the list if not already on it
                    if (!TPAListContainsFile(fileNameWithoutExtension, rgTPAExtensions, countExtensions))
                    {
                        assemblyPath.truncate(assemblyPath.begin() + (DWORD)dirLength);
                        assemblyPath.append(data.cFileName);
                        const char* assemblyPathUtf8 = ConvertUtf16ToUtf8LEWithErrorLogging(assemblyPath.c_str());
                        tpaList.append(assemblyPathUtf8);
                        free(assemblyPathUtf8);
                        tpaList.append(';');
                    }
                    // TODO: LogWarn?
                    // else
                    // {
                    //     *m_log << W("Not adding ") << targetPath << data.cFileName << W(" to the TPA list because another file with the same name is already present on the list") << Logger::endl;
                    // }
                }
            } while (0 != WszFindNextFile(findHandle, &data));

            FindClose(findHandle);
        }
    }

    delete assemblyPath;
    free(targetPath);
}
// WIN32
#else
{
    // from https://github.com/dotnet/coreclr/blob/master/src/coreclr/hosts/unixcoreruncommon/coreruncommon.cpp
    // TODO: multibyte?
    const char * const tpaExtensions[] = {
                ".ni.dll",      // Probe for .ni.dll first so that it's preferred if ni and il coexist in the same dir
                ".dll",
                ".ni.exe",
                ".exe",
                };

    DIR* dir = opendir(directory);
    if (dir == nullptr)
    {
        return;
    }

    std::set<std::string> addedAssemblies;

    // Walk the directory for each extension separately so that we first get files with .ni.dll extension,
    // then files with .dll extension, etc.
    for (int extIndex = 0; extIndex < sizeof(tpaExtensions) / sizeof(tpaExtensions[0]); extIndex++)
    {
        const char* ext = tpaExtensions[extIndex];
        int extLength = strlen(ext);

        struct dirent* entry;

        // For all entries in the directory
        // TODO: Windows
        while ((entry = readdir(dir)) != nullptr)
        {
            // We are interested in files only
            switch (entry->d_type)
            {
            case DT_REG:
                break;

            // Handle symlinks and file systems that do not support d_type
            case DT_LNK:
            case DT_UNKNOWN:
                {
                    std::string fullFilename;

                    fullFilename.append(directory);
                    fullFilename.append("/");
                    fullFilename.append(entry->d_name);

                    struct stat sb;
                    if (stat(fullFilename.c_str(), &sb) == -1)
                    {
                        continue;
                    }

                    if (!S_ISREG(sb.st_mode))
                    {
                        continue;
                    }
                }
                break;

            default:
                continue;
            }

            std::string filename(entry->d_name);

            // Check if the extension matches the one we are looking for
            int extPos = filename.length() - extLength;
            if ((extPos <= 0) || (filename.compare(extPos, extLength, ext) != 0))
            {
                continue;
            }

            std::string filenameWithoutExt(filename.substr(0, extPos));

            // Make sure if we have an assembly with multiple extensions present,
            // we insert only one version of it.
            if (addedAssemblies.find(filenameWithoutExt) == addedAssemblies.end())
            {
                addedAssemblies.insert(filenameWithoutExt);

                tpaList.append(directory);
                tpaList.append("/");
                tpaList.append(filename);
                tpaList.append(":");
            }
        }
        
        // Rewind the directory stream to be able to iterate over it for the next extension
        rewinddir(dir);
    }
    
    closedir(dir);
}
#endif // !WIN32

bool initializeCoreCLR(DOTNETCORE_HOST_CONFIG* config, DOTNETCORE_HOST_HANDLE** handle)
{
	if (g_singletonHostHandle != nullptr)
	{
		handle->hostHandle = g_singletonHostHandle;
        handle->domainId = g_domainId;
		return true;
	}


    // Get just the path component of the managed assembly path
    std::string appPath;
    GetDirectory(config->dotnetcore_module_path, appPath);

    std::string coreClrLibPath;
    if(config->core_clr_path == nullptr)
    {
	    coreClrLibPath(appPath);
    }
    else
	{
	    coreClrLibPath(config->core_clr_path);
    }
    
    coreClrLibPath.append(DirectorySeparator);
    coreClrLibPath.append(coreClrDll);

    if (coreClrDllPath.length() >= PATH_MAX)
    {
        LogError("Absolute path to %s too long\n", coreClrDll);
        return false;
    }


    // Construct native search directory paths
    std::string nativeDllSearchDirs(appPath);
    char* coreLibraries = getenv("CORE_LIBRARIES");
    if (coreLibraries)
    {
        nativeDllSearchDirs.append(PathSepartor);
        nativeDllSearchDirs.append(coreLibraries);
    }
    nativeDllSearchDirs.append(PathSepartor);
    nativeDllSearchDirs.append(config->clr_files_absolute_path);

    std::string tpaList;
    AddFilesFromDirectoryToTpaList(clrFilesAbsolutePath, tpaList);
    tpaList.append(PathSeparator);
    tpaList.append(AZUREIOTGATEWAYASSEMBLYNAME);

	void* coreclrLib;

	if (g_singletonLibraryHandle == nullptr)
	{
		coreclrLib = LoadCoreClrWithErrorLogging(coreClrDllPath.c_str());
		g_singletonLibraryHandle = coreclrLib;
	}
	else
	{
	// TODO: thread-safety?
		coreclrLib = g_singletonLibraryHandle;
	}

    bool result = false;
    if (coreclrLib != nullptr)
    {
// TODO: Windows
        coreclr_initialize_ptr initializeCoreCLR = (coreclr_initialize_ptr)dlsym(coreclrLib, "coreclr_initialize");
// TODO: Windows
		coreclr_create_delegate_ptr createDelegateCoreClr = (coreclr_create_delegate_ptr)dlsym(coreclrLib, "coreclr_create_delegate");

        if (initializeCoreCLR == nullptr)
        {
            LogError("Function coreclr_initialize not found in the %s\n", coreClrDll);
        }
        else if (createDelegateCoreClr == nullptr)
        {
            LogError("Function coreclr_create_delegate not found in the %s\n", coreClrDll);
        }
        else
        {
        	handle->createDelegate = createDelegateCoreClr;
            // Check whether we are enabling server GC (off by default)
            const char* useServerGc = std::getenv(serverGcVar);
            if (useServerGc == nullptr)
            {
                useServerGc = "0";
            }

            // CoreCLR expects strings "true" and "false" instead of "1" and "0".
            useServerGc = std::strcmp(useServerGc, "1") == 0 ? "true" : "false";

            // Allowed property names:
            // APPBASE
            // - The base path of the application from which the exe and other assemblies will be loaded
            //
            // TRUSTED_PLATFORM_ASSEMBLIES
            // - The list of complete paths to each of the fully trusted assemblies
            //
            // APP_PATHS
            // - The list of paths which will be probed by the assembly loader
            //
            // APP_NI_PATHS
            // - The list of additional paths that the assembly loader will probe for ngen images
            //
            // NATIVE_DLL_SEARCH_DIRECTORIES
            // - The list of paths that will be probed for native DLLs called by PInvoke
            //
            const char *propertyKeys[] = {
                "TRUSTED_PLATFORM_ASSEMBLIES",
                "APP_PATHS",
                "APP_NI_PATHS",
                "NATIVE_DLL_SEARCH_DIRECTORIES",
                "AppDomainCompatSwitch",
                "System.GC.Server",
            };
            const char *propertyValues[] = {
                // TRUSTED_PLATFORM_ASSEMBLIES
                tpaList.c_str(),
                // APP_PATHS
                appPath.c_str(),
                // APP_NI_PATHS
                appPath.c_str(),
                // NATIVE_DLL_SEARCH_DIRECTORIES
                nativeDllSearchDirs.c_str(),
                // AppDomainCompatSwitch
                "UseLatestBehaviorWhenTFMNotSpecified",
                // System.GC.Server
                useServerGc,
            };

            void* hostHandle;
            unsigned int domainId;
            std::string executablePath = new std::string();
            if (!GetEntrypointExecutableAbsolutePath(executablePath))
            {
                LogError("Failed to get entry point path.");
            }
            else
            {
                int status = 
                    initializeCoreCLR(
                        config->dotnetcore_module_path, 
                        "IoT Gateway SDK .NET Core Hosting Module", 
                        sizeof(propertyKeys) / sizeof(propertyKeys[0]), 
                        propertyKeys, 
                        propertyValues, 
                        &hostHandle, 
                        &domainId);
                if (FAILED(status)) 
                {
                    LogError( "Failed to initialize Core CLR w/hr 0x%08l", status);
                }
                else
                {
                    g_singletonHostHandle = hostHandle;
                    g_domainId = domainId;
                    handle->hostHandle = hostHandle;
                    handle->domainId = domainId;
                    result = true;
                }
            }

            delete executablePath;
        }
    }
    
    return status;
}


static MODULE_HANDLE DotNETCore_Create(MESSAGE_BUS_HANDLE busHandle, const void* configuration)
{
	DOTNETCORE_HOST_HANDLE_DATA* out = nullptr;

	try
	{
		if (
			(busHandle == nullptr) ||
			(configuration == nullptr)
			)
		{
			/* Codes_SRS_DOTNET_04_001: [ DotNET_Create shall return NULL if bus is NULL. ] */
			/* Codes_SRS_DOTNET_04_002: [ DotNET_Create shall return NULL if configuration is NULL. ] */
			LogError("invalid arg busHandle=%p, configuration=%p", busHandle, configuration);
		}
		else
		{
			DOTNETCORE_HOST_CONFIG* dotNetCoreConfig = (DOTNETCORE_HOST_CONFIG*)configuration;
			if (dotNetCoreConfig->dotnetcore_module_path == nullptr)
			{
				/* Codes_SRS_DOTNET_04_003: [ DotNET_Create shall return NULL if configuration->dotnet_module_path is NULL. ] */
				LogError("invalid configuration. dotnetcore_module_path=%p", dotNetCoreConfig->dotnetcore_module_path);
			}
			else if (dotNetCoreConfig->dotnetcore_module_entry_class == nullptr)
			{
				/* Codes_SRS_DOTNET_04_004: [ DotNET_Create shall return NULL if configuration->dotnet_module_entry_class is NULL. ] */
				LogError("invalid configuration. dotnetcore_module_entry_class=%p", dotNetCoreConfig->dotnetcore_module_entry_class);
			}
			else if (dotNetCoreConfig->dotnetcore_module_args == nullptr)
			{
				/* Codes_SRS_DOTNET_04_005: [ DotNET_Create shall return NULL if configuration->dotnet_module_args is NULL. ] */
				LogError("invalid configuration. dotnetcore_module_args=%p", dotNetCoreConfig->dotnetcore_module_args);
			}
			else if (dotNetCoreConfig->clr_files_absolute_path == nullptr)
			{
				/* Codes_SRS_DOTNET_04_005: [ DotNET_Create shall return NULL if configuration->dotnet_module_args is NULL. ] */
				LogError("invalid configuration. clr_files_absolute_path=%p", dotNetCoreConfig->clr_files_absolute_path);
			}
			else
			{
				/* Codes_SRS_DOTNET_04_006: [ DotNET_Create shall return NULL if an underlying API call fails. ] */
				int hr;
				try
				{
					/* Codes_SRS_DOTNET_04_008: [ DotNET_Create shall allocate memory for an instance of the DOTNET_HOST_HANDLE_DATA structure and use that as the backing structure for the module handle. ] */
					out = new DOTNETCORE_HOST_HANDLE_DATA();
				}
				catch (std::bad_alloc)
				{
					//Do not need to do anything, since we are returning NULL below.
					LogError("Memory allocation failed for DOTNETCORE_HOST_HANDLE_DATA.");
				}

				if (out != nullptr)
				{
					/* Codes_SRS_DOTNET_04_007: [ DotNET_Create shall return a non-NULL MODULE_HANDLE when successful. ] */
					out->bus = busHandle;
					module_manager_create_ptr moduleManagerCreate;
					
					/* Codes_SRS_DOTNET_04_012: [ DotNET_Create shall get the 3 CLR Host Interfaces (CLRMetaHost, CLRRuntimeInfo and CorRuntimeHost) and save it on DOTNET_HOST_HANDLE_DATA. ] */
					/* Codes_SRS_DOTNET_04_010: [ DotNET_Create shall save Client module Type and Azure IoT Gateway Assembly on DOTNET_HOST_HANDLE_DATA. ] */
					if (FAILED(hr = initializeCoreCLR(dotNetCoreConfig, &out)))
					{
						/* Codes_SRS_DOTNET_04_006: [ DotNET_Create shall return NULL if an underlying API call fails. ] */
						LogError("Error initializing CoreCLR.");
						delete out; 
						out = nullptr;
					}
					else if (FAILED(hr = out->createDelegate(out->hostHandle, out->domainId, AzureIotGatewayAssemblyName, ModuleManagerTypeName, ModuleManagerCreateMethodName, &moduleManagerCreate)))
					{
						/* Codes_SRS_DOTNET_04_006: [ DotNET_Create shall return NULL if an underlying API call fails. ] */
						LogError("Failed to get the delegate for ModuleManager.Create w/hr 0x%08lx\n", hr);
						delete out;
						out = nullptr;
					}
					else if (FAILED(hr = out->createDelegate(out->hostHandle, out->domainId, AzureIotGatewayAssemblyName, ModuleManagerTypeName, ModuleManagerReceiveMethodName, &moduleManagerReceive)))
					{
						/* Codes_SRS_DOTNET_04_006: [ DotNET_Create shall return NULL if an underlying API call fails. ] */
						LogError("Failed to get the delegate for ModuleManager.Receive w/hr 0x%08lx\n", hr);
						delete out;
						out = nullptr;
					}
					else if (FAILED(hr = out->createDelegate(out->hostHandle, out->domainId, AzureIotGatewayAssemblyName, ModuleManagerTypeName, ModuleManagerDestroyMethodName, &moduelManagerDestroy)))
					{
						/* Codes_SRS_DOTNET_04_006: [ DotNET_Create shall return NULL if an underlying API call fails. ] */
						LogError("Failed to get the delegate for ModuleManager.Destroy w/hr 0x%08lx\n", hr);
						delete out;
						out = nullptr;
					}
					else
					{
						moduleManagerCreate(out, config);
						out->moduleManagerCreateDelegate = moduleManagerCreate;
						out->moduleManagerReceiveDelegate = moduleManagerReceive;
						out->moduleManagerDestroyDelegate = moduelManagerDestroy;
					}
				}
			}
		}
	}
	catch (const _com_error& e)
	{
		LogError("Exception Thrown. Message: %s ", e.ErrorMessage());
		if(out != nullptr)
		{
			delete out;
			out = nullptr;
		}
	}

    return out;
}

static void DotNETCore_Receive(MODULE_HANDLE moduleHandle, MESSAGE_HANDLE messageHandle)
{
	try
	{
		if (
			(moduleHandle != nullptr) &&
			(messageHandle != nullptr)
			)
		{
			DOTNETCORE_HOST_HANDLE_DATA* handleData = (DOTNETCORE_HOST_HANDLE_DATA*)moduleHandle;
			int hr;

			/* Codes_SRS_DOTNET_04_018: [ DotNET_Create shall call Receive C# method passing the Message object created with the content of message serialized into Message object. ] */
			if (FAILED(hr = handleData->moduleManagerReceiveDelegate(moduleHandle, messageHandle)))
			{
				LogError("Failed to Invoke Receive method. w/hr 0x%08lx\n", hr);
			}
		}
		else
		{
			/* Codes_SRS_DOTNET_04_015: [ DotNET_Receive shall do nothing if module is NULL. ] */
			/* Codes_SRS_DOTNET_04_016: [ DotNET_Receive shall do nothing if message is NULL. ] */
			LogError("invalid arg moduleHandle=%p, messageHandle=%p", moduleHandle, messageHandle);
			/*do nothing*/
		}
	}
	catch (const _com_error& e)
	{
		LogError("Exception Thrown. Message: %s ", e.ErrorMessage());
	}
}


static void DotNETCore_Destroy(MODULE_HANDLE module)
{
	/* Codes_SRS_DOTNET_04_019: [ DotNET_Destroy shall do nothing if module is NULL. ] */
	if (module != nullptr)
	{
		DOTNETCORE_HOST_HANDLE_DATA* handleData = (DOTNETCORE_HOST_HANDLE_DATA*)moduleHandle;
		int hr;

		/* Codes_SRS_DOTNET_04_022: [ DotNET_Destroy shall call the Destroy C# method. ] */
		if (FAILED(hr = handleData->moduleManagerDestroyDelegate(module)))
		{
			LogError("Failed to Invoke Destroy method. w/hr 0x%08lx\n", hrResult);
		}
		
		/* Codes_SRS_DOTNET_04_020: [ DotNET_Destroy shall free all resources associated with the given module.. ] */
		delete(handleData);
	}
}

MODULE_EXPORT int Module_DotNetCoreHost_PublishMessage(MESSAGE_BUS_HANDLE bus, MODULE_HANDLE sourceModule, const unsigned char* source, int32_t size)
{
	int returnValue = HRESULT_E_FAIL;
	MESSAGE_HANDLE messageToPublish = nullptr;

	if (bus == nullptr || sourceModule == nullptr || source == nullptr || size < 0)
	{
		/* Codes_SRS_DOTNET_04_022: [ Module_DotNetHost_PublishMessage shall return false if bus is NULL. ] */
		/* Codes_SRS_DOTNET_04_029: [ Module_DotNetHost_PublishMessage shall return false if sourceModule is NULL. ] */
		/* Codes_SRS_DOTNET_04_023: [ Module_DotNetHost_PublishMessage shall return false if source is NULL, or size if lower than 0. ] */
		LogError("invalid arg bus=%p, sourceModule=%p", bus, sourceModule);
		returnValue = HRESULT_E_POINTER;
	}
	/* Codes_SRS_DOTNET_04_024: [ Module_DotNetHost_PublishMessage shall create a message from source and size by invoking Message_CreateFromByteArray. ] */
	else if((messageToPublish = Message_CreateFromByteArray(source, size)) == nullPtr)
	{
		/* Codes_SRS_DOTNET_04_025: [ If Message_CreateFromByteArray fails, Module_DotNetHost_PublishMessage shall fail. ] */
		LogError("Error trying to create message from Byte Array");
		returnValue = HRESULT_E_FAIL;
	}
	/* Codes_SRS_DOTNET_04_026: [ Module_DotNetHost_PublishMessage shall call MessageBus_Publish passing bus, sourceModule, byte array and msgHandle. ] */
	else if (MessageBus_Publish(bus, sourceModule, messageToPublish) != MESSAGE_BUS_OK)
	{
		/* Codes_SRS_DOTNET_04_027: [ If MessageBus_Publish fail Module_DotNetHost_PublishMessage shall fail. ] */
		LogError("Error trying to publish message on MessageBus.");
		returnValue = HRESULT_E_FAIL;
	}
	else
	{
		/* Codes_SRS_DOTNET_04_028: [If MessageBus_Publish succeed Module_DotNetHost_PublishMessage shall succeed.] */
		returnValue = HRESULT_S_SUCCESS;
	}

	if (messageToPublish != nullPtr)
	{
		Message_Destroy(messageToPublish);
	}

	return returnValue;
}

static const MODULE_APIS DOTNETCORE_APIS_all =
{
	DotNETCore_Create,
	DotNETCore_Destroy,
	DotNETCore_Receive
};



#ifdef BUILD_MODULE_TYPE_STATIC
MODULE_EXPORT const MODULE_APIS* MODULE_STATIC_GETAPIS(DOTNETCORE_HOST)(void)
#else
MODULE_EXPORT const MODULE_APIS* Module_GetAPIS(void)
#endif
{
	/* Codes_SRS_DOTNET_04_021: [ Module_GetAPIS shall return a non-NULL pointer to a structure of type MODULE_APIS that has all fields initialized to non-NULL values. ] */
	return &DOTNETCORE_APIS_all;
}
