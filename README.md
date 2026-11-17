# Zentitle2Core Library Examples
## Overview
The Zentitle2Core Library is a foundational component for developers requiring robust, reliable device fingerprinting capabilities across many programming environments and operating systems. 
This repository houses official examples demonstrating the practical integration and application of the Zentitle2Core Library within diverse development contexts. 
Developers can quickly harness the library's capabilities for security-sensitive applications requiring unique device identification through these examples.

## Supported Environments
Programming Languages: C/C++, Python, C# and more.
Operating Systems: Windows (.dll), Linux/Ubuntu (.so), MacOS (*.dylib).
Prerequisites
Before using these examples, ensure the Zentitle2Core Library is correctly installed and configured on your system. 
For installation files and detailed instructions, please refer to the official documentation of the Zentitle2Core library.

## Installation Instructions
Depending on your target operating system, follow the instructions below to prepare your environment:

### Windows
Copy the .dll file to either your project or a designated system directory.
### Linux/Ubuntu
Place the .so file in /usr/lib or an equivalent library directory.
### MacOS
Install the *.dylib file in /usr/local/lib or within your project directory.
For static library incorporation, embed the library within your project in alignment with your development environment's static library integration procedures.

### Repository Structure
This repository is organized into distinct directories, each corresponding to a different programming language demonstration:

- c/
- cpp/
- dotnet/
- python/
  
Each directory contains comprehensive instructions on compilation, execution, and any requisite preliminary setup.

### Python offline token examples

`python/zentitlecore.generate.key.and.token.sample.py` generates an AES key and
offline activation or deactivation request tokens.
`python/zentitlecore.generate.deactivation.token.sample.py` generates a deactivation
request token using the AES key saved during activation. Place the Core runtime
library next to the script and fill in the public key and activation details before
running it.

The native API in `Zentitle2Core.hpp` requires these output buffer sizes:

| Function | Minimum allocated bytes, including the null terminator |
| --- | ---: |
| `generateActivationRequestToken` | 1,000,000 |
| `parseActivationResponseToken` | 262,145 |

Both length parameters are output-only. Their initial values are ignored; passing
a smaller length does not protect a smaller allocation. The Python token examples
allocate 1,000,000 bytes and initialize the output length to zero. On success, the
returned length excludes the null terminator; on failure, it is zero.

### Customization Capabilities

Tailoring the Zentitle2Core Library to meet bespoke system requirements represents a core aspect of its design philosophy. Customization encompasses system-specific builds and naming conventions, ensuring seamless integration into tailored software solutions. Please get in touch with our technical support team for inquiries regarding advanced customization options.
