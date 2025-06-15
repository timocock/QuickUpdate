# Amiga System Tools

This repository contains three essential tools for Amiga system management and compatibility checking.

## CreateDB.c

`CreateDB.c` is a utility for creating and maintaining a checksum database of system files. It helps track file versions and changes across the system.

### Key Features:
- Creates and updates a checksum database (`QuickUpdate.db`)
- Scans directories for system files (libraries, devices, datatypes)
- Calculates checksums and file sizes
- Tracks version information and file origins
- Supports recursive directory scanning
- Handles file protection and backup operations

### Usage:
```
CreateDB FOLDER=<path> [ALL/S] [ORIGIN=<text>]
```
- `FOLDER`: Required. Path to scan for files
- `ALL`: Optional. Enable recursive directory scanning
- `ORIGIN`: Optional. Source identifier for new entries

## Natty.c

`Natty.c` is a heuristic compatibility scanner for Amiga HUNK binaries. It analyzes executables for potential compatibility issues in a hardened OS environment.

### Key Features:
- Scans HUNK binaries for risky patterns
- Detects library calls via LVO (Library Vector Offset)
- Identifies potentially dangerous operations:
  - Direct ExecBase access
  - Chip memory references
  - ROM references
  - Vector patching
  - Trap calls
  - TCB access
  - List manipulation
  - Interrupt level manipulation
  - VBR manipulation
  - Self-modifying code
- Generates detailed compatibility reports
- Assigns risk scores to findings

### Usage:
```
HunkScan <file1> [file2...]
```

## QuickUpdate.c

`QuickUpdate.c` is a system file update manager that handles version checking and installation of system components.

### Key Features:
- GUI and CLI interfaces
- Version comparison and management
- Automatic backup of existing files
- Checksum verification
- Support for multiple file types:
  - Libraries
  - Devices
  - Datatypes
  - Classes
  - Gadgets
  - Resources
  - MUI Custom Classes (MCC)
  - MUI Control Panels (MCP)
- Workbench integration
- Interactive and non-interactive modes

### Usage:
```
QuickUpdate FILE=<file> [NONINTERACTIVE/S] [QUIET/S] [FORCE/S]
```
- `FILE`: Required. File to check/update
- `NONINTERACTIVE`: Optional. Run without user prompts
- `QUIET`: Optional. Minimize output
- `FORCE`: Optional. Force installation regardless of version

## Common Features

All three tools share some common characteristics:
- AmigaOS 3.x compatibility
- Robust error handling
- Memory-efficient operation
- Support for both Workbench and CLI environments
- Detailed logging and status reporting

## Building

These tools are designed to be built with SAS/C 6.x or compatible Amiga C compilers. They require the following libraries:
- exec.library
- dos.library
- intuition.library (for GUI components)
- asl.library (for file requesters)
- utility.library
- gadtools.library

## License

[Add appropriate license information here]

## Contributing

[Add contribution guidelines here] 