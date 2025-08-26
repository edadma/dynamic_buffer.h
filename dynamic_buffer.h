/**
 * @file dynamic_buffer.h
 * @brief Reference-counted byte buffer library for efficient I/O operations
 * @version 0.1.0
 * @date August 2025
 *
 * Single header library for reference-counted byte buffers similar to libuv's buffer type.
 * Designed for efficient I/O operations, zero-copy slicing, and safe memory management
 * in both PC and microcontroller environments.
 *
 * @section config Configuration
 *
 * Customize the library by defining these macros before including:
 *
 * @code
 * #define DB_MALLOC malloc         // custom allocator
 * #define DB_REALLOC realloc       // custom reallocator
 * #define DB_FREE free             // custom deallocator
 * #define DB_ASSERT assert         // custom assert macro
 * #define DB_ATOMIC_REFCOUNT 1     // enable atomic reference counting (C11)
 *
 * #define DB_IMPLEMENTATION
 * #include "dynamic_buffer.h"
 * @endcode
 *
 * @section usage Basic Usage
 *
 * @code
 * // Create a buffer with some data
 * db_buffer buf = db_new_with_data("Hello", 5);
 * 
 * // Create a slice (zero-copy view)
 * db_buffer slice = db_slice(buf, 1, 4);  // "ello"
 * 
 * // Concatenate buffers
 * db_buffer world = db_new_with_data(" World", 6);
 * db_buffer combined = db_concat(buf, world);
 * 
 * // Access the data
 * printf("Buffer: %.*s\n", (int)db_size(combined), combined);
 * 
 * // Clean up (reference counting handles memory)
 * db_release(&buf);
 * db_release(&slice);
 * db_release(&world);
 * db_release(&combined);
 * @endcode
 *
 * @section features Key Features
 *
 * - **Reference counting**: Automatic memory management with atomic support
 * - **Buffer slicing**: Create independent copies of buffer segments  
 * - **Efficient concatenation**: Optimized buffer joining operations
 * - **I/O integration**: Compatible with read/write operations
 * - **Memory safety**: Bounds checking and safe access patterns
 * - **Microcontroller friendly**: Minimal memory overhead
 *
 * @section license License
 * Dual licensed under your choice of:
 * - MIT License
 * - The Unlicense (public domain)
 */

#ifndef DYNAMIC_BUFFER_H
#define DYNAMIC_BUFFER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

// For ssize_t
#ifdef _WIN32
#include <io.h>
typedef long ssize_t;
#else
#include <sys/types.h>
#include <unistd.h>
#endif

// Configuration macros (user can override before including)
#ifndef DB_MALLOC
#include <stdlib.h>
#define DB_MALLOC malloc
#endif

#ifndef DB_REALLOC
#include <stdlib.h>
#define DB_REALLOC realloc
#endif

#ifndef DB_FREE
#include <stdlib.h>
#define DB_FREE free
#endif

#ifndef DB_ASSERT
#include <assert.h>
#define DB_ASSERT assert
#endif

// Atomic reference counting support (requires C11)
#ifndef DB_ATOMIC_REFCOUNT
#define DB_ATOMIC_REFCOUNT 0
#endif

#if DB_ATOMIC_REFCOUNT
#include <stdatomic.h>
typedef _Atomic int db_refcount_t;
#define DB_REFCOUNT_INIT(n) ATOMIC_VAR_INIT(n)
#define DB_REFCOUNT_LOAD(ptr) atomic_load(ptr)
#define DB_REFCOUNT_INCREMENT(ptr) (atomic_fetch_add(ptr, 1) + 1)
#define DB_REFCOUNT_DECREMENT(ptr) (atomic_fetch_sub(ptr, 1) - 1)
#else
typedef int db_refcount_t;
#define DB_REFCOUNT_INIT(n) (n)
#define DB_REFCOUNT_LOAD(ptr) (*(ptr))
#define DB_REFCOUNT_INCREMENT(ptr) (++(*(ptr)))
#define DB_REFCOUNT_DECREMENT(ptr) (--(*(ptr)))
#endif

// Function visibility control
#ifdef DB_IMPLEMENTATION
#define DB_DEF static
#else
#define DB_DEF extern
#endif

// Forward declarations

/**
 * @brief Buffer handle - points directly to buffer data
 *
 * This is a char* that points directly to binary buffer data. Metadata
 * (refcount, size, capacity) is stored at negative offsets before the buffer 
 * data. This allows db_buffer to be used directly with all C memory functions.
 *
 * Memory layout: [refcount|size|capacity|buffer_data...]
 *                                        ^
 *                         db_buffer points here
 *
 * @note Use directly with memcpy, write, etc. - no conversion needed!
 * @note NULL represents an invalid/empty buffer handle
 */
typedef char* db_buffer;

/**
 * @defgroup lifecycle Buffer Lifecycle
 * @brief Functions for creating and destroying buffers
 * @{
 */

/**
 * @brief Create a new empty buffer with specified capacity
 * @param capacity Initial capacity in bytes (0 for minimal allocation)
 * @return New buffer instance or NULL on allocation failure
 */
DB_DEF db_buffer db_new(size_t capacity);

/**
 * @brief Create a new buffer initialized with data (copies the data)
 * @param data Pointer to source data (can be NULL if size is 0)
 * @param size Number of bytes to copy
 * @return New buffer instance or NULL on allocation failure
 */
DB_DEF db_buffer db_new_with_data(const void* data, size_t size);

/**
 * @brief Create a new buffer that takes ownership of existing data
 * @param data Pointer to data (must be allocated with DB_MALLOC)
 * @param size Size of the data in bytes
 * @param capacity Total allocated capacity (must be >= size)
 * @return New buffer instance or NULL on failure
 * @warning The data pointer becomes owned by the buffer and must not be freed manually
 */
DB_DEF db_buffer db_new_from_owned_data(void* data, size_t size, size_t capacity);

/**
 * @brief Increase reference count (share ownership)
 * @param buf Buffer to retain (can be NULL)
 * @return The same buffer for convenience
 */
DB_DEF db_buffer db_retain(db_buffer buf);

/**
 * @brief Decrease reference count and potentially free buffer
 * @param buf_ptr Pointer to buffer variable (will be set to NULL)
 */
DB_DEF void db_release(db_buffer* buf_ptr);

/** @} */

/**
 * @defgroup access Buffer Access
 * @brief Functions for accessing buffer data and properties
 * @{
 */


/**
 * @brief Get current size of buffer in bytes
 * @param buf Buffer instance (must not be NULL)
 * @return Current size in bytes
 */
DB_DEF size_t db_size(db_buffer buf);

/**
 * @brief Get current capacity of buffer in bytes
 * @param buf Buffer instance (must not be NULL)
 * @return Current capacity in bytes
 */
DB_DEF size_t db_capacity(db_buffer buf);

/**
 * @brief Check if buffer is empty
 * @param buf Buffer instance (must not be NULL)
 * @return true if buffer has no data, false otherwise
 */
DB_DEF bool db_is_empty(db_buffer buf);

/**
 * @brief Get current reference count
 * @param buf Buffer instance (must not be NULL)
 * @return Current reference count
 */
DB_DEF int db_refcount(db_buffer buf);

/** @} */

/**
 * @defgroup slicing Buffer Slicing
 * @brief Buffer slicing operations (creates independent copies)
 * @{
 */

/**
 * @brief Create a slice of the buffer (creates independent copy)
 * @param buf Source buffer (must not be NULL)
 * @param offset Starting offset in bytes
 * @param length Number of bytes in slice
 * @return New buffer slice or NULL if bounds are invalid
 * @note The slice is an independent copy of the specified data range
 */
DB_DEF db_buffer db_slice(db_buffer buf, size_t offset, size_t length);

/**
 * @brief Create a slice from offset to end of buffer
 * @param buf Source buffer (must not be NULL)  
 * @param offset Starting offset in bytes
 * @return New buffer slice or NULL if offset is invalid
 */
DB_DEF db_buffer db_slice_from(db_buffer buf, size_t offset);

/**
 * @brief Create a slice from start to specified length
 * @param buf Source buffer (must not be NULL)
 * @param length Number of bytes from start
 * @return New buffer slice or NULL if length is invalid
 */
DB_DEF db_buffer db_slice_to(db_buffer buf, size_t length);

/** @} */

/**
 * @defgroup modification Buffer Modification  
 * @brief Functions for modifying buffer contents and size
 * @{
 */

/**
 * @brief Resize buffer to new size (may reallocate)
 * @param buf_ptr Pointer to buffer pointer (may be updated if reallocated)
 * @param new_size New size in bytes
 * @return true on success, false on allocation failure
 * @note If buffer is shared (refcount > 1), this will fail
 * @note The buffer pointer may change due to reallocation
 */
DB_DEF bool db_resize(db_buffer* buf_ptr, size_t new_size);

/**
 * @brief Ensure buffer has at least the specified capacity
 * @param buf_ptr Pointer to buffer (must not be NULL)
 * @param min_capacity Minimum required capacity
 * @return true on success, false on allocation failure or if buffer is shared
 */
DB_DEF bool db_reserve(db_buffer* buf_ptr, size_t min_capacity);

/**
 * @brief Append data to the end of buffer
 * @param buf_ptr Pointer to buffer (must not be NULL)
 * @param data Data to append (can be NULL if size is 0)
 * @param size Number of bytes to append
 * @return true on success, false on failure or if buffer is shared
 */
DB_DEF bool db_append(db_buffer* buf_ptr, const void* data, size_t size);

/**
 * @brief Clear buffer contents (set size to 0)
 * @param buf Buffer to clear (must not be NULL)
 * @return true on success, false if buffer is shared
 */
DB_DEF bool db_clear(db_buffer buf);

/** @} */

/**
 * @defgroup concatenation Buffer Concatenation
 * @brief Functions for combining buffers
 * @{
 */

/**
 * @brief Concatenate two buffers into a new buffer
 * @param buf1 First buffer (can be NULL)
 * @param buf2 Second buffer (can be NULL)
 * @return New buffer containing concatenated data, or NULL on failure
 */
DB_DEF db_buffer db_concat(db_buffer buf1, db_buffer buf2);

/**
 * @brief Concatenate multiple buffers into a new buffer
 * @param buffers Array of buffer pointers
 * @param count Number of buffers in array
 * @return New buffer containing concatenated data, or NULL on failure
 */
DB_DEF db_buffer db_concat_many(db_buffer* buffers, size_t count);

/** @} */

/**
 * @defgroup comparison Buffer Comparison
 * @brief Functions for comparing buffer contents
 * @{
 */

/**
 * @brief Compare two buffers for equality
 * @param buf1 First buffer (can be NULL)
 * @param buf2 Second buffer (can be NULL)
 * @return true if buffers have identical contents, false otherwise
 */
DB_DEF bool db_equals(db_buffer buf1, db_buffer buf2);

/**
 * @brief Compare buffer contents lexicographically
 * @param buf1 First buffer (can be NULL)
 * @param buf2 Second buffer (can be NULL)
 * @return -1 if buf1 < buf2, 0 if equal, 1 if buf1 > buf2
 */
DB_DEF int db_compare(db_buffer buf1, db_buffer buf2);

/** @} */

/**
 * @defgroup io I/O Operations
 * @brief Functions for reading and writing buffers
 * @{
 */

/**
 * @brief Read data from file descriptor into buffer
 * @param buf_ptr Pointer to buffer (must not be NULL)
 * @param fd File descriptor to read from
 * @param max_bytes Maximum bytes to read (0 for no limit)
 * @return Number of bytes read, or -1 on error
 * @note Buffer will be resized as needed to accommodate data
 */
DB_DEF ssize_t db_read_fd(db_buffer* buf_ptr, int fd, size_t max_bytes);

/**
 * @brief Write buffer contents to file descriptor
 * @param buf Source buffer (must not be NULL)
 * @param fd File descriptor to write to
 * @return Number of bytes written, or -1 on error
 */
DB_DEF ssize_t db_write_fd(db_buffer buf, int fd);

/**
 * @brief Read entire file into a new buffer
 * @param filename Path to file to read
 * @return New buffer containing file contents, or NULL on failure
 */
DB_DEF db_buffer db_read_file(const char* filename);

/**
 * @brief Write buffer contents to file
 * @param buf Source buffer (must not be NULL)
 * @param filename Path to file to write
 * @return true on success, false on failure
 */
DB_DEF bool db_write_file(db_buffer buf, const char* filename);

/** @} */

/**
 * @defgroup utility Utility Functions
 * @brief Helper and debugging functions
 * @{
 */

/**
 * @brief Create a hexadecimal representation of buffer contents
 * @param buf Buffer to convert (must not be NULL)
 * @param uppercase Use uppercase hex digits if true
 * @return New buffer containing hex string, or NULL on failure
 */
DB_DEF db_buffer db_to_hex(db_buffer buf, bool uppercase);

/**
 * @brief Create buffer from hexadecimal string
 * @param hex_string Hexadecimal string (must be valid hex)
 * @param length Length of hex string
 * @return New buffer containing decoded bytes, or NULL on failure
 */
DB_DEF db_buffer db_from_hex(const char* hex_string, size_t length);

/**
 * @brief Print buffer information for debugging
 * @param buf Buffer to print (can be NULL)
 * @param label Optional label for the output
 */
DB_DEF void db_debug_print(db_buffer buf, const char* label);

/** @} */

// Implementation section - only compiled when DB_IMPLEMENTATION is defined
#ifdef DB_IMPLEMENTATION

/**
 * @brief Internal buffer metadata structure
 * @private
 * 
 * This structure is stored at negative offsets before the buffer data.
 * The db_buffer points directly to the data portion.
 */
struct db_buffer_header {
    db_refcount_t refcount;    ///< Reference count for memory management
    size_t size;               ///< Current size of valid data in bytes
    size_t capacity;           ///< Total allocated capacity in bytes
};

// Helper macros to access metadata at negative offsets
#define DB_HEADER(buf) ((struct db_buffer_header*)((char*)(buf) - sizeof(struct db_buffer_header)))
#define DB_REFCOUNT(buf) (&DB_HEADER(buf)->refcount)
#define DB_SIZE_PTR(buf) (&DB_HEADER(buf)->size)
#define DB_CAPACITY_PTR(buf) (&DB_HEADER(buf)->capacity)

/**
 * @brief Create internal buffer with metadata at negative offset
 * @private
 */
static db_buffer db_internal_create(size_t capacity) {
    // Allocate space for header + data
    size_t total_size = sizeof(struct db_buffer_header) + capacity;
    void* raw_memory = DB_MALLOC(total_size);
    if (!raw_memory) return NULL;
    
    // Set up header at the beginning
    struct db_buffer_header* header = (struct db_buffer_header*)raw_memory;
    header->refcount = DB_REFCOUNT_INIT(1);
    header->size = 0;
    header->capacity = capacity;
    
    // Return pointer to data portion (after header)
    return (db_buffer)((char*)raw_memory + sizeof(struct db_buffer_header));
}

/**
 * @brief Free internal buffer structure
 * @private
 */
static void db_internal_free(db_buffer buf) {
    if (!buf) return;
    
    struct db_buffer_header* header = DB_HEADER(buf);
    
    // Free header + data together (they were allocated as one block)
    DB_FREE(header);
}

// Implementation of public functions

DB_DEF db_buffer db_new(size_t capacity) {
    return db_internal_create(capacity);
}

DB_DEF db_buffer db_new_with_data(const void* data, size_t size) {
    if (size > 0 && !data) return NULL;
    
    db_buffer buf = db_internal_create(size);
    if (!buf) return NULL;
    
    if (size > 0) {
        memcpy(buf, data, size);
        *DB_SIZE_PTR(buf) = size;
    }
    
    return buf;
}

DB_DEF db_buffer db_new_from_owned_data(void* data, size_t size, size_t capacity) {
    if (!data && (size > 0 || capacity > 0)) return NULL;
    if (capacity < size) return NULL;
    
    // We can't use the negative offset trick here since we don't control the data allocation
    // Instead, we'll copy the data to maintain design consistency
    db_buffer buf = db_internal_create(capacity);
    if (!buf) return NULL;
    
    if (size > 0) {
        memcpy(buf, data, size);
        *DB_SIZE_PTR(buf) = size;
    }
    
    // Free the original data since we copied it
    DB_FREE(data);
    
    return buf;
}

DB_DEF db_buffer db_retain(db_buffer buf) {
    if (!buf) return NULL;
    DB_REFCOUNT_INCREMENT(DB_REFCOUNT(buf));
    return buf;
}

DB_DEF void db_release(db_buffer* buf_ptr) {
    if (!buf_ptr || !*buf_ptr) return;
    
    db_buffer buf = *buf_ptr;
    *buf_ptr = NULL;
    
    if (DB_REFCOUNT_DECREMENT(DB_REFCOUNT(buf)) == 0) {
        // Reference count reached 0, free the buffer
        db_internal_free(buf);
    }
}


DB_DEF size_t db_size(db_buffer buf) {
    DB_ASSERT(buf);
    return DB_HEADER(buf)->size;
}

DB_DEF size_t db_capacity(db_buffer buf) {
    DB_ASSERT(buf);
    return DB_HEADER(buf)->capacity;
}

DB_DEF bool db_is_empty(db_buffer buf) {
    DB_ASSERT(buf);
    return DB_HEADER(buf)->size == 0;
}

DB_DEF int db_refcount(db_buffer buf) {
    DB_ASSERT(buf);
    return DB_REFCOUNT_LOAD(DB_REFCOUNT(buf));
}

DB_DEF db_buffer db_slice(db_buffer buf, size_t offset, size_t length) {
    DB_ASSERT(buf);
    if (offset > DB_HEADER(buf)->size || offset + length > DB_HEADER(buf)->size) {
        return NULL; // Invalid bounds
    }
    
    // Copy the slice data to maintain the negative-offset pattern for all buffers
    db_buffer slice = db_internal_create(length);
    if (!slice) return NULL;
    
    // Copy the slice data directly from the source buffer
    if (length > 0) {
        memcpy(slice, buf + offset, length);
        *DB_SIZE_PTR(slice) = length;
    }
    
    return slice;
}

DB_DEF db_buffer db_slice_from(db_buffer buf, size_t offset) {
    DB_ASSERT(buf);
    if (offset > DB_HEADER(buf)->size) return NULL;
    return db_slice(buf, offset, DB_HEADER(buf)->size - offset);
}

DB_DEF db_buffer db_slice_to(db_buffer buf, size_t length) {
    DB_ASSERT(buf);
    if (length > DB_HEADER(buf)->size) return NULL;
    return db_slice(buf, 0, length);
}

DB_DEF bool db_resize(db_buffer* buf_ptr, size_t new_size) {
    DB_ASSERT(buf_ptr && *buf_ptr);
    db_buffer buf = *buf_ptr;
    
    // Cannot resize shared buffers
    if (DB_REFCOUNT_LOAD(DB_REFCOUNT(buf)) > 1) {
        return false;
    }
    
    if (new_size <= DB_HEADER(buf)->capacity) {
        *DB_SIZE_PTR(buf) = new_size;
        return true;
    }
    
    // Need to grow the buffer - reallocate header + data together
    size_t new_capacity = new_size * 2; // Grow by 2x for efficiency
    size_t total_size = sizeof(struct db_buffer_header) + new_capacity;
    
    struct db_buffer_header* old_header = DB_HEADER(buf);
    void* new_memory = DB_REALLOC(old_header, total_size);
    if (!new_memory) return false;
    
    // Update header in new location
    struct db_buffer_header* new_header = (struct db_buffer_header*)new_memory;
    new_header->capacity = new_capacity;
    new_header->size = new_size;
    
    // Update caller's buffer pointer (header + sizeof(header))
    *buf_ptr = (db_buffer)((char*)new_memory + sizeof(struct db_buffer_header));
    
    return true;
}

DB_DEF bool db_reserve(db_buffer* buf_ptr, size_t min_capacity) {
    DB_ASSERT(buf_ptr && *buf_ptr);
    db_buffer buf = *buf_ptr;
    
    if (min_capacity <= DB_HEADER(buf)->capacity) return true;
    
    // Cannot resize shared buffers
    if (DB_REFCOUNT_LOAD(DB_REFCOUNT(buf)) > 1) {
        return false;
    }
    
    // Reallocate header + data together
    size_t total_size = sizeof(struct db_buffer_header) + min_capacity;
    struct db_buffer_header* old_header = DB_HEADER(buf);
    void* new_memory = DB_REALLOC(old_header, total_size);
    if (!new_memory) return false;
    
    // Update header in new location
    struct db_buffer_header* new_header = (struct db_buffer_header*)new_memory;
    new_header->capacity = min_capacity;
    
    // Update caller's buffer pointer
    *buf_ptr = (db_buffer)((char*)new_memory + sizeof(struct db_buffer_header));
    
    return true;
}

DB_DEF bool db_append(db_buffer* buf_ptr, const void* data, size_t size) {
    DB_ASSERT(buf_ptr && *buf_ptr);
    if (size == 0) return true;
    if (!data) return false;
    
    db_buffer buf = *buf_ptr;
    
    // Cannot modify shared buffers
    if (DB_REFCOUNT_LOAD(DB_REFCOUNT(buf)) > 1) {
        return false;
    }
    
    size_t old_size = DB_HEADER(buf)->size;
    if (old_size + size > DB_HEADER(buf)->capacity) {
        if (!db_reserve(buf_ptr, old_size + size)) {
            return false;
        }
        buf = *buf_ptr; // Update buf after potential reallocation
    }
    
    memcpy(buf + old_size, data, size);
    *DB_SIZE_PTR(buf) = old_size + size;
    return true;
}

DB_DEF bool db_clear(db_buffer buf) {
    DB_ASSERT(buf);
    
    // Cannot modify shared buffers
    if (DB_REFCOUNT_LOAD(DB_REFCOUNT(buf)) > 1) {
        return false;
    }
    
    *DB_SIZE_PTR(buf) = 0;
    return true;
}

DB_DEF db_buffer db_concat(db_buffer buf1, db_buffer buf2) {
    size_t size1 = buf1 ? DB_HEADER(buf1)->size : 0;
    size_t size2 = buf2 ? DB_HEADER(buf2)->size : 0;
    size_t total_size = size1 + size2;
    
    if (total_size == 0) return db_new(0);
    
    db_buffer result = db_new(total_size);
    if (!result) return NULL;
    
    if (size1 > 0) {
        memcpy(result, buf1, size1);
        *DB_SIZE_PTR(result) = size1;
    }
    
    if (size2 > 0) {
        memcpy(result + size1, buf2, size2);
        *DB_SIZE_PTR(result) = total_size;
    }
    
    return result;
}

DB_DEF db_buffer db_concat_many(db_buffer* buffers, size_t count) {
    if (!buffers || count == 0) return db_new(0);
    
    // Calculate total size
    size_t total_size = 0;
    for (size_t i = 0; i < count; i++) {
        if (buffers[i]) {
            total_size += DB_HEADER(buffers[i])->size;
        }
    }
    
    db_buffer result = db_new(total_size);
    if (!result) return NULL;
    
    size_t offset = 0;
    for (size_t i = 0; i < count; i++) {
        if (buffers[i]) {
            size_t size = DB_HEADER(buffers[i])->size;
            if (size > 0) {
                memcpy(result + offset, buffers[i], size);
                offset += size;
            }
        }
    }
    
    *DB_SIZE_PTR(result) = total_size;
    return result;
}

DB_DEF bool db_equals(db_buffer buf1, db_buffer buf2) {
    if (buf1 == buf2) return true;
    if (!buf1 || !buf2) return false;
    
    size_t size1 = DB_HEADER(buf1)->size;
    size_t size2 = DB_HEADER(buf2)->size;
    if (size1 != size2) return false;
    
    return memcmp(buf1, buf2, size1) == 0;
}

DB_DEF int db_compare(db_buffer buf1, db_buffer buf2) {
    if (buf1 == buf2) return 0;
    if (!buf1) return !buf2 ? 0 : -1;
    if (!buf2) return 1;
    
    size_t size1 = DB_HEADER(buf1)->size;
    size_t size2 = DB_HEADER(buf2)->size;
    size_t min_size = size1 < size2 ? size1 : size2;
    
    int result = memcmp(buf1, buf2, min_size);
    
    if (result != 0) return result;
    
    // Contents are equal up to min_size, compare sizes
    if (size1 < size2) return -1;
    if (size1 > size2) return 1;
    return 0;
}

// I/O operations - basic implementation
#ifdef _WIN32
#include <io.h>
#define db_read read
#define db_write write
#else
#include <unistd.h>
#define db_read read  
#define db_write write
#endif

DB_DEF ssize_t db_read_fd(db_buffer* buf_ptr, int fd, size_t max_bytes) {
    DB_ASSERT(buf_ptr && *buf_ptr);
    db_buffer buf = *buf_ptr;
    
    // Cannot modify shared buffers
    if (DB_REFCOUNT_LOAD(DB_REFCOUNT(buf)) > 1) {
        return -1;
    }
    
    // Ensure we have space to read
    size_t read_size = max_bytes == 0 ? 4096 : max_bytes; // Default chunk size
    size_t current_size = DB_HEADER(buf)->size;
    if (current_size + read_size > DB_HEADER(buf)->capacity) {
        if (!db_reserve(buf_ptr, current_size + read_size)) {
            return -1;
        }
        buf = *buf_ptr; // Update buf after potential reallocation
    }
    
    ssize_t bytes_read = db_read(fd, buf + current_size, read_size);
    if (bytes_read > 0) {
        *DB_SIZE_PTR(buf) = current_size + bytes_read;
    }
    
    return bytes_read;
}

DB_DEF ssize_t db_write_fd(db_buffer buf, int fd) {
    DB_ASSERT(buf);
    size_t size = DB_HEADER(buf)->size;
    if (size == 0) return 0;
    
    return db_write(fd, buf, size);
}

DB_DEF db_buffer db_read_file(const char* filename) {
    if (!filename) return NULL;
    
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size < 0) {
        fclose(file);
        return NULL;
    }
    
    db_buffer buf = db_new(file_size);
    if (!buf) {
        fclose(file);
        return NULL;
    }
    
    size_t bytes_read = fread(buf, 1, file_size, file);
    fclose(file);
    
    *DB_SIZE_PTR(buf) = bytes_read;
    return buf;
}

DB_DEF bool db_write_file(db_buffer buf, const char* filename) {
    DB_ASSERT(buf);
    if (!filename) return false;
    
    FILE* file = fopen(filename, "wb");
    if (!file) return false;
    
    bool success = true;
    size_t size = DB_HEADER(buf)->size;
    if (size > 0) {
        size_t written = fwrite(buf, 1, size, file);
        success = (written == size);
    }
    
    fclose(file);
    return success;
}

// Utility functions
DB_DEF db_buffer db_to_hex(db_buffer buf, bool uppercase) {
    DB_ASSERT(buf);
    
    size_t size = DB_HEADER(buf)->size;
    if (size == 0) return db_new_with_data("", 0);
    
    size_t hex_size = size * 2;
    db_buffer hex_buf = db_new(hex_size);
    if (!hex_buf) return NULL;
    
    const char* hex_chars = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    const uint8_t* data = (const uint8_t*)buf;
    char* hex_data = hex_buf;
    
    for (size_t i = 0; i < size; i++) {
        hex_data[i * 2] = hex_chars[data[i] >> 4];
        hex_data[i * 2 + 1] = hex_chars[data[i] & 0x0F];
    }
    
    *DB_SIZE_PTR(hex_buf) = hex_size;
    return hex_buf;
}

static int hex_char_to_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

DB_DEF db_buffer db_from_hex(const char* hex_string, size_t length) {
    if (!hex_string || length % 2 != 0) return NULL;
    
    size_t byte_length = length / 2;
    db_buffer buf = db_new(byte_length);
    if (!buf) return NULL;
    
    uint8_t* data = (uint8_t*)buf;
    
    for (size_t i = 0; i < byte_length; i++) {
        int high = hex_char_to_value(hex_string[i * 2]);
        int low = hex_char_to_value(hex_string[i * 2 + 1]);
        
        if (high < 0 || low < 0) {
            db_release(&buf);
            return NULL;
        }
        
        data[i] = (uint8_t)((high << 4) | low);
    }
    
    *DB_SIZE_PTR(buf) = byte_length;
    return buf;
}

DB_DEF void db_debug_print(db_buffer buf, const char* label) {
    const char* name = label ? label : "buffer";
    
    if (!buf) {
        printf("%s: NULL\n", name);
        return;
    }
    
    size_t size = DB_HEADER(buf)->size;
    size_t capacity = DB_HEADER(buf)->capacity;
    int refcount = DB_REFCOUNT_LOAD(DB_REFCOUNT(buf));
    
    printf("%s: size=%zu, capacity=%zu, refcount=%d\n",
           name, size, capacity, refcount);
    
    // Print first few bytes as hex
    if (size > 0) {
        printf("  data: ");
        const uint8_t* data = (const uint8_t*)buf;
        size_t print_size = size < 16 ? size : 16;
        
        for (size_t i = 0; i < print_size; i++) {
            printf("%02x ", data[i]);
        }
        
        if (size > 16) {
            printf("... (%zu more bytes)", size - 16);
        }
        printf("\n");
    }
}

#endif // DB_IMPLEMENTATION

#endif // DYNAMIC_BUFFER_H