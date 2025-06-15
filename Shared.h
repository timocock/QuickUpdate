#ifndef SHARED_H
#define SHARED_H

#include <exec/types.h>
#include <libraries/dos.h>
#include <proto/dos.h>

#define CHECKSUM_DB "PROGDIR:QuickUpdate.db"
#define BUFFER_SIZE 8192
#define MAX_PATH 256

// Version information structure
struct VersionInfo {
    UWORD version;
    UWORD revision;
    ULONG date;
    char origin[64];
};

// Database entry structure
struct ChecksumEntry {
    ULONG checksum;
    ULONG filesize;
    char filename[108];  // Matches standard Amiga filename length
    UWORD version;
    UWORD revision;
    ULONG date;
    char origin[64];
};

// CRC32 lookup table
extern const ULONG crc32_table[256];

// Shared function prototypes
ULONG CalculateChecksum(const char *filename);
void InitCRC32Table(void);  // Internal use only
BOOL ParseVersionString(const char *verStr, struct VersionInfo *info);
LONG CompareVersions(const struct VersionInfo *current, const struct VersionInfo *new);
BOOL CheckFileVersion(const char *filename, struct VersionInfo *info);
BOOL LoadDatabaseEntry(const char *line, ULONG lineNum);

#endif /* SHARED_H */ 