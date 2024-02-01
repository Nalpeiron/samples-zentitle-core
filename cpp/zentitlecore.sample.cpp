#include <filesystem>
#include <iostream>
#include <algorithm>
#include <string>
#include <string>
#include <fstream>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#else
#include <dlfcn.h>
#endif

// Define platform-specific library extension and prefix
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#include <windows.h>
#define LIB_EXTENSION "dll"
#define LIB_PREFIX ""
#elif defined(__APPLE__)
#include <dlfcn.h>
#define LIB_EXTENSION "dylib"
#define LIB_PREFIX "lib"
#elif defined(__linux__)
#include <dlfcn.h>
#include <cstring>
#define LIB_EXTENSION "so"
#define LIB_PREFIX "lib"
#else
#error "Unsupported platform"
#endif

// Function to construct library full name based on platform specifics
std::string GetLibFullName(const std::string& libraryPath, const std::string& libraryName) 
{
  return libraryPath + LIB_PREFIX + libraryName + "." + LIB_EXTENSION;
}

// Platform-independent function to load a dynamic library
void* LoadDynamicLibrary(const std::string& libFullName) {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  return LoadLibraryA(libFullName.c_str());
#else
  return dlopen(libFullName.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

// Platform-independent function to get a function pointer from a dynamic library
void* GetFunctionPointer(void* libraryHandle, const std::string& functionSymbol) {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  return GetProcAddress((HMODULE)libraryHandle, functionSymbol.c_str());
#else
  return dlsym(libraryHandle, functionSymbol.c_str());
#endif
}

// Platform-independent function to close a dynamic library
inline void CloseDynamicLibrary(void* libraryHandle) 
{
#if defined(__APPLE__) || defined(__linux__)
  dlclose(libraryHandle);
#endif
}

int main(int argc, char* argv[])
{
  // Library path should be relative to the executable
  const std::string libraryPath = "C:\\Path\\To\\Library\\";
  const std::string libraryName = "Zentitle2Core";

  // Construct library full name based on platform specifics
  std::string libFullName = GetLibFullName(libraryPath, libraryName);

  // Load dynamic library
  void* libraryHandle = LoadDynamicLibrary(libFullName);

  std::string functionSymbol = "generateDefaultDeviceFingerprint";

  // Get function pointer from dynamic library
  void* loadedFunctionPointer = GetFunctionPointer(libraryHandle, functionSymbol);
  if (loadedFunctionPointer == nullptr)
  {
    std::cout << "Failed to load function pointer" << std::endl;
    return EXIT_FAILURE;
  }

  // Cast function pointer to function type
  auto getDeviceFingerprint = reinterpret_cast<bool (*)(char*)>(loadedFunctionPointer);

  char* deviceFingerprint = new char[100];

  // Use function pointer
  bool result = getDeviceFingerprint(deviceFingerprint);
  if (result == false)
  {
    std::cout << "Failed to get device fingerprint" << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "Device fingerprint: " << deviceFingerprint << std::endl;

  delete[] deviceFingerprint;
  CloseDynamicLibrary(libraryHandle);

  return EXIT_SUCCESS;
}
