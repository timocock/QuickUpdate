#ifndef _AMIGA_ERR_H
#define _AMIGA_ERR_H

#include <errno.h>
#include <proto/dos.h>
#include <proto/exec.h>

/* Map AmigaOS 4 error codes to POSIX errno values */
static inline int amigaos4_ioerr(void)
{
    LONG error = IDOS->IoErr();
    
    switch (error) {
        case 0:                     return 0;           /* No error */
        case ERROR_NO_FREE_STORE:   return ENOMEM;
        case ERROR_TASK_TABLE_FULL: return EAGAIN;
        case ERROR_BAD_TEMPLATE:    return EINVAL;
        case ERROR_BAD_NUMBER:      return EINVAL;
        case ERROR_REQUIRED_ARG_MISSING: return EINVAL;
        case ERROR_KEY_NEEDS_ARG:   return EINVAL;
        case ERROR_TOO_MANY_ARGS:   return E2BIG;
        case ERROR_UNMATCHED_QUOTES: return EINVAL;
        case ERROR_LINE_TOO_LONG:   return E2BIG;
        case ERROR_FILE_NOT_OBJECT: return ENOEXEC;
        case ERROR_INVALID_RESIDENT_LIBRARY: return ENOEXEC;
        case ERROR_NO_DEFAULT_DIR:  return ENOENT;
        case ERROR_OBJECT_IN_USE:   return EBUSY;
        case ERROR_OBJECT_EXISTS:   return EEXIST;
        case ERROR_DIR_NOT_FOUND:   return ENOENT;
        case ERROR_OBJECT_NOT_FOUND: return ENOENT;
        case ERROR_BAD_STREAM_NAME: return EINVAL;
        case ERROR_BAD_MESSAGE:     return EINVAL;
        case ERROR_DOS_READ_ERROR:  return EIO;
        case ERROR_DOS_WRITE_ERROR: return EIO;
        case ERROR_UNKNOWN_DOS_ERROR: return EIO;
        case ERROR_DISK_WRITE_PROTECTED: return EROFS;
        case ERROR_DISK_IS_FULL:    return ENOSPC;
        case ERROR_DELETE_PROTECTED: return EACCES;
        case ERROR_WRITE_PROTECTED: return EACCES;
        case ERROR_READ_PROTECTED:  return EACCES;
        case ERROR_NOT_A_DOS_DISK:  return EINVAL;
        case ERROR_NO_DISK:         return ENODEV;
        case ERROR_NO_MORE_ENTRIES: return ENOENT;
        case ERROR_IS_SOFT_LINK:    return ELOOP;
        case ERROR_OBJECT_LINKED:   return EMLINK;
        case ERROR_BAD_HUNK:        return ENOEXEC;
        case ERROR_NOT_IMPLEMENTED: return ENOSYS;
        case ERROR_RECORD_NOT_LOCKED: return EACCES;
        case ERROR_LOCK_COLLISION:  return EACCES;
        case ERROR_LOCK_TIMEOUT:    return ETIMEDOUT;
        case ERROR_UNLOCK_ERROR:    return EACCES;
        case ERROR_BUFFER_OVERFLOW: return EOVERFLOW;
        case ERROR_BREAK:           return EINTR;
        case ERROR_NOT_EXECUTABLE:  return ENOEXEC;
        case ERROR_INVALID_SIGNAL:  return EINVAL;
        case ERROR_BAD_QUICK_INIT:  return EINVAL;
        case ERROR_INIT_FAILED:     return EINVAL;
        case ERROR_DEVICE_NOT_MOUNTED: return ENODEV;
        case ERROR_PACKET_TOO_BIG:  return EMSGSIZE;
        case ERROR_DEVICE_IN_USE:   return EBUSY;
        case ERROR_INVALID_RESIDENT_LIBRARY: return ENOEXEC;
        case ERROR_NO_SIGNAL_SEMAPHORE: return EAGAIN;
        case ERROR_BAD_SIGNAL_SEMAPHORE: return EINVAL;
        case ERROR_BAD_TEMPLATE:    return EINVAL;
        case ERROR_BAD_NUMBER:      return EINVAL;
        case ERROR_REQUIRED_ARG_MISSING: return EINVAL;
        case ERROR_KEY_NEEDS_ARG:   return EINVAL;
        case ERROR_TOO_MANY_ARGS:   return E2BIG;
        case ERROR_UNMATCHED_QUOTES: return EINVAL;
        case ERROR_LINE_TOO_LONG:   return E2BIG;
        case ERROR_FILE_NOT_OBJECT: return ENOEXEC;
        case ERROR_INVALID_RESIDENT_LIBRARY: return ENOEXEC;
        case ERROR_NO_DEFAULT_DIR:  return ENOENT;
        case ERROR_OBJECT_IN_USE:   return EBUSY;
        case ERROR_OBJECT_EXISTS:   return EEXIST;
        case ERROR_DIR_NOT_FOUND:   return ENOENT;
        case ERROR_OBJECT_NOT_FOUND: return ENOENT;
        case ERROR_BAD_STREAM_NAME: return EINVAL;
        case ERROR_BAD_MESSAGE:     return EINVAL;
        case ERROR_DOS_READ_ERROR:  return EIO;
        case ERROR_DOS_WRITE_ERROR: return EIO;
        case ERROR_UNKNOWN_DOS_ERROR: return EIO;
        case ERROR_DISK_WRITE_PROTECTED: return EROFS;
        case ERROR_DISK_IS_FULL:    return ENOSPC;
        case ERROR_DELETE_PROTECTED: return EACCES;
        case ERROR_WRITE_PROTECTED: return EACCES;
        case ERROR_READ_PROTECTED:  return EACCES;
        case ERROR_NOT_A_DOS_DISK:  return EINVAL;
        case ERROR_NO_DISK:         return ENODEV;
        case ERROR_NO_MORE_ENTRIES: return ENOENT;
        case ERROR_IS_SOFT_LINK:    return ELOOP;
        case ERROR_OBJECT_LINKED:   return EMLINK;
        case ERROR_BAD_HUNK:        return ENOEXEC;
        case ERROR_NOT_IMPLEMENTED: return ENOSYS;
        case ERROR_RECORD_NOT_LOCKED: return EACCES;
        case ERROR_LOCK_COLLISION:  return EACCES;
        case ERROR_LOCK_TIMEOUT:    return ETIMEDOUT;
        case ERROR_UNLOCK_ERROR:    return EACCES;
        case ERROR_BUFFER_OVERFLOW: return EOVERFLOW;
        case ERROR_BREAK:           return EINTR;
        case ERROR_NOT_EXECUTABLE:  return ENOEXEC;
        case ERROR_INVALID_SIGNAL:  return EINVAL;
        case ERROR_BAD_QUICK_INIT:  return EINVAL;
        case ERROR_INIT_FAILED:     return EINVAL;
        case ERROR_DEVICE_NOT_MOUNTED: return ENODEV;
        case ERROR_PACKET_TOO_BIG:  return EMSGSIZE;
        case ERROR_DEVICE_IN_USE:   return EBUSY;
        case ERROR_INVALID_RESIDENT_LIBRARY: return ENOEXEC;
        case ERROR_NO_SIGNAL_SEMAPHORE: return EAGAIN;
        case ERROR_BAD_SIGNAL_SEMAPHORE: return EINVAL;
        default:                    return EIO;         /* Generic I/O error */
    }
}

/* Convert POSIX open flags to AmigaOS 4 flags */
static inline LONG amigaos4_oflags(int flags, int mode)
{
    LONG amiga_flags = 0;
    // Handle access modes properly
    if (flags & O_RDWR) {
        amiga_flags = MODE_READWRITE;
    } else if (flags & O_WRONLY) {
        amiga_flags = MODE_NEWFILE;
    } else {
        amiga_flags = MODE_OLDFILE;
    }
    // Handle creation flags
    if (flags & O_CREAT) {
        if (flags & O_EXCL) {
            amiga_flags = MODE_NEWFILE;
        } else {
            amiga_flags = MODE_READWRITE;
        }
    }
    // Handle truncation
    if (flags & O_TRUNC) {
        amiga_flags = MODE_NEWFILE;
    }
    return amiga_flags;
}

#endif /* _AMIGA_ERR_H */ 