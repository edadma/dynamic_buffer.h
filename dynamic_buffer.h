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
 * printf("Buffer: %.*s\n", (int)db_size(combined), db_data(combined));
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
 * - **Zero-copy slicing**: Create views without data duplication
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
typedef struct db_buffer_internal* db_buffer;

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
 * @brief Get pointer to buffer data
 * @param buf Buffer instance (must not be NULL)
 * @return Pointer to buffer data (never NULL for valid buffer)
 */
DB_DEF const void* db_data(db_buffer buf);

/**
 * @brief Get mutable pointer to buffer data
 * @param buf Buffer instance (must not be NULL)
 * @return Mutable pointer to buffer data
 * @warning Only use if you're sure you have exclusive access to the buffer
 */
DB_DEF void* db_mutable_data(db_buffer buf);

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
 * @brief Zero-copy buffer slicing operations
 * @{
 */

/**
 * @brief Create a slice of the buffer (zero-copy view)
 * @param buf Source buffer (must not be NULL)
 * @param offset Starting offset in bytes
 * @param length Number of bytes in slice
 * @return New buffer slice or NULL if bounds are invalid
 * @note The slice shares the same underlying data as the original buffer
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
 * @param buf Buffer to resize (must not be NULL)
 * @param new_size New size in bytes
 * @return true on success, false on allocation failure
 * @note If buffer is shared (refcount > 1), this will fail
 */
DB_DEF bool db_resize(db_buffer buf, size_t new_size);

/**
 * @brief Ensure buffer has at least the specified capacity
 * @param buf Buffer to resize (must not be NULL)
 * @param min_capacity Minimum required capacity
 * @return true on success, false on allocation failure or if buffer is shared
 */
DB_DEF bool db_reserve(db_buffer buf, size_t min_capacity);

/**
 * @brief Append data to the end of buffer
 * @param buf Destination buffer (must not be NULL)
 * @param data Data to append (can be NULL if size is 0)
 * @param size Number of bytes to append
 * @return true on success, false on failure or if buffer is shared
 */
DB_DEF bool db_append(db_buffer buf, const void* data, size_t size);

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
 * @param buf Destination buffer (must not be NULL)
 * @param fd File descriptor to read from
 * @param max_bytes Maximum bytes to read (0 for no limit)
 * @return Number of bytes read, or -1 on error
 * @note Buffer will be resized as needed to accommodate data
 */
DB_DEF ssize_t db_read_fd(db_buffer buf, int fd, size_t max_bytes);

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
 * @brief Internal buffer structure
 * @private
 */
struct db_buffer_internal {
    db_refcount_t refcount;    ///< Reference count for memory management
    size_t size;               ///< Current size of valid data in bytes
    size_t capacity;           ///< Total allocated capacity in bytes
    size_t offset;             ///< Offset into parent buffer (for slices)
    struct db_buffer_internal* parent; ///< Parent buffer (for slices, NULL for root)
    void* data;                ///< Pointer to buffer data
};

/**
 * @brief Create internal buffer structure
 * @private
 */
static struct db_buffer_internal* db_internal_create(size_t capacity) {
    struct db_buffer_internal* internal = DB_MALLOC(sizeof(struct db_buffer_internal));
    if (!internal) return NULL;
    
    internal->refcount = DB_REFCOUNT_INIT(1);
    internal->size = 0;
    internal->capacity = capacity;
    internal->offset = 0;
    internal->parent = NULL;
    
    if (capacity > 0) {
        internal->data = DB_MALLOC(capacity);
        if (!internal->data) {
            DB_FREE(internal);
            return NULL;
        }
    } else {
        internal->data = NULL;
    }
    
    return internal;
}

/**
 * @brief Free internal buffer structure
 * @private
 */
static void db_internal_free(struct db_buffer_internal* internal) {
    if (!internal) return;
    
    if (internal->parent) {
        // This is a slice, release parent
        db_release(&internal->parent);
    } else {
        // This is a root buffer, free data
        if (internal->data) {
            DB_FREE(internal->data);
        }
    }
    
    DB_FREE(internal);
}

// Implementation of public functions

DB_DEF db_buffer db_new(size_t capacity) {
    return db_internal_create(capacity);
}

DB_DEF db_buffer db_new_with_data(const void* data, size_t size) {
    if (size > 0 && !data) return NULL;
    
    struct db_buffer_internal* buf = db_internal_create(size);
    if (!buf) return NULL;
    
    if (size > 0) {
        memcpy(buf->data, data, size);
        buf->size = size;
    }
    
    return buf;
}

DB_DEF db_buffer db_new_from_owned_data(void* data, size_t size, size_t capacity) {
    if (!data && (size > 0 || capacity > 0)) return NULL;
    if (capacity < size) return NULL;
    
    struct db_buffer_internal* buf = DB_MALLOC(sizeof(struct db_buffer_internal));
    if (!buf) return NULL;
    
    buf->refcount = DB_REFCOUNT_INIT(1);
    buf->size = size;
    buf->capacity = capacity;
    buf->offset = 0;
    buf->parent = NULL;
    buf->data = data;
    
    return buf;
}

DB_DEF db_buffer db_retain(db_buffer buf) {
    if (!buf) return NULL;
    DB_REFCOUNT_INCREMENT(&buf->refcount);
    return buf;
}

DB_DEF void db_release(db_buffer* buf_ptr) {
    if (!buf_ptr || !*buf_ptr) return;
    
    db_buffer buf = *buf_ptr;
    *buf_ptr = NULL;
    
    if (DB_REFCOUNT_DECREMENT(&buf->refcount) == 0) {
        // Reference count reached 0, free the buffer
        db_internal_free(buf);
    }
}

DB_DEF const void* db_data(db_buffer buf) {
    DB_ASSERT(buf);
    if (buf->parent) {
        // This is a slice, calculate data pointer from parent
        const char* parent_data = (const char*)buf->parent->data;
        return parent_data + buf->offset;
    }
    return buf->data ? buf->data : "";
}

DB_DEF void* db_mutable_data(db_buffer buf) {
    DB_ASSERT(buf);
    DB_ASSERT(DB_REFCOUNT_LOAD(&buf->refcount) == 1); // Ensure exclusive access
    if (buf->parent) {
        // This is a slice, calculate data pointer from parent
        char* parent_data = (char*)buf->parent->data;
        return parent_data + buf->offset;
    }
    return buf->data ? buf->data : (void*)"";
}

DB_DEF size_t db_size(db_buffer buf) {
    DB_ASSERT(buf);
    return buf->size;
}

DB_DEF size_t db_capacity(db_buffer buf) {
    DB_ASSERT(buf);
    return buf->capacity;
}

DB_DEF bool db_is_empty(db_buffer buf) {
    DB_ASSERT(buf);
    return buf->size == 0;
}

DB_DEF int db_refcount(db_buffer buf) {
    DB_ASSERT(buf);
    return DB_REFCOUNT_LOAD(&buf->refcount);
}

DB_DEF db_buffer db_slice(db_buffer buf, size_t offset, size_t length) {
    DB_ASSERT(buf);
    if (offset > buf->size || offset + length > buf->size) {
        return NULL; // Invalid bounds
    }
    
    struct db_buffer_internal* slice = DB_MALLOC(sizeof(struct db_buffer_internal));
    if (!slice) return NULL;
    
    slice->refcount = DB_REFCOUNT_INIT(1);
    slice->size = length;
    slice->capacity = length; // Slices have capacity equal to size
    slice->offset = (buf->parent ? buf->offset : 0) + offset;
    
    // Always retain the root buffer (the one that owns the data)
    if (buf->parent) {
        slice->parent = db_retain(buf->parent);
    } else {
        slice->parent = db_retain(buf);
    }
    
    slice->data = NULL; // Slices don't own data
    
    return slice;
}

DB_DEF db_buffer db_slice_from(db_buffer buf, size_t offset) {
    DB_ASSERT(buf);
    if (offset > buf->size) return NULL;
    return db_slice(buf, offset, buf->size - offset);
}

DB_DEF db_buffer db_slice_to(db_buffer buf, size_t length) {
    DB_ASSERT(buf);
    if (length > buf->size) return NULL;
    return db_slice(buf, 0, length);
}

DB_DEF bool db_resize(db_buffer buf, size_t new_size) {
    DB_ASSERT(buf);
    
    // Cannot resize slices or shared buffers
    if (buf->parent || DB_REFCOUNT_LOAD(&buf->refcount) > 1) {
        return false;
    }
    
    if (new_size <= buf->capacity) {
        buf->size = new_size;
        return true;
    }
    
    // Need to grow the buffer
    size_t new_capacity = new_size * 2; // Grow by 2x for efficiency
    void* new_data = DB_REALLOC(buf->data, new_capacity);
    if (!new_data) return false;
    
    buf->data = new_data;
    buf->capacity = new_capacity;
    buf->size = new_size;
    return true;
}

DB_DEF bool db_reserve(db_buffer buf, size_t min_capacity) {
    DB_ASSERT(buf);
    
    if (min_capacity <= buf->capacity) return true;
    
    // Cannot resize slices or shared buffers
    if (buf->parent || DB_REFCOUNT_LOAD(&buf->refcount) > 1) {
        return false;
    }
    
    void* new_data = DB_REALLOC(buf->data, min_capacity);
    if (!new_data) return false;
    
    buf->data = new_data;
    buf->capacity = min_capacity;
    return true;
}

DB_DEF bool db_append(db_buffer buf, const void* data, size_t size) {
    DB_ASSERT(buf);
    if (size == 0) return true;
    if (!data) return false;
    
    // Cannot modify slices or shared buffers
    if (buf->parent || DB_REFCOUNT_LOAD(&buf->refcount) > 1) {
        return false;
    }
    
    if (buf->size + size > buf->capacity) {
        if (!db_reserve(buf, buf->size + size)) {
            return false;
        }
    }
    
    memcpy((char*)buf->data + buf->size, data, size);
    buf->size += size;
    return true;
}

DB_DEF bool db_clear(db_buffer buf) {
    DB_ASSERT(buf);
    
    // Cannot modify slices or shared buffers
    if (buf->parent || DB_REFCOUNT_LOAD(&buf->refcount) > 1) {
        return false;
    }
    
    buf->size = 0;
    return true;
}

DB_DEF db_buffer db_concat(db_buffer buf1, db_buffer buf2) {
    size_t size1 = buf1 ? buf1->size : 0;
    size_t size2 = buf2 ? buf2->size : 0;
    size_t total_size = size1 + size2;
    
    if (total_size == 0) return db_new(0);
    
    db_buffer result = db_new(total_size);
    if (!result) return NULL;
    
    if (size1 > 0) {
        memcpy(result->data, db_data(buf1), size1);
        result->size = size1;
    }
    
    if (size2 > 0) {
        memcpy((char*)result->data + size1, db_data(buf2), size2);
        result->size = total_size;
    }
    
    return result;
}

DB_DEF db_buffer db_concat_many(db_buffer* buffers, size_t count) {
    if (!buffers || count == 0) return db_new(0);
    
    // Calculate total size
    size_t total_size = 0;
    for (size_t i = 0; i < count; i++) {
        if (buffers[i]) {
            total_size += buffers[i]->size;
        }
    }
    
    db_buffer result = db_new(total_size);
    if (!result) return NULL;
    
    size_t offset = 0;
    for (size_t i = 0; i < count; i++) {
        if (buffers[i] && buffers[i]->size > 0) {
            memcpy((char*)result->data + offset, db_data(buffers[i]), buffers[i]->size);
            offset += buffers[i]->size;
        }
    }
    
    result->size = total_size;
    return result;
}

DB_DEF bool db_equals(db_buffer buf1, db_buffer buf2) {
    if (buf1 == buf2) return true;
    if (!buf1 || !buf2) return false;
    if (buf1->size != buf2->size) return false;
    
    return memcmp(db_data(buf1), db_data(buf2), buf1->size) == 0;
}

DB_DEF int db_compare(db_buffer buf1, db_buffer buf2) {
    if (buf1 == buf2) return 0;
    if (!buf1) return !buf2 ? 0 : -1;
    if (!buf2) return 1;
    
    size_t min_size = buf1->size < buf2->size ? buf1->size : buf2->size;
    int result = memcmp(db_data(buf1), db_data(buf2), min_size);
    
    if (result != 0) return result;
    
    // Contents are equal up to min_size, compare sizes
    if (buf1->size < buf2->size) return -1;
    if (buf1->size > buf2->size) return 1;
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

DB_DEF ssize_t db_read_fd(db_buffer buf, int fd, size_t max_bytes) {
    DB_ASSERT(buf);
    
    // Cannot modify slices or shared buffers
    if (buf->parent || DB_REFCOUNT_LOAD(&buf->refcount) > 1) {
        return -1;
    }
    
    // Ensure we have space to read
    size_t read_size = max_bytes == 0 ? 4096 : max_bytes; // Default chunk size
    if (buf->size + read_size > buf->capacity) {
        if (!db_reserve(buf, buf->size + read_size)) {
            return -1;
        }
    }
    
    ssize_t bytes_read = db_read(fd, (char*)buf->data + buf->size, read_size);
    if (bytes_read > 0) {
        buf->size += bytes_read;
    }
    
    return bytes_read;
}

DB_DEF ssize_t db_write_fd(db_buffer buf, int fd) {
    DB_ASSERT(buf);
    if (buf->size == 0) return 0;
    
    return db_write(fd, db_data(buf), buf->size);
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
    
    size_t bytes_read = fread(buf->data, 1, file_size, file);
    fclose(file);
    
    buf->size = bytes_read;
    return buf;
}

DB_DEF bool db_write_file(db_buffer buf, const char* filename) {
    DB_ASSERT(buf);
    if (!filename) return false;
    
    FILE* file = fopen(filename, "wb");
    if (!file) return false;
    
    bool success = true;
    if (buf->size > 0) {
        size_t written = fwrite(db_data(buf), 1, buf->size, file);
        success = (written == buf->size);
    }
    
    fclose(file);
    return success;
}

// Utility functions
DB_DEF db_buffer db_to_hex(db_buffer buf, bool uppercase) {
    DB_ASSERT(buf);
    
    if (buf->size == 0) return db_new_with_data("", 0);
    
    size_t hex_size = buf->size * 2;
    db_buffer hex_buf = db_new(hex_size);
    if (!hex_buf) return NULL;
    
    const char* hex_chars = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    const uint8_t* data = (const uint8_t*)db_data(buf);
    char* hex_data = (char*)hex_buf->data;
    
    for (size_t i = 0; i < buf->size; i++) {
        hex_data[i * 2] = hex_chars[data[i] >> 4];
        hex_data[i * 2 + 1] = hex_chars[data[i] & 0x0F];
    }
    
    hex_buf->size = hex_size;
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
    
    uint8_t* data = (uint8_t*)buf->data;
    
    for (size_t i = 0; i < byte_length; i++) {
        int high = hex_char_to_value(hex_string[i * 2]);
        int low = hex_char_to_value(hex_string[i * 2 + 1]);
        
        if (high < 0 || low < 0) {
            db_release(&buf);
            return NULL;
        }
        
        data[i] = (uint8_t)((high << 4) | low);
    }
    
    buf->size = byte_length;
    return buf;
}

DB_DEF void db_debug_print(db_buffer buf, const char* label) {
    const char* name = label ? label : "buffer";
    
    if (!buf) {
        printf("%s: NULL\n", name);
        return;
    }
    
    printf("%s: size=%zu, capacity=%zu, refcount=%d",
           name, buf->size, buf->capacity, DB_REFCOUNT_LOAD(&buf->refcount));
    
    if (buf->parent) {
        printf(", slice[offset=%zu, parent=%p]", buf->offset, (void*)buf->parent);
    }
    
    printf("\n");
    
    // Print first few bytes as hex
    if (buf->size > 0) {
        printf("  data: ");
        const uint8_t* data = (const uint8_t*)db_data(buf);
        size_t print_size = buf->size < 16 ? buf->size : 16;
        
        for (size_t i = 0; i < print_size; i++) {
            printf("%02x ", data[i]);
        }
        
        if (buf->size > 16) {
            printf("... (%zu more bytes)", buf->size - 16);
        }
        printf("\n");
    }
}

#endif // DB_IMPLEMENTATION

#endif // DYNAMIC_BUFFER_H