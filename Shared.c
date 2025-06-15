#include "Shared.h"
#include <proto/dos.h>
#include <proto/utility.h>
#include <string.h>
#include <ctype.h>

// CRC-32-IEEE 802.3 polynomial (0xEDB88320)
static ULONG crc32_table[256];
static BOOL crc_table_initialized = FALSE;

void InitCRC32Table(void)
{
    ULONG rem;
    int i, j;
    
    for(i = 0; i < 256; i++) {
        rem = i;
        for(j = 0; j < 8; j++) {
            if(rem & 1) {
                rem >>= 1;
                rem ^= 0xEDB88320;
            } else {
                rem >>= 1;
            }
        }
        crc32_table[i] = rem;
    }
    crc_table_initialized = TRUE;
}

ULONG CalculateChecksum(const char *filename)
{
    BPTR fh;
    ULONG crc = 0xFFFFFFFF;
    UBYTE buffer[BUFFER_SIZE];
    LONG bytes_read;
    LONG i;
    
    if (!crc_table_initialized) {
        InitCRC32Table();
    }
    
    if ((fh = Open(filename, MODE_OLDFILE)))
    {
        while ((bytes_read = Read(fh, buffer, BUFFER_SIZE)) > 0)
        {
            for (i = 0; i < bytes_read; i++) {
                crc = (crc >> 8) ^ crc32_table[(crc & 0xFF) ^ buffer[i]];
            }
        }
        Close(fh);
        // Final XOR value
        crc ^= 0xFFFFFFFF;
    }
    else
    {
        crc = 0; // Indicate error
    }
    
    return crc;
}

BOOL ParseVersionString(const char *verStr, struct VersionInfo *info)
{
    const char *p = verStr;
    LONG state = 0; // 0=find numbers, 1=version, 2=revision, 3=date
    UWORD version = 0;
    UWORD revision = 0;
    ULONG date = 0;
    ULONG num;
    struct ClockData cd;
    struct DateTime dt;
    
    // Skip non-numeric prefix
    while (*p && !isdigit(*p)) p++;
    
    // Parse version.revision
    while (*p)
    {
        if (isdigit(*p))
        {
            num = 0;
            while (isdigit(*p))
            {
                num = num * 10 + (*p - '0');
                p++;
            }
            
            if (state == 0)
            {
                version = (UWORD)num;
                state = 1;
            }
            else if (state == 1 && *p == '.')
            {
                p++;
                state = 2;
            }
            else if (state == 2)
            {
                revision = (UWORD)num;
                break;
            }
        }
        else if (*p == '(')
        {
            break; // Date parsing next
        }
        else
        {
            p++;
        }
    }
    
    // Parse date (try multiple formats)
    while (*p && *p != '(') p++;
    if (*p == '(')
    {
        p++;
        
        dt.dat_Stamp.ds_Days = 0;
        dt.dat_Stamp.ds_Minute = 0;
        dt.dat_Stamp.ds_Tick = 0;
        dt.dat_Format = FORMAT_DOS;
        dt.dat_Flags = 0;
        dt.dat_StrDay = NULL;
        dt.dat_StrDate = NULL;
        dt.dat_StrTime = NULL;
        
        if (StrToDate(&dt, (STRPTR)p))
        {
            date = (dt.dat_Stamp.ds_Days << 16) | 
                   (dt.dat_Stamp.ds_Minute << 8) | 
                   dt.dat_Stamp.ds_Tick;
        }
    }
    
    // Validate results
    if (version == 0 && revision == 0)
    {
        return FALSE; // Couldn't parse
    }
    
    info->version = version;
    info->revision = revision;
    info->date = date;
    return TRUE;
}

LONG CompareVersions(const struct VersionInfo *current, const struct VersionInfo *new)
{
    // First compare versions
    if (new->version > current->version) return 1;
    if (new->version < current->version) return -1;
    
    // Then revisions
    if (new->revision > current->revision) return 1;
    if (new->revision < current->revision) return -1;
    
    // Finally dates
    if (new->date > current->date) return 1;
    if (new->date < current->date) return -1;
    
    return 0; // Same version
}

BOOL CheckFileVersion(const char *filename, struct VersionInfo *info)
{
    BPTR fh;
    char buffer[256];
    BOOL found = FALSE;
    
    if ((fh = Open(filename, MODE_OLDFILE)))
    {
        while (FGets(fh, buffer, sizeof(buffer)))
        {
            if (strnicmp(buffer, "$VER:", 5) == 0)
            {
                if (ParseVersionString(buffer, info))
                {
                    found = TRUE;
                }
                else
                {
                    Printf("Warning: Invalid version string in %s\n", filename);
                }
                break;
            }
        }
        Close(fh);
    }
    
    if (!found)
    {
        // Fallback to file date
        BPTR lock = Lock(filename, ACCESS_READ);
        if (lock)
        {
            struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
            if (fib && Examine(lock, fib))
            {
                info->version = 0;
                info->revision = 0;
                info->date = (fib->fib_Date.ds_Days << 16) | 
                            (fib->fib_Date.ds_Minute << 8) | 
                            fib->fib_Date.ds_Tick;
                found = TRUE;
            }
            FreeDosObject(DOS_FIB, fib);
            UnLock(lock);
        }
    }
    
    return found;
}

BOOL LoadDatabaseEntry(const char *line, ULONG lineNum)
{
    LONG separators = 0;
    const char *p;
    
    if (strlen(line) > 512)
    {
        Printf("Error: Line %ld too long\n", lineNum);
        return FALSE;
    }
    
    // Count separators
    for (p = line; *p; p++)
        if (*p == '|') separators++;
        
    if (separators != 5)
    {
        Printf("Error: Corrupt entry at line %ld (wrong format)\n", lineNum);
        return FALSE;
    }
    
    return TRUE;
} 