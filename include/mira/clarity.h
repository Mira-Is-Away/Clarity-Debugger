#ifndef MIRA_CLARITY_H_
#define MIRA_CLARITY_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/*
Bypass macro definitions if application is not being compiled in
debug mode.
*/
#ifndef MIRA_CLARITY_DEBUG
    #define CLARITY_LOG_INFO(msg, ...)
    #define CLARITY_LOG_WARN(msg, ...)
    #define CLARITY_LOG_ERROR(msg, ...)
    #define CLARITY_ASSERT(condition, msg, ...)
    #define CLARITY_MALLOC(size) malloc(size)
    #define CLARITY_FREE(ptr) free(ptr);
    #define CLARITY_MEM_REPORT()
#else

#if defined(_MSC_VER) // Windows Compatibility
    #define CLARITY_TRAP() __debugbreak()
#else
    #define CLARITY_TRAP() __builtin_trap()
#endif

#define CLARITY_LOG_INFO(msg, ...) \
    clarity_log_output("INFO", false, msg, ##__VA_ARGS__)

#define CLARITY_LOG_WARN(msg, ...) \
    clarity_log_output("WARN", true, msg, ##__VA_ARGS__)

#define CLARITY_LOG_ERROR(msg, ...) \
    clarity_log_error(msg, ##__VA_ARGS__)

#define CLARITY_ASSERT(condition, msg, ...) \
    do { \
        if (!(condition)) { \
            clarity_assert_failed(#condition, __FILE__, __LINE__, __func__, \
                                  msg, ##__VA_ARGS__); \
            CLARITY_TRAP(); \
        } \
    } while (0)

#define CLARITY_MALLOC(size) clarity_malloc(size, __FILE__, __LINE__, __func__);

#define CLARITY_FREE(ptr) \
    clarity_free(ptr, __FILE__, __LINE__, __func__);

#define CLARITY_MEM_REPORT() clarity_mem_report();

void clarity_log_output(const char *prefix, int is_warning, const char *msg, ...);
void clarity_log_error(const char* msg, ...);
void clarity_assert_failed(const char *expr, const char *file, int line,
                           const char *func, const char *msg, ...);

typedef struct ClarityMemoryHeader ClarityMemoryHeader;
void* clarity_malloc(size_t size, const char *file, int line, const char *func);
void clarity_free(void* ptr, const char *file, int line, const char* func);
void clarity_mem_report(void);

#ifdef MIRA_CLARITY_IMPL

#include <stdarg.h>

#define CLARITY_MEM_MAGIC 0xDEADBEEF

/**
 * @struct ClarityMemoryHeader
 * 
 * A header struct that preceeds any memory allocated by
 * clarity_malloc() and holds debug metainfo regarding
 * said memory block (in particular, the file, line and
 * function from whence that memory block was allocated)
 */
typedef struct ClarityMemoryHeader {
    size_t size;
    int line;
    const char *file;
    struct ClarityMemoryHeader *next;
    struct ClarityMemoryHeader *prev;
    unsigned int magic;
} ClarityMemoryHeader;

static ClarityMemoryHeader *g_clarity_alloc_head = NULL;
static size_t g_clarity_alloc_amount_bytes = 0;

void* clarity_malloc(size_t size, const char *file, int line, const char *func) {
    size_t final_size = size + sizeof(ClarityMemoryHeader);
    ClarityMemoryHeader *header = (ClarityMemoryHeader*) malloc(final_size);

    if (!header) {
        CLARITY_LOG_WARN("Failed to allocate memory at %s:%d (%s)",
                         file, line, func);
        return NULL;
    }

    header->size = size;
    header->file = file;
    header->line = line;
    header->prev = NULL;
    header->next = g_clarity_alloc_head;
    if (g_clarity_alloc_head) {
        g_clarity_alloc_head->prev = header;
    }
    g_clarity_alloc_head = header;

    header->magic = CLARITY_MEM_MAGIC;

    g_clarity_alloc_amount_bytes += size;

    return (void*)(header + 1);
}

void clarity_free(void* ptr, const char *file, int line, const char *func) {
    if (ptr == NULL) {
        CLARITY_LOG_WARN("Attempted to free NULL pointer at %s:%d (%s)",
                         file, line, func);
        return;
    }

    ClarityMemoryHeader *header = ((ClarityMemoryHeader*)ptr) - 1;

    if (header->magic != CLARITY_MEM_MAGIC) {
        CLARITY_LOG_WARN("Attempted to free untracked or corrupted pointer at %s:%d (%s)",
                         file, line, func);
        return;
    }

    header->magic = 0x0;
    g_clarity_alloc_amount_bytes -= header->size;

    if (header->prev) header->prev->next = header->next;
    if (header->next) header->next->prev = header->prev;
    if (header == g_clarity_alloc_head) g_clarity_alloc_head = header->next;

    free(header);
}

void clarity_mem_report(void) {
    if (g_clarity_alloc_head == NULL) {
        printf("\033[0;32m[MEMORY CLEAN] \033[0m");
        printf("No memory leaks were detected.\n");
        return;
    }

    fprintf(stderr, "\033[0;31m[MEMORY LEAK] \033[0m");
    fprintf(stderr, "Memory leaks were detected. Total amount leaked (bytes): %zu\n\n",
            g_clarity_alloc_amount_bytes);

    ClarityMemoryHeader *cur = g_clarity_alloc_head;
    while (cur) {
        fprintf(stderr, "-> Lost %zu bytes from %s:%d\n", cur->size, cur->file,
                cur->line);
        cur = cur->next;
    }
}

void clarity_log_error(const char* msg, ...) {

    printf("\033[0;31m[ERROR] \033[0m");

    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf("\n");
}

void clarity_log_output(const char *prefix, int is_warning, const char *msg, ...) {
    if (is_warning) {
        printf("\033[0;33m");
    } else {
        printf("\033[0;32m");
    }

    printf("[%s] \033[0m", prefix);

    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf("\n");
}

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

#endif // MIRA CLARITY_DEBUG
#endif // MIRA_CLARITY_H_