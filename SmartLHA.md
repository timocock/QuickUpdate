# SmartLHA - Intelligent Amiga Package Installer

## Overview
SmartLHA is an intelligent package installer for AmigaOS that analyzes LHA archives and installs their contents to appropriate system locations based on file type analysis and dependency checking. It aims to maintain system stability while providing a convenient way to install software packages.

## Core Business Logic

### Package Analysis and Installation Strategy
The tool's primary function is to intelligently determine where to install files from an LHA archive based on their type and purpose. This involves several key decisions:

1. **System Component Detection**
   - Analyzes binaries using Natty's pattern matching to detect if they are system components
   - Identifies library files that might be system libraries
   - Detects device drivers and datatypes
   - Uses QuickUpdate's version checking to determine if system components should be installed

2. **Installation Location Decision Tree**
   - If a file is a system component (library, device, datatype):
     - Use QuickUpdate to check version compatibility
     - If newer version, install to system location
     - If same version, skip installation
     - If older version, warn user
   - If a file is an application binary:
     - Check if it's a standalone application or has dependencies
     - If standalone, install to C: or PROGDIR:
     - If has dependencies, consider bundle installation
   - If a file is a resource (fonts, images, etc.):
     - Check if it's system-wide or application-specific
     - Install to appropriate location (FONTS:, PROGDIR:, etc.)

3. **Dependency Analysis**
   - Uses Natty's binary analysis to detect library dependencies
   - Checks for required devices and datatypes
   - Verifies minimum version requirements
   - Flags missing dependencies for user notification

4. **Bundle vs System Installation**
   - Analyzes the entire package to determine if it should be installed as a bundle
   - Factors considered:
     - Number of shared libraries
     - Version of system components
     - Application's dependency requirements
     - User's system configuration
   - If bundle installation is chosen:
     - Creates application directory structure
     - Installs all components locally
     - Sets up PROGDIR: references

## Realistic AmigaOS Package Contents

### 1. Binary Files
- AmigaOS executables (HUNK format)
- Libraries (.library)
- Devices (.device)
- Datatypes (.datatype)
- BOOPSI classes (.class)
- MUI custom classes (.mcc)
- Gadgets (.gadget)

### 2. Documentation
- AmigaGuide documents (.guide)
- Plain text files (.txt, .doc, .readme, .inf)

### 3. Scripts
- Shell scripts
- ARexx scripts
- Startup scripts
- Installation scripts

### 4. Workbench Files
- .info files
- Icon sets
- Tool types
- Default tool preferences
- Drawer icons
- Disk icons

### 5. Localization
- Catalog files
- Language-specific resources
- Character sets
- Fonts

### 6. Source Code
- C source files (.c, .h, .i)
- Assembly source files (.s, .asm)
- Amiga E source files (.e)
- BASIC source files (.bas)
- Makefiles (including Smakefile, DMakefile)
- SAS/C project files
- DICE C project files
- vbcc project files
- gcc project files

### 7. Additional Resources
- IFF images
- ILBM images
- 8SVX audio files
- ANIM animations
- Fonts (bitmap, outline)
- Data files
- Configuration files
- Example files

## Installation Process

### 1. Initial Analysis
1. Extract LHA to RAM: for analysis
2. Build file inventory
3. Analyze binary files using similar algorithms to Natty.c
4. Check system components with QuickUpdate.c
5. Determine installation strategy

### 2. Location Determination
1. System components:
   - Use QuickUpdate for version checking
   - Install to system locations if appropriate
2. Application files:
   - Determine if bundle installation is needed
   - Set up appropriate directory structure
3. Resources:
   - Install to system or application locations
   - Handle userland resources appropriately

### 3. Installation Execution
1. Create necessary directories
2. Install files to determined locations
3. Update system assigns if needed
4. Generate installation manifest
5. Clean up temporary files

### 4. Post-Installation
1. Verify installation
2. Update system databases
3. Create uninstall information
4. Generate installation report

## Integration with Existing Tools

### 1. Natty Integration
- Use similar algorithms to Natty's binary analysis for:
  - Library dependency detection
  - System call detection
  - Version requirement analysis
  - Compatibility checking

### 2. QuickUpdate Integration
- Use QuickUpdate for:
  - System component version checking
  - Installation decision making
  - Version conflict resolution
  - System integrity verification

### 3. CreateDB Integration
- Use CreateDB's database for checking if any files in the lha are in the CreateDB database of known system library files:
  - Checksum verification
  - Version tracking

## Safety and Verification

### 1. Pre-Installation Checks
- Verify LHA archive integrity
- Check available disk space
- Verify system compatibility
- Check for existing installations

### 2. Installation Safety
- Backup existing files
- Verify file permissions
- Check for conflicts
- Maintain system stability

### 3. Post-Installation Verification
- Verify file integrity
- Check system stability
- Generate installation report
- Create uninstall information

## Technical Requirements

### 1. System Requirements
- AmigaOS 3.1 or higher
- 1MB RAM minimum
- LHA archiver
- SAS/C compiler

### 2. Development Standards
- C89 compliance
- SAS/C compatibility
- AmigaOS API usage
- Memory efficiency

## Conclusion
SmartLHA provides an intelligent approach to AmigaOS package installation, leveraging existing tools and AmigaOS conventions to ensure proper file placement and system stability. Its design focuses on understanding the nature of AmigaOS software packages and making informed decisions about installation locations and methods. 