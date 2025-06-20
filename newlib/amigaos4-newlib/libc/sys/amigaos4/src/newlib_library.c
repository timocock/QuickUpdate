#include <exec/exec.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include <reent.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

/* Library base and interface */
struct NewlibBase *NewlibBase = NULL;
struct NewlibIFace *INewlib = NULL;

/* Library version */
#define NEWLIB_VERSION 45
#define NEWLIB_REVISION 0

/* Interface structure */
struct NewlibIFace
{
    struct InterfaceData Data;
    
    /* Library management methods */
    APTR (*Open)(struct NewlibIFace *Self, ULONG version);
    APTR (*Close)(struct NewlibIFace *Self);
    APTR (*Expunge)(struct NewlibIFace *Self);
    APTR (*Reserved)(struct NewlibIFace *Self);
    
    /* Newlib function interface methods */
    int (*open_r)(struct _reent *r, const char *path, int flags, int mode);
    int (*close_r)(struct _reent *r, int fd);
    ssize_t (*read_r)(struct _reent *r, int fd, void *buf, size_t count);
    ssize_t (*write_r)(struct _reent *r, int fd, const void *buf, size_t count);
    off_t (*lseek_r)(struct _reent *r, int fd, off_t offset, int whence);
    int (*fstat_r)(struct _reent *r, int fd, struct stat *buf);
    int (*stat_r)(struct _reent *r, const char *path, struct stat *buf);
    int (*unlink_r)(struct _reent *r, const char *path);
    int (*rename_r)(struct _reent *r, const char *old, const char *new);
    int (*mkdir_r)(struct _reent *r, const char *path, mode_t mode);
    int (*rmdir_r)(struct _reent *r, const char *path);
    time_t (*time_r)(struct _reent *r, time_t *t);
    int (*gettimeofday_r)(struct _reent *r, struct timeval *tv, void *tz);
    int (*dup2_r)(struct _reent *r, int oldfd, int newfd);
    int (*isatty_r)(struct _reent *r, int fd);
    int (*kill_r)(struct _reent *r, pid_t pid, int sig);
    pid_t (*getpid_r)(struct _reent *r);
    int (*fork_r)(struct _reent *r);
    int (*execve_r)(struct _reent *r, const char *path, char *const argv[], char *const envp[]);
    pid_t (*wait_r)(struct _reent *r, int *status);
    int (*link_r)(struct _reent *r, const char *old, const char *new);
    int (*symlink_r)(struct _reent *r, const char *target, const char *linkpath);
    ssize_t (*readlink_r)(struct _reent *r, const char *path, char *buf, size_t bufsiz);
    int (*times_r)(struct _reent *r, struct tms *buf);
    
    /* Memory management methods */
    void *(*malloc_r)(struct _reent *r, size_t size);
    void (*free_r)(struct _reent *r, void *ptr);
    void *(*realloc_r)(struct _reent *r, void *ptr, size_t size);
    void *(*calloc_r)(struct _reent *r, size_t nmemb, size_t size);
    
    /* Error handling methods */
    int (*get_errno_r)(struct _reent *r);
    void (*set_errno_r)(struct _reent *r, int err);
    const char *(*strerror_r)(struct _reent *r, int errnum);
    
    /* Path conversion methods */
    char *(*unix_to_dos_path)(const char *unix_path, char *out, size_t len);
    char *(*dos_to_unix_path)(const char *dos_path, char *out, size_t len);
};

/* Library base structure */
struct NewlibBase
{
    struct Library lib;
    struct NewlibIFace *iface;
    ULONG version;
    ULONG revision;
    APTR syscalls;
    APTR pathconv;
    APTR reent_tls;
};

/* Forward declarations */
static APTR Newlib_Open(struct NewlibIFace *Self, ULONG version);
static APTR Newlib_Close(struct NewlibIFace *Self);
static APTR Newlib_Expunge(struct NewlibIFace *Self);
static APTR Newlib_Reserved(struct NewlibIFace *Self);

/* Interface method implementations */
static int Newlib_open_r(struct _reent *r, const char *path, int flags, int mode);
static int Newlib_close_r(struct _reent *r, int fd);
static ssize_t Newlib_read_r(struct _reent *r, int fd, void *buf, size_t count);
static ssize_t Newlib_write_r(struct _reent *r, int fd, const void *buf, size_t count);
static off_t Newlib_lseek_r(struct _reent *r, int fd, off_t offset, int whence);
static int Newlib_fstat_r(struct _reent *r, int fd, struct stat *buf);
static int Newlib_stat_r(struct _reent *r, const char *path, struct stat *buf);
static int Newlib_unlink_r(struct _reent *r, const char *path);
static int Newlib_rename_r(struct _reent *r, const char *old, const char *new);
static int Newlib_mkdir_r(struct _reent *r, const char *path, mode_t mode);
static int Newlib_rmdir_r(struct _reent *r, const char *path);
static time_t Newlib_time_r(struct _reent *r, time_t *t);
static int Newlib_gettimeofday_r(struct _reent *r, struct timeval *tv, void *tz);
static int Newlib_dup2_r(struct _reent *r, int oldfd, int newfd);
static int Newlib_isatty_r(struct _reent *r, int fd);
static int Newlib_kill_r(struct _reent *r, pid_t pid, int sig);
static pid_t Newlib_getpid_r(struct _reent *r);
static int Newlib_fork_r(struct _reent *r);
static int Newlib_execve_r(struct _reent *r, const char *path, char *const argv[], char *const envp[]);
static pid_t Newlib_wait_r(struct _reent *r, int *status);
static int Newlib_link_r(struct _reent *r, const char *old, const char *new);
static int Newlib_symlink_r(struct _reent *r, const char *target, const char *linkpath);
static ssize_t Newlib_readlink_r(struct _reent *r, const char *path, char *buf, size_t bufsiz);
static int Newlib_times_r(struct _reent *r, struct tms *buf);

/* Memory management method implementations */
static void *Newlib_malloc_r(struct _reent *r, size_t size);
static void Newlib_free_r(struct _reent *r, void *ptr);
static void *Newlib_realloc_r(struct _reent *r, void *ptr, size_t size);
static void *Newlib_calloc_r(struct _reent *r, size_t nmemb, size_t size);

/* Error handling method implementations */
static int Newlib_get_errno_r(struct _reent *r);
static void Newlib_set_errno_r(struct _reent *r, int err);
static const char *Newlib_strerror_r(struct _reent *r, int errnum);

/* Path conversion method implementations */
static char *Newlib_unix_to_dos_path(const char *unix_path, char *out, size_t len);
static char *Newlib_dos_to_unix_path(const char *dos_path, char *out, size_t len);

/* Interface method table */
static struct NewlibIFace NewlibInterface = {
    {
        /* InterfaceData */
        NULL,           /* Data.Next */
        NULL,           /* Data.LibNode */
        NULL,           /* Data.RefCount */
        NULL,           /* Data.Version */
        NULL,           /* Data.Revision */
        NULL,           /* Data.IdString */
        NULL,           /* Data.OOP */
        NULL,           /* Data.Reserved */
    },
    
    /* Library management methods */
    Newlib_Open,
    Newlib_Close,
    Newlib_Expunge,
    Newlib_Reserved,
    
    /* Newlib function interface methods */
    Newlib_open_r,
    Newlib_close_r,
    Newlib_read_r,
    Newlib_write_r,
    Newlib_lseek_r,
    Newlib_fstat_r,
    Newlib_stat_r,
    Newlib_unlink_r,
    Newlib_rename_r,
    Newlib_mkdir_r,
    Newlib_rmdir_r,
    Newlib_time_r,
    Newlib_gettimeofday_r,
    Newlib_dup2_r,
    Newlib_isatty_r,
    Newlib_kill_r,
    Newlib_getpid_r,
    Newlib_fork_r,
    Newlib_execve_r,
    Newlib_wait_r,
    Newlib_link_r,
    Newlib_symlink_r,
    Newlib_readlink_r,
    Newlib_times_r,
    
    /* Memory management methods */
    Newlib_malloc_r,
    Newlib_free_r,
    Newlib_realloc_r,
    Newlib_calloc_r,
    
    /* Error handling methods */
    Newlib_get_errno_r,
    Newlib_set_errno_r,
    Newlib_strerror_r,
    
    /* Path conversion methods */
    Newlib_unix_to_dos_path,
    Newlib_dos_to_unix_path,
};

/* Library management method implementations */
static APTR Newlib_Open(struct NewlibIFace *Self, ULONG version)
{
    if (version > NEWLIB_VERSION) {
        return NULL;
    }
    
    /* Initialize TLS if not already done */
    __amigaos4_init_tls();
    
    return Self;
}

static APTR Newlib_Close(struct NewlibIFace *Self)
{
    return NULL;
}

static APTR Newlib_Expunge(struct NewlibIFace *Self)
{
    /* Clean up TLS */
    __amigaos4_cleanup_tls();
    
    return NULL;
}

static APTR Newlib_Reserved(struct NewlibIFace *Self)
{
    return NULL;
}

/* Interface method implementations - these call the actual syscalls */
static int Newlib_open_r(struct _reent *r, const char *path, int flags, int mode)
{
    return _open_r(r, path, flags, mode);
}

static int Newlib_close_r(struct _reent *r, int fd)
{
    return _close_r(r, fd);
}

static ssize_t Newlib_read_r(struct _reent *r, int fd, void *buf, size_t count)
{
    return _read_r(r, fd, buf, count);
}

static ssize_t Newlib_write_r(struct _reent *r, int fd, const void *buf, size_t count)
{
    return _write_r(r, fd, buf, count);
}

static off_t Newlib_lseek_r(struct _reent *r, int fd, off_t offset, int whence)
{
    return _lseek_r(r, fd, offset, whence);
}

static int Newlib_fstat_r(struct _reent *r, int fd, struct stat *buf)
{
    return _fstat_r(r, fd, buf);
}

static int Newlib_stat_r(struct _reent *r, const char *path, struct stat *buf)
{
    return _stat_r(r, path, buf);
}

static int Newlib_unlink_r(struct _reent *r, const char *path)
{
    return _unlink_r(r, path);
}

static int Newlib_rename_r(struct _reent *r, const char *old, const char *new)
{
    return _rename_r(r, old, new);
}

static int Newlib_mkdir_r(struct _reent *r, const char *path, mode_t mode)
{
    return _mkdir_r(r, path, mode);
}

static int Newlib_rmdir_r(struct _reent *r, const char *path)
{
    return _rmdir_r(r, path);
}

static time_t Newlib_time_r(struct _reent *r, time_t *t)
{
    return _time_r(r, t);
}

static int Newlib_gettimeofday_r(struct _reent *r, struct timeval *tv, void *tz)
{
    return _gettimeofday_r(r, tv, tz);
}

static int Newlib_dup2_r(struct _reent *r, int oldfd, int newfd)
{
    return _dup2_r(r, oldfd, newfd);
}

static int Newlib_isatty_r(struct _reent *r, int fd)
{
    return _isatty_r(r, fd);
}

static int Newlib_kill_r(struct _reent *r, pid_t pid, int sig)
{
    return _kill_r(r, pid, sig);
}

static pid_t Newlib_getpid_r(struct _reent *r)
{
    return _getpid_r(r);
}

static int Newlib_fork_r(struct _reent *r)
{
    return _fork_r(r);
}

static int Newlib_execve_r(struct _reent *r, const char *path, char *const argv[], char *const envp[])
{
    return _execve_r(r, path, argv, envp);
}

static pid_t Newlib_wait_r(struct _reent *r, int *status)
{
    return _wait_r(r, status);
}

static int Newlib_link_r(struct _reent *r, const char *old, const char *new)
{
    return _link_r(r, old, new);
}

static int Newlib_symlink_r(struct _reent *r, const char *target, const char *linkpath)
{
    return _symlink_r(r, target, linkpath);
}

static ssize_t Newlib_readlink_r(struct _reent *r, const char *path, char *buf, size_t bufsiz)
{
    return _readlink_r(r, path, buf, bufsiz);
}

static int Newlib_times_r(struct _reent *r, struct tms *buf)
{
    return _times_r(r, buf);
}

/* Memory management method implementations */
static void *Newlib_malloc_r(struct _reent *r, size_t size)
{
    return _malloc_r(r, size);
}

static void Newlib_free_r(struct _reent *r, void *ptr)
{
    _free_r(r, ptr);
}

static void *Newlib_realloc_r(struct _reent *r, void *ptr, size_t size)
{
    return _realloc_r(r, ptr, size);
}

static void *Newlib_calloc_r(struct _reent *r, size_t nmemb, size_t size)
{
    return _calloc_r(r, nmemb, size);
}

/* Error handling method implementations */
static int Newlib_get_errno_r(struct _reent *r)
{
    if (!r) r = __getreent();
    return r->_errno;
}

static void Newlib_set_errno_r(struct _reent *r, int err)
{
    if (!r) r = __getreent();
    r->_errno = err;
}

static const char *Newlib_strerror_r(struct _reent *r, int errnum)
{
    return strerror(errnum);
}

/* Path conversion method implementations */
static char *Newlib_unix_to_dos_path(const char *unix_path, char *out, size_t len)
{
    return amigaos4_unix_to_dos(unix_path, out, len);
}

static char *Newlib_dos_to_unix_path(const char *dos_path, char *out, size_t len)
{
    return amigaos4_dos_to_unix(dos_path, out, len);
}

/* Library initialization function */
void __attribute__((constructor))
__amigaos4_init_library(void)
{
    /* Set up library base and interface */
    NewlibBase = (struct NewlibBase *)IExec->OpenLibrary("newlib.library", NEWLIB_VERSION);
    if (NewlibBase) {
        INewlib = (struct NewlibIFace *)IExec->GetInterface(NewlibBase, "main", 1, NULL);
        if (!INewlib) {
            IExec->CloseLibrary((struct Library *)NewlibBase);
            NewlibBase = NULL;
        }
    }
}

/* Library cleanup function */
void __attribute__((destructor))
__amigaos4_cleanup_library(void)
{
    if (INewlib) {
        IExec->DropInterface((struct Interface *)INewlib);
        INewlib = NULL;
    }
    
    if (NewlibBase) {
        IExec->CloseLibrary((struct Library *)NewlibBase);
        NewlibBase = NULL;
    }
} 