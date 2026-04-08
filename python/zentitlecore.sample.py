#!/usr/bin/env python3
import ctypes
import os
import platform
import sys
from ctypes import POINTER, byref, c_bool, c_char, c_int, create_string_buffer

MAX_FINGERPRINT_LENGTH = 100
FINGERPRINT_BUFFER_SIZE = MAX_FINGERPRINT_LENGTH + 1


def default_library_name() -> str:
    system = platform.system()
    if system == "Windows":
        return "Zentitle2Core.dll"
    if system == "Darwin":
        return "libZentitle2Core.dylib"
    return "libZentitle2Core.so"


def load_dynamic_library():
    here = os.path.dirname(os.path.abspath(__file__))
    if len(sys.argv) > 1:
        library_path = sys.argv[1]
    else:
        library_path = os.path.join(here, default_library_name())

    if not os.path.exists(library_path):
        print(f"Library not found: {library_path}", file=sys.stderr)
        print(
            "Place the runtime library next to the script or pass its full path as the first argument.",
            file=sys.stderr,
        )
        sys.exit(1)

    try:
        load_mode = ctypes.RTLD_GLOBAL
    except AttributeError:
        load_mode = None

    if platform.system() == "Windows":
        library_dir = os.path.dirname(library_path)
        if hasattr(os, "add_dll_directory") and library_dir:
            os.add_dll_directory(library_dir)
        return ctypes.CDLL(library_path), library_path

    if load_mode is not None:
        return ctypes.CDLL(library_path, mode=load_mode), library_path
    return ctypes.CDLL(library_path), library_path


lib, loaded_library_path = load_dynamic_library()

generate_default_device_fingerprint = lib.generateDefaultDeviceFingerprint
generate_default_device_fingerprint.restype = c_bool
generate_default_device_fingerprint.argtypes = [POINTER(c_char), ctypes.POINTER(c_int)]

device_fingerprint = create_string_buffer(FINGERPRINT_BUFFER_SIZE)
fingerprint_length = c_int(MAX_FINGERPRINT_LENGTH)

result = generate_default_device_fingerprint(device_fingerprint, byref(fingerprint_length))
if not result:
    print("generateDefaultDeviceFingerprint returned false", file=sys.stderr)
    sys.exit(1)

if fingerprint_length.value < 0 or fingerprint_length.value > MAX_FINGERPRINT_LENGTH:
    print(f"Invalid fingerprint length returned by library: {fingerprint_length.value}", file=sys.stderr)
    sys.exit(1)

if device_fingerprint.raw[fingerprint_length.value] != 0:
    print(
        f"Fingerprint buffer is missing a null terminator at index {fingerprint_length.value}",
        file=sys.stderr,
    )
    sys.exit(1)

print(f"Loaded library: {loaded_library_path}")
print("Device Fingerprint:", device_fingerprint.raw[: fingerprint_length.value].decode("utf-8", "replace"))
