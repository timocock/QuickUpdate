/*-----------------------------------------------------------------
   Natty.c  –  heuristic compatibility scanner for Amiga HUNK
   binaries, intended for a "hardened" OS mode discussion.
   • C89, builds with SAS/C 6.x
   • Reads .fd files from the "FD:" assign at startup
   • Detects (JSR|JMP)  (d16, A6)  calls and maps offsets → names
   • Computes a risk score based on dangerous APIs
------------------------------------------------------------------*/
/*
 * ============================================================================
 * EXTENDING Natty hunk scanner – Instructions for Enhancing Compatibility Analysis
 * ============================================================================
 *
 * Overview:
 * This program heuristically scans Amiga HUNK binaries for known function
 * calls and patterns that may break under a hardened OS mode (e.g. MMU-based
 * memory protection). It uses .fd files to identify library calls via offsets
 * and scores their risk. This guide explains how to expand the scanner.
 *
 * STEP-BY-STEP EXTENSION PLAN
 * ----------------------------------------------------------------------------
 *
 * 1. Add more "risky" patterns to detect:
 *    -------------------------------------
 *    Look for common low-level or forbidden operations that may cause trouble:
 *
 *    A. JSR/JMP (d16,A6)
 *       Already implemented. Matches library calls via LVO.
 *
 *    B. ExecBase references (absolute)
 *       - MOVE.L 4.W,A0     → 0x203C 00000004 (A0)
 *       - Any code reading from address 0x4 (ExecBase)
 *       Add pattern matching for MOVE/PEA/LEA 4.W into registers.
 *
 *    C. SetFunction usage:
 *       - Can be matched via FD offset (already in risk table), or:
 *       - Manual scan for known opcode patterns setting vectors.
 *
 *    D. Direct patching of system vectors:
 *       - Example: MOVE.L #newfunc,$4A.W
 *       - Look for MOVEs into absolute addresses (e.g. $68, $84, $4A)
 *       - Add opcode matching for: MOVE.L #imm, abs.w
 *
 *    E. Use of Supervisor(), SuperState(), etc:
 *       - Match by FD offset (Supervisor = -30 in exec.library)
 *       - Can also be detected by opcode: TRAP #0 with special sequence
 *
 *    F. Use of nonstandard base pointers:
 *       - JMP/JSR via (An) or (Ax,Dx) where An != A6
 *       - Heuristic: any indirect jump/call via unknown base
 *
 *    G. Inline ROM references:
 *       - MOVE.L or PEA with absolute ROM addresses ($F80000+)
 *       - Look for 0x2XXX with 0xF8xxxx values
 *
 *    H. Patching or peeking task list or TCB:
 *       - Access to address offsets around ExecBase+...
 *       - Look for: TASK struct offsets like pr_Task, pr_CLI, tc_Node
 *
 *    I. Custom interrupt manipulation:
 *       - AddIntServer(), SetIntVector(), or manual vector poking
 *       - Match known offset or absolute address use
 *
 *    J. Accessing Chip RAM via fixed address:
 *       - E.g., $C00000–$DFFFFF
 *       - Look for code referencing this range
 *
 * 2. Expand the Weight Table
 *    ------------------------
 *    Located in the Weights[] array.
 *    Add names of dangerous functions and assign a "risk score".
 *
 *    Example:
 *      { "AddIntServer", 30 },
 *      { "SuperState",   40 },
 *      { "SetIntVector", 35 },
 *
 * 3. Support for Traps, illegal opcodes or SVC mode entry
 *    -----------------------------------------------------
 *    Trap #x is encoded as:
 *      - TRAP #0 = 0x4E40
 *      - TRAP #1 = 0x4E41
 *    You can scan for these directly.
 *
 *    Certain tools install custom vectors via traps (nonportable).
 *
 * 4. Visual Output Improvements
 *    ---------------------------
 *    - Add printf diagnostics for each match (opcode, offset, function)
 *    - Use color codes or severity levels if desired
 *    - Optional: write summary to a file or stdout in JSON/CSV
 *
 * 5. Cross-reference against known safe APIs
 *    ---------------------------------------
 *    Load a list of allowed/safe LVOs (e.g. via safelist.txt) and flag any
 *    unknown or non-whitelisted calls for review.
 *
 * 6. Add segment-level SHA1 or CRC
 *    ------------------------------
 *    Useful for cataloging known-good binaries and detecting tampering.
 *
 *    Optional: use exec CheckSum() if reimplementing in OS context
 *
 * ============================================================================
 * SUMMARY: What to Detect (Pattern Checklist)
 * ============================================================================
 *  ✔ JSR/JMP (d16,A6) → LVO-based calls
 *  ✔ MOVE/PEA/LEA 4.W → ExecBase access
 *  ✔ MOVE.L #imm, abs.w → direct patching
 *  ✔ TRAP #0/1 opcodes → SVC/ROM calls
 *  ✔ MOVE.L / PEA to 0xF80000+ → ROM references
 *  ✔ JMP (A0), JMP (Ax,Dx) etc. → nonstandard indirection
 *  ✔ References to $C00000–$DFFFFF → chipmem
 *  ✔ Known dangerous functions via FD: → SetFunction, Forbid, etc
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <proto/dos.h>

/* ---------- minimal hash structures ----------------------------------- */
#define MAX_FUNC   32
#define HASH_SIZE  257
#define MAX_LINE   256

struct FDEntry {
    LONG  offset;                /* negative LVO in bytes (e.g., -42)   */
    char  name[MAX_FUNC];
    struct FDEntry *next;
};

static struct FDEntry *Hash[HASH_SIZE];

/* ---------- risk weights ---------------------------------------------- */
struct WeightEntry { const char *name; int weight; };
static const struct WeightEntry Weights[] = {
    { "SetFunction", 40 },
    { "Supervisor",  20 },
    { "Forbid",      20 },
    { "Permit",      20 },
    { "AddIntServer", 30 },
    { "SetIntVector", 35 },
    { "SuperState",   40 },
    { "ExecBase",     25 },
    { "ChipMem",      30 },
    { "ROMRef",       25 },
    { "VectorPatch",  35 },
    { "TCBAccess",    30 },
    { "ListManip",    25 },
    { "IntLevel",     35 },
    { "VBRManip",     40 },
    { "SelfMod",      45 },
    { NULL,           0 }
};

/* ---------- hash helpers ---------------------------------------------- */
static UWORD hash_long(LONG v) { return ((UWORD)(v)) % HASH_SIZE; }

static void insert_fd(LONG off, const char *name)
{
    struct FDEntry *e = (struct FDEntry *)malloc(sizeof(struct FDEntry));
    if (!e) return;
    e->offset = off;
    strncpy(e->name, name, MAX_FUNC-1); e->name[MAX_FUNC-1] = '\0';
    UWORD h = hash_long(off);
    e->next = Hash[h];
    Hash[h] = e;
}

static const char *lookup_fd(LONG off)
{
    struct FDEntry *e = Hash[hash_long(off)];
    while (e) {
        if (e->offset == off) return e->name;
        e = e->next;
    }
    return NULL;
}

/* ---------- FD file parsing helpers ---------------------------------- */
static int parse_fd_line(const char *line, char *func, int *neg)
{
    char *p;
    int i;
    
    /* Skip leading whitespace */
    while (*line && isspace(*line)) line++;
    
    /* Skip comments */
    if (*line == ';') return 0;
    
    /* Parse function name */
    p = func;
    i = 0;
    while (*line && !isspace(*line) && i < MAX_FUNC-1) {
        *p++ = *line++;
        i++;
    }
    *p = '\0';
    
    /* Skip to offset */
    while (*line && isspace(*line)) line++;
    
    /* Skip optional minus sign */
    if (*line == '-') {
        line++;
        *neg = 1;
    } else {
        *neg = 0;
    }
    
    /* Parse number */
    if (!isdigit(*line)) return 0;
    *neg = atoi(line);
    if (*neg == 0) return 0;
    
    return 1;
}

/* ---------- load all .fd from FD: ------------------------------------- */
static void load_fd_dir(void)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    char path[MAX_LINE];
    char line[MAX_LINE];
    char func[MAX_FUNC];
    int neg;
    BPTR fd;
    
    /* Initialize hash table */
    for (i = 0; i < HASH_SIZE; i++) {
        Hash[i] = NULL;
    }
    
    lock = Lock("FD:", ACCESS_READ);
    if (!lock) { 
        printf("No FD: assign found\n"); 
        return; 
    }

    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    if (!fib) { 
        UnLock(lock); 
        return; 
    }

    if (Examine(lock, fib))
    {
        while (ExNext(lock, fib))
        {
            /* Skip directories */
            if (fib->fib_DirEntryType < 0) continue;
            
            /* Check for .fd extension */
            i = strlen(fib->fib_FileName);
            if (i < 3 || stricmp(fib->fib_FileName+i-3, ".fd")) continue;

            /* Build path */
            strcpy(path, "FD:");
            strcat(path, fib->fib_FileName);

            fd = Open(path, MODE_OLDFILE);
            if (!fd) continue;

            while (FGets(fd, line, sizeof(line)-1))
            {
                if (parse_fd_line(line, func, &neg)) {
                    insert_fd(-neg, func);
                }
            }
            Close(fd);
        }
    }
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
}

/* ---------- constants for HUNK parsing --------------------------------- */
#define HUNK_HEADER  0x3F3
#define HUNK_CODE    0x3E9
#define HUNK_END     0x3F2

/* read a big‑endian LONG safely */
static LONG read_be_long(FILE *f)
{
    UBYTE b[4];
    if (fread(b,1,4,f)!=4) return -1;
    return ((LONG)b[0]<<24)|((LONG)b[1]<<16)|((LONG)b[2]<<8)|b[3];
}

/* ---------- pattern detection helpers --------------------------------- */
static int is_execbase_ref(const UBYTE *p)
{
    /* MOVE.L 4.W,A0 = 0x203C 00000004 */
    if (p[0] == 0x20 && p[1] == 0x3C && p[2] == 0x00 && p[3] == 0x00 && 
        p[4] == 0x00 && p[5] == 0x04) return 1;
    return 0;
}

static int is_chipmem_ref(const UBYTE *p)
{
    /* Check for references to $C00000-$DFFFFF */
    if (p[0] == 0x20 && p[1] == 0x3C) {  /* MOVE.L #imm, */
        ULONG addr = (p[2]<<24)|(p[3]<<16)|(p[4]<<8)|p[5];
        if (addr >= 0xC00000 && addr <= 0xDFFFFF) return 1;
    }
    return 0;
}

static int is_rom_ref(const UBYTE *p)
{
    /* Check for references to $F80000+ */
    if (p[0] == 0x20 && p[1] == 0x3C) {  /* MOVE.L #imm, */
        ULONG addr = (p[2]<<24)|(p[3]<<16)|(p[4]<<8)|p[5];
        if (addr >= 0xF80000) return 1;
    }
    return 0;
}

static int is_vector_patch(const UBYTE *p)
{
    /* MOVE.L #imm,abs.w */
    if (p[0] == 0x20 && p[1] == 0x3C) {  /* MOVE.L #imm, */
        /* Check for common vector addresses */
        ULONG addr = (p[2]<<24)|(p[3]<<16)|(p[4]<<8)|p[5];
        if (addr == 0x68 || addr == 0x84 || addr == 0x4A) return 1;
    }
    return 0;
}

static int is_trap_call(const UBYTE *p)
{
    /* TRAP #0 = 0x4E40, TRAP #1 = 0x4E41 */
    if ((p[0] == 0x4E && p[1] == 0x40) || (p[0] == 0x4E && p[1] == 0x41)) return 1;
    return 0;
}

/* ---------- additional pattern detection helpers ---------------------- */
static int is_tcb_access(const UBYTE *p)
{
    /* Check for both direct and indirect TCB access patterns */
    if (p[0] == 0x20 && p[1] == 0x3C) {  /* MOVE.L #imm, */
        ULONG addr = (p[2]<<24)|(p[3]<<16)|(p[4]<<8)|p[5];
        /* Common TCB offsets */
        if (addr == 0x0C || addr == 0x10 || addr == 0x14 || /* pr_Task, pr_CLI, pr_Console */
            addr == 0x18 || addr == 0x1C || addr == 0x20)   /* pr_FileSystem, pr_CES, pr_COS */
            return 1;
    }
    /* Check for indirect TCB access via ExecBase */
    if (p[0] == 0x20 && p[1] == 0x68) {  /* MOVE.L (a0,d0), */
        /* Common TCB offset patterns */
        if (p[2] == 0x0C || p[2] == 0x10 || p[2] == 0x14 ||
            p[2] == 0x18 || p[2] == 0x1C || p[2] == 0x20)
            return 1;
    }
    return 0;
}

static int is_list_manipulation(const UBYTE *p)
{
    /* Check for List structure manipulation patterns */
    if (p[0] == 0x20 && p[1] == 0x3C) {  /* MOVE.L #imm, */
        ULONG addr = (p[2]<<24)|(p[3]<<16)|(p[4]<<8)|p[5];
        /* Common List offsets */
        if (addr == 0x00 || addr == 0x04 || addr == 0x08)   /* LH_HEAD, LH_TAIL, LH_TAILPRED */
            return 1;
    }
    /* Check for common list operation patterns */
    if ((p[0] == 0x20 && p[1] == 0x68) ||  /* MOVE.L (a0,d0), */
        (p[0] == 0x20 && p[1] == 0x69) ||  /* MOVE.L (a1,d0), */
        (p[0] == 0x20 && p[1] == 0x6A))    /* MOVE.L (a2,d0), */
    {
        /* Common list operation offsets */
        if (p[2] == 0x00 || p[2] == 0x04 || p[2] == 0x08)
            return 1;
    }
    return 0;
}

static int is_int_level_manip(const UBYTE *p)
{
    /* Check for INTENA/INTREQ manipulation */
    if (p[0] == 0x20 && p[1] == 0x3C) {  /* MOVE.L #imm, */
        ULONG addr = (p[2]<<24)|(p[3]<<16)|(p[4]<<8)|p[5];
        if (addr == 0xDFF09A || addr == 0xDFF09C)  /* INTENA/INTREQ */
            return 1;
    }
    /* Check for common interrupt manipulation patterns */
    if (p[0] == 0x4E && p[1] == 0x75) {  /* RTS */
        /* Look for interrupt manipulation sequences */
        if (p[-2] == 0x4E && p[-1] == 0x73)  /* RTE */
            return 1;
    }
    return 0;
}

static int is_vbr_manipulation(const UBYTE *p)
{
    /* Check for VBR manipulation */
    if (p[0] == 0x4E && p[1] == 0x73)  /* MOVEC VBR, */
        return 1;
    /* Check for common vector manipulation patterns */
    if (p[0] == 0x20 && p[1] == 0x3C) {  /* MOVE.L #imm, */
        ULONG addr = (p[2]<<24)|(p[3]<<16)|(p[4]<<8)|p[5];
        if (addr >= 0x00000000 && addr <= 0x00000100)  /* Common vector table range */
            return 1;
    }
    return 0;
}

static int is_self_modifying(const UBYTE *p)
{
    /* Check for common self-modifying code patterns */
    if (p[0] == 0x20 && p[1] == 0x3C) {  /* MOVE.L #imm, */
        /* Look for writes to code segment addresses */
        ULONG addr = (p[2]<<24)|(p[3]<<16)|(p[4]<<8)|p[5];
        /* Amiga code segments typically start at 0x00000000 and can extend to 0x00FFFFFF */
        /* However, we should also check for writes to the current segment */
        if ((addr >= 0x00000000 && addr <= 0x00FFFFFF) ||  /* Common code segment range */
            (addr >= 0x01000000 && addr <= 0x01FFFFFF))    /* Extended code segment range */
            return 1;
    }
    /* Check for indirect code modification */
    if ((p[0] == 0x20 && p[1] == 0x68) ||  /* MOVE.L (a0,d0), */
        (p[0] == 0x20 && p[1] == 0x69) ||  /* MOVE.L (a1,d0), */
        (p[0] == 0x20 && p[1] == 0x6A))    /* MOVE.L (a2,d0), */
    {
        /* Look for common code modification patterns */
        if (p[2] >= 0x00 && p[2] <= 0xFF)  /* Common code modification range */
            return 1;
    }
    return 0;
}

static int is_stack_manipulation(const UBYTE *p)
{
    /* Check for stack manipulation */
    if ((p[0] == 0x4E && p[1] == 0x75) ||  /* RTS */
        (p[0] == 0x4E && p[1] == 0x77) ||  /* RTR */
        (p[0] == 0x4E && p[1] == 0x73))    /* RTE */
        return 1;
    /* Check for common stack manipulation patterns */
    if ((p[0] == 0x4E && p[1] == 0x71) ||  /* NOP */
        (p[0] == 0x4E && p[1] == 0x72))    /* STOP */
        return 1;
    return 0;
}

/* ---------- diagnostic reporting structures ------------------------- */
struct Finding {
    const char *type;      /* Category of finding */
    const char *desc;      /* Description of the issue */
    ULONG offset;          /* Offset in code where found */
    int severity;          /* Risk score for this finding */
    struct Finding *next;
};

struct DiagnosticReport {
    const char *filename;
    int total_score;
    struct Finding *findings;
    int finding_count;
};

/* ---------- diagnostic helpers -------------------------------------- */
static struct Finding *add_finding(struct DiagnosticReport *report, 
                                 const char *type, const char *desc, 
                                 ULONG offset, int severity)
{
    struct Finding *f = (struct Finding *)malloc(sizeof(struct Finding));
    if (!f) return NULL;
    
    f->type = type;
    f->desc = desc;
    f->offset = offset;
    f->severity = severity;
    f->next = report->findings;
    report->findings = f;
    report->finding_count++;
    return f;
}

static void free_report(struct DiagnosticReport *report)
{
    struct Finding *f = report->findings;
    while (f) {
        struct Finding *next = f->next;
        free(f);
        f = next;
    }
    report->findings = NULL;
    report->finding_count = 0;
}

static void print_report(const struct DiagnosticReport *report)
{
    printf("\n=== Compatibility Analysis Report ===\n");
    printf("File: %s\n", report->filename);
    printf("Total Risk Score: %d\n", report->total_score);
    printf("Findings: %d\n\n", report->finding_count);
    
    if (report->finding_count == 0) {
        printf("No compatibility issues found.\n");
        return;
    }
    
    printf("Detailed Findings:\n");
    printf("-----------------\n");
    
    struct Finding *f = report->findings;
    while (f) {
        printf("[%s] at offset 0x%08lx\n", f->type, f->offset);
        printf("  Severity: %d\n", f->severity);
        printf("  Issue: %s\n\n", f->desc);
        f = f->next;
    }
    
    printf("Compatibility Assessment:\n");
    printf("------------------------\n");
    if (report->total_score <= 20) {
        printf("Status: LIKELY SAFE\n");
        printf("This binary appears to be compatible with hardened OS mode.\n");
    } else if (report->total_score <= 50) {
        printf("Status: NEEDS REVIEW\n");
        printf("This binary may have compatibility issues that require manual review.\n");
    } else {
        printf("Status: PROBABLY BREAKS\n");
        printf("This binary is likely incompatible with hardened OS mode.\n");
    }
    printf("\n");
}

/* ---------- scan a code segment for risky patterns ------------------- */
static int scan_segment(const UBYTE *p, ULONG size, int *risk, 
                       struct DiagnosticReport *report, ULONG base_offset)
{
    const UBYTE *end;
    UWORD op;
    ULONG current_offset;
    const char *func_name;
    
    end = p + size;
    while (p+4 <= end)
    {
        op = (p[0]<<8)|p[1];
        current_offset = base_offset + (p - (const UBYTE *)p);
        
        /* JSR (d16,An): 0x4E90 | reg */
        if ((op & 0xFFF8) == 0x4E90 && (op & 0x0007) == 6)  /* A6 */
        {
            WORD disp = (p[2]<<8)|p[3];      /* signed */
            func_name = lookup_fd(disp);
            if (func_name)
            {
                for (const struct WeightEntry *w = Weights; w->name; ++w)
                    if (!strcmp(w->name,func_name)) {
                        *risk += w->weight;
                        add_finding(report, "Library Call", 
                                  func_name, current_offset, w->weight);
                    }
            }
            p += 4; continue;
        }
        
        /* JMP (d16,An): 0x4EB8 | reg */
        if ((op & 0xFFF8) == 0x4EB8 && (op & 0x0007) == 6)
        {
            WORD disp = (p[2]<<8)|p[3];
            func_name = lookup_fd(disp);
            if (func_name)
            {
                for (const struct WeightEntry *w = Weights; w->name; ++w)
                    if (!strcmp(w->name,func_name)) {
                        *risk += w->weight;
                        add_finding(report, "Library Jump", 
                                  func_name, current_offset, w->weight);
                    }
            }
            p += 4; continue;
        }

        /* Check for ExecBase references */
        if (is_execbase_ref(p)) {
            *risk += 25;
            add_finding(report, "ExecBase Access", 
                       "Direct access to ExecBase (4.W)", current_offset, 25);
            p += 6; continue;
        }

        /* Check for Chip RAM references */
        if (is_chipmem_ref(p)) {
            *risk += 30;
            add_finding(report, "Chip RAM Access", 
                       "Direct access to Chip RAM region", current_offset, 30);
            p += 6; continue;
        }

        /* Check for ROM references */
        if (is_rom_ref(p)) {
            *risk += 25;
            add_finding(report, "ROM Access", 
                       "Direct access to ROM region", current_offset, 25);
            p += 6; continue;
        }

        /* Check for vector patching */
        if (is_vector_patch(p)) {
            *risk += 35;
            add_finding(report, "Vector Patching", 
                       "Attempt to patch system vector", current_offset, 35);
            p += 6; continue;
        }

        /* Check for trap calls */
        if (is_trap_call(p)) {
            *risk += 20;
            add_finding(report, "Trap Call", 
                       "Use of TRAP instruction", current_offset, 20);
            p += 2; continue;
        }

        /* Check for TCB access */
        if (is_tcb_access(p)) {
            *risk += 30;
            add_finding(report, "TCB Access", 
                       "Direct access to Task Control Block", current_offset, 30);
            p += 6; continue;
        }

        /* Check for List manipulation */
        if (is_list_manipulation(p)) {
            *risk += 25;
            add_finding(report, "List Manipulation", 
                       "Direct manipulation of system lists", current_offset, 25);
            p += 6; continue;
        }

        /* Check for interrupt level manipulation */
        if (is_int_level_manip(p)) {
            *risk += 35;
            add_finding(report, "Interrupt Manipulation", 
                       "Direct manipulation of interrupt levels", current_offset, 35);
            p += 6; continue;
        }

        /* Check for VBR manipulation */
        if (is_vbr_manipulation(p)) {
            *risk += 40;
            add_finding(report, "VBR Manipulation", 
                       "Attempt to modify Vector Base Register", current_offset, 40);
            p += 2; continue;
        }

        /* Check for self-modifying code */
        if (is_self_modifying(p)) {
            *risk += 45;
            add_finding(report, "Self-Modifying Code", 
                       "Code attempts to modify itself", current_offset, 45);
            p += 6; continue;
        }

        /* Check for stack manipulation */
        if (is_stack_manipulation(p)) {
            *risk += 20;
            add_finding(report, "Stack Manipulation", 
                       "Unusual stack manipulation detected", current_offset, 20);
            p += 2; continue;
        }

        p += 2;
    }
    return 0;
}

/* ---------- score a single program ------------------------------------ */
static int score_program(const char *path, int *score_out)
{
    FILE *f = fopen(path,"rb");
    if (!f) { printf("%s: cannot open\n", path); return -1; }

    struct DiagnosticReport report = {0};
    report.filename = path;
    report.findings = NULL;
    report.finding_count = 0;

    LONG id = read_be_long(f);
    if (id != HUNK_HEADER) { 
        fclose(f); 
        printf("%s: not HUNK\n",path); 
        return -1; 
    }

    /* skip the rest of header table quickly */
    LONG tab_len = read_be_long(f);          /* # longs in name table */
    fseek(f, (tab_len+1)*4, SEEK_CUR);       /* names + first hunk num etc. */

    int risk = 0;
    ULONG current_offset = 0;
    
    for (;;)
    {
        LONG htype = read_be_long(f);
        if (htype == -1) break;
        if (htype == HUNK_CODE)
        {
            LONG sz = read_be_long(f) * 4;          /* size in bytes */
            UBYTE *buf = (UBYTE *)malloc(sz);
            if (!buf) { 
                free_report(&report);
                fclose(f); 
                return -1; 
            }
            fread(buf,1,sz,f);
            scan_segment(buf, sz, &risk, &report, current_offset);
            current_offset += sz;
            free(buf);
        }
        else if (htype == HUNK_END)
        {
            break;
        }
        else
        {
            /* skip unhandled hunk types */
            LONG sz = read_be_long(f) * 4;
            fseek(f, sz, SEEK_CUR);
        }
    }
    
    report.total_score = risk;
    print_report(&report);
    free_report(&report);
    
    fclose(f);
    *score_out = risk;
    return 0;
}

/* ---------- main -------------------------------------------------------- */
int main(int argc, char **argv)
{
    int i;
    int score;
    const char *verdict;
    
    if (argc < 2) {
        puts("Usage: HunkScan <file1> [file2...]");
        return 0;
    }

    load_fd_dir();   /* build hash from FD: */

    for (i = 1; i < argc; i++)
    {
        score = 0;
        if (score_program(argv[i], &score)) continue;

        if (score <= 20) {
            verdict = "Likely Safe";
        } else if (score <= 50) {
            verdict = "Needs Review";
        } else {
            verdict = "Probably Breaks";
        }

        printf("%-28s  Score:%3d  -> %s\n", argv[i], score, verdict);
    }
    return 0;
}
