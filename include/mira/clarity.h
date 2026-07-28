#ifndef MIRA_CLARITY_H_
#define MIRA_CLARITY_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

/**
 * ============================================================================
 * CLARITY DEBUGGING LIBRARY (clarity.h)
 * ============================================================================
 * 
 * Clarity is a single-header C library for logging, debugging, and memory 
 * leak tracking during development.
 * 
 * --- CONCEPT 1: Single-Header Library Pattern (STB Style) ---
 * In standard C projects, code is divided into header files (.h) for declarations
 * and source files (.c) for implementations.
 * Header-only libraries package both into a single file for easy integration.
 * 
 * How to use:
 * 1. Include this header (#include "mira/clarity.h") wherever you need its functions.
 * 2. In EXACTLY ONE C source file (.c) in the code where you want yo use it,
 * you must define MIRA_CLARITY_IMPL before including, like such:
 * 
 *    #define MIRA_CLARITY_IMPL
 *    #include "mira/clarity.h"
 * 
 * This causes the preprocessor to generate the actual function implementations 
 * in that single source file, avoiding "duplicate symbol" errors during linking.
 * ============================================================================
 */

/*
 * --- CONCEPT 2: Zero-Overhead Debugging via Conditional Compilation ---
 * Conditional compilation (#ifndef / #else / #endif) allows code to compile 
 * differently depending on preprocessor flags.
 * 
 * When the user wants to debug their application, they'll define a macro called
 * MIRA_CLARITY_DEBUG. If MIRA_CLARITY_DEBUG is NOT defined (e.g. when compiling
 * their code with "Release" mode turned on), all Clarity macros expand to empty
 * statements or raw standard library functions. This guarantees zero performance
 * or memory overhead from debugging tools in the final version of the code.
 */
#ifndef MIRA_CLARITY_DEBUG

    /* Release mode stubs: macros expand to nothing or direct malloc/free calls */
    #define CLARITY_LOG_INFO(...)
    #define CLARITY_LOG_WARN(...)
    #define CLARITY_LOG_ERROR(...)
    #define CLARITY_ASSERT(condition, ...)
    #define CLARITY_MALLOC(size) malloc(size)
    #define CLARITY_FREE(ptr) free(ptr);
    #define CLARITY_MEM_REPORT()

#else

/*
 * --- CONCEPT 3: Platform-Specific Debug Traps (Breakpoints) ---
 * When an assertion fails, we want execution to stop immediately in a debugger.
 * 
 * Built-in compiler functions (builtins):
 * - MSVC (Windows): __debugbreak() generates an instruction that stops the program's execution.
 * - GCC / Clang: __builtin_trap() emits an invalid instruction that triggers a hardware
 * exception (SIGTRAP/SIGILL) that pauses the debugger and stops the program's execution.
 */
#if defined(_MSC_VER) // Windows Compatibility (Microsoft Visual C++)
    #define CLARITY_TRAP() __debugbreak()
#else                 // GCC, Clang, and other C compilers
    #define CLARITY_TRAP() __builtin_trap()
#endif

/*
 * --- CONCEPT 4: Variadic Macros and Call-Site Context ---
 * 
 * Variadic Macros:
 * The `...` syntax allows macros to accept a variable number of arguments,
 * which are expanded in the macro body using `__VA_ARGS__`.
 * 
 * Source Location Diagnostics:
 * Standard predefined macros automatically provided by the C compiler:
 * - __FILE__ : Expands to a string literal of the current source filename (e.g., "main.c").
 * - __LINE__ : Expands to an integer constant of the current line number (e.g., 42).
 * - __func__ : Expands to a string literal of the current function name (e.g., "process_data").
 * 
 * Calling these inside a macro captures the location where the MACRO WAS CALLED,
 * rather than where the function is defined.
 */

#define CLARITY_LOG_INFO(...) \
    clarity_log_output("INFO", false, __VA_ARGS__)

#define CLARITY_LOG_WARN(...) \
    clarity_log_output("WARN", true, __VA_ARGS__)

#define CLARITY_LOG_ERROR(...) \
    clarity_log_error(__VA_ARGS__)

/*
 * --- CONCEPT 5: The `do { ... } while (0)` Macro Idiom & Stringification ---
 * 
 * Why `do { ... } while (0)`?
 * If a multi-statement macro is written without a block, e.g.:
 *     #define MACRO() stmt1(); stmt2()
 * Using it in an if-statement without braces will break syntax:
 *     if (condition) MACRO(); else foo();
 * Expands to:
 *     if (condition) stmt1(); stmt2(); else foo(); // ERROR: 'else' without 'if'!
 * 
 * Wrapping the code in `do { ... } while (0)` creates a single compound statement 
 * that safely requires a trailing semicolon and behaves like a standard C function call.
 * 
 * Preprocessor Stringification Operator (`#`):
 * Placing `#` before a macro parameter converts the expression text into a string literal.
 * `#condition` converts an expression like `x > 5` into the string `"x > 5"`.
 */
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
void clarity_log_output(const char *prefix, int is_warning, const char *msg, ...);

/**
 * @brief Logs an error formatted message to standard output.
 */
void clarity_log_error(const char* msg, ...);

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

/*
 * --- CONCEPT 6: Magic Numbers & Defensive Programming ---
 * A "magic number" is a hardcoded hexadecimal constant placed in memory structures.
 * 0xDEADBEEF is a famous 32-bit hex value often used in debugging.
 * We check if this value is present to verify that a pointer passed to free()
 * was actually allocated by clarity_malloc and has not been corrupted or already freed.
 * if the memory was not allocated by Clarity or is corrupted, the value 0xDEADBEEF won't
 * be present; therefore, Clarity knows that memory doesn't belong to itself and shouldn't
 * be touched.
 */
#define CLARITY_MEM_MAGIC 0xDEADBEEF

/**
 * @struct ClarityMemoryHeader
 * 
 * --- CONCEPT 7: Hidden Allocation Metadata (Header Prefixing) ---
 * 
 * Memory Layout Diagram:
 * +------------------------+---------------------------------------+
 * | ClarityMemoryHeader    | User Allocation Memory                |
 * | (size, file, line...)  | (size bytes returned to caller)       |
 * +------------------------+---------------------------------------+
 * ^ Pointer from malloc()  ^ Pointer returned to user by clarity_malloc()
 * 
 * When clarity_malloc() is called, it allocates enough space for both this 
 * header AND the requested user payload. It hides this header just before
 * the pointer given to the caller.
 * 
 * --- CONCEPT 8: Doubly-Linked Lists ---
 * Every allocation header contains `next` and `prev` pointers.
 * This allows all active memory blocks to be linked together into a list,
 * enabling clarity_mem_report() to iterate through and detect leaks.
 */
typedef struct ClarityMemoryHeader {
    size_t size;                      /* Size of the user payload in bytes */
    int line;                         /* Line number where allocation occurred */
    const char *file;                 /* Source file where allocation occurred */
    struct ClarityMemoryHeader *next; /* Pointer to the next allocation node in the list */
    struct ClarityMemoryHeader *prev; /* Pointer to the previous allocation node in the list */
    unsigned int magic;               /* Validation magic number (CLARITY_MEM_MAGIC) */
} ClarityMemoryHeader;

/*
 * --- CONCEPT 9: Static Global Variables (Internal Module State) ---
 * In C, applying `static` to a global variable restricts its scope to the 
 * current translation unit (this file compilation unit).
 * 
 * - g_clarity_alloc_head: Pointer to the head (first element) of the linked list.
 * - g_clarity_alloc_amount_bytes: Accumulator tracking total allocated bytes.
 */
static ClarityMemoryHeader *g_clarity_alloc_head = NULL;
static size_t g_clarity_alloc_amount_bytes = 0;

/**
 * @brief Allocates memory with a metadata header attached for leak tracking.
 * 
 * --- CONCEPT 10: Pointer Arithmetic for Header Offset ---
 * In C, adding 1 to a pointer increments the memory address by the SIZE of the 
 * type it points to.
 * 
 * If `header` is of type `ClarityMemoryHeader*`:
 *   `header + 1`
 * moves the memory address forward by `sizeof(ClarityMemoryHeader)` bytes!
 * This conveniently points to the start of the user's usable memory region.
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
     * Time Complexity: O(1)
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
 * 
 * --- CONCEPT 11: Reversing Pointer Arithmetic ---
 * Since clarity_malloc() returned `header + 1`, the caller passes that user 
 * pointer back to clarity_free().
 * To find the hidden header, we subtract 1 from the typed pointer:
 *   `((ClarityMemoryHeader*)ptr) - 1`
 * This moves the address backwards by `sizeof(ClarityMemoryHeader)` bytes.
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
     * 1. If prev exists, point its 'next' to our 'next'.
     * 2. If next exists, point its 'prev' to our 'prev'.
     * 3. If this was the head of the list, update global head to point to 'next'.
     */
    if (header->prev) header->prev->next = header->next;
    if (header->next) header->next->prev = header->prev;
    if (header == g_clarity_alloc_head) g_clarity_alloc_head = header->next;

    /* Free the entire memory block (header + user payload) */
    free(header);
}

/**
 * @brief Iterates over the linked list of active allocations to report memory leaks.
 * 
 * --- CONCEPT 12: ANSI Terminal Escape Codes ---
 * Sequences like `\033[0;32m` (Green) and `\033[0;31m` (Red) are ANSI escape codes.
 * Terminals interpret these characters as commands to set output text colors.
 * `\033[0m` resets color back to default.
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

/**
 * @brief Logs error messages with red prefix formatting.
 * 
 * --- CONCEPT 13: Variadic Functions & stdarg.h ---
 * Functions accepting variable arguments (`...`) use C's `stdarg.h` library:
 * - `va_list args`: Declares a list iterator handle for arguments.
 * - `va_start(args, msg)`: Initializes `args` to point to the first argument after `msg`.
 * - `vprintf(msg, args)`: Variant of printf that takes a `va_list` instead of explicit parameters.
 * - `va_end(args)`: Cleans up the `va_list` resources.
 */
void clarity_log_error(const char* msg, ...) {

    printf("\033[0;31m[ERROR] \033[0m");

    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf("\n");
}

/**
 * @brief Logs output messages with customizable color prefixing.
 */
void clarity_log_output(const char *prefix, int is_warning, const char *msg, ...) {
    if (is_warning) {
        printf("\033[0;33m"); // Yellow text
    } else {
        printf("\033[0;32m"); // Green text
    }

    printf("[%s] \033[0m", prefix);

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