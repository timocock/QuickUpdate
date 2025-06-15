#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/gadtools.h>
#include <proto/window.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/asl.h>
#include <workbench/workbench.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "Shared.h"
#include "QuickUpdate.h"

// Global variable definitions
struct DosLibrary *DOSBase = NULL;
struct Library *AslBase = NULL;

// Structure to hold version information
struct VersionInfo {
    UWORD version;
    UWORD revision;
    ULONG date;
    char origin[64];
};

// Add to existing struct definitions
struct DiskObject *AppIcon = NULL;
struct MsgPort *AppPort = NULL;

// Function prototypes
BOOL OpenLibraries(void);
void CloseLibraries(void);
BOOL HandleWorkbench(void);
BOOL HandleCLI(int argc, char **argv);
BOOL CheckFileVersion(const char *filename, struct VersionInfo *info);
BOOL InstallFile(const char *source, const char *dest);
BOOL VerifyChecksum(const char *filename);
BOOL HandleGUI(void);
void ShowFileRequester(void);
void ShowAboutRequester(void);
void ProcessFile(const char *filepath);
BOOL HandleAppMessage(struct AppMessage *msg);
BOOL CreateAppIcon(void);
BOOL IsValidFileType(const char *filename);
BOOL GetInstalledVersion(const char *filename, struct VersionInfo *info);
BOOL ParseVersionString(const char *verStr, struct VersionInfo *info);
LONG CompareVersions(const struct VersionInfo *current, const struct VersionInfo *new);
BOOL GetUserResponse(void);
BOOL Copy(const char *source, const char *dest);
BOOL BackupFile(const char *filepath);
void SetStatusText(const char *text);

// Menu IDs
#define ID_OPEN  1
#define ID_ABOUT 2
#define ID_QUIT  3
#define ID_CHECK 4

struct RDArgs *rdargs = NULL;
static const char template[] = "FILE/A,NONINTERACTIVE/S,QUIET/S,FORCE/S";
static const char version[] = "$VER: QuickUpdate 1.0 (2024-03-20)";

struct {
    char *file;          // Required argument (/A)
    LONG noninteractive; // Switch (/S)
    LONG quiet;          // Switch (/S)
    LONG force;          // Switch (/S)
} args = { NULL, FALSE, FALSE, FALSE };

#define CHECKSUM_DB "PROGDIR:QuickUpdate.db"
#define BACKUP_DIR "SYS:Backups/QuickUpdate/"
#define BUFFER_SIZE 8192

struct ChecksumEntry {
    ULONG checksum;
    char filename[108];  // Matches standard Amiga filename length
    UWORD version;
    UWORD revision;
    ULONG date;
    char origin[64];
};

// Window-related globals
static struct Window *MainWindow = NULL;
static Object *MainWindowObj = NULL;
static struct Menu *MainMenu = NULL;

BOOL CalculateChecksum(const char *filename)
{
    BPTR fh;
    ULONG crc = 0xFFFFFFFF;
    UBYTE buffer[BUFFER_SIZE];
    LONG bytes_read;
    LONG i;
    
    // Initialize CRC32 table if not already done
    static BOOL table_initialized = FALSE;
    if (!table_initialized) {
        InitCRC32Table();
        table_initialized = TRUE;
    }
    
    if ((fh = Open(filename, MODE_OLDFILE)))
    {
        while ((bytes_read = Read(fh, buffer, BUFFER_SIZE)) > 0)
        {
            for (i = 0; i < bytes_read; i++)
            {
                crc = (crc >> 8) ^ crc32_table[(crc & 0xFF) ^ buffer[i]];
            }
        }
        Close(fh);
        crc ^= 0xFFFFFFFF;
        return TRUE;
    }
    return FALSE;
}

BOOL VerifyChecksum(const char *filename)
{
    BPTR fh;
    BOOL found = FALSE;
    struct ChecksumEntry entry;
    ULONG actual_checksum;
    
    actual_checksum = CalculateChecksum(filename);
    
    if ((fh = Open(CHECKSUM_DB, MODE_OLDFILE)))
    {
        while (Read(fh, &entry, sizeof(entry)) == sizeof(entry))
        {
            if (stricmp(FilePart(filename), entry.filename) == 0)
            {
                if (actual_checksum == entry.checksum)
                {
                    // Match found, update version info
                    found = TRUE;
                    break;
                }
            }
        }
        Close(fh);
    }
    
    return found;
}

BOOL BackupFile(const char *filepath)
{
    char backup_path[256];
    char datestamp[32];
    struct DateTime dt;
    
    // Get current date/time
    DateStamp(&dt.dat_Stamp);
    dt.dat_Format = FORMAT_DOS;
    dt.dat_Flags = 0;
    dt.dat_StrDay = NULL;
    dt.dat_StrDate = datestamp;
    dt.dat_StrTime = NULL;
    
    // Format backup path
    strcpy(backup_path, BACKUP_DIR);
    AddPart(backup_path, FilePart(filepath), sizeof(backup_path));
    strcat(backup_path, ".");
    strcat(backup_path, datestamp);
    
    return Copy(filepath, backup_path, NULL) ? TRUE : FALSE;
}

BOOL InstallFile(const char *source, const char *dest)
{
    BPTR lock;
    BOOL success = FALSE;
    
    // First verify the source file exists and is readable
    if (!(lock = Lock(source, ACCESS_READ)))
    {
        Printf("Error: Cannot access source file\n");
        return FALSE;
    }
    UnLock(lock);
    
    // Check if destination exists
    if ((lock = Lock(dest, ACCESS_READ)))
    {
        UnLock(lock);
        // Backup existing file
        if (!BackupFile(dest))
        {
            Printf("Warning: Could not create backup\n");
            return FALSE;
        }
        
        // Delete existing file
        if (!DeleteFile(dest))
        {
            Printf("Error: Could not remove existing file\n");
            return FALSE;
        }
    }
    
    // Copy new file
    if (Copy(source, dest))
    {
        // Set proper protection bits
        SetProtection(dest, FIBF_READ|FIBF_EXECUTE|FIBF_WRITE);
        success = TRUE;
    }
    else
    {
        Printf("Error: Failed to copy file\n");
    }
    
    return success;
}

BOOL Copy(const char *source, const char *dest)
{
    BPTR src_fh, dst_fh;
    BOOL success = FALSE;
    UBYTE buffer[BUFFER_SIZE];
    LONG bytes_read;
    
    if ((src_fh = Open(source, MODE_OLDFILE)))
    {
        if ((dst_fh = Open(dest, MODE_NEWFILE)))
        {
            success = TRUE;
            
            while ((bytes_read = Read(src_fh, buffer, BUFFER_SIZE)) > 0)
            {
                if (Write(dst_fh, buffer, bytes_read) != bytes_read)
                {
                    success = FALSE;
                    break;
                }
            }
            
            Close(dst_fh);
            
            if (!success)
            {
                DeleteFile(dest);  // Clean up failed copy
            }
        }
        Close(src_fh);
    }
    
    return success;
}

int main(int argc, char **argv)
{
    BOOL success = FALSE;
    
    if (OpenLibraries())
    {
        struct WBStartup *wbmsg = NULL;
        
        // Check if we're started from Workbench
        if (argc == 0)
        {
            wbmsg = (struct WBStartup *)argv;
            success = HandleWorkbench();
        }
        else
        {
            success = HandleCLI(argc, argv);
        }
        
        CloseLibraries();
    }
    
    return success ? RETURN_OK : RETURN_FAIL;
}

BOOL OpenLibraries(void)
{
    if (!(IntuitionBase = (struct Library *)OpenLibrary("intuition.library", 37)))
        return FALSE;
    
    if (!(AslBase = (struct Library *)OpenLibrary("asl.library", 37)))
    {
        CloseLibrary((struct Library *)IntuitionBase);
        return FALSE;
    }
    
    return TRUE;
}

void CloseLibraries(void)
{
    if (AslBase) CloseLibrary((struct Library *)AslBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
}

BOOL HandleCLI(int argc, char **argv)
{
    BOOL success = FALSE;
    
    // Parse command line arguments
    rdargs = ReadArgs(template, (LONG *)&args, NULL);
    if (!rdargs)
    {
        PrintFault(IoErr(), "QuickUpdate");
        return FALSE;
    }
    
    if (args.file)
    {
        struct VersionInfo currentInfo, newInfo;
        BOOL hasCurrentVersion;
        
        Printf("Checking file: %s\n", (LONG)args.file);
        
        // Get version info of the new file
        if (!CheckFileVersion(args.file, &newInfo))
        {
            Printf("Error: Unable to read version information from file\n");
            FreeArgs(rdargs);
            return FALSE;
        }
        
        // Try to find current version in system
        if (!GetInstalledVersion(args.file, &currentInfo))
        {
            Printf("Error accessing version database\n");
            FreeArgs(rdargs);
            return FALSE;
        }
        
        hasCurrentVersion = TRUE;
        
        if (hasCurrentVersion)
        {
            Printf("Current version: %ld.%ld (Date: %ld)\n", 
                currentInfo.version, currentInfo.revision, currentInfo.date);
            Printf("New version: %ld.%ld (Date: %ld)\n",
                newInfo.version, newInfo.revision, newInfo.date);
            
            if (newInfo.version > currentInfo.version ||
                (newInfo.version == currentInfo.version && 
                 newInfo.revision > currentInfo.revision))
            {
                if (!args.noninteractive)
                {
                    Printf("Would you like to install the newer version? (y/n): ");
                    if (GetUserResponse())
                    {
                        success = InstallFile(args.file, GetDestPath(args.file));
                        if (success)
                            Printf("Update completed successfully.\n");
                        else
                            Printf("Update failed!\n");
                    }
                }
                else
                {
                    Printf("Newer version available.\n");
                    success = TRUE;
                }
            }
            else
            {
                Printf("No newer version available.\n");
                success = TRUE;
            }
        }
        else
        {
            Printf("No existing version found. New installation.\n");
            if (!args.noninteractive)
            {
                Printf("Would you like to install this file? (y/n): ");
                if (GetUserResponse())
                {
                    success = InstallFile(args.file, GetDestPath(args.file));
                    if (success)
                        Printf("Installation completed successfully.\n");
                    else
                        Printf("Installation failed!\n");
                }
            }
            else
            {
                Printf("Installation possible.\n");
                success = TRUE;
            }
        }
    }
    
    FreeArgs(rdargs);
    return success;
}

BOOL GetUserResponse(void)
{
    char buffer[2];
    LONG chars;
    
    chars = Read(Input(), buffer, 1);
    if (chars == 1)
    {
        // Consume the newline
        Read(Input(), buffer + 1, 1);
        return (buffer[0] == 'y' || buffer[0] == 'Y') ? TRUE : FALSE;
    }
    return FALSE;
}

const char *GetDestPath(const char *filename)
{
    static char destpath[256];
    const char *basename = FilePart(filename);
    
    // Determine the correct destination based on file extension
    const char *ext = FilePart(filename);
    ext = strrchr(ext, '.');
    if (ext && stricmp(ext, ".library") == 0)
        strcpy(destpath, "LIBS:");
    else if (strstr(filename, ".device"))
        strcpy(destpath, "DEVS:");
    else if (strstr(filename, ".datatype"))
        strcpy(destpath, "SYS:Classes/DataTypes/");
    else
        return NULL;
    
    AddPart(destpath, basename, sizeof(destpath));
    return destpath;
}

BOOL HandleGUI(void)
{
    BOOL running = TRUE;
    ULONG result;
    UWORD code;
    struct MenuItem *item;
    
    while (running)
    {
        WaitPort(MainWindow->UserPort);
        
        while ((result = RA_HandleInput(MainWindowObj, &code)) != WMHI_LASTMSG)
        {
            switch (result & WMHI_CLASSMASK)
            {
                case WMHI_CLOSEWINDOW:
                    running = FALSE;
                    break;
                
                case WMHI_MENUPICK:
                    while ((item = ItemAddress(MainMenu, code)))
                    {
                        switch ((ULONG)GTMENUITEM_USERDATA(item))
                        {
                            case ID_OPEN:
                                ShowFileRequester();
                                break;
                            
                            case ID_ABOUT:
                                ShowAboutRequester();
                                break;
                            
                            case ID_QUIT:
                                running = FALSE;
                                break;
                        }
                        code = item->NextSelect;
                    }
                    break;
                
                case WMHI_GADGETUP:
                    switch (result & WMHI_GADGETMASK)
                    {
                        case ID_CHECK:
                            ShowFileRequester();
                            break;
                    }
                    break;
                
                case WMHI_ICONIFY:
                    if (MainWindowObj)
                    {
                        RA_Iconify(MainWindowObj);
                        MainWindow = NULL;
                    }
                    break;
                
                case WMHI_UNICONIFY:
                    if ((MainWindow = (struct Window *)RA_OpenWindow(MainWindowObj)))
                    {
                        SetMenuStrip(MainWindow, MainMenu);
                    }
                    break;
                
                case WMHI_RAWKEY:
                    break;
            }
        }
    }
    
    return TRUE;
}

void ShowFileRequester(void)
{
    struct FileRequester *req;
    char filepath[256];
    
    if ((req = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
        ASLFR_TitleText,     "Select File to Check",
        ASLFR_DoPatterns,    TRUE,
        ASLFR_InitialPattern,"#?.library|#?.device|#?.datatype",
        ASLFR_DoSaveMode,    FALSE,
        TAG_DONE)))
    {
        if (AslRequest(req, NULL))
        {
            strcpy(filepath, req->fr_Drawer);
            AddPart(filepath, req->fr_File, sizeof(filepath));
            ProcessFile(filepath);
        }
        FreeAslRequest(req);
    }
}

void ShowAboutRequester(void)
{
    struct EasyStruct es = {
        sizeof(struct EasyStruct),
        0,
        "About QuickUpdate",
        "QuickUpdate 1.0\n\n"
        "A utility to check and update system files",
        "OK"
    };
    
    EasyRequest(MainWindow, &es, NULL, NULL);
}

void ProcessFile(const char *filepath)
{
    struct VersionInfo currentInfo, newInfo;
    char statusText[256];
    BOOL hasCurrentVersion;
    LONG cmp;
    
    SetStatusText("Checking file...");
    
    if (!CheckFileVersion(filepath, &newInfo))
    {
        SetStatusText("Error: Unable to read version information from file");
        return;
    }
    
    hasCurrentVersion = GetInstalledVersion(filepath, &currentInfo);
    
    if (hasCurrentVersion)
    {
        sprintf(statusText, "Current: v%ld.%ld (%ld) - New: v%ld.%ld (%ld)",
                currentInfo.version, currentInfo.revision, currentInfo.date,
                newInfo.version, newInfo.revision, newInfo.date);
        SetStatusText(statusText);
        
        cmp = CompareVersions(&currentInfo, &newInfo);
        if (cmp > 0)
        {
            // Newer version
            Printf("New version available from %s\n", newInfo.origin);
            if (args.force)
            {
                InstallFile(filepath, GetDestPath(filepath));
            }
            else
            {
                // Prompt with origin info
                Printf("Current: v%ld.%ld (%s)\nNew: v%ld.%ld (%s)\n",
                    currentInfo.version, currentInfo.revision, currentInfo.origin,
                    newInfo.version, newInfo.revision, newInfo.origin);
                if (GetUserResponse())
                {
                    InstallFile(filepath, GetDestPath(filepath));
                }
            }
        }
        else if (cmp < 0)
        {
            Printf("Warning: New file is older than installed version!\n");
            if (args.force)
            {
                Printf("Forcing installation...\n");
                InstallFile(filepath, GetDestPath(filepath));
            }
        }
        else
        {
            Printf("File versions are identical\n");
        }
    }
    else
    {
        sprintf(statusText, "New file: v%ld.%ld (%ld)",
                newInfo.version, newInfo.revision, newInfo.date);
        SetStatusText(statusText);
        
        struct EasyStruct es = {
            sizeof(struct EasyStruct),
            0,
            "Install New File",
            "Would you like to install this file?",
            "Install|Cancel"
        };
        
        if (EasyRequest(MainWindow, &es, NULL, NULL) == 1)
        {
            if (InstallFile(filepath, GetDestPath(filepath)))
            {
                SetStatusText("Installation completed successfully");
            }
            else
            {
                SetStatusText("Installation failed!");
            }
        }
    }
}

BOOL HandleAppMessage(struct AppMessage *msg)
{
    char filepath[256];
    
    if (msg->am_NumArgs == 1)
    {
        NameFromLock(msg->am_ArgList[0].wa_Lock, filepath, sizeof(filepath));
        AddPart(filepath, msg->am_ArgList[0].wa_Name, sizeof(filepath));
        
        if (IsValidFileType(filepath))
        {
            // If window is iconified, open it
            if (!MainWindow && MainWindowObj)
            {
                if ((MainWindow = (struct Window *)RA_OpenWindow(MainWindowObj)))
                {
                    SetMenuStrip(MainWindow, MainMenu);
                }
            }
            
            ProcessFile(filepath);
            return TRUE;
        }
    }
    else if (msg->am_NumArgs > 1)
    {
        SetStatusText("Error: Please check one file at a time");
    }
    
    return FALSE;
}

BOOL CreateAppIcon(void)
{
    if ((AppPort = CreateMsgPort()))
    {
        if ((AppIcon = GetDiskObject("PROGDIR:QuickUpdate")))
        {
            // Set up AppIcon
            AppIcon->do_Type = WBPROJECT;  // Allow drops
            
            if (AddAppIconA(0, 0, "QuickUpdate",
                           AppPort, NULL, AppIcon, NULL))
            {
                return TRUE;
            }
            FreeDiskObject(AppIcon);
            AppIcon = NULL;
        }
        DeleteMsgPort(AppPort);
        AppPort = NULL;
    }
    return FALSE;
}

BOOL IsValidFileType(const char *filename)
{
    const char *ext = FilePart(filename);
    
    // Find the last dot in filename
    while (*ext && *ext != '.') ext++;
    if (*ext == '.')
    {
        ext++;  // Skip the dot
        if (stricmp(ext, "library") == 0 ||
            stricmp(ext, "device") == 0 ||
            stricmp(ext, "datatype") == 0 ||
            stricmp(ext, "class") == 0 ||
            stricmp(ext, "gadget") == 0 ||
            stricmp(ext, "resource") == 0 ||
            stricmp(ext, "mcc") == 0 ||
            stricmp(ext, "mcp") == 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL GetInstalledVersion(const char *filename, struct VersionInfo *info)
{
    BPTR fh;
    BOOL found = FALSE;
    struct ChecksumEntry entry;
    char *baseName = FilePart(filename);
    
    if ((fh = Open(CHECKSUM_DB, MODE_OLDFILE)))
    {
        while (Read(fh, &entry, sizeof(entry)) == sizeof(entry))
        {
            if (stricmp(baseName, entry.filename) == 0)
            {
                // Verify the file still exists and matches
                BPTR lock;
                if ((lock = Lock(filename, ACCESS_READ)))
                {
                    struct FileInfoBlock *fib;
                    if ((fib = AllocDosObject(DOS_FIB, NULL)))
                    {
                        if (Examine(lock, fib))
                        {
                            if (fib->fib_Size == entry.filesize &&
                                CalculateChecksum(filename) == entry.checksum)
                            {
                                info->version = entry.version;
                                info->revision = entry.revision;
                                info->date = entry.date;
                                strncpy(info->origin, entry.origin, sizeof(info->origin)-1);
                                found = TRUE;
                            }
                        }
                        FreeDosObject(DOS_FIB, fib);
                    }
                    UnLock(lock);
                }
                break;
            }
        }
        Close(fh);
    }
    
    return found;
}

// Compare versions properly (34.2 < 34.19)
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

BOOL ParseVersionString(const char *verStr, struct VersionInfo *info)
{
    const char *p = verStr;
    LONG state = 0; // 0=find numbers, 1=version, 2=revision, 3=date
    UWORD version = 0;
    UWORD revision = 0;
    ULONG date = 0;
    struct DateTime dt;
    
    // Skip non-numeric prefix
    while (*p && !isdigit(*p)) p++;
    
    // Parse version.revision
    while (*p)
    {
        if (isdigit(*p))
        {
            ULONG num = 0;
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

BOOL HandleWorkbench(void)
{
    struct MsgPort *port;
    struct IntuiMessage *msg;
    ULONG result;
    ULONG code;
    BOOL done = FALSE;
    
    port = CreateMsgPort();
    if (!port) return FALSE;
    
    while (!done)
    {
        WaitPort(port);
        while ((msg = (struct IntuiMessage *)GetMsg(port)))
        {
            result = msg->Class;
            code = msg->Code;
            ReplyMsg((struct Message *)msg);
            
            switch (result)
            {
                case IDCMP_CLOSEWINDOW:
                    done = TRUE;
                    break;
                    
                case IDCMP_GADGETUP:
                    // Handle gadget events
                    break;
            }
        }
    }
    
    DeleteMsgPort(port);
    return TRUE;
}

BOOL IsLibrary(const char *filename)
{
    const char *ext = FilePart(filename);
    
    if (strstr(ext, ".library"))
        return TRUE;
    else if (strstr(ext, ".device"))
        return TRUE;
    
    return FALSE;
}
