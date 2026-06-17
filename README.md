# Clarity

A lightweight, header-only debugging and memory-tracking suite for C.

Clarity provides simple, zero-dependency logging, assertion checking, and memory tracking features that can be easily compiled out in production builds.

## Features

- **Header-Only**: Easy to integrate, only requires including a single header file.
- **Zero Overhead in Release**: All debugging features can be completely compiled out by not defining `MIRA_CLARITY_DEBUG`.
- **Memory Tracking & Leak Detection**:
  - Track allocations (`CLARITY_MALLOC`) and deallocations (`CLARITY_FREE`).
  - Detection of double frees, untracked frees, or heap corruption.
  - Detailed memory leak reports (`CLARITY_MEM_REPORT()`) highlighting the exact file and line where leaked memory was allocated.

---

## Integration

To use Clarity, copy `clarity.h` to your project directory.

In **exactly one** source (`.c`) file, define `MIRA_CLARITY_IMPL` before including `clarity.h` to instantiate the implementation:

```c
#define MIRA_CLARITY_IMPL
#include "clarity.h"
```

In other source files, simply include the header:

```c
#include "clarity.h"
```

### Enabling Debug Mode

To enable Clarity's debugging features, define `MIRA_CLARITY_DEBUG` globally in your build system (e.g. `-DMIRA_CLARITY_DEBUG` in compiler flags) or define it before including the header:

```c
#define MIRA_CLARITY_DEBUG
#include "clarity.h"
```

If `MIRA_CLARITY_DEBUG` is not defined, all macros resolve to their standard runtime equivalents or compile to nothing, resulting in zero performance or binary size overhead.

---

## API Reference

### Logging
*   `CLARITY_LOG_INFO(msg, ...)`: Print an informational log prefixed with a green `[INFO]`.
*   `CLARITY_LOG_WARN(msg, ...)`: Print a warning log prefixed with a yellow `[WARN]`.

### Assertions
*   `CLARITY_ASSERT(condition, msg, ...)`: Evaluates `condition`. If false, prints assertion failure details to `stderr` and traps execution (triggers a debugger breakpoint).

### Memory Tracking
*   `CLARITY_MALLOC(size)`: A malloc() wrapper that enables tracking of each memory allocation.
*   `CLARITY_FREE(ptr)`: Frees tracked memory. Warns if the pointer is corrupted, untracked or has already been freed.
*   `CLARITY_MEM_REPORT()`: Prints a detailed list of all currently leaked allocations (if any).

---

## Usage Example

```c
#define MIRA_CLARITY_DEBUG
#define MIRA_CLARITY_IMPL
#include "clarity.h"

int main(void) {
    CLARITY_LOG_INFO("Initializing application...");

    // Allocate memory with tracking
    char *buffer = CLARITY_MALLOC(64 * sizeof(char));
    
    // Check condition with assertion
    CLARITY_ASSERT(buffer != NULL, "Failed to allocate memory buffer!");

    // Free the allocated memory
    CLARITY_FREE(buffer);

    // Attempting to free again triggers a corruption warning:
    CLARITY_FREE(buffer);

    // Report leaks (if any remain)
    CLARITY_MEM_REPORT();

    return 0;
}
```
