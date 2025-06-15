#include "CreateDB.h"

#include <exec/types.h>
#include <libraries/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include <stdio.h>
#include <proto/utility.h>

#define CHECKSUM_DB "PROGDIR:QuickUpdate.db"
#define MAX_PATH 256
#define MAX_ENTRIES 1000

struct DosLibrary *DOSBase = NULL;

struct Entry {
    ULONG checksum;
    ULONG filesize;
    char filename[108];
    UWORD version;
    UWORD revision;
    ULONG date;
    BOOL isNew;
    char origin[64];
};

static const char template[] = "FOLDER/A,ALL/S,ORIGIN/K";
struct {
    char *folder;
    LONG all;
    char *origin;
} args = { NULL, FALSE, NULL };

struct RDArgs *rdargs = NULL;
static const char version[] = "$VER: CreateDB 1.0 (2024-03-20)";

// Reuse these functions from QuickUpdate.c
BOOL IsValidFileType(const char *filename);
BOOL CheckFileVersion(const char *filename, struct VersionInfo *info);

struct Entry *entries = NULL;
LONG numEntries = 0;

// Add signal handling
volatile BOOL break_signal_received = FALSE;

void HandleBreak(void)
{
    break_signal_received = TRUE;
}

BOOL LoadExistingDB(void)
{
    BPTR fh;
    char line[512];
    BOOL success = TRUE;
    
    if ((fh = Open(CHECKSUM_DB, MODE_OLDFILE)))
    {
        while ((line = ReadLine(fh)))
        {
            // Skip comments and empty lines
            if (line[0] == '#' || line[0] == '\n')
            {
                FreeVec(line);
                continue;
            }
            
            struct Entry *entry = &entries[numEntries];
            
            // Parse line: CHECKSUM|FILESIZE|FILENAME|VERSION.REVISION|DATE|ORIGIN
            if (sscanf(line, "%lx|%lu|%[^|]|%hu.%hu|%lu|%[^\n]",
                      &entry->checksum, &entry->filesize, entry->filename,
                      &entry->version, &entry->revision,
                      &entry->date, entry->origin) == 7)
            {
                entry->isNew = FALSE;
                numEntries++;
                
                if (numEntries >= MAX_ENTRIES)
                {
                    Printf("Warning: Maximum entries reached\n");
                    break;
                }
            }
            else
            {
                Printf("Warning: Skipping invalid entry in database\n");
            }
            FreeVec(line);
        }
        Close(fh);
    }
    else
    {
        // No existing DB is not an error
        success = TRUE;
    }
    
    return success;
}

BOOL EntryExists(const char *filename, ULONG checksum, ULONG filesize)
{
    for (LONG i = 0; i < numEntries; i++)
    {
        if (stricmp(FilePart(filename), entries[i].filename) == 0 &&
            checksum == entries[i].checksum &&
            filesize == entries[i].filesize)
        {
            return TRUE;
        }
    }
    return FALSE;
}

void ScanDirectory(const char *path, BOOL recursive)
{
    BPTR lock, oldDir;
    struct FileInfoBlock *fib;
    char fullpath[MAX_PATH];
    
    if ((lock = Lock(path, ACCESS_READ)))
    {
        if ((fib = AllocDosObject(DOS_FIB, NULL)))
        {
            oldDir = CurrentDir(lock);
            
            if (Examine(lock, fib))
            {
                while (ExNext(lock, fib))
                {
                    if (fib->fib_DirEntryType > 0)  // Directory
                    {
                        if (recursive)
                        {
                            strncpy(fullpath, path, MAX_PATH - 1);
                            fullpath[MAX_PATH - 1] = '\0';
                            AddPart(fullpath, fib->fib_FileName, MAX_PATH);
                            ScanDirectory(fullpath, TRUE);
                        }
                    }
                    else  // File
                    {
                        if (IsValidFileType(fib->fib_FileName))
                        {
                            strncpy(fullpath, path, MAX_PATH - 1);
                            fullpath[MAX_PATH - 1] = '\0';
                            AddPart(fullpath, fib->fib_FileName, MAX_PATH);
                            
                            ULONG checksum = CalculateChecksum(fullpath);
                            
                            if (!EntryExists(fib->fib_FileName, checksum, fib->fib_Size))
                            {
                                struct VersionInfo info;
                                if (CheckFileVersion(fullpath, &info))
                                {
                                    struct Entry *entry = &entries[numEntries];
                                    entry->checksum = checksum;
                                    entry->filesize = fib->fib_Size;
                                    strcpy(entry->filename, fib->fib_FileName);
                                    entry->version = info.version;
                                    entry->revision = info.revision;
                                    entry->date = info.date;
                                    entry->isNew = TRUE;
                                    numEntries++;
                                    
                                    Printf("Found: %s (v%ld.%ld, %ld bytes)\n", 
                                           fib->fib_FileName,
                                           info.version, info.revision,
                                           fib->fib_Size);
                                    
                                    if (numEntries >= MAX_ENTRIES)
                                    {
                                        Printf("Warning: Maximum entries reached\n");
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            CurrentDir(oldDir);
            FreeDosObject(DOS_FIB, fib);
        }
        UnLock(lock);
    }
}

BOOL SaveDatabase(const char *origin)
{
    BPTR fh;
    char tempDB[MAX_PATH];
    BOOL success = FALSE;
    LONG writeError = FALSE;
    
    // Create temporary file
    strcpy(tempDB, CHECKSUM_DB);
    strcat(tempDB, ".new");
    
    if ((fh = Open(tempDB, MODE_NEWFILE)))
    {
        // Set buffer for better performance
        SetVBuf(fh, NULL, BUF_LINE, 4096);
        
        // Write header
        if (FPuts(fh, "# QuickUpdate Checksum Database\n") == -1 ||
            FPuts(fh, "# Format: CHECKSUM|FILESIZE|FILENAME|VERSION.REVISION|DATE|ORIGIN\n") == -1)
        {
            Printf("Error writing database header\n");
            writeError = TRUE;
        }

        // Write all entries if header was successful
        if (!writeError)
        {
            for (LONG i = 0; i < numEntries; i++)
            {
                if (VFPrintf(fh, "%08lx|%lu|%s|%d.%d|%lu|%s\n",
                           entries[i].checksum,
                           entries[i].filesize,
                           entries[i].filename,
                           entries[i].version,
                           entries[i].revision,
                           entries[i].date,
                           entries[i].isNew ? origin : entries[i].origin) == -1)
                {
                    Printf("Error writing database entry %ld\n", i);
                    writeError = TRUE;
                    break;
                }
            }
        }

        // Only proceed if all writes were successful
        if (!writeError)
        {
            Close(fh);
            
            // Check if original file exists
            BPTR lock = Lock(CHECKSUM_DB, ACCESS_READ);
            BOOL exists = lock ? TRUE : FALSE;
            if (lock) UnLock(lock);

            // Delete existing file if present
            if (exists)
            {
                if (!DeleteFile(CHECKSUM_DB))
                {
                    Printf("Error removing old database (read-only?)\n");
                    DeleteFile(tempDB);
                    return FALSE;
                }
            }

            // Rename temp file
            if (Rename(tempDB, CHECKSUM_DB))
            {
                success = TRUE;
            }
            else
            {
                Printf("Error renaming database file\n");
                // Try to preserve temp file for recovery
                DeleteFile(tempDB);
            }
        }
        else
        {
            Close(fh);
            DeleteFile(tempDB);
            Printf("Aborting due to write errors\n");
        }
    }
    else
    {
        Printf("Error creating temporary database (disk full? read-only?)\n");
    }

    // Set protection if we succeeded
    if (success)
    {
        if (!SetProtection(CHECKSUM_DB, FIBF_READ|FIBF_WRITE))
        {
            Printf("Warning: Could not set file protection\n");
        }
    }

    return success;
}

ULONG CalculateChecksum(const char *filename)
{
    BPTR fh;
    ULONG checksum = 0;
    UBYTE buffer[4096];  // Smaller 4KB buffer for 68000
    LONG bytes_read;
    
    if ((fh = Open(filename, MODE_OLDFILE)))
    {
        while ((bytes_read = Read(fh, buffer, sizeof(buffer))) > 0)
        {
            if (break_signal_received)
            {
                Close(fh);
                return 0;
            }
            
            // Simple rotating XOR - very fast on 68000
            for (LONG i = 0; i < bytes_read; i++)
            {
                checksum = (checksum << 1) | (checksum >> 31);  // Rotate left
                checksum ^= buffer[i];
            }
        }
        Close(fh);
    }
    
    return checksum;
}

BOOL LoadDatabaseEntry(const char *line, ULONG lineNum)
{
    if (strlen(line) > 512)
    {
        Printf("Error: Line %ld too long\n", lineNum);
        return FALSE;
    }
    
    // Count separators
    LONG separators = 0;
    for (const char *p = line; *p; p++)
        if (*p == '|') separators++;
        
    if (separators != 5)
    {
        Printf("Error: Corrupt entry at line %ld (wrong format)\n", lineNum);
        return FALSE;
    }
    
    // Rest of parsing with validation...
    return TRUE;
}

// Update entry point to use _main
LONG _main(LONG argc, char **argv)
{
    LONG result = RETURN_FAIL;
    char origin[64];
    LONG newEntries = 0;
    struct Task *task = FindTask(NULL);
    ULONG oldSignals = SetSignal(0, SIGBREAKF_CTRL_C);
    
    // Set up break handling
    break_signal_received = FALSE;
    
    if ((DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 37)))
    {
        if ((entries = AllocMem(sizeof(struct Entry) * MAX_ENTRIES, MEMF_CLEAR|MEMF_PUBLIC)))
        {
            rdargs = ReadArgs(template, (LONG *)&args, NULL);
            if (rdargs)
            {
                if (args.folder)
                {
                    if (LoadExistingDB())
                    {
                        LONG startEntries = numEntries;
                        
                        Printf("Scanning directory: %s\n", (LONG)args.folder);
                        ScanDirectory(args.folder, args.all);
                        
                        // Check if break was received during scan
                        if (break_signal_received)
                        {
                            Printf("\n*** Break received - aborting ***\n");
                            goto cleanup;
                        }
                        
                        newEntries = numEntries - startEntries;
                        Printf("\nFound %ld new files\n", newEntries);
                        
                        if (newEntries > 0)
                        {
                            if (args.origin)
                            {
                                strncpy(origin, args.origin, sizeof(origin)-1);
                                origin[sizeof(origin)-1] = '\0';
                            }
                            else
                            {
                                Printf("Enter origin for new entries: ");
                                if (FGets(Input(), origin, sizeof(origin)))
                                {
                                    // Remove newline
                                    char *nl = strchr(origin, '\n');
                                    if (nl) *nl = '\0';
                                }
                                else
                                {
                                    Printf("Error reading origin input\n");
                                    goto cleanup;
                                }
                            }
                            
                            // Once we start writing, we complete even if break received
                            if (SaveDatabase(origin))
                            {
                                Printf("Database updated successfully\n");
                                result = RETURN_OK;
                            }
                        }
                        else
                        {
                            Printf("No new entries found\n");
                            result = RETURN_OK;
                        }
                    }
                    else
                    {
                        Printf("Error loading existing database\n");
                    }
                }
                else
                {
                    Printf("Error: FOLDER argument is required\n");
                    Printf("Usage: CreateDB FOLDER=<path> [ALL/S] [ORIGIN=<text>]\n");
                }
            cleanup:
                FreeArgs(rdargs);
            }
            else
            {
                Printf("Error parsing arguments\n");
                Printf("Usage: CreateDB FOLDER=<path> [ALL/S] [ORIGIN=<text>]\n");
            }
            FreeMem(entries, sizeof(struct Entry) * MAX_ENTRIES);
        }
        else
        {
            Printf("Error: Out of memory\n");
        }
        CloseLibrary((struct Library *)DOSBase);
    }
    else
    {
        Printf("Error: Could not open dos.library\n");
    }
    
    // Restore break signal handling
    SetSignal(oldSignals, SIGBREAKF_CTRL_C);
    
    return result;
} 