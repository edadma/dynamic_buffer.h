# dynamic_buffer.h

A modern, efficient, single-header byte buffer library for C featuring reference counting, zero-copy slicing, and I/O integration. Designed to be similar to libuv's buffer type but with additional safety and convenience features.

## Features

- **Reference counting**: Automatic memory management with optional atomic support
- **Zero-copy slicing**: Create buffer views without data duplication
- **I/O integration**: Direct file descriptor and file operations
- **Memory safety**: Bounds checking and safe access patterns
- **Lock-free reference counting**: Optional atomic reference counting operations
- **Microcontroller friendly**: Minimal memory overhead and configurable allocators
- **Single header**: Just drop in the `.h` file - no dependencies

## Quick Start

```c
#define DB_IMPLEMENTATION
#include "dynamic_buffer.h"

int main() {
    // Create a buffer with some data
    db_buffer buf = db_new_with_data("Hello", 5);
    
    // Create a zero-copy slice
    db_buffer slice = db_slice(buf, 1, 3);  // "ell"
    
    // Concatenate buffers
    db_buffer world = db_new_with_data(" World!", 7);
    db_buffer greeting = db_concat(buf, world);
    
    // Print the result
    printf("Buffer: %.*s\n", (int)db_size(greeting), (char*)db_data(greeting));
    
    // Clean up (reference counting handles memory)
    db_release(&buf);
    db_release(&slice);
    db_release(&world);
    db_release(&greeting);
    
    return 0;
}
```

## Core Concepts

### Reference Counting
All buffers use reference counting for memory management:
- `db_retain()` increases reference count (safe sharing)
- `db_release()` decreases reference count (automatic cleanup)
- Buffers are freed when reference count reaches zero

### Zero-Copy Slicing
Create views of existing buffers without copying data:
- `db_slice()` creates a view of a portion of a buffer
- Slices maintain references to parent buffers
- Multiple slices can share the same underlying data

### Memory Safety
- Bounds checking on slice operations
- Modification requires exclusive access (refcount == 1)
- Safe access patterns prevent use-after-free errors

## API Overview

### Buffer Creation
```c
db_buffer db_new(size_t capacity);                              // Empty buffer
db_buffer db_new_with_data(const void* data, size_t size);      // Copy data
db_buffer db_new_from_owned_data(void* data, size_t size, size_t capacity); // Take ownership
```

### Memory Management
```c
db_buffer db_retain(db_buffer buf);     // Increase refcount
void db_release(db_buffer* buf_ptr);    // Decrease refcount
```

### Data Access
```c
const void* db_data(db_buffer buf);     // Read-only data pointer
void* db_mutable_data(db_buffer buf);   // Mutable data pointer (exclusive access required)
size_t db_size(db_buffer buf);          // Current size
size_t db_capacity(db_buffer buf);      // Total capacity
bool db_is_empty(db_buffer buf);        // Check if empty
```

### Slicing Operations
```c
db_buffer db_slice(db_buffer buf, size_t offset, size_t length); // Create slice
db_buffer db_slice_from(db_buffer buf, size_t offset);           // Slice from offset to end
db_buffer db_slice_to(db_buffer buf, size_t length);             // Slice from start to length
```

### Buffer Modification
```c
bool db_resize(db_buffer buf, size_t new_size);           // Resize buffer
bool db_reserve(db_buffer buf, size_t min_capacity);      // Ensure capacity
bool db_append(db_buffer buf, const void* data, size_t size); // Append data
bool db_clear(db_buffer buf);                             // Clear contents
```

### Concatenation
```c
db_buffer db_concat(db_buffer buf1, db_buffer buf2);              // Join two buffers
db_buffer db_concat_many(db_buffer* buffers, size_t count);       // Join multiple buffers
```

### Comparison
```c
bool db_equals(db_buffer buf1, db_buffer buf2);    // Test equality
int db_compare(db_buffer buf1, db_buffer buf2);    // Lexicographic comparison
```

### I/O Operations
```c
ssize_t db_read_fd(db_buffer buf, int fd, size_t max_bytes);  // Read from file descriptor
ssize_t db_write_fd(db_buffer buf, int fd);                  // Write to file descriptor
db_buffer db_read_file(const char* filename);                // Read entire file
bool db_write_file(db_buffer buf, const char* filename);     // Write to file
```

### Utility Functions
```c
db_buffer db_to_hex(db_buffer buf, bool uppercase);              // Convert to hex string
db_buffer db_from_hex(const char* hex_string, size_t length);    // Parse hex string
void db_debug_print(db_buffer buf, const char* label);          // Debug output
```

## Configuration

Customize the library by defining macros before including:

```c
#define DB_MALLOC malloc         // Custom allocator
#define DB_REALLOC realloc       // Custom reallocator  
#define DB_FREE free             // Custom deallocator
#define DB_ASSERT assert         // Custom assert macro
#define DB_ATOMIC_REFCOUNT 1     // Enable atomic reference counting (C11)

#define DB_IMPLEMENTATION
#include "dynamic_buffer.h"
```

## Concurrency Considerations

With `DB_ATOMIC_REFCOUNT=1`:
- Reference counting operations (`db_retain`, `db_release`) are atomic and lock-free
- Buffer contents are immutable once created (safe for concurrent reading)
- Buffer modification operations require external synchronization
- Slice creation uses atomic reference counting but requires external synchronization for the buffer being sliced

Without atomic reference counting:
- All operations require external synchronization when used concurrently

## Building

### CMake
```bash
mkdir build && cd build
cmake ..
make
```

### Manual Compilation
```bash
gcc -std=c99 -Wall -Wextra your_program.c -o your_program
```

### Running Tests
```bash
# Build and run tests
make tests
./tests

# Or use CTest
ctest
```

## Use Cases

### Network I/O
```c
// Read data from socket
db_buffer recv_buf = db_new(4096);
ssize_t bytes = db_read_fd(recv_buf, socket_fd, 0);

// Process header and payload separately (zero-copy)
db_buffer header = db_slice_to(recv_buf, HEADER_SIZE);
db_buffer payload = db_slice_from(recv_buf, HEADER_SIZE);
```

### File Processing
```c
// Read entire file
db_buffer file_data = db_read_file("data.bin");

// Process in chunks without copying
for (size_t offset = 0; offset < db_size(file_data); offset += CHUNK_SIZE) {
    size_t chunk_len = CHUNK_SIZE;
    if (offset + chunk_len > db_size(file_data)) {
        chunk_len = db_size(file_data) - offset;
    }
    
    db_buffer chunk = db_slice(file_data, offset, chunk_len);
    process_chunk(chunk);
    db_release(&chunk);
}

db_release(&file_data);
```

### Protocol Parsing
```c
// Parse message with header and variable payload
db_buffer message = receive_message();
db_buffer header = db_slice_to(message, 8);  // First 8 bytes

// Read length from header
uint32_t payload_len = read_uint32_from_buffer(header);

// Extract payload (zero-copy)
db_buffer payload = db_slice(message, 8, payload_len);

// Process without additional allocations
process_payload(payload);
```

## Memory Layout

Buffers store metadata before the data:
```
[refcount | size | capacity | offset | parent* | data...]
```

- **Root buffers**: Own their data and manage allocation
- **Slice buffers**: Reference parent buffers with calculated offsets
- **Reference counting**: Prevents memory leaks and use-after-free

## Performance Characteristics

- **Buffer creation**: O(1) for empty buffers, O(n) when copying data
- **Slicing**: O(1) - no data copying, just metadata allocation
- **Concatenation**: O(n) - creates new buffer with combined data
- **Append**: Amortized O(1) with exponential growth strategy
- **Comparison**: O(n) - compares byte-by-byte

## Error Handling

The library uses return values to indicate errors:
- `NULL` return for allocation failures or invalid parameters
- `false` return for operations that fail (e.g., modifying shared buffers)
- `-1` return for I/O operations that fail

All functions that can fail are documented with their failure conditions.

## License

Dual licensed under your choice of:
- MIT License  
- The Unlicense (public domain)

See LICENSE files for full details.

## Contributing

This is a single-header library. The main development happens in `dynamic_buffer.h`. 

To contribute:
1. Make changes to the header file
2. Add appropriate tests in `test.c`
3. Ensure all tests pass: `make tests && ./tests`
4. Update documentation as needed

## Documentation

Full API documentation is available at: [GitHub Pages](https://your-username.github.io/dynamic_buffer.h/)

Generated automatically from source code comments using Doxygen.