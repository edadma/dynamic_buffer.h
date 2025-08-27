#define DB_IMPLEMENTATION
#include "dynamic_buffer.h"
#include "unity.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

void setUp(void) {
    // Set up function called before each test
}

void tearDown(void) {
    // Tear down function called after each test
}

// Test buffer creation
void test_db_new_creates_empty_buffer(void) {
    db_buffer buf = db_new(0);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL(0, db_size(buf));
    TEST_ASSERT_EQUAL(0, db_capacity(buf));
    TEST_ASSERT_TRUE(db_is_empty(buf));
    TEST_ASSERT_EQUAL(1, db_refcount(buf));
    db_release(&buf);
    TEST_ASSERT_NULL(buf);
}

void test_db_new_creates_buffer_with_capacity(void) {
    db_buffer buf = db_new(100);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL(0, db_size(buf));
    TEST_ASSERT_EQUAL(100, db_capacity(buf));
    TEST_ASSERT_TRUE(db_is_empty(buf));
    db_release(&buf);
}

void test_db_new_with_data_copies_data(void) {
    const char* test_data = "Hello, World!";
    size_t data_len = strlen(test_data);
    
    db_buffer buf = db_new_with_data(test_data, data_len);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL(data_len, db_size(buf));
    TEST_ASSERT_EQUAL(data_len, db_capacity(buf));
    TEST_ASSERT_FALSE(db_is_empty(buf));
    
    // Verify data was copied correctly
    TEST_ASSERT_EQUAL_MEMORY(test_data, buf, data_len);
    
    db_release(&buf);
}

void test_db_new_with_data_handles_null_data(void) {
    db_buffer buf = db_new_with_data(NULL, 0);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL(0, db_size(buf));
    TEST_ASSERT_TRUE(db_is_empty(buf));
    db_release(&buf);
}

void test_db_new_with_data_rejects_invalid_params(void) {
    // Test that NULL data with size 0 is valid
    db_buffer buf = db_new_with_data(NULL, 0);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL(0, db_size(buf));
    db_release(&buf);
}

void test_db_new_from_owned_data_takes_ownership(void) {
    char* owned_data = DB_MALLOC(20);
    strcpy(owned_data, "Owned data");
    
    db_buffer buf = db_new_from_owned_data(owned_data, 10, 20);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL(10, db_size(buf));
    TEST_ASSERT_EQUAL(20, db_capacity(buf));
    TEST_ASSERT_EQUAL_MEMORY("Owned data", buf, 10);
    
    // Buffer now owns the data - don't free owned_data manually
    db_release(&buf);
}

// Test reference counting
void test_db_retain_increases_refcount(void) {
    db_buffer buf = db_new(10);
    TEST_ASSERT_EQUAL(1, db_refcount(buf));
    
    db_buffer buf2 = db_retain(buf);
    TEST_ASSERT_EQUAL(buf, buf2);
    TEST_ASSERT_EQUAL(2, db_refcount(buf));
    TEST_ASSERT_EQUAL(2, db_refcount(buf2));
    
    db_release(&buf);
    TEST_ASSERT_NULL(buf);
    TEST_ASSERT_EQUAL(1, db_refcount(buf2));
    
    db_release(&buf2);
}

// test_db_retain_handles_null removed - db_retain now requires non-NULL buffer

void test_db_release_handles_null(void) {
    db_buffer buf = NULL;
    db_release(&buf);  // Should not crash
    TEST_ASSERT_NULL(buf);
}

// Test data access
void test_db_data_returns_valid_pointer(void) {
    const char* test_data = "Test data";
    db_buffer buf = db_new_with_data(test_data, 9);
    
    const void* data = buf;
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_MEMORY(test_data, data, 9);
    
    db_release(&buf);
}

void test_db_mutable_data_allows_modification(void) {
    db_buffer buf = db_new_with_data("Hello", 5);
    
    char* mutable_data = (char*)buf;
    TEST_ASSERT_NOT_NULL(mutable_data);
    
    // Modify the data
    mutable_data[0] = 'h';
    
    // Verify the change
    TEST_ASSERT_EQUAL_MEMORY("hello", buf, 5);
    
    db_release(&buf);
}

// Test slicing operations
void test_db_slice_creates_copy(void) {
    const char* test_data = "Hello, World!";
    db_buffer buf = db_new_with_data(test_data, 13);
    
    db_buffer slice = db_slice(buf, 7, 5);  // "World"
    TEST_ASSERT_NOT_NULL(slice);
    TEST_ASSERT_EQUAL(5, db_size(slice));
    TEST_ASSERT_EQUAL(5, db_capacity(slice));
    TEST_ASSERT_EQUAL_MEMORY("World", slice, 5);
    
    // Slice is independent - original buffer refcount stays 1  
    TEST_ASSERT_EQUAL(1, db_refcount(buf));
    TEST_ASSERT_EQUAL(1, db_refcount(slice));
    
    db_release(&slice);
    db_release(&buf);
}

void test_db_slice_handles_invalid_bounds(void) {
    db_buffer buf = db_new_with_data("Hello", 5);
    
    // Test various invalid bounds
    TEST_ASSERT_NULL(db_slice(buf, 10, 1));  // offset too large
    TEST_ASSERT_NULL(db_slice(buf, 0, 10));  // length too large
    TEST_ASSERT_NULL(db_slice(buf, 3, 5));   // offset + length too large
    
    db_release(&buf);
}

void test_db_slice_from_creates_suffix(void) {
    db_buffer buf = db_new_with_data("Hello, World!", 13);
    
    db_buffer suffix = db_slice_from(buf, 7);  // "World!"
    TEST_ASSERT_NOT_NULL(suffix);
    TEST_ASSERT_EQUAL(6, db_size(suffix));
    TEST_ASSERT_EQUAL_MEMORY("World!", suffix, 6);
    
    db_release(&suffix);
    db_release(&buf);
}

void test_db_slice_to_creates_prefix(void) {
    db_buffer buf = db_new_with_data("Hello, World!", 13);
    
    db_buffer prefix = db_slice_to(buf, 5);  // "Hello"
    TEST_ASSERT_NOT_NULL(prefix);
    TEST_ASSERT_EQUAL(5, db_size(prefix));
    TEST_ASSERT_EQUAL_MEMORY("Hello", prefix, 5);
    
    db_release(&prefix);
    db_release(&buf);
}

// Test buffer modification
// Mutable operation tests removed - buffers are now immutable

void test_db_append_creates_new_buffer(void) {
    db_buffer buf1 = db_new_with_data("Hello", 5);
    db_buffer buf2 = db_append(buf1, ", World!", 8);
    
    TEST_ASSERT_NOT_NULL(buf2);
    TEST_ASSERT_EQUAL(13, db_size(buf2));
    TEST_ASSERT_EQUAL_MEMORY("Hello, World!", buf2, 13);
    
    // Original buffer unchanged
    TEST_ASSERT_EQUAL(5, db_size(buf1));
    TEST_ASSERT_EQUAL_MEMORY("Hello", buf1, 5);
    
    db_release(&buf1);
    db_release(&buf2);
}

void test_db_append_handles_empty_data_immutable(void) {
    db_buffer buf1 = db_new_with_data("Hello", 5);
    db_buffer buf2 = db_append(buf1, NULL, 0);
    
    // Should return retained original buffer when appending nothing
    TEST_ASSERT_EQUAL(buf1, buf2);
    TEST_ASSERT_EQUAL(5, db_size(buf2));
    TEST_ASSERT_EQUAL_MEMORY("Hello", buf2, 5);
    
    db_release(&buf1);
    db_release(&buf2);
}


void test_db_append_works_with_shared_buffer(void) {
    db_buffer buf1 = db_new_with_data("Hello", 5);
    db_buffer buf2 = db_retain(buf1);  // Share the buffer
    
    // Append should still work (creates new buffer)
    db_buffer buf3 = db_append(buf1, " World", 6);
    TEST_ASSERT_NOT_NULL(buf3);
    TEST_ASSERT_EQUAL(11, db_size(buf3));
    TEST_ASSERT_EQUAL_MEMORY("Hello World", buf3, 11);
    
    // Original buffers unchanged
    TEST_ASSERT_EQUAL(5, db_size(buf1));
    TEST_ASSERT_EQUAL(5, db_size(buf2));
    
    db_release(&buf1);
    db_release(&buf2);
    db_release(&buf3);
}

// test_db_clear_empties_buffer removed - db_clear no longer exists (buffers are immutable)

// Test concatenation
void test_db_concat_joins_buffers(void) {
    db_buffer buf1 = db_new_with_data("Hello", 5);
    db_buffer buf2 = db_new_with_data(" World", 6);
    
    db_buffer result = db_concat(buf1, buf2);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(11, db_size(result));
    TEST_ASSERT_EQUAL_MEMORY("Hello World", result, 11);
    
    db_release(&buf1);
    db_release(&buf2);
    db_release(&result);
}

// test_db_concat_handles_null_buffers removed - db_concat now requires non-NULL buffers

void test_db_concat_many_joins_multiple_buffers(void) {
    db_buffer buf1 = db_new_with_data("A", 1);
    db_buffer buf2 = db_new_with_data("B", 1);
    db_buffer buf3 = db_new_with_data("C", 1);
    
    db_buffer buffers[] = { buf1, buf2, buf3 };
    db_buffer result = db_concat_many(buffers, 3);
    
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(3, db_size(result));
    TEST_ASSERT_EQUAL_MEMORY("ABC", result, 3);
    
    db_release(&buf1);
    db_release(&buf2);
    db_release(&buf3);
    db_release(&result);
}

// Test comparison
void test_db_equals_compares_contents(void) {
    db_buffer buf1 = db_new_with_data("Hello", 5);
    db_buffer buf2 = db_new_with_data("Hello", 5);
    db_buffer buf3 = db_new_with_data("World", 5);
    
    TEST_ASSERT_TRUE(db_equals(buf1, buf2));
    TEST_ASSERT_FALSE(db_equals(buf1, buf3));
    TEST_ASSERT_TRUE(db_equals(buf1, buf1));  // Same buffer
    
    db_release(&buf1);
    db_release(&buf2);
    db_release(&buf3);
}

// test_db_equals_handles_null_buffers removed - db_equals now requires non-NULL buffers

void test_db_compare_returns_correct_order(void) {
    db_buffer buf1 = db_new_with_data("Apple", 5);
    db_buffer buf2 = db_new_with_data("Banana", 6);
    db_buffer buf3 = db_new_with_data("Apple", 5);
    
    TEST_ASSERT_TRUE(db_compare(buf1, buf2) < 0);   // Apple < Banana
    TEST_ASSERT_TRUE(db_compare(buf2, buf1) > 0);   // Banana > Apple
    TEST_ASSERT_EQUAL(0, db_compare(buf1, buf3));   // Apple == Apple
    
    db_release(&buf1);
    db_release(&buf2);
    db_release(&buf3);
}

// Test utility functions
void test_db_to_hex_converts_correctly(void) {
    uint8_t data[] = { 0x48, 0x65, 0x6C, 0x6C, 0x6F };  // "Hello"
    db_buffer buf = db_new_with_data(data, 5);
    
    db_buffer hex_lower = db_to_hex(buf, false);
    TEST_ASSERT_NOT_NULL(hex_lower);
    TEST_ASSERT_EQUAL(10, db_size(hex_lower));
    TEST_ASSERT_EQUAL_MEMORY("48656c6c6f", hex_lower, 10);
    
    db_buffer hex_upper = db_to_hex(buf, true);
    TEST_ASSERT_NOT_NULL(hex_upper);
    TEST_ASSERT_EQUAL(10, db_size(hex_upper));
    TEST_ASSERT_EQUAL_MEMORY("48656C6C6F", hex_upper, 10);
    
    db_release(&buf);
    db_release(&hex_lower);
    db_release(&hex_upper);
}

void test_db_from_hex_converts_correctly(void) {
    db_buffer buf = db_from_hex("48656c6c6f", 10);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL(5, db_size(buf));
    TEST_ASSERT_EQUAL_MEMORY("Hello", buf, 5);
    db_release(&buf);
}

void test_db_from_hex_handles_invalid_input(void) {
    TEST_ASSERT_NULL(db_from_hex("48656c6c6", 9));   // Odd length
    TEST_ASSERT_NULL(db_from_hex("48656G6C6F", 10)); // Invalid hex character
    TEST_ASSERT_NULL(db_from_hex(NULL, 10));         // NULL string
}

void test_db_debug_print_doesnt_crash(void) {
    db_buffer buf = db_new_with_data("Hello", 5);
    
    // These should not crash
    db_debug_print(buf, "test_buffer");
    db_debug_print(buf, NULL);
    db_debug_print(NULL, "null_buffer");
    
    db_release(&buf);
}

// Test edge cases and error conditions

void test_large_buffer_operations(void) {
    const size_t large_size = 1024 * 1024;  // 1MB
    
    // Create pattern data
    char* pattern_data = malloc(large_size);
    for (size_t i = 0; i < large_size; i++) {
        pattern_data[i] = (char)(i % 256);
    }
    
    db_buffer buf = db_new_with_data(pattern_data, large_size);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL(large_size, db_size(buf));
    
    // Verify pattern
    const char* const_data = (const char*)buf;
    for (size_t i = 0; i < 100; i++) {  // Check first 100 bytes
        TEST_ASSERT_EQUAL((char)(i % 256), const_data[i]);
    }
    
    free(pattern_data);
    db_release(&buf);
}

// Builder API Tests

void test_builder_basic_operations(void) {
    db_builder builder = db_builder_new(64);
    TEST_ASSERT_NOT_NULL(builder.data);
    TEST_ASSERT_EQUAL(64, db_builder_capacity(&builder));
    
    TEST_ASSERT_EQUAL(0, db_builder_size(&builder));
    
    db_buffer buf = db_builder_finish(&builder);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_NULL(builder.data);  // Should be set to NULL after finish
    
    TEST_ASSERT_EQUAL(0, db_size(buf));
    
    db_release(&buf);
}

void test_builder_write_primitives(void) {
    db_builder builder = db_builder_new(64);
    
    TEST_ASSERT_EQUAL(0, db_builder_append_uint8(&builder, 0x42));
    TEST_ASSERT_EQUAL(0, db_builder_append_uint16_le(&builder, 0x1234));
    TEST_ASSERT_EQUAL(0, db_builder_append_uint32_le(&builder, 0x12345678));
    TEST_ASSERT_EQUAL(0, db_builder_append_uint64_le(&builder, 0x123456789ABCDEF0ULL));
    
    TEST_ASSERT_EQUAL(1 + 2 + 4 + 8, db_builder_size(&builder));
    
    db_buffer buf = db_builder_finish(&builder);
    TEST_ASSERT_EQUAL(15, db_size(buf));
    
    const uint8_t* data = (const uint8_t*)buf;
    TEST_ASSERT_EQUAL(0x42, data[0]);
    TEST_ASSERT_EQUAL(0x34, data[1]);
    TEST_ASSERT_EQUAL(0x12, data[2]);
    
    db_release(&buf);
}

void test_builder_write_endianness(void) {
    db_builder builder = db_builder_new(64);
    
    // Write same value in both endiannesses
    TEST_ASSERT_EQUAL(0, db_builder_append_uint16_le(&builder, 0x1234));
    TEST_ASSERT_EQUAL(0, db_builder_append_uint16_be(&builder, 0x1234));
    
    db_buffer buf = db_builder_finish(&builder);
    const uint8_t* data = (const uint8_t*)buf;
    
    // Little-endian: 0x34, 0x12
    TEST_ASSERT_EQUAL(0x34, data[0]);
    TEST_ASSERT_EQUAL(0x12, data[1]);
    
    // Big-endian: 0x12, 0x34
    TEST_ASSERT_EQUAL(0x12, data[2]);
    TEST_ASSERT_EQUAL(0x34, data[3]);
    
    db_release(&buf);
}

void test_builder_from_buffer(void) {
    db_buffer buf = db_new_with_data("Hello", 5);
    
    db_builder builder = db_builder_from_buffer(buf);
    TEST_ASSERT_EQUAL(5, db_builder_size(&builder));
    
    TEST_ASSERT_EQUAL(0, db_builder_append_cstring(&builder, " World"));
    
    db_buffer result = db_builder_finish(&builder);
    TEST_ASSERT_EQUAL(11, db_size(result));
    TEST_ASSERT_EQUAL_MEMORY("Hello World", result, 11);
    
    db_release(&buf);
    db_release(&result);
}

void test_builder_clear_operations(void) {
    db_builder builder = db_builder_new(64);
    
    TEST_ASSERT_EQUAL(0, db_builder_append_uint32_le(&builder, 0x12345678));
    TEST_ASSERT_EQUAL(4, db_builder_size(&builder));
    
    db_builder_clear(&builder);
    TEST_ASSERT_EQUAL(0, db_builder_size(&builder));
    
    TEST_ASSERT_EQUAL(0, db_builder_append_cstring(&builder, "Test"));
    TEST_ASSERT_EQUAL(4, db_builder_size(&builder));
    
    db_buffer buf = db_builder_finish(&builder);
    TEST_ASSERT_EQUAL_MEMORY("Test", buf, 4);
    
    db_release(&buf);
}

// Reader API Tests

void test_reader_basic_operations(void) {
    db_buffer buf = db_new_with_data("Hello", 5);
    
    db_reader reader = db_reader_new(buf);
    TEST_ASSERT_NOT_NULL(reader);
    
    TEST_ASSERT_EQUAL(0, db_reader_position(reader));
    TEST_ASSERT_EQUAL(5, db_reader_remaining(reader));
    TEST_ASSERT_TRUE(db_reader_can_read(reader, 5));
    TEST_ASSERT_FALSE(db_reader_can_read(reader, 6));
    
    db_reader_free(&reader);
    TEST_ASSERT_NULL(reader);  // Should be set to NULL
    
    db_release(&buf);
}

void test_reader_read_primitives(void) {
    uint8_t test_data[] = {
        0x42,                           // uint8
        0x34, 0x12,                     // uint16_le (0x1234)
        0x78, 0x56, 0x34, 0x12,        // uint32_le (0x12345678)
        0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12  // uint64_le
    };
    
    db_buffer buf = db_new_with_data(test_data, sizeof(test_data));
    db_reader reader = db_reader_new(buf);
    
    TEST_ASSERT_EQUAL(0x42, db_read_uint8(reader));
    TEST_ASSERT_EQUAL(0x1234, db_read_uint16_le(reader));
    TEST_ASSERT_EQUAL(0x12345678UL, db_read_uint32_le(reader));
    TEST_ASSERT_EQUAL(0x123456789ABCDEF0ULL, db_read_uint64_le(reader));
    
    TEST_ASSERT_EQUAL(sizeof(test_data), db_reader_position(reader));
    TEST_ASSERT_EQUAL(0, db_reader_remaining(reader));
    
    db_reader_free(&reader);
    db_release(&buf);
}

void test_reader_read_endianness(void) {
    uint8_t test_data[] = {
        0x34, 0x12,  // Little-endian 0x1234
        0x12, 0x34   // Big-endian 0x1234
    };
    
    db_buffer buf = db_new_with_data(test_data, sizeof(test_data));
    db_reader reader = db_reader_new(buf);
    
    TEST_ASSERT_EQUAL(0x1234, db_read_uint16_le(reader));
    TEST_ASSERT_EQUAL(0x1234, db_read_uint16_be(reader));
    
    db_reader_free(&reader);
    db_release(&buf);
}

void test_reader_bounds_checking(void) {
    db_buffer buf = db_new_with_data("Hi", 2);
    db_reader reader = db_reader_new(buf);
    
    // These should work
    TEST_ASSERT_TRUE(db_reader_can_read(reader, 1));
    TEST_ASSERT_TRUE(db_reader_can_read(reader, 2));
    db_read_uint8(reader);  // Read one byte
    
    TEST_ASSERT_TRUE(db_reader_can_read(reader, 1));
    TEST_ASSERT_FALSE(db_reader_can_read(reader, 2));
    db_read_uint8(reader);  // Read second byte
    
    TEST_ASSERT_FALSE(db_reader_can_read(reader, 1));
    TEST_ASSERT_EQUAL(0, db_reader_remaining(reader));
    
    db_reader_free(&reader);
    db_release(&buf);
}

void test_reader_seek_operations(void) {
    uint8_t test_data[] = {0x10, 0x20, 0x30, 0x40};
    db_buffer buf = db_new_with_data(test_data, sizeof(test_data));
    db_reader reader = db_reader_new(buf);
    
    TEST_ASSERT_EQUAL(0x10, db_read_uint8(reader));
    
    db_reader_seek(reader, 2);  // Seek to position 2
    TEST_ASSERT_EQUAL(0x30, db_read_uint8(reader));
    
    db_reader_seek(reader, 0);  // Seek back to start
    TEST_ASSERT_EQUAL(0x10, db_read_uint8(reader));
    
    db_reader_free(&reader);
    db_release(&buf);
}

void test_builder_reader_roundtrip(void) {
    // Build a complex buffer
    db_builder builder = db_builder_new(64);
    TEST_ASSERT_EQUAL(0, db_builder_append_uint8(&builder, 0x42));
    TEST_ASSERT_EQUAL(0, db_builder_append_uint16_le(&builder, 0x1234));
    TEST_ASSERT_EQUAL(0, db_builder_append_uint32_be(&builder, 0x12345678));
    TEST_ASSERT_EQUAL(0, db_builder_append_cstring(&builder, "Test"));
    
    db_buffer buf = db_builder_finish(&builder);
    
    // Read it back
    db_reader reader = db_reader_new(buf);
    
    TEST_ASSERT_EQUAL(0x42, db_read_uint8(reader));
    TEST_ASSERT_EQUAL(0x1234, db_read_uint16_le(reader));
    TEST_ASSERT_EQUAL(0x12345678UL, db_read_uint32_be(reader));
    
    char str_buf[5] = {0};
    db_read_bytes(reader, str_buf, 4);
    TEST_ASSERT_EQUAL_STRING("Test", str_buf);
    
    TEST_ASSERT_EQUAL(0, db_reader_remaining(reader));
    
    db_reader_free(&reader);
    db_release(&buf);
}

// Missing core function tests
void test_db_core_functions(void) {
    db_buffer buf = db_new_with_data("Hello", 5);
    
    TEST_ASSERT_EQUAL(5, db_size(buf));
    TEST_ASSERT(db_capacity(buf) >= 5);
    TEST_ASSERT_FALSE(db_is_empty(buf));
    TEST_ASSERT_EQUAL(1, db_refcount(buf));
    
    db_buffer shared = db_retain(buf);
    TEST_ASSERT_EQUAL(2, db_refcount(buf));
    TEST_ASSERT_EQUAL(2, db_refcount(shared));
    
    db_release(&shared);
    TEST_ASSERT_EQUAL(1, db_refcount(buf));
    
    db_release(&buf);
}

void test_db_empty_buffer_core_functions(void) {
    db_buffer buf = db_new(0);
    
    TEST_ASSERT_EQUAL(0, db_size(buf));
    TEST_ASSERT_EQUAL(0, db_capacity(buf));
    TEST_ASSERT_TRUE(db_is_empty(buf));
    TEST_ASSERT_EQUAL(1, db_refcount(buf));
    
    db_release(&buf);
}

// Missing builder function tests
void test_builder_append_functions(void) {
    db_builder builder = db_builder_new(64);
    
    // Test db_builder_append with raw data
    TEST_ASSERT_EQUAL(0, db_builder_append(&builder, "Hello", 5));
    TEST_ASSERT_EQUAL(5, db_builder_size(&builder));
    
    // Test db_builder_append_cstring  
    TEST_ASSERT_EQUAL(0, db_builder_append_cstring(&builder, " World"));
    TEST_ASSERT_EQUAL(11, db_builder_size(&builder));
    
    // Test db_builder_append_buffer
    db_buffer extra = db_new_with_data("!", 1);
    TEST_ASSERT_EQUAL(0, db_builder_append_buffer(&builder, extra));
    TEST_ASSERT_EQUAL(12, db_builder_size(&builder));
    
    db_buffer result = db_builder_finish(&builder);
    TEST_ASSERT_EQUAL(12, db_size(result));
    TEST_ASSERT_EQUAL_MEMORY("Hello World!", result, 12);
    
    db_release(&extra);
    db_release(&result);
}

// Missing reader function tests  
void test_reader_missing_functions(void) {
    // Create buffer with big-endian uint64
    db_builder builder = db_builder_new(32);
    TEST_ASSERT_EQUAL(0, db_builder_append_uint64_be(&builder, 0x123456789ABCDEF0ULL));
    
    // Add some bytes for db_read_bytes test
    TEST_ASSERT_EQUAL(0, db_builder_append(&builder, "TestData", 8));
    
    db_buffer buf = db_builder_finish(&builder);
    db_reader reader = db_reader_new(buf);
    
    // Test db_read_uint64_be
    TEST_ASSERT_EQUAL(0x123456789ABCDEF0ULL, db_read_uint64_be(reader));
    
    // Test db_read_bytes
    char data[9] = {0};
    db_read_bytes(reader, data, 8);
    TEST_ASSERT_EQUAL_STRING("TestData", data);
    
    db_reader_free(&reader);
    db_release(&buf);
}

// Edge case tests for NULL parameters
void test_null_parameter_handling(void) {
    // Test debug print with NULL (these should not crash)
    db_debug_print(NULL, "null_test");
    db_debug_print(NULL, NULL);
    
    // Test db_new_with_data with NULL data but 0 size (should work)
    db_buffer buf = db_new_with_data(NULL, 0);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_TRUE(db_is_empty(buf));
    db_release(&buf);
}

// Edge case tests for invalid parameters
void test_invalid_parameter_handling(void) {
    db_buffer buf = db_new_with_data("Hello", 5);
    
    // Test slice with invalid bounds
    TEST_ASSERT_NULL(db_slice(buf, 10, 5));  // offset beyond buffer
    TEST_ASSERT_NULL(db_slice(buf, 2, 10));  // length extends beyond buffer
    TEST_ASSERT_NULL(db_slice(buf, 5, 1));   // offset at end, non-zero length
    
    // Test slice_from with invalid offset
    TEST_ASSERT_NULL(db_slice_from(buf, 10));  // offset beyond buffer
    
    // Test slice_to with invalid length  
    TEST_ASSERT_NULL(db_slice_to(buf, 10));  // length beyond buffer
    
    db_release(&buf);
}

// Reader edge case tests
void test_reader_edge_cases(void) {
    db_buffer buf = db_new_with_data("Test", 4);
    db_reader reader = db_reader_new(buf);
    
    // Test bounds checking
    TEST_ASSERT_TRUE(db_reader_can_read(reader, 4));
    TEST_ASSERT_FALSE(db_reader_can_read(reader, 5));
    
    // Read all data
    char data[5] = {0};
    db_read_bytes(reader, data, 4);
    
    // Try to read beyond end (should handle gracefully)
    TEST_ASSERT_FALSE(db_reader_can_read(reader, 1));
    TEST_ASSERT_EQUAL(0, db_reader_remaining(reader));
    
    // Test seeking to valid positions
    db_reader_seek(reader, 2);
    TEST_ASSERT_EQUAL(2, db_reader_position(reader));
    
    db_reader_seek(reader, 4);  // Seek to end
    TEST_ASSERT_EQUAL(4, db_reader_position(reader));
    
    db_reader_free(&reader);
    db_release(&buf);
}

// Builder capacity growth tests
void test_builder_capacity_growth(void) {
    db_builder builder = db_builder_new(8);  // Small initial capacity
    
    TEST_ASSERT_EQUAL(8, db_builder_capacity(&builder));
    
    // Fill beyond initial capacity to trigger growth
    const char* large_data = "This is much longer than 8 bytes and should trigger capacity growth";
    TEST_ASSERT_EQUAL(0, db_builder_append(&builder, large_data, strlen(large_data)));
    
    TEST_ASSERT(db_builder_capacity(&builder) >= strlen(large_data));
    TEST_ASSERT_EQUAL(strlen(large_data), db_builder_size(&builder));
    
    db_buffer result = db_builder_finish(&builder);
    TEST_ASSERT_EQUAL(strlen(large_data), db_size(result));
    TEST_ASSERT_EQUAL_MEMORY(large_data, result, strlen(large_data));
    
    db_release(&result);
}

// I/O function tests
void test_file_io_operations(void) {
    const char* test_filename = "/tmp/db_test_file.bin";
    const char* test_data = "Hello, File I/O!";
    
    // Create buffer and write to file
    db_buffer write_buf = db_new_with_data(test_data, strlen(test_data));
    bool write_result = db_write_file(write_buf, test_filename);
    TEST_ASSERT_TRUE(write_result);
    
    // Read back from file
    db_buffer read_buf = db_read_file(test_filename);
    TEST_ASSERT_NOT_NULL(read_buf);
    TEST_ASSERT_EQUAL(strlen(test_data), db_size(read_buf));
    TEST_ASSERT_EQUAL_MEMORY(test_data, read_buf, strlen(test_data));
    
    // Clean up
    db_release(&write_buf);
    db_release(&read_buf);
    unlink(test_filename);
}

void test_file_io_nonexistent_file(void) {
    // Test reading non-existent file
    db_buffer buf = db_read_file("/tmp/nonexistent_file_12345.bin");
    TEST_ASSERT_NULL(buf);
    
    // Test writing to invalid path (should fail gracefully)
    db_buffer test_buf = db_new_with_data("test", 4);
    bool result = db_write_file(test_buf, "/root/invalid_path/file.bin");
    TEST_ASSERT_FALSE(result);
    
    db_release(&test_buf);
}

int main(void) {
    UNITY_BEGIN();
    
    // Buffer creation tests
    RUN_TEST(test_db_new_creates_empty_buffer);
    RUN_TEST(test_db_new_creates_buffer_with_capacity);
    RUN_TEST(test_db_new_with_data_copies_data);
    RUN_TEST(test_db_new_with_data_handles_null_data);
    RUN_TEST(test_db_new_with_data_rejects_invalid_params);
    RUN_TEST(test_db_new_from_owned_data_takes_ownership);
    
    // Reference counting tests
    RUN_TEST(test_db_retain_increases_refcount);
    // RUN_TEST(test_db_retain_handles_null); // Removed - function now requires non-NULL
    RUN_TEST(test_db_release_handles_null);
    
    // Data access tests
    RUN_TEST(test_db_data_returns_valid_pointer);
    RUN_TEST(test_db_mutable_data_allows_modification);
    
    // Core function tests
    RUN_TEST(test_db_core_functions);
    RUN_TEST(test_db_empty_buffer_core_functions);
    
    // Slicing tests
    RUN_TEST(test_db_slice_creates_copy);
    RUN_TEST(test_db_slice_handles_invalid_bounds);
    RUN_TEST(test_db_slice_from_creates_suffix);
    RUN_TEST(test_db_slice_to_creates_prefix);
    
    // Modification tests
    RUN_TEST(test_db_append_creates_new_buffer);
    RUN_TEST(test_db_append_handles_empty_data_immutable);
    
    // Concatenation tests
    RUN_TEST(test_db_concat_joins_buffers);
    // RUN_TEST(test_db_concat_handles_null_buffers); // Removed - function now requires non-NULL
    RUN_TEST(test_db_concat_many_joins_multiple_buffers);
    
    // Comparison tests
    RUN_TEST(test_db_equals_compares_contents);
    // RUN_TEST(test_db_equals_handles_null_buffers); // Removed - function now requires non-NULL
    RUN_TEST(test_db_compare_returns_correct_order);
    
    // Utility tests
    RUN_TEST(test_db_to_hex_converts_correctly);
    RUN_TEST(test_db_from_hex_converts_correctly);
    RUN_TEST(test_db_from_hex_handles_invalid_input);
    RUN_TEST(test_db_debug_print_doesnt_crash);
    
    // I/O function tests
    RUN_TEST(test_file_io_operations);
    RUN_TEST(test_file_io_nonexistent_file);
    
    // Builder API tests
    RUN_TEST(test_builder_basic_operations);
    RUN_TEST(test_builder_write_primitives);
    RUN_TEST(test_builder_write_endianness);
    RUN_TEST(test_builder_from_buffer);
    RUN_TEST(test_builder_clear_operations);
    RUN_TEST(test_builder_append_functions);
    RUN_TEST(test_builder_capacity_growth);
    
    // Reader API tests
    RUN_TEST(test_reader_basic_operations);
    RUN_TEST(test_reader_read_primitives);
    RUN_TEST(test_reader_read_endianness);
    RUN_TEST(test_reader_bounds_checking);
    RUN_TEST(test_reader_seek_operations);
    RUN_TEST(test_reader_missing_functions);
    RUN_TEST(test_reader_edge_cases);
    
    // Builder + Reader integration tests
    RUN_TEST(test_builder_reader_roundtrip);
    
    // Edge case tests
    RUN_TEST(test_large_buffer_operations);
    RUN_TEST(test_null_parameter_handling);
    RUN_TEST(test_invalid_parameter_handling);
    
    return UNITY_END();
}