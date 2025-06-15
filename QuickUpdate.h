#ifndef QUICKUPDATE_H
#define QUICKUPDATE_H

#include "Shared.h"
#include <workbench/startup.h>
#include <intuition/intuition.h>
#include <workbench/workbench.h>
#include <proto/reqtools.h>

#define BACKUP_DIR "SYS:Backups/QuickUpdate/"

// Menu IDs
#define ID_OPEN  1
#define ID_ABOUT 2
#define ID_QUIT  3
#define ID_CHECK 4

// Global variables
extern struct DosLibrary *DOSBase;
extern struct Library *AslBase;

// QuickUpdate-specific prototypes
BOOL OpenLibraries(void);
void CloseLibraries(void);
BOOL InstallFile(const char *source, const char *dest);
const char *GetDestPath(const char *filename);
BOOL IsValidFileType(const char *filename);
void ShowFileRequester(void);
BOOL GetUserResponse(void);
BOOL Copy(const char *source, const char *dest);
BOOL BackupFile(const char *filepath);
void SetStatusText(const char *text);
void RA_Iconify(Object *obj);
Object *RA_OpenWindow(Object *obj);
ULONG RA_HandleInput(Object *obj, UWORD *code);

#endif /* QUICKUPDATE_H */ 