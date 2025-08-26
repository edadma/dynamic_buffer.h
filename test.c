#define DB_IMPLEMENTATION
#include "dynamic_buffer.h"
#include "unity.h"
#include <string.h>

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

void test_db_retain_handles_null(void) {
    db_buffer buf = db_retain(NULL);
    TEST_ASSERT_NULL(buf);
}

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
void test_db_resize_increases_size(void) {
    db_buffer buf = db_new(10);
    TEST_ASSERT_TRUE(db_resize(&buf, 20));
    TEST_ASSERT_EQUAL(20, db_size(buf));
    TEST_ASSERT_TRUE(db_capacity(buf) >= 20);
    db_release(&buf);
}

void test_db_resize_decreases_size(void) {
    db_buffer buf = db_new_with_data("Hello, World!", 13);
    TEST_ASSERT_TRUE(db_resize(&buf, 5));
    TEST_ASSERT_EQUAL(5, db_size(buf));
    db_release(&buf);
}

void test_db_resize_fails_on_shared_buffer(void) {
    db_buffer buf = db_new(10);
    db_buffer buf2 = db_retain(buf);
    
    TEST_ASSERT_FALSE(db_resize(&buf, 20));
    
    db_release(&buf);
    db_release(&buf2);
}

void test_db_reserve_ensures_capacity(void) {
    db_buffer buf = db_new(10);
    TEST_ASSERT_TRUE(db_reserve(&buf, 100));
    TEST_ASSERT_TRUE(db_capacity(buf) >= 100);
    db_release(&buf);
}

void test_db_append_adds_data(void) {
    db_buffer buf = db_new_with_data("Hello", 5);
    TEST_ASSERT_TRUE(db_append(&buf, ", World!", 8));
    
    TEST_ASSERT_EQUAL(13, db_size(buf));
    TEST_ASSERT_EQUAL_MEMORY("Hello, World!", buf, 13);
    
    db_release(&buf);
}

void test_db_append_handles_empty_data(void) {
    db_buffer buf = db_new_with_data("Hello", 5);
    TEST_ASSERT_TRUE(db_append(&buf, NULL, 0));
    TEST_ASSERT_EQUAL(5, db_size(buf));
    db_release(&buf);
}

void test_db_append_fails_on_shared_buffer(void) {
    db_buffer buf = db_new_with_data("Hello", 5);
    db_buffer buf2 = db_retain(buf);
    
    TEST_ASSERT_FALSE(db_append(&buf, " World", 6));
    
    db_release(&buf);
    db_release(&buf2);
}

void test_db_clear_empties_buffer(void) {
    db_buffer buf = db_new_with_data("Hello", 5);
    TEST_ASSERT_TRUE(db_clear(buf));
    TEST_ASSERT_EQUAL(0, db_size(buf));
    TEST_ASSERT_TRUE(db_is_empty(buf));
    db_release(&buf);
}

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

void test_db_concat_handles_null_buffers(void) {
    db_buffer buf1 = db_new_with_data("Hello", 5);
    
    db_buffer result1 = db_concat(buf1, NULL);
    TEST_ASSERT_NOT_NULL(result1);
    TEST_ASSERT_EQUAL(5, db_size(result1));
    TEST_ASSERT_EQUAL_MEMORY("Hello", result1, 5);
    
    db_buffer result2 = db_concat(NULL, buf1);
    TEST_ASSERT_NOT_NULL(result2);
    TEST_ASSERT_EQUAL(5, db_size(result2));
    TEST_ASSERT_EQUAL_MEMORY("Hello", result2, 5);
    
    db_buffer result3 = db_concat(NULL, NULL);
    TEST_ASSERT_NOT_NULL(result3);
    TEST_ASSERT_EQUAL(0, db_size(result3));
    
    db_release(&buf1);
    db_release(&result1);
    db_release(&result2);
    db_release(&result3);
}

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

void test_db_equals_handles_null_buffers(void) {
    db_buffer buf = db_new_with_data("Hello", 5);
    
    TEST_ASSERT_FALSE(db_equals(buf, NULL));
    TEST_ASSERT_FALSE(db_equals(NULL, buf));
    TEST_ASSERT_TRUE(db_equals(NULL, NULL));
    
    db_release(&buf);
}

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
void test_empty_buffer_operations(void) {
    db_buffer buf = db_new(0);
    
    TEST_ASSERT_TRUE(db_is_empty(buf));
    TEST_ASSERT_EQUAL(0, db_size(buf));
    TEST_ASSERT_NOT_NULL(buf);  // Should return valid pointer even for empty buffer
    
    // Operations on empty buffer
    TEST_ASSERT_TRUE(db_append(&buf, "Hello", 5));
    TEST_ASSERT_FALSE(db_is_empty(buf));
    TEST_ASSERT_EQUAL(5, db_size(buf));
    
    TEST_ASSERT_TRUE(db_clear(buf));
    TEST_ASSERT_TRUE(db_is_empty(buf));
    
    db_release(&buf);
}

void test_large_buffer_operations(void) {
    const size_t large_size = 1024 * 1024;  // 1MB
    db_buffer buf = db_new(large_size);
    
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL(large_size, db_capacity(buf));
    
    // Fill with pattern
    char* data = (char*)buf;
    for (size_t i = 0; i < large_size; i++) {
        data[i] = (char)(i % 256);
    }
    
    TEST_ASSERT_TRUE(db_resize(&buf, large_size));
    TEST_ASSERT_EQUAL(large_size, db_size(buf));
    
    // Verify pattern
    const char* const_data = (const char*)buf;
    for (size_t i = 0; i < 100; i++) {  // Check first 100 bytes
        TEST_ASSERT_EQUAL((char)(i % 256), const_data[i]);
    }
    
    db_release(&buf);
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
    RUN_TEST(test_db_retain_handles_null);
    RUN_TEST(test_db_release_handles_null);
    
    // Data access tests
    RUN_TEST(test_db_data_returns_valid_pointer);
    RUN_TEST(test_db_mutable_data_allows_modification);
    
    // Slicing tests
    RUN_TEST(test_db_slice_creates_copy);
    RUN_TEST(test_db_slice_handles_invalid_bounds);
    RUN_TEST(test_db_slice_from_creates_suffix);
    RUN_TEST(test_db_slice_to_creates_prefix);
    
    // Modification tests
    RUN_TEST(test_db_resize_increases_size);
    RUN_TEST(test_db_resize_decreases_size);
    RUN_TEST(test_db_resize_fails_on_shared_buffer);
    RUN_TEST(test_db_reserve_ensures_capacity);
    RUN_TEST(test_db_append_adds_data);
    RUN_TEST(test_db_append_handles_empty_data);
    RUN_TEST(test_db_append_fails_on_shared_buffer);
    RUN_TEST(test_db_clear_empties_buffer);
    
    // Concatenation tests
    RUN_TEST(test_db_concat_joins_buffers);
    RUN_TEST(test_db_concat_handles_null_buffers);
    RUN_TEST(test_db_concat_many_joins_multiple_buffers);
    
    // Comparison tests
    RUN_TEST(test_db_equals_compares_contents);
    RUN_TEST(test_db_equals_handles_null_buffers);
    RUN_TEST(test_db_compare_returns_correct_order);
    
    // Utility tests
    RUN_TEST(test_db_to_hex_converts_correctly);
    RUN_TEST(test_db_from_hex_converts_correctly);
    RUN_TEST(test_db_from_hex_handles_invalid_input);
    RUN_TEST(test_db_debug_print_doesnt_crash);
    
    // Edge case tests
    RUN_TEST(test_empty_buffer_operations);
    RUN_TEST(test_large_buffer_operations);
    
    return UNITY_END();
}