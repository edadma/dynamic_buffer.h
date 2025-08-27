# dynamic_buffer.h

[![Version](https://img.shields.io/badge/version-v0.2.1-blue.svg)](https://github.com/your-username/dynamic_buffer.h/releases)
[![Language](https://img.shields.io/badge/language-C11-blue.svg)](https://en.cppreference.com/w/c/11)
[![License](https://img.shields.io/badge/license-MIT%20OR%20Unlicense-green.svg)](#license)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS%20%7C%20MCU-lightgrey.svg)](#building)

A modern, efficient, single-header byte buffer library for C featuring reference counting, immutable operations, and I/O integration. Designed for safe, efficient buffer management with immutable slicing and automatic memory management.

## Features

**High Performance**
- Direct buffer access - pointers work with all C functions (no conversion needed!)
- Builder pattern with amortized O(1) append operations
- Reader pattern with type-safe primitive parsing
- Optional atomic reference counting for lock-free concurrent access

**Memory Safe**
- Reference counting prevents memory leaks and use-after-free for buffers, builders, and readers
- Immutable buffers safe for concurrent reading once created
- Bounds checking on all slice operations
- Automatic cleanup when last reference is released

**Developer Friendly**
- Single header-only library - just include the `.h` file
- Direct C compatibility - buffers work with `memcpy`, `write`, etc.
- Comprehensive test coverage with 46 unit tests
- Clear error handling with descriptive assertions

**Cross-Platform**
- Works on Linux, Windows, macOS
- Microcontroller support: ARM Cortex-M, ESP32, Raspberry Pi Pico
- C11 standard with optional atomic features
- Zero external dependencies

## Quick Start

```c
#define DB_IMPLEMENTATION
#include "dynamic_buffer.h"

int main() {
    // Create a buffer with some data
    db_buffer buf = db_new_with_data("Hello", 5);
    
    // Create an immutable appended buffer
    db_buffer greeting = db_append(buf, " World!", 7);
    
    // Print the result (buffer points directly to data)
    printf("Buffer: %.*s\n", (int)db_size(greeting), greeting);
    
    // Use builder for efficient construction
    db_builder builder = db_builder_new(64);
    db_builder_append_cstring(builder, "Built with ");
    db_builder_append_uint16_le(builder, 2024);
    db_buffer built = db_builder_finish(&builder);  // builder becomes NULL
    
    // Clean up (reference counting handles memory)
    db_release(&buf);
    db_release(&greeting);
    db_release(&built);
    
    return 0;
}
```

## Core Concepts

### Immutable Buffers
All buffers are immutable once created:
- `db_append()` creates a new buffer with appended data
- `db_concat()` creates a new buffer combining multiple buffers
- Original buffers remain unchanged (safe for sharing)
- Use `db_builder` for efficient mutable construction

### Reference Counting
All buffers use reference counting for memory management:
- `db_retain()` increases reference count (enables sharing)
- `db_release()` decreases reference count (automatic cleanup)
- Buffers are freed when reference count reaches zero

### Buffer Slicing
Create independent copies of buffer portions:
- `db_slice()` creates an independent copy of a buffer portion
- `db_slice_from()` and `db_slice_to()` for common slice patterns
- Slices are independent buffers with their own memory

### Builder Pattern
Efficient construction of complex buffers:
- `db_builder` provides mutable construction with reference counting
- Append primitives with endianness control
- Convert to immutable buffer when done
- Reference counting allows safe sharing of builders

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
// Buffers point directly to data (no db_data() needed)
size_t db_size(db_buffer buf);          // Current size
size_t db_capacity(db_buffer buf);      // Total capacity
bool db_is_empty(db_buffer buf);        // Check if empty
int db_refcount(db_buffer buf);         // Reference count
```

### Slicing Operations
```c
db_buffer db_slice(db_buffer buf, size_t offset, size_t length); // Create slice copy
db_buffer db_slice_from(db_buffer buf, size_t offset);           // Slice from offset to end
db_buffer db_slice_to(db_buffer buf, size_t length);             // Slice from start to length
```

### Immutable Operations
```c
db_buffer db_append(db_buffer buf, const void* data, size_t size); // Create new buffer with appended data
```

### Builder API
```c
db_builder db_builder_new(size_t initial_capacity);              // Create builder
db_builder db_builder_retain(db_builder builder);               // Increase builder refcount
void db_builder_release(db_builder* builder_ptr);               // Decrease builder refcount
int db_builder_append(db_builder builder, const void* data, size_t size); // Append raw data
int db_builder_append_uint16_le(db_builder builder, uint16_t value);      // Append primitives
db_buffer db_builder_finish(db_builder* builder_ptr);           // Convert to immutable buffer
```

### Reader API
```c
db_reader db_reader_new(db_buffer buf);                          // Create reader
db_reader db_reader_retain(db_reader reader);                   // Increase reader refcount
void db_reader_release(db_reader* reader_ptr);                  // Decrease reader refcount
uint16_t db_read_uint16_le(db_reader reader);                   // Read primitives
void db_read_bytes(db_reader reader, void* data, size_t size);  // Read raw data
void db_reader_free(db_reader* reader_ptr);                     // Legacy compatibility
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
ssize_t db_read_fd(db_buffer* buf_ptr, int fd, size_t max_bytes);  // Read from file descriptor
ssize_t db_write_fd(db_buffer buf, int fd);                       // Write to file descriptor
db_buffer db_read_file(const char* filename);                     // Read entire file
bool db_write_file(db_buffer buf, const char* filename);          // Write to file
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

**Important**: This library is **NOT generally thread-safe**. All operations require external synchronization for concurrent access.

With `DB_ATOMIC_REFCOUNT=1`:
- Reference counting operations (`db_retain`, `db_release`) are atomic and lock-free
- Buffer contents are immutable once created (safe for concurrent reading)
- **All other operations** require external synchronization including:
  - Buffer creation (`db_new*`, `db_append`, `db_concat`, `db_slice`)
  - Builder operations (`db_builder*`)
  - Reader operations (`db_reader*`, `db_read*`)

Without atomic reference counting:
- **All operations** require external synchronization when used concurrently

## Building

### CMake
```bash
mkdir build && cd build
cmake ..
make
```

### Manual Compilation
```bash
gcc -std=c11 -Wall -Wextra your_program.c -o your_program
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
ssize_t bytes = db_read_fd(&recv_buf, socket_fd, 0);

// Process header and payload separately
db_buffer header = db_slice_to(recv_buf, HEADER_SIZE);
db_buffer payload = db_slice_from(recv_buf, HEADER_SIZE);

db_release(&recv_buf);
db_release(&header);
db_release(&payload);
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

### Protocol Parsing with Reader
```c
// Parse message with typed data
db_buffer message = receive_message();
db_reader reader = db_reader_new(message);

// Read header fields
uint32_t magic = db_read_uint32_le(reader);
uint16_t version = db_read_uint16_le(reader);
uint16_t payload_len = db_read_uint16_le(reader);

// Read payload data
char* payload = malloc(payload_len);
db_read_bytes(reader, payload, payload_len);

// Process data
process_payload(payload, payload_len);

// Cleanup
free(payload);
db_reader_free(&reader);
db_release(&message);
```

### Building Binary Data
```c
// Construct a network packet
db_builder builder = db_builder_new(256);

// Add header
db_builder_append_uint32_le(&builder, 0xDEADBEEF);  // Magic
db_builder_append_uint16_le(&builder, 1);           // Version
db_builder_append_uint16_le(&builder, data_len);    // Length

// Add payload
db_builder_append(&builder, data, data_len);

// Convert to immutable buffer
db_buffer packet = db_builder_finish(&builder);

// Send over network
send_packet(packet);
db_release(&packet);
```

## Memory Layout

Buffers store metadata before the data:
```
[refcount | size | capacity | data...]
```

- **All buffers**: Own their data independently (slices are copies)
- **db_buffer**: Points directly to the data portion (not metadata)
- **Reference counting**: Prevents memory leaks and use-after-free
- **Direct access**: Use buffer pointer directly with C string functions

## Performance Characteristics

- **Buffer creation**: O(1) for empty buffers, O(n) when copying data
- **Slicing**: O(n) - creates independent copy of slice data
- **Concatenation**: O(n) - creates new buffer with combined data
- **Append**: O(n) - creates new buffer with original + appended data
- **Builder operations**: Amortized O(1) append with capacity growth
- **Comparison**: O(n) - compares byte-by-byte

## Error Handling

The library uses return values to indicate errors:
- `NULL` return for allocation failures or invalid parameters
- `false` return for operations that fail (e.g., modifying shared buffers)
- `-1` return for I/O operations that fail

All functions that can fail are documented with their failure conditions.

## Version History

### v0.2.1 (Current)
- **Bug Fixes**: Fixed db_builder_from_buffer to preserve buffer immutability by making immediate copy
- **Compatibility**: All 51 tests pass with corrected immutability semantics

### v0.2.0
- **Reference Counting**: Add reference counting to builders and readers  
- **API Changes**: Convert db_builder from struct to opaque pointer type
- **New Functions**: Add db_builder_retain(), db_builder_release(), db_reader_retain(), db_reader_release()
- **Error Handling**: Implement assertion-based error handling for NULL inputs
- **Testing**: Update comprehensive test suite with reference counting tests
- **Documentation**: Update API documentation and examples

### v0.1.1
- **Memory Safety**: All allocation failures now assert instead of returning NULL
- **Performance**: Builder uses DB_REALLOC for efficient capacity growth
- **Bug Fixes**: Fixed db_new_from_owned_data memory ownership semantics
- **Documentation**: Updated Doxygen comments to reflect assertion behavior
- **Compatibility**: Maintains 46/46 test coverage

### v0.1.0
- **Initial Release**: Complete immutable buffer system with reference counting
- **Builder Pattern**: Efficient mutable construction with `db_builder` API
- **Reader Pattern**: Type-safe parsing with cursor-based access  
- **I/O Integration**: Direct file and file descriptor operations
- **Comprehensive Testing**: 46 unit tests with 100% function coverage
- **Atomic Operations**: Optional lock-free reference counting for concurrent access
- **Cross-Platform**: Tested on PC and microcontroller platforms
- **Documentation**: Complete Doxygen API documentation with examples

## Dependencies

- **Core**: Only standard C library (`stdlib.h`, `string.h`, `stdint.h`, `stdbool.h`)
- **Atomic Operations**: `stdatomic.h` (C11, optional)
- **Testing**: Unity framework (included in `libs/unity/`)

## Platform Support

**Tested Platforms:**
- Linux (GCC, Clang)
- Windows (MinGW, MSVC)  
- macOS (Clang)
- ARM microcontrollers (Cortex-M series)
- ESP32/ESP32-C3 (Espressif toolchain)
- Raspberry Pi Pico (arm-none-eabi-gcc)

**Requirements:**
- C11 standard (uses atomic operations and other C11 features)
- ~100 bytes memory overhead per buffer
- No external dependencies for core functionality

## License

This project is dual-licensed under your choice of:

- [MIT License](LICENSE-MIT)
- [The Unlicense](LICENSE-UNLICENSE) (public domain)

Choose whichever license works best for your project!

## Contributing

Contributions welcome! Please ensure:
- All tests pass (`make tests && ./tests`)
- Code follows existing style conventions
- New features include comprehensive unit tests
- Documentation is updated for API changes
- Maintain compatibility across target platforms

## Documentation

Full API documentation is generated automatically from source code comments using Doxygen. Build with:

```bash
doxygen
# Output available in docs/html/index.html
```

---

*Built for safety, designed for performance, crafted for portability.*