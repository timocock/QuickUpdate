#include <proto/dos.h>
#include <proto/exec.h>
#include <reent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include "amiga_err.h"
#include <dos/datetime.h>

/* File descriptor table */
#define MAX_FDS 256
static BPTR fd_table[MAX_FDS];
static int fd_allocated[MAX_FDS];

/* Initialize file descriptor table */
static void init_fd_table(void)
{
    static int initialized = 0;
    if (!initialized) {
        memset(fd_table, 0, sizeof(fd_table));
        memset(fd_allocated, 0, sizeof(fd_allocated));
        
        /* Reserve standard file descriptors */
        fd_allocated[0] = 1; /* stdin */
        fd_allocated[1] = 1; /* stdout */
        fd_allocated[2] = 1; /* stderr */
        
        initialized = 1;
    }
}

/* Allocate a file descriptor */
static int alloc_fd(BPTR file)
{
    init_fd_table();
    
    for (int i = 0; i < MAX_FDS; i++) {
        if (!fd_allocated[i]) {
            fd_table[i] = file;
            fd_allocated[i] = 1;
            return i;
        }
    }
    
    return -1;
}

/* Free a file descriptor */
static void free_fd(int fd)
{
    if (fd >= 0 && fd < MAX_FDS && fd_allocated[fd]) {
        fd_allocated[fd] = 0;
        fd_table[fd] = 0;
    }
}

/* Get BPTR from file descriptor */
static BPTR get_bptr(int fd)
{
    if (fd >= 0 && fd < MAX_FDS && fd_allocated[fd]) {
        return fd_table[fd];
    }
    return 0;
}

/* Open file */
int _open_r(struct _reent *r, const char *path, int flags, int mode)
{
    if (!r) r = __getreent();
    if (!path) {
        r->_errno = EINVAL;
        return -1;
    }
    
    char npath[512];
    amigaos4_unix_to_dos(path, npath, sizeof(npath));
    
    BPTR fh = IDOS->Open(npath, amigaos4_oflags(flags, mode));
    if (!fh) {
        r->_errno = amigaos4_ioerr();
        return -1;
    }
    
    int fd = alloc_fd(fh);
    if (fd == -1) {
        IDOS->Close(fh);
        r->_errno = EMFILE;
        return -1;
    }
    
    return fd;
}

/* Close file */
int _close_r(struct _reent *r, int fd)
{
    if (!r) r = __getreent();
    
    BPTR fh = get_bptr(fd);
    if (!fh) {
        r->_errno = EBADF;
        return -1;
    }
    
    if (IDOS->Close(fh)) {
        free_fd(fd);
        return 0;
    } else {
        r->_errno = amigaos4_ioerr();
        return -1;
    }
}

/* Read from file */
_ssize_t _read_r(struct _reent *r, int fd, void *buf, size_t count)
{
    if (!r) r = __getreent();
    if (!buf) {
        r->_errno = EINVAL;
        return -1;
    }
    
    BPTR fh = get_bptr(fd);
    if (!fh) {
        r->_errno = EBADF;
        return -1;
    }
    
    LONG bytes_read = IDOS->Read(fh, buf, count);
    if (bytes_read == -1) {
        r->_errno = amigaos4_ioerr();
        return -1;
    }
    
    return bytes_read;
}

/* Write to file */
_ssize_t _write_r(struct _reent *r, int fd, const void *buf, size_t count)
{
    if (!r) r = __getreent();
    if (!buf) {
        r->_errno = EINVAL;
        return -1;
    }
    
    BPTR fh = get_bptr(fd);
    if (!fh) {
        r->_errno = EBADF;
        return -1;
    }
    
    LONG bytes_written = IDOS->Write(fh, (void *)buf, count);
    if (bytes_written == -1) {
        r->_errno = amigaos4_ioerr();
        return -1;
    }
    
    return bytes_written;
}

/* Seek in file */
_off_t _lseek_r(struct _reent *r, int fd, _off_t offset, int whence)
{
    if (!r) r = __getreent();
    
    BPTR fh = get_bptr(fd);
    if (!fh) {
        r->_errno = EBADF;
        return -1;
    }
    
    LONG mode;
    switch (whence) {
        case SEEK_SET: mode = OFFSET_BEGINNING; break;
        case SEEK_CUR: mode = OFFSET_CURRENT; break;
        case SEEK_END: mode = OFFSET_END; break;
        default:
            r->_errno = EINVAL;
            return -1;
    }
    
    LONG new_pos = IDOS->Seek(fh, offset, mode);
    if (new_pos == -1) {
        r->_errno = amigaos4_ioerr();
        return -1;
    }
    
    return new_pos;
}

/* Get file status */
int _fstat_r(struct _reent *r, int fd, struct stat *st)
{
    if (!r) r = __getreent();
    if (!st) {
        r->_errno = EINVAL;
        return -1;
    }
    
    BPTR fh = get_bptr(fd);
    if (!fh) {
        r->_errno = EBADF;
        return -1;
    }
    
    struct FileInfoBlock fib;
    if (!IDOS->ExamineFH(fh, &fib)) {
        r->_errno = amigaos4_ioerr();
        return -1;
    }
    
    memset(st, 0, sizeof(struct stat));
    
    /* File type */
    if (fib.fib_DirEntryType < 0) {
        st->st_mode = S_IFDIR | 0755;
    } else {
        st->st_mode = S_IFREG | 0644;
    }
    
    /* File size */
    st->st_size = fib.fib_Size;
    
    /* Timestamps */
    st->st_atime = fib.fib_Date.ds_Days * 86400 + 
                   fib.fib_Date.ds_Minute * 60 + 
                   fib.fib_Date.ds_Tick / 50;
    st->st_mtime = st->st_atime;
    st->st_ctime = st->st_atime;
    
    /* Device and inode */
    st->st_dev = 1;
    st->st_ino = (ino_t)(uintptr_t)fh;
    
    return 0;
}

/* Get file status by path */
int _stat_r(struct _reent *r, const char *path, struct stat *st)
{
    if (!r) r = __getreent();
    if (!path || !st) {
        r->_errno = EINVAL;
        return -1;
    }
    
    char npath[512];
    amigaos4_unix_to_dos(path, npath, sizeof(npath));
    
    struct FileInfoBlock fib;
    if (!IDOS->Examine(npath, &fib)) {
        r->_errno = amigaos4_ioerr();
        return -1;
    }
    
    memset(st, 0, sizeof(struct stat));
    
    /* File type */
    if (fib.fib_DirEntryType < 0) {
        st->st_mode = S_IFDIR | 0755;
    } else {
        st->st_mode = S_IFREG | 0644;
    }
    
    /* File size */
    st->st_size = fib.fib_Size;
    
    /* Timestamps */
    st->st_atime = fib.fib_Date.ds_Days * 86400 + 
                   fib.fib_Date.ds_Minute * 60 + 
                   fib.fib_Date.ds_Tick / 50;
    st->st_mtime = st->st_atime;
    st->st_ctime = st->st_atime;
    
    /* Device and inode */
    st->st_dev = 1;
    st->st_ino = (ino_t)(uintptr_t)&fib;
    
    return 0;
}

/* Create directory */
int _mkdir_r(struct _reent *r, const char *path, mode_t mode)
{
    if (!r) r = __getreent();
    if (!path) {
        r->_errno = EINVAL;
        return -1;
    }
    
    char npath[512];
    amigaos4_unix_to_dos(path, npath, sizeof(npath));
    
    if (IDOS->CreateDir(npath)) {
        return 0;
    } else {
        r->_errno = amigaos4_ioerr();
        return -1;
    }
}

/* Remove directory */
int _rmdir_r(struct _reent *r, const char *path)
{
    if (!r) r = __getreent();
    if (!path) {
        r->_errno = EINVAL;
        return -1;
    }
    
    char npath[512];
    amigaos4_unix_to_dos(path, npath, sizeof(npath));
    
    if (IDOS->DeleteFile(npath)) {
        return 0;
    } else {
        r->_errno = amigaos4_ioerr();
        return -1;
    }
}

/* Remove file */
int _unlink_r(struct _reent *r, const char *path)
{
    if (!r) r = __getreent();
    if (!path) {
        r->_errno = EINVAL;
        return -1;
    }
    
    char npath[512];
    amigaos4_unix_to_dos(path, npath, sizeof(npath));
    
    if (IDOS->DeleteFile(npath)) {
        return 0;
    } else {
        r->_errno = amigaos4_ioerr();
        return -1;
    }
}

/* Rename file */
int _rename_r(struct _reent *r, const char *old, const char *new)
{
    if (!r) r = __getreent();
    if (!old || !new) {
        r->_errno = EINVAL;
        return -1;
    }
    
    char old_path[512], new_path[512];
    amigaos4_unix_to_dos(old, old_path, sizeof(old_path));
    amigaos4_unix_to_dos(new, new_path, sizeof(new_path));
    
    if (IDOS->Rename(old_path, new_path)) {
        return 0;
    } else {
        r->_errno = amigaos4_ioerr();
        return -1;
    }
}

/* Get current working directory */
char *_getcwd_r(struct _reent *r, char *buf, size_t size)
{
    if (!r) r = __getreent();
    if (!buf || size == 0) {
        r->_errno = EINVAL;
        return NULL;
    }
    
    char dos_path[512];
    if (IDOS->GetCurrentDirName(dos_path, sizeof(dos_path)) == 0) {
        r->_errno = amigaos4_ioerr();
        return NULL;
    }
    
    amigaos4_dos_to_unix(dos_path, buf, size);
    return buf;
}

/* Change working directory */
int _chdir_r(struct _reent *r, const char *path)
{
    if (!r) r = __getreent();
    if (!path) {
        r->_errno = EINVAL;
        return -1;
    }
    
    char npath[512];
    amigaos4_unix_to_dos(path, npath, sizeof(npath));
    
    if (IDOS->ChangeDir(npath)) {
        return 0;
    } else {
        r->_errno = amigaos4_ioerr();
        return -1;
    }
}

/* Get time */
time_t _time_r(struct _reent *r, time_t *t)
{
    if (!r) r = __getreent();
    struct DateStamp ds;
    IDOS->DateStamp(&ds);
    // Convert AmigaOS 4 date to Unix timestamp
    // AmigaOS 4 uses days since 1978-01-01
    time_t current_time = (ds.ds_Days - 2922) * 86400 +
                         ds.ds_Minute * 60 +
                         ds.ds_Tick / 50;
    if (t) {
        *t = current_time;
    }
    return current_time;
}

/* Get clock time */
clock_t _clock_r(struct _reent *r)
{
    if (!r) r = __getreent();
    
    /* Use ExecSG's GetUpTime() for high-resolution timing */
    uint64_t uptime = IExec->GetUpTime();
    return (clock_t)(uptime / 1000); /* Convert to milliseconds */
}

/* Sleep */
unsigned int _sleep_r(struct _reent *r, unsigned int seconds)
{
    if (!r) r = __getreent();
    
    IDOS->Delay(seconds * 50); /* Convert seconds to ticks */
    return 0;
}

/* Get process ID */
pid_t _getpid_r(struct _reent *r)
{
    if (!r) r = __getreent();
    
    /* Return a unique identifier for the current process */
    return (pid_t)(uintptr_t)IExec->FindTask(NULL);
}

/* Exit process */
void _exit_r(struct _reent *r, int status)
{
    if (!r) r = __getreent();
    
    /* Clean up */
    _reclaim_reent(r);
    
    /* Exit the process */
    IDOS->Exit(status);
}

/* Not implemented syscalls */
int _fork_r(struct _reent *r) { r->_errno = ENOSYS; return -1; }
int _execve_r(struct _reent *r, const char *path, char *const argv[], char *const envp[]) { r->_errno = ENOSYS; return -1; }
int _wait_r(struct _reent *r, int *status) { r->_errno = ENOSYS; return -1; }
int _kill_r(struct _reent *r, pid_t pid, int sig) { r->_errno = ENOSYS; return -1; }
int _gettimeofday_r(struct _reent *r, struct timeval *tv, void *tz) { r->_errno = ENOSYS; return -1; }
int _times_r(struct _reent *r, struct tms *tms) { r->_errno = ENOSYS; return -1; }
int _link_r(struct _reent *r, const char *old, const char *new) { r->_errno = ENOSYS; return -1; }
int _symlink_r(struct _reent *r, const char *old, const char *new) { r->_errno = ENOSYS; return -1; }
int _readlink_r(struct _reent *r, const char *path, char *buf, size_t bufsize) { r->_errno = ENOSYS; return -1; }
int _dup2_r(struct _reent *r, int oldfd, int newfd) { r->_errno = ENOSYS; return -1; }
int _isatty_r(struct _reent *r, int fd) { r->_errno = ENOSYS; return -1; } 