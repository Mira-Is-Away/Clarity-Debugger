#ifndef MIRA_CLARITY_H_
#define MIRA_CLARITY_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#ifndef MIRA_CLARITY_DEBUG

    /* Release mode stubs: macros expand to nothing or direct malloc/free calls */
    #define CLARITY_LOG_OK(...)
    #define CLARITY_LOG_INFO(...)
    #define CLARITY_LOG_WARN(...)
    #define CLARITY_LOG_ERROR(...)
    #define CLARITY_ASSERT(condition, ...)
    #define CLARITY_MALLOC(size) malloc(size)
    #define CLARITY_FREE(ptr) free(ptr);
    #define CLARITY_MEM_REPORT()

#else
#if defined(_MSC_VER) // Windows Compatibility (Microsoft Visual C++)
    #define CLARITY_TRAP() __debugbreak()
#else                 // GCC, Clang, and other C compilers
    #define CLARITY_TRAP() __builtin_trap()
#endif

typedef enum ClarityMessageType {
    CLARITY_OK,
    CLARITY_INFO,
    CLARITY_WARN,
    CLARITY_ERROR 
} ClarityMessageType;

#define CLARITY_LOG_OK(...) \
    clarity_log_message("OK", CLARITY_OK, __VA_ARGS__)

#define CLARITY_LOG_INFO(...) \
    clarity_log_message("INFO", CLARITY_INFO, __VA_ARGS__)

#define CLARITY_LOG_WARN(...) \
    clarity_log_message("WARN", CLARITY_WARN, __VA_ARGS__)

#define CLARITY_LOG_ERROR(...) \
    clarity_log_message("ERROR", CLARITY_ERROR, __VA_ARGS__)


#define CLARITY_ASSERT(condition, ...) \
    do { \
        if (!(condition)) { \
            clarity_assert_failed(#condition, __FILE__, __LINE__, __func__, \
                                  __VA_ARGS__); \
            CLARITY_TRAP(); \
        } \
    } while (0)

#define CLARITY_MALLOC(size) clarity_malloc(size, __FILE__, __LINE__, __func__);

#define CLARITY_FREE(ptr) \
    clarity_free(ptr, __FILE__, __LINE__, __func__);

#define CLARITY_MEM_REPORT() clarity_mem_report();

/* ----------------------------------------------------------------------------
 * Function Declarations (Public Interface)
 * ---------------------------------------------------------------------------- */

/**
 * @brief Logs a formatted message with a custom prefix and warning highlight flag.
 */
void clarity_log_message(const char *tag, ClarityMessageType type, const char *msg, ...);

/**
 * @brief Handles assertion failures by printing diagnostic info and message.
 */
void clarity_assert_failed(const char *expr, const char *file, int line,
                           const char *func, const char *msg, ...);

/* Forward declaration of the internal memory header struct */
typedef struct ClarityMemoryHeader ClarityMemoryHeader;

/**
 * @brief Allocates memory with tracking header prepended.
 */
void* clarity_malloc(size_t size, const char *file, int line, const char *func);

/**
 * @brief Frees tracked memory by reading the prepended header.
 */
void clarity_free(void* ptr, const char *file, int line, const char* func);

/**
 * @brief Prints a summary report of all currently active memory allocations/leaks.
 */
void clarity_mem_report(void);

/* ============================================================================
 * IMPLEMENTATION SECTION
 * 
 * Included only when MIRA_CLARITY_IMPL is defined prior to including clarity.h
 * ============================================================================ */
#ifdef MIRA_CLARITY_IMPL

#define CLARITY_MEM_MAGIC 0xDEADBEEF

typedef struct ClarityMemoryHeader {
    size_t size;                      /* Size of the user payload in bytes */
    int line;                         /* Line number where allocation occurred */
    const char *file;                 /* Source file where allocation occurred */
    struct ClarityMemoryHeader *next; /* Pointer to the next allocation node in the list */
    struct ClarityMemoryHeader *prev; /* Pointer to the previous allocation node in the list */
    unsigned int magic;               /* Validation magic number (CLARITY_MEM_MAGIC) */
} ClarityMemoryHeader;

static ClarityMemoryHeader *g_clarity_alloc_head = NULL;
static size_t g_clarity_alloc_amount_bytes = 0;

/**
 * @brief Allocates memory with a metadata header attached for leak tracking.
 */
void *clarity_malloc(size_t size, const char *file, int line, const char *func) {
    /* Calculate total bytes needed: Header + User Requested Payload */
    size_t final_size = size + sizeof(ClarityMemoryHeader);
    
    /* Allocate the combined memory block using system malloc */
    ClarityMemoryHeader *header = (ClarityMemoryHeader*) malloc(final_size);

    /* Handle allocation failure gracefully */
    if (!header) {
        CLARITY_LOG_WARN("Failed to allocate memory at %s:%d (%s)",
                         file, line, func);
        return NULL;
    }

    /* Populate debug metadata */
    header->size = size;
    header->file = file;
    header->line = line;

    /* 
     * Insert the new header at the beginning (head) of the doubly-linked list.
     */
    header->prev = NULL;
    header->next = g_clarity_alloc_head;
    if (g_clarity_alloc_head) {
        g_clarity_alloc_head->prev = header;
    }
    g_clarity_alloc_head = header;

    /* Set magic number to mark valid active allocation */
    header->magic = CLARITY_MEM_MAGIC;

    /* Update total allocated memory counter */
    g_clarity_alloc_amount_bytes += size;

    /* Return pointer to memory directly AFTER the header */
    return (void*)(header + 1);
}

/**
 * @brief Frees a tracked memory block and removes it from the leak tracking list.
 */
void clarity_free(void* ptr, const char *file, int line, const char *func) {
    /* Standard C free(NULL) is a valid no-op, but we log a warning for transparency */
    if (ptr == NULL) {
        CLARITY_LOG_WARN("Attempted to free NULL pointer at %s:%d (%s)",
                         file, line, func);
        return;
    }

    /* Offset backwards to obtain the original memory header */
    ClarityMemoryHeader *header = ((ClarityMemoryHeader*)ptr) - 1;

    /* Verify magic number to catch double-frees, un-tracked pointers, or buffer overflows */
    if (header->magic != CLARITY_MEM_MAGIC) {
        CLARITY_LOG_WARN("Attempted to free untracked or corrupted pointer at %s:%d (%s)",
                         file, line, func);
        return;
    }

    /* Clear magic number to prevent accidental re-use or double-freeing */
    header->magic = 0x0;
    
    /* Deduct size from total tracked allocation count */
    g_clarity_alloc_amount_bytes -= header->size;

    /* 
     * Unlink the node from the doubly-linked list:
     */
    if (header->prev) header->prev->next = header->next;
    if (header->next) header->next->prev = header->prev;
    if (header == g_clarity_alloc_head) g_clarity_alloc_head = header->next;

    /* Free the entire memory block (header + user payload) */
    free(header);
}

/**
 * @brief Iterates over the linked list of active allocations to report memory leaks.
 */
void clarity_mem_report(void) {
    if (g_clarity_alloc_head == NULL) {
        printf("\033[0;32m[MEMORY CLEAN] \033[0m");
        printf("No memory leaks were detected.\n");
        return;
    }

    fprintf(stderr, "\033[0;31m[MEMORY LEAK] \033[0m");
    fprintf(stderr, "Memory leaks were detected. Total amount leaked (bytes): %zu\n\n",
            g_clarity_alloc_amount_bytes);

    /* Traverse the linked list from head to tail */
    ClarityMemoryHeader *cur = g_clarity_alloc_head;
    while (cur) {
        fprintf(stderr, "-> Lost %zu bytes from %s:%d\n", cur->size, cur->file,
                cur->line);
        cur = cur->next;
    }
}

void clarity_log_message(const char *tag, ClarityMessageType type, const char *msg, ...) {
    // Tag prefix colour
    switch (type) {
    case CLARITY_INFO:
        printf("\033[0;37m");
        break;
    case CLARITY_WARN:
        printf("\033[0;33m");
        break;
    case CLARITY_OK:
        printf("\033[0;32m");
        break;
    case CLARITY_ERROR:
        printf("\033[0;31m");
        break;
    }
    

    printf("[%s] \033[0m", tag);
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf("\n");
}

/**
 * @brief Prints assertion failure diagnostics to stderr.
 */
void clarity_assert_failed(const char *expr, const char *file, int line,
                           const char *func, const char *msg, ...) {
    fprintf(stderr, "\033[0;31m[ASSERT FAILED]\033[0m\n");
    fprintf(stderr, "   Expression: %s\n", expr);
    fprintf(stderr, "   Location: %s:%d (%s)\n", file, line, func);

    fprintf(stderr, "   Message: ");
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    fprintf(stderr, "\n");
}

#endif // MIRA_CLARITY_IMPL

#endif // MIRA_CLARITY_DEBUG
#endif // MIRA_CLARITY_H_