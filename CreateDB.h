#ifndef CREATEDB_H
#define CREATEDB_H

#include "Shared.h"

#define MAX_ENTRIES 1000

struct CreateDBEntry {
    ULONG checksum;
    ULONG filesize;
    char filename[108];
    UWORD version;
    UWORD revision;
    ULONG date;
    BOOL isNew;
};

// CreateDB-specific prototypes
BOOL EntryExists(const char *filename, ULONG checksum, ULONG filesize);
void ScanDirectory(const char *path, BOOL recursive);
BOOL SaveDatabase(const char *origin);
BOOL LoadExistingDB(void);

#endif /* CREATEDB_H */ 