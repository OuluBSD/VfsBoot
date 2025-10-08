/*
 * Test shared library for mount.lib functionality
 * Demonstrates various function signatures and types
 */

#include <stdio.h>
#include <string.h>

// Simple function returning int
int add_numbers(int a, int b) {
    return a + b;
}

// Function with no parameters
int get_magic_number(void) {
    return 42;
}

// Function returning float
float multiply_floats(float a, float b) {
    return a * b;
}

// Function with string parameter
int string_length(const char* str) {
    if (!str) return 0;
    return (int)strlen(str);
}

// Function with pointer parameter
void fill_array(int* arr, int size, int value) {
    if (!arr) return;
    for (int i = 0; i < size; i++) {
        arr[i] = value;
    }
}

// Function returning string (const char*)
const char* get_greeting(void) {
    return "Hello from test library!";
}

// Function with multiple types
double calculate(int x, float y, double z) {
    return (double)x + (double)y + z;
}

// Boolean-like function
int is_even(int n) {
    return (n % 2) == 0;
}

// Void function (side effect only)
void print_message(const char* msg) {
    if (msg) {
        printf("Library says: %s\n", msg);
    }
}

// Function with unsigned types
unsigned int factorial(unsigned int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}
