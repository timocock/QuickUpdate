#include <string.h>
#include <proto/dos.h>
#include <proto/exec.h>

/* Global flag for Unix path semantics */
int __unix_path_semantics = 1;

/* Convert Unix-style paths to AmigaOS 4 DOS-style paths */
char *amigaos4_unix_to_dos(const char *unix, char *out, size_t len)
{
    if (!unix || !out || len == 0) {
        return NULL;
    }
    
    // Ensure we don't exceed buffer size
    if (strlen(unix) >= len) {
        return NULL;
    }
    
    /* If not using Unix path semantics, just copy as-is */
    if (!__unix_path_semantics) {
        IDOS->Strlcpy(out, unix, len);
        return out;
    }
    
    /* Handle relative paths */
    if (unix[0] != '/') {
        IDOS->Strlcpy(out, unix, len);
        return out;
    }
    
    /* Handle PROGDIR: special case */
    if (IDOS->Strnicmp(unix, "/progdir/", 9) == 0) {
        IDOS->Strlcpy(out, "PROGDIR:", len);
        IDOS->Strlcat(out, unix + 8, len);
        return out;
    }
    
    /* Handle RAM: special case */
    if (IDOS->Strnicmp(unix, "/ram/", 5) == 0) {
        IDOS->Strlcpy(out, "RAM:", len);
        IDOS->Strlcat(out, unix + 4, len);
        return out;
    }
    
    /* Handle SYS: special case */
    if (IDOS->Strnicmp(unix, "/sys/", 5) == 0) {
        IDOS->Strlcpy(out, "SYS:", len);
        IDOS->Strlcat(out, unix + 4, len);
        return out;
    }
    
    /* Handle T: special case */
    if (IDOS->Strnicmp(unix, "/tmp/", 5) == 0) {
        IDOS->Strlcpy(out, "T:", len);
        IDOS->Strlcat(out, unix + 4, len);
        return out;
    }
    
    /* Handle root directory */
    if (unix[1] == '\0') {
        IDOS->Strlcpy(out, "SYS:", len);
        return out;
    }
    
    /* Handle volume:path format */
    const char *slash = IDOS->Strchr(unix + 1, '/');
    if (slash) {
        size_t vol_len = slash - (unix + 1);
        if (vol_len > 0 && vol_len < len - 2) {
            /* Copy volume name */
            strncpy(out, unix + 1, vol_len);
            out[vol_len] = ':';
            out[vol_len + 1] = '\0';
            
            /* Append remaining path */
            IDOS->Strlcat(out, slash + 1, len);
        } else {
            /* Volume name too long, fall back to SYS: */
            IDOS->Strlcpy(out, "SYS:", len);
            IDOS->Strlcat(out, unix + 1, len);
        }
    } else {
        /* Single volume name */
        if (strlen(unix + 1) < len - 2) {
            IDOS->Strlcpy(out, unix + 1, len - 1);
            IDOS->Strlcat(out, ":", len);
        } else {
            IDOS->Strlcpy(out, "SYS:", len);
        }
    }
    
    return out;
}

/* Convert DOS-style paths to Unix-style paths */
char *amigaos4_dos_to_unix(const char *dos, char *out, size_t len)
{
    if (!dos || !out || len == 0) {
        return NULL;
    }
    
    /* If not using Unix path semantics, just copy as-is */
    if (!__unix_path_semantics) {
        IDOS->Strlcpy(out, dos, len);
        return out;
    }
    
    /* Handle special volumes */
    if (IDOS->Strnicmp(dos, "PROGDIR:", 8) == 0) {
        IDOS->Strlcpy(out, "/progdir", len);
        if (dos[8] != '\0') {
            IDOS->Strlcat(out, "/", len);
            IDOS->Strlcat(out, dos + 8, len);
        }
        return out;
    }
    
    if (IDOS->Strnicmp(dos, "RAM:", 4) == 0) {
        IDOS->Strlcpy(out, "/ram", len);
        if (dos[4] != '\0') {
            IDOS->Strlcat(out, "/", len);
            IDOS->Strlcat(out, dos + 4, len);
        }
        return out;
    }
    
    if (IDOS->Strnicmp(dos, "SYS:", 4) == 0) {
        IDOS->Strlcpy(out, "/sys", len);
        if (dos[4] != '\0') {
            IDOS->Strlcat(out, "/", len);
            IDOS->Strlcat(out, dos + 4, len);
        }
        return out;
    }
    
    if (IDOS->Strnicmp(dos, "T:", 2) == 0) {
        IDOS->Strlcpy(out, "/tmp", len);
        if (dos[2] != '\0') {
            IDOS->Strlcat(out, "/", len);
            IDOS->Strlcat(out, dos + 2, len);
        }
        return out;
    }
    
    /* Handle volume:path format */
    const char *colon = IDOS->Strchr(dos, ':');
    if (colon) {
        size_t vol_len = colon - dos;
        if (vol_len > 0 && vol_len < len - 2) {
            IDOS->Strlcpy(out, "/", len);
            strncat(out, dos, vol_len);
            if (colon[1] != '\0') {
                IDOS->Strlcat(out, "/", len);
                IDOS->Strlcat(out, colon + 1, len);
            }
        } else {
            IDOS->Strlcpy(out, "/sys", len);
        }
    } else {
        /* No colon, treat as relative path */
        IDOS->Strlcpy(out, dos, len);
    }
    
    return out;
}

/* Normalize path separators */
void amigaos4_normalize_path(char *path)
{
    if (!path) return;
    
    char *p = path;
    while (*p) {
        if (*p == '\\') {
            *p = '/';
        }
        p++;
    }
}

/* Get current working directory in Unix format */
int amigaos4_getcwd_unix(char *buf, size_t size)
{
    if (!buf || size == 0) {
        return -1;
    }
    
    char dos_path[512];
    if (IDOS->GetCurrentDirName(dos_path, sizeof(dos_path)) == 0) {
        return -1;
    }
    
    amigaos4_dos_to_unix(dos_path, buf, size);
    return 0;
}

/* Change directory using Unix path */
int amigaos4_chdir_unix(const char *path)
{
    if (!path) {
        return -1;
    }
    
    char dos_path[512];
    amigaos4_unix_to_dos(path, dos_path, sizeof(dos_path));
    
    return IDOS->ChangeDir(dos_path) ? 0 : -1;
} 