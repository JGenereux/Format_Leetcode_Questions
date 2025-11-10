# Testing Guide

## Overview

This project includes a comprehensive test suite using Google Test (gtest) to verify the functionality of the utility functions.

## Test Coverage

The test suite includes 22 tests across 3 test suites:

### FormatHTMLToStringTest (11 tests)
- HTML tag removal (simple and nested)
- HTML entity conversion (&lt;, &gt;, &amp;, &#39;s, &nbsp;)
- Whitespace handling (newlines and tabs)
- Edge cases (empty strings, complex HTML)

### GetTestCasesTest (5 tests)
- Single and multiple test case parsing
- Empty content handling
- Content without examples
- Single and multiple parameter parsing

### GetParamNameTest (6 tests)
- Parameter name and value extraction
- Various formatting (with/without spaces)
- Complex array parameters
- String parameters

## Building and Running Tests

### Prerequisites

```bash
sudo apt-get install libgtest-dev libcurl4-openssl-dev nlohmann-json3-dev cmake g++
```

### Build

```bash
mkdir -p build
cd build
cmake ..
make
```

### Run Tests

There are two ways to run the tests:

1. **Direct execution:**
```bash
cd build
./test_main
```

2. **Using CTest:**
```bash
cd build
ctest --verbose
```

## Test Results

All 22 tests currently pass:
- ✅ 11/11 FormatHTMLToStringTest
- ✅ 5/5 GetTestCasesTest  
- ✅ 6/6 GetParamNameTest

## Code Structure

- `test_main.cpp` - Test implementation using Google Test framework
- `utils.h` - Header file with function declarations
- `utils.cpp` - Implementation of utility functions
- `main.cpp` - Main application (uses the utilities)

## Bug Fixes

During test development, we identified and fixed boundary check issues in `FormatHTMLToString()` where HTML entities at the end of strings were not being processed correctly.
