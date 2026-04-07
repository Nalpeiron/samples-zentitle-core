#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#else
#include <dlfcn.h>
#endif

#define MAX_PATH_LENGTH 1024
#define MAX_FINGERPRINT_LENGTH 100
#define FINGERPRINT_BUFFER_SIZE (MAX_FINGERPRINT_LENGTH + 1)

#if defined(_WIN32)
typedef HMODULE LibraryHandle;
#define PATH_SEPARATOR "\\"
#define PLATFORM_FOLDER "Windows_x86_64"
#define LIBRARY_FILE_NAME "Zentitle2Core.dll"
#elif defined(__APPLE__) && defined(__aarch64__)
typedef void* LibraryHandle;
#define PATH_SEPARATOR "/"
#define PLATFORM_FOLDER "MacOS_arm64"
#define LIBRARY_FILE_NAME "libZentitle2Core.dylib"
#elif defined(__APPLE__)
typedef void* LibraryHandle;
#define PATH_SEPARATOR "/"
#define PLATFORM_FOLDER "MacOS_x86_64"
#define LIBRARY_FILE_NAME "libZentitle2Core.dylib"
#elif defined(__linux__) && defined(__aarch64__)
typedef void* LibraryHandle;
#define PATH_SEPARATOR "/"
#define PLATFORM_FOLDER "Linux_aarch64"
#define LIBRARY_FILE_NAME "libZentitle2Core.so"
#elif defined(__linux__) && defined(__x86_64__)
typedef void* LibraryHandle;
#define PATH_SEPARATOR "/"
#define PLATFORM_FOLDER "Linux_x86_64"
#define LIBRARY_FILE_NAME "libZentitle2Core.so"
#else
#error "Unsupported platform"
#endif

typedef bool (*GenerateDefaultDeviceFingerprintFn)(char*, int*);

static const char* get_last_library_error(void)
{
#if defined(_WIN32)
    static char messageBuffer[512];
    const DWORD errorCode = GetLastError();

    if (errorCode == 0)
    {
        return "unknown error";
    }

    const DWORD result = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        messageBuffer,
        (DWORD)sizeof(messageBuffer),
        NULL);

    if (result == 0)
    {
        return "unknown error";
    }

    return messageBuffer;
#else
    const char* error = dlerror();
    return error != NULL ? error : "unknown error";
#endif
}

static LibraryHandle load_dynamic_library(const char* libraryPath)
{
#if defined(_WIN32)
    return LoadLibraryA(libraryPath);
#else
    dlerror();
    return dlopen(libraryPath, RTLD_NOW | RTLD_LOCAL);
#endif
}

static void* get_function_pointer(LibraryHandle libraryHandle, const char* functionSymbol)
{
#if defined(_WIN32)
    return (void*)GetProcAddress(libraryHandle, functionSymbol);
#else
    dlerror();
    return dlsym(libraryHandle, functionSymbol);
#endif
}

static void close_dynamic_library(LibraryHandle libraryHandle)
{
    if (libraryHandle == NULL)
    {
        return;
    }

#if defined(_WIN32)
    FreeLibrary(libraryHandle);
#else
    dlclose(libraryHandle);
#endif
}

static bool build_default_library_path(char* output, size_t outputSize)
{
    const int written = snprintf(
        output,
        outputSize,
        ".%sZentitle2_SDK_v2.5.0%sZentitle2Core%s%s%s",
        PATH_SEPARATOR,
        PATH_SEPARATOR,
        PATH_SEPARATOR,
        PLATFORM_FOLDER,
        PATH_SEPARATOR LIBRARY_FILE_NAME);

    return written > 0 && (size_t)written < outputSize;
}

int main(int argc, char* argv[])
{
    char libraryPath[MAX_PATH_LENGTH] = {0};

    if (argc > 1)
    {
        const size_t inputLength = strlen(argv[1]);
        if (inputLength >= sizeof(libraryPath))
        {
            fprintf(stderr, "Library path is too long\n");
            return EXIT_FAILURE;
        }

        memcpy(libraryPath, argv[1], inputLength + 1U);
    }
    else if (!build_default_library_path(libraryPath, sizeof(libraryPath)))
    {
        fprintf(stderr, "Failed to build default library path\n");
        return EXIT_FAILURE;
    }

    LibraryHandle libraryHandle = load_dynamic_library(libraryPath);
    if (libraryHandle == NULL)
    {
        fprintf(stderr, "Failed to load library: %s (%s)\n", libraryPath, get_last_library_error());
        return EXIT_FAILURE;
    }

    void* loadedFunctionPointer = get_function_pointer(libraryHandle, "generateDefaultDeviceFingerprint");
    if (loadedFunctionPointer == NULL)
    {
        fprintf(stderr, "Failed to load symbol: generateDefaultDeviceFingerprint (%s)\n", get_last_library_error());
        close_dynamic_library(libraryHandle);
        return EXIT_FAILURE;
    }

    GenerateDefaultDeviceFingerprintFn generateDefaultDeviceFingerprint =
        (GenerateDefaultDeviceFingerprintFn)loadedFunctionPointer;

    char deviceFingerprint[FINGERPRINT_BUFFER_SIZE] = {0};
    int fingerprintLength = MAX_FINGERPRINT_LENGTH;

    if (!generateDefaultDeviceFingerprint(deviceFingerprint, &fingerprintLength))
    {
        fprintf(stderr, "generateDefaultDeviceFingerprint returned false\n");
        close_dynamic_library(libraryHandle);
        return EXIT_FAILURE;
    }

    if (fingerprintLength < 0 || fingerprintLength > MAX_FINGERPRINT_LENGTH)
    {
        fprintf(stderr, "Invalid fingerprint length returned by library: %d\n", fingerprintLength);
        close_dynamic_library(libraryHandle);
        return EXIT_FAILURE;
    }

    if (deviceFingerprint[fingerprintLength] != '\0')
    {
        fprintf(stderr, "Fingerprint buffer is missing a null terminator at index %d\n", fingerprintLength);
        close_dynamic_library(libraryHandle);
        return EXIT_FAILURE;
    }

    printf("Loaded library: %s\n", libraryPath);
    printf("Device fingerprint: %.*s\n", fingerprintLength, deviceFingerprint);

    close_dynamic_library(libraryHandle);
    return EXIT_SUCCESS;
}
