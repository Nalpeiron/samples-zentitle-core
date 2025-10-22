#!/usr/bin/env python3
"""
generate_deactivation_token.py (OFFICIAL, placeholders)

Purpose:
  Generate an OFFLINE deactivation request token for Zentitle2.
  Requires the same RSA public key and AES key that were used for the activation flow.

Outputs:
  - aes_key_deactivation.base64.txt (copy of the reused activation AES key)
  - offline_deactivation_request_token.txt
"""

import os
import sys
import json
import ctypes
import platform
import datetime
from ctypes import c_char_p, c_int, POINTER, create_string_buffer, byref

# ---------- Library autodetect ----------
system = platform.system()
DEFAULT_LIB_NAME = {
    "Windows": "Zentitle2Core.dll",
    "Darwin": "libZentitle2Core.dylib",
}.get(system, "libZentitle2Core.so")
LIB_NAME = os.environ.get("ZENTITLE_LIB_NAME", DEFAULT_LIB_NAME)

HERE = os.path.dirname(os.path.abspath(__file__))
LIB_PATH = os.path.join(HERE, LIB_NAME)
if not os.path.exists(LIB_PATH):
    print(f"❌ Library not found: {LIB_PATH}")
    print("ℹ️  Place the DLL/.so/.dylib next to this script or export ZENTITLE_LIB_NAME.")
    sys.exit(2)

try:
    load_mode = ctypes.RTLD_GLOBAL
except AttributeError:
    load_mode = None

if system == "Windows":
    if hasattr(os, "add_dll_directory"):
        os.add_dll_directory(HERE)
    lib = ctypes.CDLL(LIB_PATH)
else:
    lib = ctypes.CDLL(LIB_PATH, mode=load_mode) if load_mode else ctypes.CDLL(LIB_PATH)

# ---------- Signatures ----------
lib.enableLogging.argtypes = (c_char_p,)
lib.enableLogging.restype = None

lib.generateActivationRequestToken.argtypes = (c_char_p, POINTER(c_int), c_char_p, c_char_p, c_char_p)
lib.generateActivationRequestToken.restype = ctypes.c_bool

lib.getVersion.argtypes = (c_char_p, POINTER(c_int))
lib.getVersion.restype = ctypes.c_bool

# ---------- CONFIG (USER MUST FILL) ----------
PUBLIC_KEY_PEM = b"""-----BEGIN PUBLIC KEY-----
<INSERT YOUR RSA PUBLIC KEY HERE>
-----END PUBLIC KEY-----
"""

ACTIVATION_ID = "<INSERT ACTIVATION ID>"  # e.g., "act_01HZYAC4XMVFVGKJ4ZX0S3MY0K"
PARSED_ACTIVATION_FILE = "parsed_activation_response.json"  # fallback source for ActivationId

# AES handling: reuse the activation AES key (Base64)
ACTIVATION_AES_KEY = ""  # optional inline override
ACTIVATION_AES_FILE = "aes_key_generated.base64.txt"  # produced by generate_key_and_token.py

DEACTIVATION_TOKEN_ISSUE_DATE = ""  # leave blank to auto-fill with current UTC

AES_KEY_FILE_OUT = "aes_key_deactivation.base64.txt"
DEACTIVATION_TOKEN_OUT = "offline_deactivation_request_token.txt"

BUFFER_SIZE = 100_000

# ---------- Helpers ----------
def iso8601_utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="microseconds")

def require_public_key() -> None:
    pem = PUBLIC_KEY_PEM.decode("utf-8", "ignore")
    if "<INSERT" in pem or "BEGIN PUBLIC KEY" not in pem:
        print("❌ PUBLIC_KEY_PEM is not set. Paste your RSA public key (PEM format).")
        sys.exit(1)

def resolve_activation_id() -> str:
    if ACTIVATION_ID and "<INSERT" not in ACTIVATION_ID and ACTIVATION_ID.strip():
        return ACTIVATION_ID.strip()
    path = os.path.join(HERE, PARSED_ACTIVATION_FILE)
    try:
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        act_id = data["ActivationState"]["Id"]
        if not act_id:
            raise KeyError("ActivationState.Id missing")
        return str(act_id)
    except FileNotFoundError:
        print("❌ ACTIVATION_ID not provided and parsed file not found:", path)
        sys.exit(1)
    except Exception as exc:
        print(f"❌ Failed to read ActivationId from {path}: {exc}")
        sys.exit(1)

def resolve_activation_aes() -> str:
    if ACTIVATION_AES_KEY and "<INSERT" not in ACTIVATION_AES_KEY:
        return ACTIVATION_AES_KEY.strip()
    path = os.path.join(HERE, ACTIVATION_AES_FILE)
    try:
        with open(path, "r", encoding="utf-8") as f:
            key = f.read().strip()
        if not key:
            raise ValueError("file empty")
        return key
    except FileNotFoundError:
        print("❌ Activation AES key not provided and file not found:", path)
        print("   Run generate_key_and_token.py first or set ACTIVATION_AES_KEY.")
        sys.exit(1)
    except Exception as exc:
        print(f"❌ Failed to read AES key from {path}: {exc}")
        sys.exit(1)

# ---------- Main ----------
def main() -> None:
    print("=== Zentitle2: Generate OFFLINE Deactivation Token ===")
    print("Library:", LIB_PATH)

    lib.enableLogging(None)

    try:
        buf = create_string_buffer(1024)
        n = c_int(1024)
        if lib.getVersion(buf, byref(n)):
            print("Version:", buf.value.decode("utf-8", "replace"))
    except Exception:
        pass

    require_public_key()
    activation_id = resolve_activation_id()
    print("ActivationId:", activation_id)

    aes_key_b64 = resolve_activation_aes()
    print("Using activation AES key (Base64): [hidden] len=", len(aes_key_b64))

    token_issue_date = (
        DEACTIVATION_TOKEN_ISSUE_DATE.strip()
        if DEACTIVATION_TOKEN_ISSUE_DATE and "<" not in DEACTIVATION_TOKEN_ISSUE_DATE
        else iso8601_utc_now()
    )

    payload = {
        "ActivationId": activation_id,
        "TokenIssueDate": token_issue_date,
    }
    payload_json = json.dumps(payload, separators=(",", ":")).encode("utf-8")

    out = create_string_buffer(BUFFER_SIZE)
    out_len = c_int(BUFFER_SIZE)

    ok = lib.generateActivationRequestToken(
        out,
        byref(out_len),
        payload_json,
        aes_key_b64.encode("ascii"),
        PUBLIC_KEY_PEM,
    )
    print(f"Deactivation token result: {ok} | length: {out_len.value}")

    if not ok or out_len.value <= 3:
        print("❌ Deactivation FAILED – invalid RSA public key or payload.")
        sys.exit(1)

    token = out.value.decode("utf-8", "replace")

    with open(os.path.join(HERE, AES_KEY_FILE_OUT), "w", encoding="utf-8") as f:
        f.write(aes_key_b64)
    with open(os.path.join(HERE, DEACTIVATION_TOKEN_OUT), "w", encoding="utf-8") as f:
        f.write(token)

    print("\n✅ SUCCESS – deactivation token generated.")
    print("Preview:", token[:100] + ("..." if len(token) > 100 else ""))
    print("\nSaved:")
    print(" -", AES_KEY_FILE_OUT)
    print(" -", DEACTIVATION_TOKEN_OUT)

if __name__ == "__main__":
    main()
