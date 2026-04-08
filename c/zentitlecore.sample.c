#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

#define MAX_PATH_LENGTH 1024
#define MAX_FINGERPRINT_LENGTH 100
#define FINGERPRINT_BUFFER_SIZE (MAX_FINGERPRINT_LENGTH + 1)

#if defined(_WIN32)
typedef HMODULE LibraryHandle;
#define PATH_SEPARATOR "\\"
#define LIBRARY_FILE_NAME "Zentitle2Core.dll"
#elif defined(__APPLE__)
typedef void* LibraryHandle;
#define PATH_SEPARATOR "/"
#define LIBRARY_FILE_NAME "libZentitle2Core.dylib"
#elif defined(__linux__)
typedef void* LibraryHandle;
#define PATH_SEPARATOR "/"
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
    int wideLength = MultiByteToWideChar(CP_UTF8, 0, libraryPath, -1, NULL, 0);
    WCHAR* wideLibraryPath = NULL;
    LibraryHandle libraryHandle = NULL;

    if (wideLength <= 0)
    {
        return NULL;
    }

    wideLibraryPath = (WCHAR*)malloc((size_t)wideLength * sizeof(WCHAR));
    if (wideLibraryPath == NULL)
    {
        SetLastError(ERROR_OUTOFMEMORY);
        return NULL;
    }

    if (MultiByteToWideChar(CP_UTF8, 0, libraryPath, -1, wideLibraryPath, wideLength) <= 0)
    {
        free(wideLibraryPath);
        return NULL;
    }

    libraryHandle = LoadLibraryW(wideLibraryPath);
    free(wideLibraryPath);
    return libraryHandle;
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

static GenerateDefaultDeviceFingerprintFn load_generate_default_device_fingerprint(void* loadedFunctionPointer)
{
#if defined(__GNUC__) && !defined(_WIN32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
    return (GenerateDefaultDeviceFingerprintFn)loadedFunctionPointer;
#if defined(__GNUC__) && !defined(_WIN32)
#pragma GCC diagnostic pop
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

static bool get_executable_directory(char* output, size_t outputSize)
{
#if defined(_WIN32)
    WCHAR widePath[MAX_PATH_LENGTH] = {0};
    char executablePath[MAX_PATH_LENGTH] = {0};
    const DWORD pathLength = GetModuleFileNameW(NULL, widePath, MAX_PATH_LENGTH);
    const char* lastSeparator = NULL;
    size_t directoryLength = 0;

    if (pathLength == 0 || pathLength >= MAX_PATH_LENGTH)
    {
        return false;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, widePath, -1, executablePath, MAX_PATH_LENGTH, NULL, NULL) <= 0)
    {
        return false;
    }
#elif defined(__APPLE__)
    char executablePath[MAX_PATH_LENGTH] = {0};
    uint32_t pathLength = (uint32_t)sizeof(executablePath);
    const char* lastSeparator = NULL;
    size_t directoryLength = 0;

    if (_NSGetExecutablePath(executablePath, &pathLength) != 0)
    {
        return false;
    }
#else
    char executablePath[MAX_PATH_LENGTH] = {0};
    const ssize_t pathLength = readlink("/proc/self/exe", executablePath, sizeof(executablePath) - 1U);
    const char* lastSeparator = NULL;
    size_t directoryLength = 0;

    if (pathLength <= 0)
    {
        return false;
    }

    executablePath[pathLength] = '\0';
#endif

    lastSeparator = strrchr(executablePath, PATH_SEPARATOR[0]);

#if defined(_WIN32)
    const char* alternateSeparator = strrchr(executablePath, '/');
    if (alternateSeparator != NULL && (lastSeparator == NULL || alternateSeparator > lastSeparator))
    {
        lastSeparator = alternateSeparator;
    }
#endif

    if (lastSeparator != NULL)
    {
        directoryLength = (size_t)(lastSeparator - executablePath) + 1U;
    }

    if (directoryLength == 0 || directoryLength >= outputSize)
    {
        return false;
    }

    memcpy(output, executablePath, directoryLength);
    output[directoryLength] = '\0';
    return true;
}

static bool build_default_library_path(char* output, size_t outputSize)
{
    char executableDirectory[MAX_PATH_LENGTH] = {0};
    int written = 0;

    if (!get_executable_directory(executableDirectory, sizeof(executableDirectory)))
    {
        return false;
    }

    written = snprintf(output, outputSize, "%s%s", executableDirectory, LIBRARY_FILE_NAME);
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
        load_generate_default_device_fingerprint(loadedFunctionPointer);

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
