// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#if defined(_CRTDBG_MAP_ALLOC)
#include <crtdbg.h>
#endif

#include "azure_c_shared_utility/gballoc.h"

#include "message.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/base64.h"

#if defined(UNDER_TEST)
#define SafeArrayCreateVector myTest_SafeArrayCreateVector
#define SafeArrayPutElement   myTest_SafeArrayPutElement
#define SafeArrayDestroy      myTest_SafeArrayDestroy
#define SafeArrayCreate       myTest_SafeArrayCreate
#define SafeArrayAccessData   myTest_SafeArrayAccessData
#define SafeArrayUnaccessData myTest_SafeArrayUnaccessData
#endif

#if defined(WIN32)
#include <windows.h>
#else // WIN32
#include <dlfcn.h>
#endif // WIN32

#include "dotnet.h"

#include <new>
#include <memory>
#if !defined(DOTNET_CORE)
#include <metahost.h>
#include <atlbase.h>
#endif // !DOTNET_CORE
#if !defined(_HRESULT_DEFINED)
// Copy of HRESULT related macros
typedef int32_t HRESULT;
#endif // !_HRESULT_DEFINED
#if !defined(FAILED)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#endif // !FAILED
#include <stdio.h>

#if defined(DOTNET_CORE)

#include <string>

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/param.h>
#endif // __FreeBSD__
#if defined(HAVE_SYS_SYSCTL_H) || defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif // HAVE_SYS_SYSCTL_H || __FreeBSD__

#if defined(WIN32)

#define CORE_CLR_LIB        L"coreclr.dll"
#define CORE_ROOT           L"CORE_ROOT"
#define CORE_LIBRARIES      L"CORE_LIBRARIES"
#define CORECLR_SERVER_GC   L"CORECLR_SERVER_GC"
#define W_MAX_PATH          32767
#define DIR_SEPARATOR       L'\\'
#define PATH_SEPARATOR      L';'
#define EXTENSION_HEAD      L'.'
#define NI_SUFFIX           L"NI"
#define NI_EXTENSION        L".NI"

typedef WCHAR pchar;
typedef std::wstring pstring;

#else // WIN32

#if defined(__APPLE__)
#define CORE_CLR_LIB        "libcorclr.dylib"
#else // __APPLE__
#define CORE_CLR_LIB        "libcorclr.so"
#endif // __APPLE__
#define CORE_ROOT           "CORE_ROOT"
#define CORE_LIBRARIES      "CORE_LIBRARIES"
#define CORECLR_SERVER_GC   "CORECLR_SERVER_GC"
#define DIR_SEPARATOR       '/'
#define PATH_SEPARATOR      ':'
#define EXTENSION_HEAD      '.'
#define NI_SUFFIX           "NI"
#define NI_EXTENSION        ".NI"

typedef char pchar;
typedef std::string pstring;

#if defined(__linux__)
#define SYMLINK_ENTRYPOINT_EXECUTABLE "/proc/self/exe"
#elif !defined(__APPLE__)
#define SYMLINK_ENTRYPOINT_EXECUTABLE "/proc/curproc/exe"
#endif // __linux__

#endif // WIN32


// CoreCLR automatically convert UTF8 to UTF16 conversion.
#define HOST_MANAGER_ASSEMBLY_NAME          "Microsoft.Azure.IoT.Gateway, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null"
#define HOST_MANAGER_TYPE_NAME              "Microsoft.Azure.IoT.Gateway.Hosting.HostManager"
#define HOST_MANAGER_CREATE_METHOD_NAME     "Create"
#define HOST_MANAGER_DESTROY_METHOD_NAME    "Destroy"
#define HOST_MANAGER_START_METHOD_NAME      "Start"
#define HOST_MANAGER_RECEIVE_METHOD_NAME    "Receive"

typedef HRESULT(*pfCoreClrInitialize)(const char* exePath, const char* appDomainFriendlyName, int propertyCount, const char** propertyKeys, const char** propertyValues, void** hostHandle, unsigned int* domainId);
typedef HRESULT(*pfCoreClrCreateDelegate)(void* hostHandle, unsigned int domainId, const char* entryPointAssemblyName, const char* entryPointTypeName, const char* entryPointMethodName, void** delegate);

#else // DOTNET_CORE

#define DEFAULT_CLR_VERSION                 L"v4.0.30319"

#define HOST_MANAGER_ASSEMBLY_NAME          L"Microsoft.Azure.IoT.Gateway"
#define HOST_MANAGER_TYPE_NAME              L"Microsoft.Azure.IoT.Gateway.Hosting.HostManager"
#define HOST_MANAGER_GET_API_METHOD_NAME    L"GetApi"
#define HOST_MANAGER_CREATE_METHOD_NAME     L"Create"
#define HOST_MANAGER_DESTROY_METHOD_NAME    L"Destroy"
#define HOST_MANAGER_START_METHOD_NAME      L"Start"
#define HOST_MANAGER_RECEIVE_METHOD_NAME    L"Receive"

// Import mscorlib.tlb (Microsoft Common Language Runtime Class Library).
#import "mscorlib.tlb" raw_interfaces_only                 \
   high_property_prefixes("_get","_put","_putref")        \
   rename("ReportEvent", "InteropServices_ReportEvent")
using namespace mscorlib;

#endif // DOTNET_CORE

#define HOST_MANAGER_RESULT_VALUES \
    HOST_MANAGER_RESULT_SUCCESS, \
    HOST_MANAGER_RESULT_INVALID_ARG, \
    HOST_MANAGER_RESULT_ASSEMBLY_LOAD_FAIL, \
    HOST_MANAGER_RESULT_TYPE_LOAD_FAIL, \
    HOST_MANAGER_RESULT_MODULE_INSTANTIATION_FAIL, \
    HOST_MANAGER_RESULT_INVALID_MODULE_TYPE, \
    HOST_MANAGER_RESULT_MODULE_FAIL, \
    HOST_MANAGER_RESULT_UNEXPECTED_ERROR

DEFINE_ENUM(HOST_MANAGER_RESULT, HOST_MANAGER_RESULT_VALUES);

// Function pointers for delegates of HostManager
typedef HOST_MANAGER_RESULT(*pfDotNet_Create)(BROKER_HANDLE broker, MODULE_HANDLE module, const char* assembly_name, const char* module_type_name, const void* configuration);
typedef HOST_MANAGER_RESULT(*pfDotNet_Destroy)(MODULE_HANDLE module);

typedef HOST_MANAGER_RESULT(*pfDotNet_Start)(MODULE_HANDLE module);
typedef HOST_MANAGER_RESULT(*pfDotNet_Receive)(MODULE_HANDLE module, const unsigned char* message, const size_t message_size);

struct DOTNET_HOST_HANDLE_DATA
{
    DOTNET_HOST_HANDLE_DATA()
    {

    };

    BROKER_HANDLE            broker;

#if defined(DOTNET_CORE)
    void*                    pCoreClr;
    pfCoreClrInitialize      pfInitializeCoreClr;
    pfCoreClrCreateDelegate  pfCreateDelegate;
    void*                    pHost;
    unsigned int             uiDomainId;
#else // DOTNET_CORE
    CComPtr<ICLRMetaHost>    spMetaHost;
    CComPtr<ICLRRuntimeInfo> spRuntimeInfo;
    CComPtr<ICorRuntimeHost> spRuntimeHost;
    _AppDomainPtr            spDefaultAppDomain;
#endif // DOTNET_CORE
    pfDotNet_Create          pfCreate;
    pfDotNet_Destroy         pfDestroy;
    pfDotNet_Start           pfStart;
    pfDotNet_Receive         pfReceive;

private:
    DOTNET_HOST_HANDLE_DATA& operator=(const DOTNET_HOST_HANDLE_DATA&);
};

void handleError(HOST_MANAGER_RESULT result) 
{
    switch(result)
    {
        case HOST_MANAGER_RESULT_ASSEMBLY_LOAD_FAIL:
            LogError("Failed to load .NET module assembly specified in configuration. ");
            return;
        case HOST_MANAGER_RESULT_INVALID_ARG:
            LogError("Unexpected argument. ");
            return;
        case HOST_MANAGER_RESULT_INVALID_MODULE_TYPE:
            LogError("Specified .NET module does not implement required interface. ");
            return;
        case HOST_MANAGER_RESULT_MODULE_FAIL:
            LogError("An unexpected exception is thrown from .NET module. ");
            return;
        case HOST_MANAGER_RESULT_MODULE_INSTANTIATION_FAIL:
            LogError("Failed to instantiate .NET module with specified configuration. ");
            return;
        case HOST_MANAGER_RESULT_SUCCESS:
            // OK
            return;
        case HOST_MANAGER_RESULT_TYPE_LOAD_FAIL:
            LogError("Failed to load .NET module type specified in configuration. ");
            return;
        case HOST_MANAGER_RESULT_UNEXPECTED_ERROR:
        default:
            LogError("Unexpected expection is thrown. ");
            return;
    }
}

#if defined(DOTNET_CORE)

bool getEnvironmentVariable(const pchar* name, pstring& value)
{
    bool returnResult = false;
    value.clear();
#if defined(WIN32)
    WCHAR result[W_MAX_PATH];
    DWORD actualSize = GetEnvironmentVariableW(name, result, W_MAX_PATH);
    if (actualSize > 0)
    {
        value.assign(result, actualSize);
        returnResult = true;
    }
#else // WIN32
    pchar* env = getenv(name);
    if (env != NULL)
    {
        value.assign(env);
        returnResult = true;
    }
#endif // WIN32
    return returnResult;
}

// Followings to initialzeClr are ported from corerun related codes in coreclr
// * src/coreclr/hosts/corerun/corerun.cpp for WIN32
// * src/coreclr/hosts/unixcorerun/corerun.cpp and coreruncommon.cpp for non-WIN32

bool getDirectory(const pstring& absolutePath, pstring& directory)
{
    directory.assign(absolutePath);
    size_t lastSeparator = directory.rfind(DIR_SEPARATOR);
    if (lastSeparator != pstring::npos)
    {
        directory.erase(lastSeparator);
        return true;
    }

    return false;
} // getDirectory

void ensureDoesNotEndWithSeparator(pstring& path)
{
    size_t lastSlash = path.rfind(DIR_SEPARATOR);
    if (lastSlash == path.length() - 1)
    {
        path.erase(lastSlash);
    }
}

void removeExtensionAndNi(pstring& fileName)
{
    // Remove extension, if it exists
    size_t extension = fileName.rfind(EXTENSION_HEAD);
    if (extension != pstring::npos)
    {
        fileName.erase(extension);

        // Check for .nis
        size_t niExtension = fileName.rfind(NI_EXTENSION);
        if (niExtension != pstring::npos &&
            fileName.length() - 4 == niExtension)
        {
            fileName.erase(niExtension);
        }
    }
}

#if defined(WIN32)
int convertWideToAnsi(LPCWSTR wideChars, int wideCharCounts, LPSTR ansiCharsBuffer, int ansiCharsBufferLength)
{
    return WideCharToMultiByte(
        CP_THREAD_ACP,
        WC_NO_BEST_FIT_CHARS,
        wideChars,
        wideCharCounts,
        ansiCharsBuffer,
        ansiCharsBufferLength,
        NULL,
        NULL);
}
#endif // WIN32

HRESULT toCString(pstring& platformStlString, char** result)
{
    HRESULT hr = S_OK;
#if defined(WIN32)
    const wchar_t* wide = platformStlString.c_str();
    int requiredSize = convertWideToAnsi(wide, -1, NULL, 0);
    // c_str() always returns null-terminated chars, so buffer will be terminated with \0.
    char* buffer = (char*)malloc(requiredSize);
    if (convertWideToAnsi(wide, -1, buffer, requiredSize) == 0)
    {
        hr = __HRESULT_FROM_WIN32(GetLastError());
        free(buffer);
        *result = NULL;
    }
    else
    {
        *result = buffer;
    }
#else // WIN32
    // pstring is std::string in non-Windows
    result = platformStlString.c_str();
#endif // WIN32
    return hr;
}

bool getEntrypointExecutableAbsolutePath(pstring& entrypointExecutable)
{
    bool result = false;

    entrypointExecutable.clear();

    // Get path to the executable for the current process using
    // platform specific means.
#if defined(WIN32)
    HMODULE hMod = GetModuleHandle(NULL);
    WCHAR exe[W_MAX_PATH];
    if (GetModuleFileNameW(hMod, exe, W_MAX_PATH) == 0)
    {
        HRESULT hr = __HRESULT_FROM_WIN32(GetLastError());
        LogError("Failed to get module file name w/hr 0x%08lx. ", hr);
    }
    else
    {
        entrypointExecutable.assign(exe);
        result = true;
    }
#elif defined(__linux__) || (defined(__NetBSD__) && !defined(KERN_PROC_PATHNAME))
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
#else // WIN32 ...
    // On non-Mac OS, return the symlink that will be resolved by GetAbsolutePath
    // to fetch the entrypoint EXE absolute path, inclusive of filename.
    entrypointExecutable.assign(SYMLINK_ENTRYPOINT_EXECUTABLE);
    result = true;
#endif // WIN32

    return result;
} // getEntrypointExecutableAbsolutePath

bool containsFileInTpaList(pstring& tpaList, const pstring& fileNameWithoutExtension, const wchar_t** tpaExtensions, int countExtensions)
{
    if (tpaList.empty())
    {
        return false;
    }

    for (int iExtension = 0; iExtension < countExtensions; iExtension++)
    {
        pstring fileName;
        fileName.push_back(DIR_SEPARATOR); // So that we don't match other files that end with the current file name
        fileName.append(fileNameWithoutExtension);
        fileName.append(tpaExtensions[iExtension] + 1); // Exclude first *
        fileName.push_back(PATH_SEPARATOR); // So that we don't match other files that begin with the current file name

        if (tpaList.find(fileName, 0) != pstring::npos)
        {
            return true;
        }
    }
    return false;
}
void addFilesFromDirectoryToTpaList(const pstring& directory, pstring& tpaList)
{
    const pchar* tpaExtensions[] = {
#if defined(WIN32)
        L"*.ni.dll",      // Probe for .ni.dll first so that it's preferred if ni and il coexist in the same dir
        L"*.dll",
        L"*.ni.exe",
        L"*.exe",
        L"*.ni.winmd",
        L"*.winmd",
#else // WIN32
        ".ni.dll",      // Probe for .ni.dll first so that it's preferred if ni and il coexist in the same dir
        ".dll",
        ".ni.exe",
        ".exe",
#endif // WIN32
    };
    const size_t countExtensions = _countof(tpaExtensions);

#if defined(WIN32)
    pstring assemblyPath;
    size_t dirLength = directory.length() + 1;

    for (int iExtension = 0; iExtension < countExtensions; iExtension++)
    {
        assemblyPath.clear();
        assemblyPath.assign(directory);
        assemblyPath.push_back(DIR_SEPARATOR);
        assemblyPath.append(tpaExtensions[iExtension]);
        WIN32_FIND_DATAW data;
        HANDLE findHandle = FindFirstFileW(assemblyPath.c_str(), &data);

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
                    pstring fileNameWithoutExtension(data.cFileName);
                    removeExtensionAndNi(fileNameWithoutExtension);

                    // Add to the list if not already on it
                    if (!containsFileInTpaList(tpaList, fileNameWithoutExtension, tpaExtensions, countExtensions))
                    {
                        assemblyPath.resize(dirLength);
                        assemblyPath.append(data.cFileName);
                        tpaList.append(assemblyPath);
                        tpaList.push_back(PATH_SEPARATOR);
                    }
                }
            } while (0 != FindNextFileW(findHandle, &data));

            FindClose(findHandle);
        }
    }

#else // WIN32

    DIR* dir = opendir(directory);
    if (dir == nullptr)
    {
        return;
    }

    // Walk the directory for each extension separately so that we first get files with .ni.dll extension,
    // then files with .dll extension, etc.
    for (int extIndex = 0; extIndex < sizeof(tpaExtensions) / sizeof(tpaExtensions[0]); extIndex++)
    {
        const char* ext = tpaExtensions[extIndex];
        int extLength = strlen(ext);

        struct dirent* entry;

        // For all entries in the directory
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
                fullFilename.push_back(DIR_SEPARATOR);
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

            pstring filename(entry->d_name);

            // Check if the extension matches the one we are looking for
            int extPos = filename.length() - extLength;
            if ((extPos <= 0) || (filename.compare(extPos, extLength, ext) != 0))
            {
                continue;
            }

            pstring filenameWithoutExt(filename.substr(0, extPos));
            removeExtensionAndNi(filenameWithoutExt);

            // Add to the list if not already on it
            if (!containsFileInTpaList(tpaList, filenameWithoutExt, tpaExtensions, countExtensions))
            {
                assemblyPath.resize(dirLength);
                assemblyPath.append(data.cFileName);
                tpaList.append(assemblyPath);
                tpaList.push_back(PATH_SEPARATOR);
            }
        }

        // Rewind the directory stream to be able to iterate over it for the next extension
        rewinddir(dir);
    }

    closedir(dir);
#endif // WIN32
} // addFilesFromDirectoryToTpaList

void* loadLibrary(const pchar* nameOrPath)
{
#if defined(WIN32)
    return LoadLibraryW(nameOrPath);
#else // WIN32
    return dlopen(nameOrPath, RTLD_NOW | RTLD_LOCAL);
#endif // WIN32
} // loadLibrary

void* loadFunction(const void* pLibrary, const char* name)
{
#if defined(WIN32)
    return GetProcAddress((HINSTANCE)pLibrary, name);
#else // WIN32
    return dlsym(pLibrary, name);
#endif // WIN32
} // loadFunction

bool loadCoreClr(const pchar* coreClrPath, DOTNET_HOST_HANDLE_DATA* pData)
{
    bool returnResult = false;

    if ((pData->pCoreClr = loadLibrary(coreClrPath)) == NULL)
    {
#if defined(WIN32)
        HRESULT hr = __HRESULT_FROM_WIN32(GetLastError());
        LogError("Failed to load CoreCLR library '%ls' w/hr 0x%08lx. ", coreClrPath, hr);
#else // WIN32
        LogError("Failed to load CoreCLR library '%s'. ", coreClrPath);
#endif // WIN32
    }
    else if((pData->pfInitializeCoreClr = (pfCoreClrInitialize)loadFunction(pData->pCoreClr, "coreclr_initialize")) == NULL)
    {
#if defined(WIN32)
        HRESULT hr = __HRESULT_FROM_WIN32(GetLastError());
        LogError("Failed to get address of coreclr_create_delegate w/hr 0x%08lx. ", hr);
#else // WIN32
        LogError("Failed to get address of coreclr_initialize. ");
#endif // WIN32
    }
    else if((pData->pfCreateDelegate = (pfCoreClrCreateDelegate)loadFunction(pData->pCoreClr, "coreclr_create_delegate")) == NULL)
    {
#if defined(WIN32)
        HRESULT hr = __HRESULT_FROM_WIN32(GetLastError());
        LogError("Failed to get address of coreclr_create_delegate w/hr 0x%08lx. ", hr);
#else // WIN32
        LogError("Failed to get address of coreclr_create_delegate. ");
#endif // WIN32
    }
    else
    {
        returnResult = true;
    }

    return returnResult;
} // loadCoreClr

bool initializeClr(DOTNET_HOST_HANDLE_DATA* pData)
{
    // From unixcorrun.cpp in coreclr

    // envs:
    // CORE_ROOT:           A directory path to [lib]corclr.{dll|so|dylib}.
    // CORE_LIBRARIES:      A directory path to native libraries.
    // CORECLR_SERVER_GC:   A 0 or 1 to indicate whether use server GC.

    bool returnResult = false;
    pstring clrDir;
    if (!getEnvironmentVariable(CORE_ROOT, clrDir))
    {
        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
        pData = NULL;
        LogError("Environment variable CORE_ROOT is required. ");
    }
    else
    {
        ensureDoesNotEndWithSeparator(clrDir);
        pstring coreClrPath(clrDir);
        coreClrPath.push_back(DIR_SEPARATOR);
        coreClrPath.append(CORE_CLR_LIB);

        pstring exePath;

        if (!getEntrypointExecutableAbsolutePath(exePath))
        {
            /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
            pData = NULL;
            LogError("Failed to detect the absolute path of the entrypoint executable. ");
        }
        else
        {
            pstring appPath;
            if (!getDirectory(exePath, appPath))
            {
                /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
                pData = NULL;
                LogError("Failed to determine app path from entrypoint executable path. ");
            }
            else
            {
                pstring appNiPath(appPath);
                appNiPath.append(NI_SUFFIX);
                appNiPath.push_back(PATH_SEPARATOR);
                appNiPath.append(appPath);

                // Construct native search directory paths
                pstring nativeDllSearchDirs(appPath);
                pstring coreLibraries;
                if(getEnvironmentVariable(CORE_LIBRARIES, coreLibraries))
                {
                    nativeDllSearchDirs.push_back(PATH_SEPARATOR);
                    nativeDllSearchDirs.append(coreLibraries);
                }

                nativeDllSearchDirs.push_back(PATH_SEPARATOR);
                nativeDllSearchDirs.append(clrDir);

                pstring tpaList;
                addFilesFromDirectoryToTpaList(clrDir, tpaList);

                if (!loadCoreClr(coreClrPath.c_str(), pData))
                {
                    /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
                    pData = NULL;
                    LogError("Failed to load CoreCLR library. ");
                }
                else
                {
                    const char* propertyKeys[] = {
                        "TRUSTED_PLATFORM_ASSEMBLIES",
                        "APP_PATHS",
                        "APP_NI_PATHS",
                        "NATIVE_DLL_SEARCH_DIRECTORIES",
                        "AppDomainCompatSwitch"
                    };

                    char* exePathAnsi;
                    char* tpaListAnsi;
                    char* appPathAnsi;
                    char* appNIPathAnsi;
                    char* nativeDllSearchDirsAnsi;

                    HRESULT hr;
                    if (FAILED(hr = toCString(exePath, &exePathAnsi)))
                    {
                        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
                        pData = NULL;
                        LogError("Failed to convert entrypoint executable path from UTF-16 to ANSI w/hr 0x%08lx. ", hr);
                    }
                    else if (FAILED(hr = toCString(tpaList, &tpaListAnsi)))
                    {
                        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
                        pData = NULL;
                        LogError("Failed to convert TPA list from UTF-16 to ANSI w/hr 0x%08lx. ", hr);
                    }
                    else if (FAILED(hr = toCString(appPath, &appPathAnsi)))
                    {
                        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
                        pData = NULL;
                        LogError("Failed to convert app path from UTF-16 to ANSI w/hr 0x%08lx. ", hr);
                    }
                    else if (FAILED(hr = toCString(appNiPath, &appNIPathAnsi)))
                    {
                        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
                        pData = NULL;
                        LogError("Failed to convert app native image path from UTF-16 to ANSI w/hr 0x%08lx. ", hr);
                    }
                    else if (FAILED(hr = toCString(nativeDllSearchDirs, &nativeDllSearchDirsAnsi)))
                    {
                        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
                        pData = NULL;
                        LogError("Failed to convert native library search directories from UTF-16 to ANSI w/hr 0x%08lx. ", hr);
                    }
                    else
                    {
                        const char* propertyValues[] = {
                            tpaListAnsi,
                            appPathAnsi,
                            appNIPathAnsi,
                            nativeDllSearchDirsAnsi,
                            "UseLatestBehaviorWhenTFMNotSpecified"
                        };

                        void* pHost;
                        unsigned int uiDomainId;
                        if (FAILED(hr = pData->pfInitializeCoreClr(
                            exePathAnsi,
                            "Microsoft.Azure.IoT.Gateway", // AppDomainName
                            sizeof(propertyKeys) / sizeof(propertyKeys[0]),
                            propertyKeys,
                            propertyValues,
                            &pHost,
                            &uiDomainId)))
                        {
                            /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
                            pData = NULL;
                            LogError("Failed to initialize CoreCLR w/hr 0x%08lx. ", hr);
                        }
                        else
                        {
                            pData->pHost = pHost;
                            pData->uiDomainId = uiDomainId;
                            returnResult = true;
                        }
                    }
                }
            }
        }
    }

    return returnResult;
}

HRESULT setHostManagerApi(const char* methodName, const DOTNET_HOST_HANDLE_DATA* pData, void** ppDelegate)
{
    return pData->pfCreateDelegate(
        pData->pHost,
        pData->uiDomainId,
        HOST_MANAGER_ASSEMBLY_NAME,
        HOST_MANAGER_TYPE_NAME,
        methodName,
        ppDelegate);
}

bool setHostManagerApis(DOTNET_HOST_HANDLE_DATA* pData)
{
    HRESULT hr;
    bool returnResult = false;
    if (FAILED(hr =setHostManagerApi(HOST_MANAGER_CREATE_METHOD_NAME, pData, (void**)&pData->pfCreate)))
    {
        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
        LogError("Failed to get Create API delegate w/hr 0x%08lx. ", hr);
    }
    else if (FAILED(hr = setHostManagerApi(HOST_MANAGER_DESTROY_METHOD_NAME, pData, (void**)&pData->pfDestroy)))
    {
        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
        LogError("Failed to get Destroy API delegate w/hr 0x%08lx. ", hr);
    }
    else if (FAILED(hr = setHostManagerApi(HOST_MANAGER_START_METHOD_NAME, pData, (void**)&pData->pfStart)))
    {
        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
        LogError("Failed to get Start API delegate w/hr 0x%08lx. ", hr);
    }
    else if (FAILED(hr = setHostManagerApi(HOST_MANAGER_RECEIVE_METHOD_NAME, pData, (void**)&pData->pfReceive)))
    {
        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
        LogError("Failed to get Receive API delegate w/hr 0x%08lx. ", hr);
    }
    else
    {
        returnResult = true;
    }

    return returnResult;
}

#else // DOTNET_CORE

bool initializeClr(DOTNET_HOST_HANDLE_DATA* pData)
{
    /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
    bool returnResult = false;
    HRESULT hr;
    BOOL fLoadable;
    IUnknownPtr spAppDomainThunk;      

    if (FAILED(hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_PPV_ARGS(&pData->spMetaHost))))
    {
        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
        LogError("CLRCreateInstance failed w/hr 0x%08lx\n", hr);
    }
    else if (FAILED(hr = pData->spMetaHost->GetRuntime(DEFAULT_CLR_VERSION, IID_PPV_ARGS(&pData->spRuntimeInfo))))
    {
        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
        LogError("ICLRMetaHost::GetRuntime failed w/hr 0x%08lx", hr);
    }
    else if (FAILED(hr = pData->spRuntimeInfo->IsLoadable(&fLoadable)))
    {
        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
        LogError("ICLRRuntimeInfo::IsLoadable failed w/hr 0x%08lx\n", hr);
    }
    else if (!fLoadable)
    {
        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
        LogError(".NET runtime %ls cannot be loaded\n", DEFAULT_CLR_VERSION);
    }
    else if (FAILED(hr = pData->spRuntimeInfo->GetInterface(CLSID_CorRuntimeHost, IID_PPV_ARGS(&pData->spRuntimeHost))))
    {
        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
        LogError("ICLRRuntimeInfo::GetInterface failed w/hr 0x%08lx\n", hr);
    }
    else if (FAILED(hr = pData->spRuntimeHost->Start()))
    {
        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
        LogError("CLR failed to start w/hr 0x%08lx\n", hr);
    }
    else if (FAILED(hr = pData->spRuntimeHost->GetDefaultDomain(&spAppDomainThunk)))
    {
        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
        LogError("ICorRuntimeHost::GetDefaultDomain failed w/hr 0x%08lx\n", hr);
    }
    else if (FAILED(hr = spAppDomainThunk->QueryInterface(IID_PPV_ARGS(&pData->spDefaultAppDomain))))
    {
        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
        LogError("Failed to get default AppDomain w/hr 0x%08lx\n", hr);
    }
    else
    {
        returnResult = true;
    }

    return returnResult;
}

bool setHostManagerApi(_TypePtr type, const wchar_t* methodName, SAFEARRAY* psaArgumentsHolder, void** ppDelegate)
{
    bool returnResult = false;
    HRESULT hr;
    bstr_t bstrGetApiMethodName(HOST_MANAGER_GET_API_METHOD_NAME);
    bstr_t bstrMethodName(methodName);
    variant_t vtMethodName;
    variant_t vtEmpty;
    variant_t vtResult;
    LONG index = 0;

    V_BSTR(&vtMethodName) = bstrMethodName;
    V_VT(&vtMethodName) = VT_BSTR;

    if (FAILED(hr = SafeArrayPutElement(psaArgumentsHolder, &index, &vtMethodName)))
    {
        LogError("Adding Element on the safe array failed. w/hr 0x%08lx\n", hr);
    }
    else if (FAILED(hr = type->InvokeMember_3(
        bstrGetApiMethodName,
        static_cast<BindingFlags>(BindingFlags_Static | BindingFlags_Public | BindingFlags_InvokeMethod),
        NULL,
        vtEmpty,
        psaArgumentsHolder,
        &vtResult)))
    {
        LogError("Failed to get GetApi method w/hr 0x%08lx. ", hr);
    }
    else
    {
        *ppDelegate = (void*)(__int64)vtResult;
        returnResult = true;
    }

    return returnResult;
}

bool setHostManagerApis(DOTNET_HOST_HANDLE_DATA* pData)
{
    bool returnResult = false;
    HRESULT hr;
    bstr_t bstrAssemblyName(HOST_MANAGER_ASSEMBLY_NAME);
    bstr_t bstrTypeName(HOST_MANAGER_TYPE_NAME);
    _AssemblyPtr pAssembly;
    _TypePtr pType;

    if (FAILED(hr = pData->spDefaultAppDomain->Load_2(bstrAssemblyName, &pAssembly)))
    {
        LogError("Failed to load Gateway assembly w/hr 0x%08lx. ", hr);
    }
    else if (FAILED(hr = pAssembly->GetType_2(bstrTypeName, &pType)))
    {
        LogError("Failed to get HostManager type w/hr 0x%08lx. ", hr);
    }
    else
    {
        SAFEARRAY* psaApiParameters = NULL;

        if ((psaApiParameters = SafeArrayCreateVector(VT_VARIANT, 0, 1)) == NULL)
        {
            LogError("Error building SafeArray Vector for arguments. ");
        }
        else if (!setHostManagerApi(pType, HOST_MANAGER_CREATE_METHOD_NAME, psaApiParameters, (void**)&pData->pfCreate))
        {
            LogError("Failed to get Create method. ");
        }
        else if (!setHostManagerApi(pType, HOST_MANAGER_DESTROY_METHOD_NAME, psaApiParameters, (void**)&pData->pfDestroy))
        {
            LogError("Failed to get Destroy method. ");
        }
        else if (!setHostManagerApi(pType, HOST_MANAGER_START_METHOD_NAME, psaApiParameters, (void**)&pData->pfStart))
        {
            LogError("Failed to get Start method. ");
        }
        else if (!setHostManagerApi(pType, HOST_MANAGER_RECEIVE_METHOD_NAME, psaApiParameters, (void**)&pData->pfReceive))
        {
            LogError("Failed to get Receive method. ");
        }
        else
        {
            returnResult = true;
        }
    }

    return returnResult;
}

#endif // DOTNET_CORE

static MODULE_HANDLE DotNet_Create(BROKER_HANDLE broker, const void* configuration)
{
    DOTNET_HOST_HANDLE_DATA* out = NULL;

#if !defined(DOTNET_CORE)
    try
    {
#endif // !DOTNET_CORE
        if (
            (broker == NULL) ||
            (configuration == NULL)
            )
        {
            /* Codes_SRS_DOTNET_04_001: [ DotNet_Create shall return NULL if broker is NULL. ] */
            /* Codes_SRS_DOTNET_04_002: [ DotNet_Create shall return NULL if configuration is NULL. ] */
            LogError("invalid arg broker=%p, configuration=%p", broker, configuration);
        }
        else
        {
            DOTNET_HOST_CONFIG* dotNetConfig = (DOTNET_HOST_CONFIG*)configuration;
            if (dotNetConfig->assembly_name == NULL)
            {
                /* Codes_SRS_DOTNET_04_003: [ DotNet_Create shall return NULL if configuration->assembly_name is NULL. ] */
                LogError("invalid configuration. assembly_name=%p", dotNetConfig->assembly_name);
            }
            else if (dotNetConfig->entry_type == NULL)
            {
                /* Codes_SRS_DOTNET_04_004: [ DotNet_Create shall return NULL if configuration->entry_type is NULL. ] */
                LogError("invalid configuration. entry_type=%p", dotNetConfig->entry_type);
            }
            else if (dotNetConfig->module_args == NULL)
            {
                /* Codes_SRS_DOTNET_04_005: [ DotNet_Create shall return NULL if configuration->module_args is NULL. ] */
                LogError("invalid configuration. module_args=%p", dotNetConfig->module_args);
            }
            else
            {
                /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
                try
                {
                    /* Codes_SRS_DOTNET_04_008: [ DotNet_Create shall allocate memory for an instance of the DOTNET_HOST_HANDLE_DATA structure and use that as the backing structure for the module handle. ] */
                    out = new DOTNET_HOST_HANDLE_DATA();
                }
                catch (std::bad_alloc)
                {
                    //Do not need to do anything, since we are returning NULL below.
                    LogError("Memory allocation failed for DOTNET_HOST_HANDLE_DATA.");
                }

                if (out != NULL)
                {
                    /* Codes_SRS_DOTNET_04_007: [ DotNet_Create shall return a non-NULL MODULE_HANDLE when successful. ] */
                    out->broker = broker;

                    // !!!These requirements should be updated.
                    /* Codes_SRS_DOTNET_04_012: [ DotNet_Create shall get the 3 CLR Host Interfaces (CLRMetaHost, CLRRuntimeInfo and CorRuntimeHost) and save it on DOTNET_HOST_HANDLE_DATA. ] */
                    /* Codes_SRS_DOTNET_04_010: [ DotNet_Create shall save Client module Type and Azure IoT Gateway Assembly on DOTNET_HOST_HANDLE_DATA. ] */
                    if (!initializeClr(out))
                    {
                        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
                        LogError("Error creating CLR Intance, Getting Interfaces and Starting it.");
                        delete out; 
                        out = NULL;
                    }
                    else if (!setHostManagerApis(out))
                    {
                        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
                        LogError("Failed to initialize host manager APIs\n");
                        delete out;
                        out = NULL;
                    }
                    else
                    {
                        HOST_MANAGER_RESULT result = out->pfCreate(
                            broker,
                            out, // MODULE_HANDLE
                            dotNetConfig->assembly_name,
                            dotNetConfig->entry_type,
                            dotNetConfig->module_args
                        );
                        if (result != HOST_MANAGER_RESULT_SUCCESS)
                        {
                            /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
                            LogError("Failed in IGatewayModule.Create. ");
                            handleError(result);
                            LogError("\n");
                            delete out;
                            out = NULL;
                        }
                    }
                }
            }
        }
#if !defined(DOTNET_CORE)
    }
    catch (const _com_error& e)
    {
        /* Codes_SRS_DOTNET_04_006: [ DotNet_Create shall return NULL if an underlying API call fails. ] */
        LogError("Exception Thrown. Message: %s ", e.ErrorMessage());
        if (out != NULL)
        {
            delete out;
            out = NULL;
        }
    }
#endif // !DOTNET_CORE

    return out;
}

static void* DotNet_ParseConfigurationFromJson(const char* configuration)
{
    return (void*)configuration;
}


void DotNet_FreeConfiguration(void* configuration)
{
    //Nothing to be freed here.
}

static void DotNet_Receive(MODULE_HANDLE moduleHandle, MESSAGE_HANDLE messageHandle)
{
#if !defined(DOTNET_CORE)
    try
    {
#endif // !DOTNET_CORE
        if (
            (moduleHandle != NULL) &&
            (messageHandle != NULL)
            )
        {
            DOTNET_HOST_HANDLE_DATA* pData = (DOTNET_HOST_HANDLE_DATA*)moduleHandle;
            int32_t msg_size = Message_ToByteArray(messageHandle, NULL, 0);
            auto msgByteArray = std::make_unique<unsigned char[]>(msg_size);
            if (msgByteArray)
            {
                Message_ToByteArray(messageHandle, msgByteArray.get(), msg_size);

                HOST_MANAGER_RESULT result = pData->pfReceive(moduleHandle, msgByteArray.get(), msg_size);
                if (result != HOST_MANAGER_RESULT_SUCCESS)
                {
                    LogError("Failed to Invoke Receive method. ");
                    handleError(result);
                    LogError("\n");
                }
            }
        }
        else
        {
            /* Codes_SRS_DOTNET_04_015: [ DotNet_Receive shall do nothing if module is NULL. ] */
            /* Codes_SRS_DOTNET_04_016: [ DotNet_Receive shall do nothing if message is NULL. ] */
            LogError("invalid arg moduleHandle=%p, messageHandle=%p", moduleHandle, messageHandle);
            /*do nothing*/
        }
#if !defined(DOTNET_CORE)
    }
    catch (const _com_error& e)
    {
        LogError("Exception Thrown. Message: %s ", e.ErrorMessage());
    }
#endif // !DOTNET_CORE
}

static void DotNet_Destroy(MODULE_HANDLE module)
{
    /* Codes_SRS_DOTNET_04_019: [ DotNet_Destroy shall do nothing if module is NULL. ] */
    if (module != NULL)
    {
        DOTNET_HOST_HANDLE_DATA* pData = (DOTNET_HOST_HANDLE_DATA*)module;

        /* Codes_SRS_DOTNET_04_022: [ DotNet_Destroy shall call the Destroy C# method. ] */

#if !defined(DOTNET_CORE)
        try
        {
#endif // !DOTNET_CORE

            HOST_MANAGER_RESULT result = pData->pfDestroy(module);
            if (result != HOST_MANAGER_RESULT_SUCCESS)
            {
                LogError("Failed to Invoke Destroy method. ");
                handleError(result);
                LogError("\n");
            }
#if !defined(DOTNET_CORE)
        }
        catch (const _com_error& e)
        {
            LogError("Exception Thrown. Message: %s ", e.ErrorMessage());
        }
#endif // !DOTNET_CORE

        /* Codes_SRS_DOTNET_04_020: [ DotNet_Destroy shall free all resources associated with the given module.. ] */
        delete(pData);
    }
}

MODULE_EXPORT bool Module_DotNetHost_PublishMessage(BROKER_HANDLE broker, MODULE_HANDLE sourceModule, const unsigned char* message, int32_t size)
{
    bool returnValue = false;
    MESSAGE_HANDLE messageToPublish = NULL;

    if (broker == NULL || sourceModule == NULL || message == NULL || size < 0)
    {
        /* Codes_SRS_DOTNET_04_022: [ Module_DotNetHost_PublishMessage shall return false if broker is NULL. ] */
        /* Codes_SRS_DOTNET_04_029: [ Module_DotNetHost_PublishMessage shall return false if sourceModule is NULL. ] */
        /* Codes_SRS_DOTNET_04_023: [ Module_DotNetHost_PublishMessage shall return false if message is NULL, or size if lower than 0. ] */
        LogError("invalid arg broker=%p, sourceModule=%p", broker, sourceModule);
    }
    /* Codes_SRS_DOTNET_04_024: [ Module_DotNetHost_PublishMessage shall create a message from message and size by invoking Message_CreateFromByteArray. ] */
    else if((messageToPublish = Message_CreateFromByteArray(message, size)) == NULL)
    {
        /* Codes_SRS_DOTNET_04_025: [ If Message_CreateFromByteArray fails, Module_DotNetHost_PublishMessage shall fail. ] */
        LogError("Error trying to create message from Byte Array");
    }
    /* Codes_SRS_DOTNET_04_026: [ Module_DotNetHost_PublishMessage shall call Broker_Publish passing broker, sourceModule, message and size. ] */
    else if (Broker_Publish(broker, sourceModule, messageToPublish) != BROKER_OK)
    {
        /* Codes_SRS_DOTNET_04_027: [ If Broker_Publish fails Module_DotNetHost_PublishMessage shall fail. ] */
        LogError("Error trying to publish message on Broker.");
    }
    else
    {
        /* Codes_SRS_DOTNET_04_028: [If Broker_Publish succeeds Module_DotNetHost_PublishMessage shall succeed.] */
        returnValue = true;
    }

    if (messageToPublish != NULL)
    {
        Message_Destroy(messageToPublish);
    }

    return returnValue;
}

static void DotNet_Start(MODULE_HANDLE module)
{
    /*Codes_SRS_DOTNET_17_001: [ DotNet_Start shall do nothing if module is NULL. ]*/
    if (module != NULL)
    {
        DOTNET_HOST_HANDLE_DATA* pData = (DOTNET_HOST_HANDLE_DATA*)module;

#if !defined(DOTNET_CORE)
        try
        {
#endif // !DOTNET_CORE

            HOST_MANAGER_RESULT result = pData->pfStart(module);
            if (result != HOST_MANAGER_RESULT_SUCCESS)
            {
                LogError("Failed to Invoke Start method. ");
                handleError(result);
                LogError("\n");
            }
#if !defined(DOTNET_CORE)
        }
        catch (const _com_error& e)
        {
            LogError("Exception Thrown. Message: %s ", e.ErrorMessage());
        }
#endif // DOTNET_CORE
    }
}

static const MODULE_API_1 DOTNET_APIS_all =
{
    {MODULE_API_VERSION_1},
    DotNet_ParseConfigurationFromJson,
    DotNet_FreeConfiguration,
    DotNet_Create,
    DotNet_Destroy,
    DotNet_Receive,
    DotNet_Start
};

/*Codes_SRS_DOTNET_26_001: [ Module_GetApi shall return out the provided MODULES_API structure with required module's APIs functions. ]*/
#if defined(BUILD_MODULE_TYPE_STATIC)
MODULE_EXPORT const MODULE_API* MODULE_STATIC_GETAPI(DOTNET_HOST)(MODULE_API_VERSION gateway_api_version)
#else // !BUILD_MODULE_TYPE_STATIC
MODULE_EXPORT const MODULE_API* Module_GetApi(MODULE_API_VERSION gateway_api_version)
#endif // !BUILD_MODULE_TYPE_STATIC
{
    (void)gateway_api_version;
    return (const MODULE_API *)&DOTNET_APIS_all;
}
