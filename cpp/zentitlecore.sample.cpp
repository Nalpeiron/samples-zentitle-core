#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#else
#include <dlfcn.h>
#endif

namespace
{
constexpr int kMaxFingerprintLength = 100;

#if defined(_WIN32)
using LibraryHandle = HMODULE;
constexpr const char* kLibraryFileName = "Zentitle2Core.dll";
constexpr const char* kPlatformFolder = "Windows_x86_64";
#elif defined(__APPLE__) && defined(__aarch64__)
using LibraryHandle = void*;
constexpr const char* kLibraryFileName = "libZentitle2Core.dylib";
constexpr const char* kPlatformFolder = "MacOS_arm64";
#elif defined(__APPLE__)
using LibraryHandle = void*;
constexpr const char* kLibraryFileName = "libZentitle2Core.dylib";
constexpr const char* kPlatformFolder = "MacOS_x86_64";
#elif defined(__linux__) && defined(__aarch64__)
using LibraryHandle = void*;
constexpr const char* kLibraryFileName = "libZentitle2Core.so";
constexpr const char* kPlatformFolder = "Linux_aarch64";
#elif defined(__linux__) && defined(__x86_64__)
using LibraryHandle = void*;
constexpr const char* kLibraryFileName = "libZentitle2Core.so";
constexpr const char* kPlatformFolder = "Linux_x86_64";
#else
#error "Unsupported platform"
#endif

std::string getLastLibraryError()
{
#if defined(_WIN32)
    const DWORD errorCode = GetLastError();
    if (errorCode == 0)
    {
        return "unknown error";
    }

    LPSTR messageBuffer = nullptr;
    const DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&messageBuffer),
        0,
        nullptr);

    std::string message = (size > 0 && messageBuffer != nullptr) ? std::string(messageBuffer, size) : "unknown error";
    if (messageBuffer != nullptr)
    {
        LocalFree(messageBuffer);
    }
    return message;
#else
    const char* error = dlerror();
    return error != nullptr ? std::string(error) : "unknown error";
#endif
}

LibraryHandle loadDynamicLibrary(const std::filesystem::path& libraryPath)
{
#if defined(_WIN32)
    return LoadLibraryA(libraryPath.string().c_str());
#else
    dlerror();
    return dlopen(libraryPath.string().c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void* getFunctionPointer(LibraryHandle libraryHandle, const char* functionSymbol)
{
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(libraryHandle, functionSymbol));
#else
    dlerror();
    return dlsym(libraryHandle, functionSymbol);
#endif
}

void closeDynamicLibrary(LibraryHandle libraryHandle)
{
    if (libraryHandle == nullptr)
    {
        return;
    }

#if defined(_WIN32)
    FreeLibrary(libraryHandle);
#else
    dlclose(libraryHandle);
#endif
}

class ScopedLibrary
{
public:
    explicit ScopedLibrary(const std::filesystem::path& libraryPath)
        : handle_(loadDynamicLibrary(libraryPath))
    {
        if (handle_ == nullptr)
        {
            throw std::runtime_error("Failed to load library: " + libraryPath.string() + " (" + getLastLibraryError() + ")");
        }
    }

    ~ScopedLibrary()
    {
        closeDynamicLibrary(handle_);
    }

    ScopedLibrary(const ScopedLibrary&) = delete;
    ScopedLibrary& operator=(const ScopedLibrary&) = delete;

    template <typename FunctionType>
    FunctionType getFunction(const char* functionSymbol) const
    {
        void* symbol = getFunctionPointer(handle_, functionSymbol);
        if (symbol == nullptr)
        {
            throw std::runtime_error("Failed to load symbol: " + std::string(functionSymbol) + " (" + getLastLibraryError() + ")");
        }

        return reinterpret_cast<FunctionType>(symbol);
    }

private:
    LibraryHandle handle_ = nullptr;
};

std::filesystem::path resolveLibraryPath(int argc, char* argv[])
{
    if (argc > 1)
    {
        return std::filesystem::path(argv[1]);
    }

    const std::filesystem::path defaultSdkRoot = std::filesystem::current_path() / "Zentitle2_SDK_v2.5.0";
    return defaultSdkRoot / "Zentitle2Core" / kPlatformFolder / kLibraryFileName;
}
}

int main(int argc, char* argv[])
{
    try
    {
        const std::filesystem::path libraryPath = resolveLibraryPath(argc, argv);

        if (!std::filesystem::exists(libraryPath))
        {
            std::cerr << "Library not found: " << libraryPath << '\n';
            std::cerr << "Pass the full path to the runtime library as the first argument if needed.\n";
            return EXIT_FAILURE;
        }

        ScopedLibrary library(libraryPath);

        using GenerateDefaultDeviceFingerprint = bool (*)(char*, int*);
        const auto generateDefaultDeviceFingerprint =
            library.getFunction<GenerateDefaultDeviceFingerprint>("generateDefaultDeviceFingerprint");

        std::array<char, static_cast<std::size_t>(kMaxFingerprintLength) + 1U> fingerprint = {};
        int fingerprintLength = kMaxFingerprintLength;

        const bool result = generateDefaultDeviceFingerprint(fingerprint.data(), &fingerprintLength);
        if (!result)
        {
            std::cerr << "generateDefaultDeviceFingerprint returned false\n";
            return EXIT_FAILURE;
        }

        if (fingerprintLength < 0 || fingerprintLength > kMaxFingerprintLength)
        {
            std::cerr << "Invalid fingerprint length returned by library: " << fingerprintLength << '\n';
            return EXIT_FAILURE;
        }

        if (fingerprint[static_cast<std::size_t>(fingerprintLength)] != '\0')
        {
            std::cerr << "Fingerprint buffer is missing a null terminator at index " << fingerprintLength << '\n';
            return EXIT_FAILURE;
        }

        std::cout << "Loaded library: " << libraryPath << '\n';
        std::cout << "Device fingerprint: "
                  << std::string(fingerprint.data(), static_cast<std::size_t>(fingerprintLength)) << '\n';
        return EXIT_SUCCESS;
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
