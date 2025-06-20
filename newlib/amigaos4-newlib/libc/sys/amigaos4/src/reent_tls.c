#include <exec/exec.h>
#include <proto/exec.h>
#include <reent.h>
#include <stdlib.h>
#include <string.h>

/* TLS key for storing per-thread reentrant structure */
static APTR tlskey = NULL;
static struct SignalSemaphore *tls_sem = NULL;

/* Global reentrant structure for main thread */
static struct _reent _global_reent;

/* Initialize TLS key for reentrant structures */
void __attribute__((constructor))
__amigaos4_init_tls(void)
{
    if (!tls_sem) {
        tls_sem = IExec->AllocMem(sizeof(struct SignalSemaphore), MEMF_ANY);
        if (tls_sem) {
            IExec->InitSemaphore(tls_sem);
        }
    }
    
    if (tls_sem) {
        IExec->ObtainSemaphore(tls_sem);
        if (!tlskey) {
            tlskey = IExec->NewTLS();
            if (!tlskey) {
                /* Fallback: use global reentrant structure */
                memset(&_global_reent, 0, sizeof(_global_reent));
            }
        }
        IExec->ReleaseSemaphore(tls_sem);
    } else {
        /* Fallback: use global reentrant structure */
        memset(&_global_reent, 0, sizeof(_global_reent));
    }
}

/* Get the reentrant structure for the current thread */
struct _reent *__getreent(void)
{
    struct _reent *r;
    
    if (!tlskey) {
        /* TLS not initialized, use global structure */
        return &_global_reent;
    }
    
    r = (struct _reent *)IExec->GetTLS(tlskey);
    if (!r) {
        /* Allocate new reentrant structure for this thread */
        r = (struct _reent *)IExec->AllocMemTags(sizeof(struct _reent),
            MEMF_PRIVATE,
            TAG_END);
        
        if (r) {
            /* Initialize the reentrant structure */
            memset(r, 0, sizeof(struct _reent));
            
            /* Set up default values */
            r->_errno = 0;
            r->_stdin = NULL;
            r->_stdout = NULL;
            r->_stderr = NULL;
            
            /* Store in TLS */
            IExec->SetTLS(tlskey, r);
        } else {
            /* Allocation failed, use global structure */
            r = &_global_reent;
        }
    }
    
    return r;
}

/* Clean up TLS when thread exits */
void __attribute__((destructor))
__amigaos4_cleanup_tls(void)
{
    if (tlskey) {
        struct _reent *r = (struct _reent *)IExec->GetTLS(tlskey);
        if (r && r != &_global_reent) {
            /* Free thread-specific reentrant structure */
            _reclaim_reent(r);
            IExec->FreeMem(r, sizeof(struct _reent));
        }
        
        /* Free TLS key */
        IExec->FreeTLS(tlskey);
        tlskey = NULL;
    }
    
    if (tls_sem) {
        IExec->FreeMem(tls_sem, sizeof(struct SignalSemaphore));
        tls_sem = NULL;
    }
}

/* Initialize reentrant structure with default values */
void _reclaim_reent(struct _reent *r)
{
    if (!r) return;
    
    /* Free any allocated memory in the reentrant structure */
    if (r->_emergency) {
        IExec->FreeMem(r->_emergency, IExec->GetVecSize(r->_emergency));
        r->_emergency = NULL;
    }
    
    if (r->_mp) {
        /* Free mp-related memory if allocated */
        if (r->_mp->_result_k) {
            IExec->FreeMem(r->_mp->_result_k, IExec->GetVecSize(r->_mp->_result_k));
            r->_mp->_result_k = NULL;
        }
        IExec->FreeMem(r->_mp, sizeof(*r->_mp));
        r->_mp = NULL;
    }
    
    if (r->_misc) {
        IExec->FreeMem(r->_misc, IExec->GetVecSize(r->_misc));
        r->_misc = NULL;
    }
    
    if (r->_localtime_buf) {
        IExec->FreeMem(r->_localtime_buf, IExec->GetVecSize(r->_localtime_buf));
        r->_localtime_buf = NULL;
    }
    
    if (r->_asctime_buf) {
        IExec->FreeMem(r->_asctime_buf, IExec->GetVecSize(r->_asctime_buf));
        r->_asctime_buf = NULL;
    }
    
    if (r->_sig_func) {
        IExec->FreeMem(r->_sig_func, IExec->GetVecSize(r->_sig_func));
        r->_sig_func = NULL;
    }
    
    if (r->_atexit) {
        IExec->FreeMem(r->_atexit, IExec->GetVecSize(r->_atexit));
        r->_atexit = NULL;
    }
    
    if (r->_atexit0) {
        IExec->FreeMem(r->_atexit0, IExec->GetVecSize(r->_atexit0));
        r->_atexit0 = NULL;
    }
    
    /* Reset other fields */
    r->_errno = 0;
    r->_stdin = NULL;
    r->_stdout = NULL;
    r->_stderr = NULL;
    r->_inc = 0;
    r->_current_category = 0;
    r->_current_locale = NULL;
}

/* Allocate memory for reentrant structure */
void *_malloc_r(struct _reent *r, size_t size)
{
    if (!r) r = __getreent();
    
    void *ptr = IExec->AllocMemTags(size,
        MEMF_PRIVATE,
        TAG_END);
    
    if (!ptr) {
        r->_errno = ENOMEM;
    }
    
    return ptr;
}

/* Free memory allocated by _malloc_r */
void _free_r(struct _reent *r, void *ptr)
{
    if (!r) r = __getreent();
    
    if (ptr) {
        IExec->FreeMem(ptr, IExec->GetVecSize(ptr));
    }
}

/* Reallocate memory */
void *_realloc_r(struct _reent *r, void *ptr, size_t size)
{
    if (!r) r = __getreent();
    
    if (!ptr) {
        return _malloc_r(r, size);
    }
    
    if (size == 0) {
        _free_r(r, ptr);
        return NULL;
    }
    
    void *new_ptr = IExec->AllocMemTags(size,
        MEMF_PRIVATE,
        TAG_END);
    
    if (!new_ptr) {
        r->_errno = ENOMEM;
        return NULL;
    }
    
    /* Copy old data */
    size_t old_size = IExec->GetVecSize(ptr);
    if (old_size > 0) {
        size_t copy_size = (old_size < size) ? old_size : size;
        memcpy(new_ptr, ptr, copy_size);
    }
    
    IExec->FreeMem(ptr, old_size);
    return new_ptr;
}

/* Calloc equivalent */
void *_calloc_r(struct _reent *r, size_t nmemb, size_t size)
{
    if (!r) r = __getreent();
    
    size_t total_size = nmemb * size;
    void *ptr = _malloc_r(r, total_size);
    
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    
    return ptr;
} 