# Contributing to fast-alloc

Thank you for your interest in contributing to fast-alloc! This document provides guidelines and information for contributors.

## Code of Conduct

- Be respectful and constructive in all interactions
- Focus on what is best for the community and the project
- Show empathy towards other community members

## How to Contribute

### Reporting Bugs

If you find a bug, please create an issue with:
- A clear, descriptive title
- Steps to reproduce the issue
- Expected behavior vs actual behavior
- Your environment (OS, compiler, version)
- Minimal code example demonstrating the bug

### Suggesting Enhancements

Enhancement suggestions are welcome! Please include:
- Clear description of the enhancement
- Use cases and benefits
- Potential implementation approach (if you have ideas)
- Any trade-offs or considerations

### Pull Requests

1. **Fork and Branch**: Fork the repository and create a feature branch
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Code Style**:
   - Follow existing code style and conventions
   - Use C++20 features appropriately
   - Keep functions focused and reasonably sized
   - Add comments for complex algorithms
   - Use descriptive variable names

3. **Testing**:
   - Add tests for new features
   - Ensure all existing tests pass
   - Test on multiple platforms if possible (Windows, Linux, macOS)
   - Run with sanitizers to catch memory issues:
     ```bash
     # Address Sanitizer
     cmake -B build -DCMAKE_BUILD_TYPE=Debug \
       -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g"
     cmake --build build
     ctest --test-dir build --output-on-failure
     
     # Thread Sanitizer (for thread-safe allocators)
     cmake -B build -DCMAKE_BUILD_TYPE=Debug \
       -DCMAKE_CXX_FLAGS="-fsanitize=thread -g"
     cmake --build build
     TSAN_OPTIONS="second_deadlock_stack=1 suppressions=tsan.supp" \
       ctest --test-dir build --output-on-failure
     ```

4. **Documentation**:
   - Update README.md if adding new features
   - Add Doxygen comments for public APIs
   - Update docs/architecture.md for architectural changes
   - Include usage examples for new allocators

5. **Commit Messages**:
   - Use clear, descriptive commit messages
   - Start with a verb in imperative mood (e.g., "Add", "Fix", "Update")
   - Keep the first line under 72 characters
   - Add detailed explanation in the commit body if needed

6. **Pull Request Process**:
   - Ensure CI passes (all tests, sanitizers)
   - Update documentation as needed
   - Reference any related issues
   - Wait for review and address feedback

## Development Setup

### Prerequisites

- C++20 compatible compiler (GCC 10+, Clang 10+, MSVC 2019+)
- CMake 3.14 or higher
- Git

### Building

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Run tests
ctest --test-dir build --output-on-failure

# Run benchmarks
./build/alloc_benchmarks
```

### Project Structure

```
fast-alloc/
├── src/                      # Source code
│   ├── *_allocator.h/cpp    # Allocator implementations
├── tests/                    # Unit tests (Catch2)
├── benchmarks/               # Performance benchmarks (Google Benchmark)
├── docs/                     # Documentation
└── .github/workflows/        # CI configuration
```

## Code Guidelines

### Memory Safety

- Always validate pointers in debug builds using assertions
- Use RAII for resource management
- Avoid raw pointer arithmetic where alternatives exist
- Test with sanitizers (ASan, TSan, UBSan)

### Performance

- Profile before optimizing
- Benchmark any performance-related changes
- Document performance characteristics
- Consider cache locality and alignment

### Assertions

Use assertions for preconditions that should never fail in correct usage:
```cpp
assert(size > 0 && "Size must be positive");
assert(memory_ && "Allocator not initialized");
```

### Thread Safety

- Document thread-safety guarantees in headers
- Use appropriate synchronization primitives
- Test concurrent operations under ThreadSanitizer
- Consider lock-free alternatives for hot paths

## Testing Guidelines

### Unit Tests

- Test one thing per test case
- Use descriptive test names
- Cover edge cases (nullptr, zero size, exhaustion)
- Test move semantics for movable types
- Verify alignment requirements

### Benchmark Tests

- Compare against standard allocation (malloc/new)
- Test realistic usage patterns
- Run multiple iterations to reduce variance
- Document platform and compiler used

## Release Process

Maintainers will:
1. Review and merge pull requests
2. Update version numbers
3. Create release notes
4. Tag releases

## Questions?

Feel free to:
- Open an issue for questions
- Start a discussion for general topics
- Contact the maintainer directly

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
