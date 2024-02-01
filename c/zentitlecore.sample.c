#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

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

// Define the maximum path length
#define MAX_PATH_LENGTH 1024

// Function to get the current directory path
void getLibraryPath(char* libraryPath)
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  // For Windows systems, use strcpy_s for safer copying
  strcpy_s(libraryPath, MAX_PATH_LENGTH, ".");
#else
  // For Unix-based systems, use strcpy
  strcpy(libraryPath, ".");
#endif
}

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
#define LIB_EXTENSION "so"
#define LIB_PREFIX "lib"
#else
#error "Unsupported platform"
#endif

// Function to construct library full name based on platform specifics
void GetLibFullName(const char* libraryPath, const char* libraryName, char* libFullName)
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  strcpy_s(libFullName, MAX_PATH_LENGTH, libraryPath);
  strcat_s(libFullName, MAX_PATH_LENGTH, LIB_PREFIX);
  strcat_s(libFullName, MAX_PATH_LENGTH, libraryName);
  strcat_s(libFullName, MAX_PATH_LENGTH, ".");
  strcat_s(libFullName, MAX_PATH_LENGTH, LIB_EXTENSION);
#else
  strcpy(libFullName, libraryPath);
  strcat(libFullName, LIB_PREFIX);
  strcat(libFullName, libraryName);
  strcat(libFullName, ".");
  strcat(libFullName, LIB_EXTENSION);
#endif
}

bool exists(const char* filePath)
{
  FILE* file;
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  if (fopen_s(&file, filePath, "r") == 0)
#else
  if ((file = fopen(filePath, "r")))
#endif
  {
    fclose(file);
    return true;
  }
  return false;
}

// Platform-independent function to load a dynamic library
void* LoadDynamicLibrary(const char* libFullName)
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  return LoadLibraryA(libFullName);
#else
  return dlopen(libFullName, RTLD_NOW | RTLD_LOCAL);
#endif
}

// Platform-independent function to get a function pointer from a dynamic library
void* GetFunctionPointer(void* libraryHandle, const char* functionSymbol)
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  return GetProcAddress((HMODULE)libraryHandle, functionSymbol);
#else
  return dlsym(libraryHandle, functionSymbol);
#endif
}

// Platform-independent function to close a dynamic library
void CloseDynamicLibrary(void* libraryHandle)
{
#if defined(__APPLE__) || defined(__linux__)
  dlclose(libraryHandle);
#endif
}

int main(int argc, char* argv[])
{

  char libraryPath[MAX_PATH_LENGTH] = { 0 };
  getLibraryPath( libraryPath );
  const char* libraryName = "Zentitle2Core";

  char libFullName[MAX_PATH_LENGTH];
  GetLibFullName( libraryPath, libraryName, libFullName );

  if ( false == exists( libFullName ) )
  {
    printf( "Library %s does not exist\n", libFullName );
    return EXIT_FAILURE;
  }

  // Load dynamic library  
  void* libraryHandle = LoadDynamicLibrary(libFullName);
  if (NULL == libraryHandle)
  {
    printf( "Library %s could not be loaded\n", libFullName );
    return EXIT_FAILURE;
  }  

  char* functionSymbol = "generateDefaultDeviceFingerprint";

  // Get function pointer from dynamic library
  void* loadedFunctionPointer = GetFunctionPointer(libraryHandle, functionSymbol);

  if ( NULL == loadedFunctionPointer)
  {
    printf( "Function %s could not be loaded\n", functionSymbol );
    return EXIT_FAILURE;
  }

  // Cast function pointer to function type
  bool ( *getDeviceFingerprint )( char* ) = ( bool ( * )( char* ) ) loadedFunctionPointer;

  char deviceFingerprint[100];

  // Use function pointer
  bool result = getDeviceFingerprint(deviceFingerprint);

  if (false == result)
  {
    printf( "Function %s could not be executed\n", functionSymbol );
    return EXIT_FAILURE;
  }

  printf("Device fingerprint: %s\n", deviceFingerprint);

  CloseDynamicLibrary(libraryHandle);  
  return EXIT_SUCCESS;
}
