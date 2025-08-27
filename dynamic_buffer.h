/**
 * @file dynamic_buffer.h
 * @brief Reference-counted byte buffer library for efficient I/O operations
 * @version 0.2.1
 * @date August 2025
 *
 * Single header library for reference-counted byte buffers similar to libuv's buffer type.
 * Designed for efficient I/O operations, immutable slicing, and safe memory management
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
 * // Create a slice (independent copy)
 * db_buffer slice = db_slice(buf, 1, 4);  // "ello"
 * 
 * // Concatenate buffers
 * db_buffer world = db_new_with_data(" World", 6);
 * db_buffer combined = db_concat(buf, world);
 * 
 * // Use a builder to construct data
 * db_builder builder = db_builder_new(64);
 * db_builder_append_cstring(builder, "Built: ");
 * db_builder_append_uint32_le(builder, 0x12345678);
 * db_buffer built = db_builder_finish(&builder);
 * 
 * // Use a reader to parse data
 * db_reader reader = db_reader_new(built);
 * char prefix[8];
 * db_read_bytes(reader, prefix, 7);
 * prefix[7] = '\0';  // "Built: "
 * uint32_t value = db_read_uint32_le(reader);  // 0x12345678
 * 
 * // Clean up (reference counting handles memory automatically)
 * db_release(&buf);
 * db_release(&slice);
 * db_release(&world);
 * db_release(&combined);
 * db_release(&built);
 * db_reader_release(&reader);
 * @endcode
 *
 * @section features Key Features
 *
 * - **Reference counting**: Automatic memory management with atomic support for buffers, builders, and readers
 * - **Buffer slicing**: Create independent copies of buffer segments  
 * - **Efficient concatenation**: Optimized buffer joining operations
 * - **Builder API**: Type-safe construction of binary data with reference counting
 * - **Reader API**: Cursor-based parsing of binary data with reference counting  
 * - **I/O integration**: Compatible with read/write operations
 * - **Memory safety**: Bounds checking and safe access patterns
 * - **Concurrent access**: Lock-free atomic reference counting for safe sharing
 * - **Microcontroller friendly**: Minimal memory overhead
 *
 * @section license License
 * Dual licensed under your choice of:
 * - MIT License
 * - The Unlicense (public domain)
 */

#ifndef DYNAMIC_BUFFER_H
#define DYNAMIC_BUFFER_H

// Version information
#define DB_VERSION_MAJOR 0
#define DB_VERSION_MINOR 2
#define DB_VERSION_PATCH 0
#define DB_VERSION_STRING "0.2.0"

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
 * @return New buffer instance (asserts on allocation failure)
 */
DB_DEF db_buffer db_new(size_t capacity);

/**
 * @brief Create a new buffer initialized with data (copies the data)
 * @param data Pointer to source data (can be NULL if size is 0)
 * @param size Number of bytes to copy
 * @return New buffer instance (asserts on allocation failure)
 * 
 * @par Example:
 * @code
 * db_buffer buf = db_new_with_data("Hello", 5);
 * if (buf) {
 *     printf("Buffer size: %zu\n", db_size(buf));
 *     db_release(&buf);
 * }
 * @endcode
 */
DB_DEF db_buffer db_new_with_data(const void* data, size_t size);

/**
 * @brief Create a new buffer by copying existing data
 * @param data Pointer to data to copy
 * @param size Size of the data in bytes
 * @param capacity Total allocated capacity (must be >= size)
 * @return New buffer instance or NULL on invalid parameters (asserts on allocation failure)
 * @note This function copies the data into the new buffer. You are responsible for freeing the original data if needed.
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
 * @brief Create new buffer with data appended
 * @param buf Source buffer (must not be NULL)
 * @param data Data to append (can be NULL if size is 0)
 * @param size Number of bytes to append
 * @return New buffer with appended data (asserts on allocation failure)
 * @note Original buffer remains unchanged (immutable operation)
 * 
 * @par Example:
 * @code
 * db_buffer buf1 = db_new_with_data("Hello", 5);
 * db_buffer buf2 = db_append(buf1, " World", 6);
 * 
 * // buf1 still contains "Hello"
 * // buf2 contains "Hello World"
 * 
 * db_release(&buf1);
 * db_release(&buf2);
 * @endcode
 */
DB_DEF db_buffer db_append(db_buffer buf, const void* data, size_t size);


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
 * @return New buffer containing concatenated data (asserts on allocation failure)
 */
DB_DEF db_buffer db_concat(db_buffer buf1, db_buffer buf2);

/**
 * @brief Concatenate multiple buffers into a new buffer
 * @param buffers Array of buffer pointers
 * @param count Number of buffers in array
 * @return New buffer containing concatenated data (asserts on allocation failure)
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
 * @return New buffer containing file contents, or NULL if file cannot be read
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
 * @return New buffer containing hex string (asserts on allocation failure)
 */
DB_DEF db_buffer db_to_hex(db_buffer buf, bool uppercase);

/**
 * @brief Create buffer from hexadecimal string
 * @param hex_string Hexadecimal string (must be valid hex)
 * @param length Length of hex string
 * @return New buffer containing decoded bytes, or NULL on invalid hex string (asserts on allocation failure)
 */
DB_DEF db_buffer db_from_hex(const char* hex_string, size_t length);

/**
 * @brief Print buffer information for debugging
 * @param buf Buffer to print (can be NULL)
 * @param label Optional label for the output
 */
DB_DEF void db_debug_print(db_buffer buf, const char* label);

/** @} */

/**
 * @defgroup builder Buffer Builder API
 * @brief Functions for building buffers with primitive types
 * @{
 */

/**
 * @brief Opaque builder handle for constructing buffers efficiently
 */
typedef struct db_builder_internal* db_builder;

/**
 * @brief Create a new buffer builder
 * @param initial_capacity Initial capacity in bytes
 * @return New builder instance
 * 
 * @par Example:
 * @code
 * db_builder builder = db_builder_new(64);
 * db_builder_append_uint16_le(builder, 0x1234);
 * db_builder_append_cstring(builder, "Hello");
 * 
 * db_buffer buf = db_builder_finish(&builder);  // builder becomes NULL
 * printf("Built buffer with %zu bytes\n", db_size(buf));
 * db_release(&buf);
 * @endcode
 */
DB_DEF db_builder db_builder_new(size_t initial_capacity);

/**
 * @brief Create builder from existing buffer (continues at end)
 * @param buf Buffer to extend
 * @return New builder instance
 */
DB_DEF db_builder db_builder_from_buffer(db_buffer buf);

/**
 * @brief Increase builder reference count (share ownership)
 * @param builder Builder to retain (must not be NULL)
 * @return The same builder for convenience
 */
DB_DEF db_builder db_builder_retain(db_builder builder);

/**
 * @brief Decrease builder reference count and potentially free builder
 * @param builder_ptr Pointer to builder variable (will be set to NULL)
 */
DB_DEF void db_builder_release(db_builder* builder_ptr);

/**
 * @brief Finalize builder and return the constructed buffer
 * @param builder_ptr Pointer to builder (will be set to NULL)
 * @return Constructed buffer
 */
DB_DEF db_buffer db_builder_finish(db_builder* builder_ptr);

/**
 * @brief Get current write position in builder
 * @param builder Builder instance
 * @return Current position in bytes
 */
DB_DEF size_t db_builder_size(db_builder builder);

/**
 * @brief Get current capacity of builder
 * @param builder Builder instance  
 * @return Current capacity in bytes
 */
DB_DEF size_t db_builder_capacity(db_builder builder);

/**
 * @brief Clear builder contents
 * @param builder Builder instance
 */
DB_DEF void db_builder_clear(db_builder builder);

/**
 * @brief Write uint8 value
 * @param builder Builder instance
 * @param value Value to write
 * @return 0 on success, -1 on error
 */
DB_DEF int db_builder_append_uint8(db_builder builder, uint8_t value);

/**
 * @brief Write uint16 value in little-endian format
 * @param builder Builder instance
 * @param value Value to write
 * @return 0 on success, -1 on error
 */
DB_DEF int db_builder_append_uint16_le(db_builder builder, uint16_t value);

/**
 * @brief Write uint16 value in big-endian format
 * @param builder Builder instance
 * @param value Value to write
 * @return 0 on success, -1 on error
 */
DB_DEF int db_builder_append_uint16_be(db_builder builder, uint16_t value);

/**
 * @brief Write uint32 value in little-endian format
 * @param builder Builder instance
 * @param value Value to write
 * @return 0 on success, -1 on error
 */
DB_DEF int db_builder_append_uint32_le(db_builder builder, uint32_t value);

/**
 * @brief Write uint32 value in big-endian format
 * @param builder Builder instance
 * @param value Value to write
 * @return 0 on success, -1 on error
 */
DB_DEF int db_builder_append_uint32_be(db_builder builder, uint32_t value);

/**
 * @brief Write uint64 value in little-endian format
 * @param builder Builder instance
 * @param value Value to write
 * @return 0 on success, -1 on error
 */
DB_DEF int db_builder_append_uint64_le(db_builder builder, uint64_t value);

/**
 * @brief Write uint64 value in big-endian format
 * @param builder Builder instance
 * @param value Value to write
 * @return 0 on success, -1 on error
 */
DB_DEF int db_builder_append_uint64_be(db_builder builder, uint64_t value);

/**
 * @brief Write raw bytes
 * @param builder Builder instance
 * @param data Data to write
 * @param size Number of bytes to write
 * @return 0 on success, -1 on error
 */
DB_DEF int db_builder_append(db_builder builder, const void* data, size_t size);

/**
 * @brief Write null-terminated string (without null terminator)
 * @param builder Builder instance
 * @param str String to write
 * @return 0 on success, -1 on error
 */
DB_DEF int db_builder_append_cstring(db_builder builder, const char* str);

/**
 * @brief Append buffer contents
 * @param builder Builder instance
 * @param buf Buffer to append
 * @return 0 on success, -1 on error
 */
DB_DEF int db_builder_append_buffer(db_builder builder, db_buffer buf);

/** @} */

/**
 * @defgroup reader Buffer Reader API
 * @brief Functions for reading buffers with primitive types and cursor
 * @{
 */

/**
 * @brief Opaque reader handle for parsing buffers
 */
typedef struct db_reader_internal* db_reader;

/**
 * @brief Create a new buffer reader
 * @param buf Buffer to read from
 * @return New reader instance
 * 
 * @par Example:
 * @code
 * db_reader reader = db_reader_new(buffer);
 * uint16_t value = db_read_uint16_le(reader);
 * char data[10];
 * db_read_bytes(reader, data, sizeof(data));
 * db_reader_release(&reader);
 * @endcode
 */
DB_DEF db_reader db_reader_new(db_buffer buf);

/**
 * @brief Increase reader reference count (share ownership)
 * @param reader Reader to retain (must not be NULL)
 * @return The same reader for convenience
 */
DB_DEF db_reader db_reader_retain(db_reader reader);

/**
 * @brief Decrease reader reference count and potentially free reader
 * @param reader_ptr Pointer to reader variable (will be set to NULL)
 */
DB_DEF void db_reader_release(db_reader* reader_ptr);

/**
 * @brief Free reader resources (legacy name, use db_reader_release instead)
 * @param reader_ptr Pointer to reader (will be set to NULL)
 */
DB_DEF void db_reader_free(db_reader* reader_ptr);

/**
 * @brief Get current read position
 * @param reader Reader instance
 * @return Current position in bytes
 */
DB_DEF size_t db_reader_position(db_reader reader);

/**
 * @brief Get number of bytes remaining
 * @param reader Reader instance
 * @return Remaining bytes from current position
 */
DB_DEF size_t db_reader_remaining(db_reader reader);

/**
 * @brief Check if reader can read specified number of bytes
 * @param reader Reader instance
 * @param bytes Number of bytes to check
 * @return true if bytes are available
 */
DB_DEF bool db_reader_can_read(db_reader reader, size_t bytes);

/**
 * @brief Seek to specific position
 * @param reader Reader instance
 * @param position Position to seek to
 */
DB_DEF void db_reader_seek(db_reader reader, size_t position);

/**
 * @brief Read uint8 value
 * @param reader Reader instance
 * @return Read value
 */
DB_DEF uint8_t db_read_uint8(db_reader reader);

/**
 * @brief Read uint16 value in little-endian format
 * @param reader Reader instance
 * @return Read value
 */
DB_DEF uint16_t db_read_uint16_le(db_reader reader);

/**
 * @brief Read uint16 value in big-endian format
 * @param reader Reader instance
 * @return Read value
 */
DB_DEF uint16_t db_read_uint16_be(db_reader reader);

/**
 * @brief Read uint32 value in little-endian format
 * @param reader Reader instance
 * @return Read value
 */
DB_DEF uint32_t db_read_uint32_le(db_reader reader);

/**
 * @brief Read uint32 value in big-endian format
 * @param reader Reader instance
 * @return Read value
 */
DB_DEF uint32_t db_read_uint32_be(db_reader reader);

/**
 * @brief Read uint64 value in little-endian format
 * @param reader Reader instance
 * @return Read value
 */
DB_DEF uint64_t db_read_uint64_le(db_reader reader);

/**
 * @brief Read uint64 value in big-endian format
 * @param reader Reader instance
 * @return Read value
 */
DB_DEF uint64_t db_read_uint64_be(db_reader reader);

/**
 * @brief Read raw bytes
 * @param reader Reader instance
 * @param data Output buffer for data
 * @param size Number of bytes to read
 */
DB_DEF void db_read_bytes(db_reader reader, void* data, size_t size);

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
typedef struct db_internal {
    db_refcount_t refcount;    ///< Reference count for memory management
    size_t size;               ///< Current size of valid data in bytes
    size_t capacity;           ///< Total allocated capacity in bytes
} db_internal;

/**
 * @brief Get pointer to metadata for a buffer
 * @param buf Buffer handle (must not be NULL)
 * @return Pointer to metadata structure
 */
static inline db_internal* db_meta(db_buffer buf) {
    return (db_internal*)((char*)(buf) - sizeof(db_internal));
}

/**
 * @brief Allocate memory for buffer with metadata
 * @private
 */
static db_buffer db_alloc(size_t capacity) {
    // Allocate: metadata + buffer data
    size_t total_size = sizeof(db_internal) + capacity;
    
    // Check for overflow
    if (total_size < sizeof(db_internal) || total_size < capacity) {
        return NULL;
    }
    
    void* block = DB_MALLOC(total_size);
    DB_ASSERT(block && "db_alloc: memory allocation failed");
    
    // Initialize metadata
    db_internal* meta = (db_internal*)block;
    meta->refcount = DB_REFCOUNT_INIT(1);
    meta->size = 0;
    meta->capacity = capacity;
    
    // Return pointer to buffer data portion
    return (db_buffer)((char*)block + sizeof(db_internal));
}

/**
 * @brief Free buffer memory
 * @private
 */
static void db_dealloc(db_buffer buf) {
    if (!buf) return;
    
    // Get original malloc pointer and free it
    void* block = (char*)buf - sizeof(db_internal);
    DB_FREE(block);
}

// Implementation of public functions

DB_DEF db_buffer db_new(size_t capacity) {
    return db_alloc(capacity);
}

DB_DEF db_buffer db_new_with_data(const void* data, size_t size) {
    DB_ASSERT((data || size == 0) && "db_new_with_data: data cannot be NULL when size > 0");
    
    db_buffer buf = db_alloc(size);
    // db_alloc now asserts on allocation failure
    
    if (size > 0) {
        memcpy(buf, data, size);
        db_meta(buf)->size = size;
    }
    
    return buf;
}

DB_DEF db_buffer db_new_from_owned_data(void* data, size_t size, size_t capacity) {
    DB_ASSERT((data || (size == 0 && capacity == 0)) && "db_new_from_owned_data: data cannot be NULL with non-zero size/capacity");
    DB_ASSERT(capacity >= size && "db_new_from_owned_data: capacity must be >= size");
    
    // We can't use the negative offset trick here since we don't control the data allocation
    // Instead, we'll copy the data to maintain design consistency
    db_buffer buf = db_alloc(capacity);
    // db_alloc now asserts on allocation failure
    
    if (size > 0) {
        memcpy(buf, data, size);
        db_meta(buf)->size = size;
    }
    
    return buf;
}

DB_DEF db_buffer db_retain(db_buffer buf) {
    DB_ASSERT(buf && "db_retain: buf cannot be NULL");
    DB_REFCOUNT_INCREMENT(&db_meta(buf)->refcount);
    return buf;
}

DB_DEF void db_release(db_buffer* buf_ptr) {
    DB_ASSERT(buf_ptr && "db_release: buf_ptr cannot be NULL");
    if (!*buf_ptr) return;
    
    db_buffer buf = *buf_ptr;
    *buf_ptr = NULL;
    
    if (DB_REFCOUNT_DECREMENT(&db_meta(buf)->refcount) == 0) {
        // Reference count reached 0, free the buffer
        db_dealloc(buf);
    }
}


DB_DEF size_t db_size(db_buffer buf) {
    DB_ASSERT(buf && "db_size: buf cannot be NULL");
    return db_meta(buf)->size;
}

DB_DEF size_t db_capacity(db_buffer buf) {
    DB_ASSERT(buf && "db_capacity: buf cannot be NULL");
    return db_meta(buf)->capacity;
}

DB_DEF bool db_is_empty(db_buffer buf) {
    DB_ASSERT(buf && "db_is_empty: buf cannot be NULL");
    return db_meta(buf)->size == 0;
}

DB_DEF int db_refcount(db_buffer buf) {
    DB_ASSERT(buf && "db_refcount: buf cannot be NULL");
    return DB_REFCOUNT_LOAD(&db_meta(buf)->refcount);
}

DB_DEF db_buffer db_slice(db_buffer buf, size_t offset, size_t length) {
    DB_ASSERT(buf && "db_slice: buf cannot be NULL");
    
    size_t buf_size = db_meta(buf)->size;
    if (offset > buf_size || offset + length > buf_size) {
        return NULL; // Invalid bounds
    }
    
    // Create an independent copy of the slice data
    db_buffer slice = db_alloc(length);
    // db_alloc now asserts on allocation failure
    
    // Copy the slice data directly from the source buffer
    if (length > 0) {
        memcpy(slice, buf + offset, length);
        db_meta(slice)->size = length;
    }
    
    return slice;
}

DB_DEF db_buffer db_slice_from(db_buffer buf, size_t offset) {
    DB_ASSERT(buf && "db_slice_from: buf cannot be NULL");
    size_t buf_size = db_meta(buf)->size;
    if (offset > buf_size) return NULL;
    return db_slice(buf, offset, buf_size - offset);
}

DB_DEF db_buffer db_slice_to(db_buffer buf, size_t length) {
    DB_ASSERT(buf && "db_slice_to: buf cannot be NULL");
    if (length > db_meta(buf)->size) return NULL;
    return db_slice(buf, 0, length);
}



DB_DEF db_buffer db_append(db_buffer buf, const void* data, size_t size) {
    DB_ASSERT(buf && "db_append: buf cannot be NULL");
    DB_ASSERT((data || size == 0) && "db_append: data cannot be NULL when size > 0");
    
    if (size == 0) {
        return db_retain(buf);  // Return retained original if nothing to append
    }
    
    size_t old_size = db_meta(buf)->size;
    size_t new_size = old_size + size;
    db_buffer result = db_new(new_size);
    // db_new now asserts on allocation failure
    
    // Copy original buffer
    if (old_size > 0) {
        memcpy(result, buf, old_size);
    }
    // Append new data
    memcpy(result + old_size, data, size);
    
    // Set the final size
    db_meta(result)->size = new_size;
    
    return result;
}

// Internal helper functions for builder (similar to ds_stringbuilder helpers)

/**
 * @brief Internal function to ensure buffer has exclusive access
 * @param builder_data Pointer to builder's data pointer
 * @return 0 on success, -1 on failure
 */
static int db_internal_ensure_unique(db_buffer* builder_data) {
    if (!*builder_data) return -1;
    
    if (db_refcount(*builder_data) <= 1) {
        return 0; // Already unique
    }
    
    // Need to create our own copy
    size_t current_size = db_size(*builder_data);
    db_buffer new_buf = db_new_with_data(*builder_data, current_size);
    // db_new_with_data now asserts on allocation failure
    
    db_release(builder_data);
    *builder_data = new_buf;
    return 0;
}

/**
 * @brief Internal function to ensure buffer has enough capacity
 * @param builder_data Pointer to builder's data pointer  
 * @param builder_capacity Pointer to builder's capacity
 * @param required_capacity Required capacity in bytes
 * @return 0 on success, -1 on failure
 */
static int db_internal_ensure_capacity(db_buffer* builder_data, size_t* builder_capacity, size_t required_capacity) {
    if (*builder_capacity >= required_capacity) {
        return 0; // Already have enough capacity
    }
    
    size_t new_capacity = *builder_capacity;
    if (new_capacity == 0) {
        new_capacity = 16; // Initial capacity
    }
    
    while (new_capacity < required_capacity) {
        new_capacity *= 2; // Double the capacity
    }
    
    // Use realloc for efficient buffer growth
    db_internal* meta = db_meta(*builder_data);
    
    // Calculate new block size (metadata + data)
    size_t new_block_size = sizeof(db_internal) + new_capacity;
    
    // Realloc the entire block (metadata + data)
    db_internal* new_meta = (db_internal*)DB_REALLOC(meta, new_block_size);
    DB_ASSERT(new_meta && "db_internal_ensure_capacity: memory reallocation failed");
    
    // Update capacity (size remains the same)
    new_meta->capacity = new_capacity;
    
    // Update the buffer pointer to point to data portion
    *builder_data = (db_buffer)((char*)new_meta + sizeof(db_internal));
    *builder_capacity = new_capacity;
    return 0;
}

/**
 * @brief Internal function to append data to builder buffer (mutable)
 * @param builder_data Pointer to builder's data pointer
 * @param builder_capacity Pointer to builder's capacity
 * @param data Data to append
 * @param size Size of data to append
 * @return 0 on success, -1 on failure
 */
static int db_internal_append(db_buffer* builder_data, size_t* builder_capacity, const void* data, size_t size) {
    if (size == 0) return 0;
    
    if (db_internal_ensure_unique(builder_data) != 0) {
        return -1;
    }
    
    size_t current_size = db_size(*builder_data);
    size_t needed_capacity = current_size + size;
    
    if (db_internal_ensure_capacity(builder_data, builder_capacity, needed_capacity) != 0) {
        return -1;
    }
    
    // Append the data
    memcpy(*builder_data + current_size, data, size);
    db_meta(*builder_data)->size = current_size + size;
    
    return 0;
}

DB_DEF db_buffer db_concat(db_buffer buf1, db_buffer buf2) {
    DB_ASSERT(buf1 && "db_concat: buf1 cannot be NULL");
    DB_ASSERT(buf2 && "db_concat: buf2 cannot be NULL");
    
    size_t size1 = db_meta(buf1)->size;
    size_t size2 = db_meta(buf2)->size;
    size_t total_size = size1 + size2;
    
    if (total_size == 0) return db_new(0);
    
    db_buffer result = db_new(total_size);
    // db_new now asserts on allocation failure
    
    if (size1 > 0) {
        memcpy(result, buf1, size1);
    }
    
    if (size2 > 0) {
        memcpy(result + size1, buf2, size2);
    }
    
    db_meta(result)->size = total_size;
    return result;
}

DB_DEF db_buffer db_concat_many(db_buffer* buffers, size_t count) {
    if (!buffers || count == 0) return db_new(0);
    
    // Calculate total size
    size_t total_size = 0;
    for (size_t i = 0; i < count; i++) {
        if (buffers[i]) {
            total_size += db_meta(buffers[i])->size;
        }
    }
    
    db_buffer result = db_new(total_size);
    // db_new now asserts on allocation failure
    
    size_t offset = 0;
    for (size_t i = 0; i < count; i++) {
        if (buffers[i]) {
            size_t size = db_meta(buffers[i])->size;
            if (size > 0) {
                memcpy(result + offset, buffers[i], size);
                offset += size;
            }
        }
    }
    
    db_meta(result)->size = total_size;
    return result;
}

DB_DEF bool db_equals(db_buffer buf1, db_buffer buf2) {
    DB_ASSERT(buf1 && "db_equals: buf1 cannot be NULL");
    DB_ASSERT(buf2 && "db_equals: buf2 cannot be NULL");
    
    if (buf1 == buf2) return true;
    
    size_t size1 = db_meta(buf1)->size;
    size_t size2 = db_meta(buf2)->size;
    if (size1 != size2) return false;
    
    return memcmp(buf1, buf2, size1) == 0;
}

DB_DEF int db_compare(db_buffer buf1, db_buffer buf2) {
    DB_ASSERT(buf1 && "db_compare: buf1 cannot be NULL");
    DB_ASSERT(buf2 && "db_compare: buf2 cannot be NULL");
    
    if (buf1 == buf2) return 0;
    
    size_t size1 = db_meta(buf1)->size;
    size_t size2 = db_meta(buf2)->size;
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
    DB_ASSERT(buf_ptr && *buf_ptr && "db_read_fd: buf_ptr and *buf_ptr cannot be NULL");
    DB_ASSERT(fd >= 0 && "db_read_fd: invalid file descriptor");
    
    // Read data into a temporary buffer first
    size_t read_size = max_bytes == 0 ? 4096 : max_bytes;
    char* temp_buffer = (char*)DB_MALLOC(read_size);
    DB_ASSERT(temp_buffer && "db_read_fd: memory allocation failed");
    
    ssize_t bytes_read = db_read(fd, temp_buffer, read_size);
    if (bytes_read > 0) {
        // Create new buffer with appended data (immutable operation)
        db_buffer new_buf = db_append(*buf_ptr, temp_buffer, bytes_read);
        if (new_buf) {
            db_release(buf_ptr);  // Release old buffer
            *buf_ptr = new_buf;   // Update with new buffer
        } else {
            bytes_read = -1;  // Failed to append
        }
    }
    
    DB_FREE(temp_buffer);
    return bytes_read;
}

DB_DEF ssize_t db_write_fd(db_buffer buf, int fd) {
    DB_ASSERT(buf && "db_write_fd: buf cannot be NULL");
    DB_ASSERT(fd >= 0 && "db_write_fd: invalid file descriptor");
    size_t size = db_meta(buf)->size;
    if (size == 0) return 0;
    
    return db_write(fd, buf, size);
}

DB_DEF db_buffer db_read_file(const char* filename) {
    if (!filename) return NULL; // Allow NULL filename as runtime error
    
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
    
    db_meta(buf)->size = bytes_read;
    return buf;
}

DB_DEF bool db_write_file(db_buffer buf, const char* filename) {
    DB_ASSERT(buf && "db_write_file: buf cannot be NULL");
    if (!filename) return false;
    
    FILE* file = fopen(filename, "wb");
    if (!file) return false;
    
    bool success = true;
    size_t size = db_meta(buf)->size;
    if (size > 0) {
        size_t written = fwrite(buf, 1, size, file);
        success = (written == size);
    }
    
    fclose(file);
    return success;
}

// Utility functions
DB_DEF db_buffer db_to_hex(db_buffer buf, bool uppercase) {
    DB_ASSERT(buf && "db_to_hex: buf cannot be NULL");
    
    size_t size = db_meta(buf)->size;
    if (size == 0) return db_new_with_data("", 0);
    
    size_t hex_size = size * 2;
    db_buffer hex_buf = db_new(hex_size);
    // db_new now asserts on allocation failure
    
    const char* hex_chars = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    const uint8_t* data = (const uint8_t*)buf;
    char* hex_data = hex_buf;
    
    for (size_t i = 0; i < size; i++) {
        hex_data[i * 2] = hex_chars[data[i] >> 4];
        hex_data[i * 2 + 1] = hex_chars[data[i] & 0x0F];
    }
    
    db_meta(hex_buf)->size = hex_size;
    return hex_buf;
}

static int hex_char_to_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

DB_DEF db_buffer db_from_hex(const char* hex_string, size_t length) {
    if (!hex_string || length % 2 != 0) return NULL; // Runtime validation
    
    size_t byte_length = length / 2;
    db_buffer buf = db_new(byte_length);
    // db_new now asserts on allocation failure
    
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
    
    db_meta(buf)->size = byte_length;
    return buf;
}

DB_DEF void db_debug_print(db_buffer buf, const char* label) {
    const char* name = label ? label : "buffer";
    
    if (!buf) {
        printf("%s: NULL\n", name);
        return;
    }
    
    db_internal* meta = db_meta(buf);
    size_t size = meta->size;
    size_t capacity = meta->capacity;
    int refcount = DB_REFCOUNT_LOAD(&meta->refcount);
    
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

// Builder and Reader Implementation

struct db_builder_internal {
    db_refcount_t refcount;  // Reference count for the builder itself
    db_buffer data;          // Points to buffer data (same layout as db_buffer)
    size_t capacity;         // Capacity for growth (size is in metadata)
};

struct db_reader_internal {
    db_refcount_t refcount;  // Reference count for the reader itself
    db_buffer buf;           // Buffer being read (retained reference)
    size_t position;         // Current read position
};

// Builder implementation

DB_DEF db_builder db_builder_new(size_t initial_capacity) {
    struct db_builder_internal* builder = (struct db_builder_internal*)DB_MALLOC(sizeof(struct db_builder_internal));
    DB_ASSERT(builder && "db_builder_new: memory allocation failed");
    
    db_buffer buf = db_new(initial_capacity);
    // db_new asserts on allocation failure
    
    builder->refcount = DB_REFCOUNT_INIT(1);
    builder->data = buf;  // Take ownership of newly created buffer
    builder->capacity = initial_capacity;
    
    return builder;
}

DB_DEF db_builder db_builder_from_buffer(db_buffer buf) {
    DB_ASSERT(buf && "db_builder_from_buffer: buf cannot be NULL");
    
    struct db_builder_internal* builder = (struct db_builder_internal*)DB_MALLOC(sizeof(struct db_builder_internal));
    DB_ASSERT(builder && "db_builder_from_buffer: memory allocation failed");
    
    // Make a copy of the buffer to preserve immutability
    builder->data = db_new_with_data(buf, db_size(buf));
    builder->refcount = DB_REFCOUNT_INIT(1);
    builder->capacity = db_capacity(buf);
    
    return builder;
}

DB_DEF db_builder db_builder_retain(db_builder builder) {
    DB_ASSERT(builder && "db_builder_retain: builder cannot be NULL");
    DB_REFCOUNT_INCREMENT(&builder->refcount);
    return builder;
}

DB_DEF void db_builder_release(db_builder* builder_ptr) {
    DB_ASSERT(builder_ptr && "db_builder_release: builder_ptr cannot be NULL");
    if (!*builder_ptr) return;
    
    db_builder builder = *builder_ptr;
    *builder_ptr = NULL;
    
    if (DB_REFCOUNT_DECREMENT(&builder->refcount) == 0) {
        // Reference count reached 0, free the builder and release buffer
        db_release(&builder->data);
        DB_FREE(builder);
    }
}

DB_DEF db_buffer db_builder_finish(db_builder* builder_ptr) {
    DB_ASSERT(builder_ptr && "db_builder_finish: builder_ptr cannot be NULL");
    DB_ASSERT(*builder_ptr && "db_builder_finish: builder cannot be NULL");
    
    struct db_builder_internal* builder = *builder_ptr;
    db_buffer result = builder->data;
    
    // Invalidate the builder - don't release the buffer, it's being returned
    builder->data = NULL;
    DB_FREE(builder);
    *builder_ptr = NULL;
    
    return result;
}

DB_DEF size_t db_builder_size(db_builder builder) {
    DB_ASSERT(builder && "db_builder_size: builder cannot be NULL");
    return db_size(builder->data);
}

DB_DEF size_t db_builder_capacity(db_builder builder) {
    DB_ASSERT(builder && "db_builder_capacity: builder cannot be NULL");
    return builder->capacity;
}

DB_DEF void db_builder_clear(db_builder builder) {
    DB_ASSERT(builder && "db_builder_clear: builder cannot be NULL");
    
    // Always create a new empty buffer (since buffers are immutable)
    db_release(&builder->data);
    builder->data = db_new(builder->capacity);
}



DB_DEF int db_builder_append_uint8(db_builder builder, uint8_t value) {
    DB_ASSERT(builder && "db_builder_append_uint8: builder cannot be NULL");
    
    return db_internal_append(&builder->data, &builder->capacity, &value, 1);
}

DB_DEF int db_builder_append_uint16_le(db_builder builder, uint16_t value) {
    DB_ASSERT(builder && "db_builder_append_uint16_le: builder cannot be NULL");
    
    uint8_t bytes[2] = {
        (uint8_t)(value & 0xFF),
        (uint8_t)((value >> 8) & 0xFF)
    };
    
    return db_internal_append(&builder->data, &builder->capacity, bytes, 2);
}

DB_DEF int db_builder_append_uint16_be(db_builder builder, uint16_t value) {
    DB_ASSERT(builder && "db_builder_append_uint16_be: builder cannot be NULL");
    
    uint8_t bytes[2] = {
        (uint8_t)((value >> 8) & 0xFF),
        (uint8_t)(value & 0xFF)
    };
    
    return db_internal_append(&builder->data, &builder->capacity, bytes, 2);
}

DB_DEF int db_builder_append_uint32_le(db_builder builder, uint32_t value) {
    DB_ASSERT(builder && "db_builder_append_uint32_le: builder cannot be NULL");
    
    uint8_t bytes[4] = {
        (uint8_t)(value & 0xFF),
        (uint8_t)((value >> 8) & 0xFF),
        (uint8_t)((value >> 16) & 0xFF),
        (uint8_t)((value >> 24) & 0xFF)
    };
    
    return db_internal_append(&builder->data, &builder->capacity, bytes, 4);
}

DB_DEF int db_builder_append_uint32_be(db_builder builder, uint32_t value) {
    DB_ASSERT(builder && "db_builder_append_uint32_be: builder cannot be NULL");
    
    uint8_t bytes[4] = {
        (uint8_t)((value >> 24) & 0xFF),
        (uint8_t)((value >> 16) & 0xFF),
        (uint8_t)((value >> 8) & 0xFF),
        (uint8_t)(value & 0xFF)
    };
    
    return db_internal_append(&builder->data, &builder->capacity, bytes, 4);
}

DB_DEF int db_builder_append_uint64_le(db_builder builder, uint64_t value) {
    DB_ASSERT(builder && "db_builder_append_uint64_le: builder cannot be NULL");
    
    uint8_t bytes[8] = {
        (uint8_t)(value & 0xFF),
        (uint8_t)((value >> 8) & 0xFF),
        (uint8_t)((value >> 16) & 0xFF),
        (uint8_t)((value >> 24) & 0xFF),
        (uint8_t)((value >> 32) & 0xFF),
        (uint8_t)((value >> 40) & 0xFF),
        (uint8_t)((value >> 48) & 0xFF),
        (uint8_t)((value >> 56) & 0xFF)
    };
    
    return db_internal_append(&builder->data, &builder->capacity, bytes, 8);
}

DB_DEF int db_builder_append_uint64_be(db_builder builder, uint64_t value) {
    DB_ASSERT(builder && "db_builder_append_uint64_be: builder cannot be NULL");
    
    uint8_t bytes[8] = {
        (uint8_t)((value >> 56) & 0xFF),
        (uint8_t)((value >> 48) & 0xFF),
        (uint8_t)((value >> 40) & 0xFF),
        (uint8_t)((value >> 32) & 0xFF),
        (uint8_t)((value >> 24) & 0xFF),
        (uint8_t)((value >> 16) & 0xFF),
        (uint8_t)((value >> 8) & 0xFF),
        (uint8_t)(value & 0xFF)
    };
    
    return db_internal_append(&builder->data, &builder->capacity, bytes, 8);
}

DB_DEF int db_builder_append(db_builder builder, const void* data, size_t size) {
    DB_ASSERT(builder && "db_builder_append: builder cannot be NULL");
    DB_ASSERT((data || size == 0) && "db_builder_append: data cannot be NULL when size > 0");
    
    if (size == 0) return 0;
    
    return db_internal_append(&builder->data, &builder->capacity, data, size);
}

DB_DEF int db_builder_append_cstring(db_builder builder, const char* str) {
    DB_ASSERT(builder && "db_builder_append_cstring: builder cannot be NULL");
    DB_ASSERT(str && "db_builder_append_cstring: str cannot be NULL");
    
    size_t len = strlen(str);
    return db_builder_append(builder, str, len);
}

DB_DEF int db_builder_append_buffer(db_builder builder, db_buffer buf) {
    DB_ASSERT(builder && "db_builder_append_buffer: builder cannot be NULL");
    DB_ASSERT(buf && "db_builder_append_buffer: buf cannot be NULL");
    
    return db_builder_append(builder, buf, db_size(buf));
}

// Reader implementation

DB_DEF db_reader db_reader_new(db_buffer buf) {
    DB_ASSERT(buf && "db_reader_new: buf cannot be NULL");
    
    struct db_reader_internal* reader = (struct db_reader_internal*)DB_MALLOC(sizeof(struct db_reader_internal));
    DB_ASSERT(reader && "db_reader_new: memory allocation failed");
    
    reader->refcount = DB_REFCOUNT_INIT(1);
    reader->buf = db_retain(buf);  // Keep a reference to the buffer
    reader->position = 0;
    
    return reader;
}

DB_DEF db_reader db_reader_retain(db_reader reader) {
    DB_ASSERT(reader && "db_reader_retain: reader cannot be NULL");
    DB_REFCOUNT_INCREMENT(&reader->refcount);
    return reader;
}

DB_DEF void db_reader_release(db_reader* reader_ptr) {
    DB_ASSERT(reader_ptr && "db_reader_release: reader_ptr cannot be NULL");
    if (!*reader_ptr) return;
    
    db_reader reader = *reader_ptr;
    *reader_ptr = NULL;
    
    if (DB_REFCOUNT_DECREMENT(&reader->refcount) == 0) {
        // Reference count reached 0, free the reader and release buffer
        db_release(&reader->buf);
        DB_FREE(reader);
    }
}

DB_DEF void db_reader_free(db_reader* reader_ptr) {
    // Legacy function - just call the new release function
    db_reader_release(reader_ptr);
}

DB_DEF size_t db_reader_position(db_reader reader) {
    DB_ASSERT(reader && "reader cannot be NULL");
    return reader->position;
}

DB_DEF size_t db_reader_remaining(db_reader reader) {
    DB_ASSERT(reader && "reader cannot be NULL");
    size_t buffer_size = db_meta(reader->buf)->size;
    return (reader->position < buffer_size) ? (buffer_size - reader->position) : 0;
}

DB_DEF bool db_reader_can_read(db_reader reader, size_t bytes) {
    DB_ASSERT(reader && "reader cannot be NULL");
    return db_reader_remaining(reader) >= bytes;
}

DB_DEF void db_reader_seek(db_reader reader, size_t position) {
    DB_ASSERT(reader && "reader cannot be NULL");
    size_t buffer_size = db_meta(reader->buf)->size;
    DB_ASSERT(position <= buffer_size && "db_reader_seek: cannot seek past buffer end");
    reader->position = position;
}

DB_DEF uint8_t db_read_uint8(db_reader reader) {
    DB_ASSERT(reader && "reader cannot be NULL");
    DB_ASSERT(db_reader_can_read(reader, 1) && "db_read_uint8: insufficient data available");
    
    uint8_t value = *(uint8_t*)(reader->buf + reader->position);
    reader->position += 1;
    
    return value;
}

DB_DEF uint16_t db_read_uint16_le(db_reader reader) {
    DB_ASSERT(reader && "reader cannot be NULL");
    DB_ASSERT(db_reader_can_read(reader, 2) && "insufficient data available");
    
    const uint8_t* ptr = (const uint8_t*)(reader->buf + reader->position);
    uint16_t value = (uint16_t)ptr[0] | ((uint16_t)ptr[1] << 8);
    reader->position += 2;
    
    return value;
}

DB_DEF uint16_t db_read_uint16_be(db_reader reader) {
    DB_ASSERT(reader && "reader cannot be NULL");
    DB_ASSERT(db_reader_can_read(reader, 2) && "insufficient data available");
    
    const uint8_t* ptr = (const uint8_t*)(reader->buf + reader->position);
    uint16_t value = ((uint16_t)ptr[0] << 8) | (uint16_t)ptr[1];
    reader->position += 2;
    
    return value;
}

DB_DEF uint32_t db_read_uint32_le(db_reader reader) {
    DB_ASSERT(reader && "reader cannot be NULL");
    DB_ASSERT(db_reader_can_read(reader, 4) && "insufficient data available");
    
    const uint8_t* ptr = (const uint8_t*)(reader->buf + reader->position);
    uint32_t value = (uint32_t)ptr[0] | 
                    ((uint32_t)ptr[1] << 8) | 
                    ((uint32_t)ptr[2] << 16) | 
                    ((uint32_t)ptr[3] << 24);
    reader->position += 4;
    
    return value;
}

DB_DEF uint32_t db_read_uint32_be(db_reader reader) {
    DB_ASSERT(reader && "reader cannot be NULL");
    DB_ASSERT(db_reader_can_read(reader, 4) && "insufficient data available");
    
    const uint8_t* ptr = (const uint8_t*)(reader->buf + reader->position);
    uint32_t value = ((uint32_t)ptr[0] << 24) | 
                    ((uint32_t)ptr[1] << 16) | 
                    ((uint32_t)ptr[2] << 8) | 
                    (uint32_t)ptr[3];
    reader->position += 4;
    
    return value;
}

DB_DEF uint64_t db_read_uint64_le(db_reader reader) {
    DB_ASSERT(reader && "reader cannot be NULL");
    DB_ASSERT(db_reader_can_read(reader, 8) && "insufficient data available");
    
    const uint8_t* ptr = (const uint8_t*)(reader->buf + reader->position);
    uint64_t value = (uint64_t)ptr[0] | 
                    ((uint64_t)ptr[1] << 8) | 
                    ((uint64_t)ptr[2] << 16) | 
                    ((uint64_t)ptr[3] << 24) |
                    ((uint64_t)ptr[4] << 32) | 
                    ((uint64_t)ptr[5] << 40) | 
                    ((uint64_t)ptr[6] << 48) | 
                    ((uint64_t)ptr[7] << 56);
    reader->position += 8;
    
    return value;
}

DB_DEF uint64_t db_read_uint64_be(db_reader reader) {
    DB_ASSERT(reader && "reader cannot be NULL");
    DB_ASSERT(db_reader_can_read(reader, 8) && "insufficient data available");
    
    const uint8_t* ptr = (const uint8_t*)(reader->buf + reader->position);
    uint64_t value = ((uint64_t)ptr[0] << 56) | 
                    ((uint64_t)ptr[1] << 48) | 
                    ((uint64_t)ptr[2] << 40) | 
                    ((uint64_t)ptr[3] << 32) |
                    ((uint64_t)ptr[4] << 24) | 
                    ((uint64_t)ptr[5] << 16) | 
                    ((uint64_t)ptr[6] << 8) | 
                    (uint64_t)ptr[7];
    reader->position += 8;
    
    return value;
}

DB_DEF void db_read_bytes(db_reader reader, void* data, size_t size) {
    DB_ASSERT(reader && "reader cannot be NULL");
    DB_ASSERT((data || size == 0) && "db_read_bytes: data cannot be NULL when size > 0");
    DB_ASSERT(db_reader_can_read(reader, size) && "db_read_bytes: insufficient data available");
    
    if (size > 0) {
        memcpy(data, reader->buf + reader->position, size);
        reader->position += size;
    }
}

#endif // DB_IMPLEMENTATION

#endif // DYNAMIC_BUFFER_H