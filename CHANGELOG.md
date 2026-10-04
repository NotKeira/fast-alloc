# Changelog

## 1.2.0

### Added

- Configurable alignment for pool and thread-safe pool blocks, with padded strides
  to keep every block aligned. The default alignment is `alignof(std::max_align_t)`.
- Reproducible mixed-operation regression coverage for both free-list strategies,
  including payload integrity, accounting and full-capacity reuse.

### Fixed

- Keep free-list metadata aligned when splitting blocks and preserve capacity
  when consuming an unsplittable remainder.
- Guard allocator size and alignment calculations against arithmetic overflow.
- Centralise aligned backing-memory ownership and use the appropriate allocation
  and deallocation functions on Windows and other platforms.
- Validate invalid constructor parameters and allocation requests consistently in
  Debug and Release builds.
- Support CMake consumers using `add_subdirectory()`, export C++20 and threading
  requirements, and apply Release flags with multi-configuration generators.
- Keep project warning and optimisation flags out of fetched dependencies.
- Build fetched Catch2 with C++20 so its formatters match the test headers on
  platforms whose compilers default to an older language standard.
- Run concurrent pool assertions after worker threads join to avoid intermittent
  Catch2 failures.

### Changed

- Make native CPU optimisation opt-in with `FAST_ALLOC_NATIVE_ARCH`, disabled by
  default, to avoid compiler feature-detection failures on hosted CI runners.
- Simplify mutex-protected pool bookkeeping while keeping atomic statistics reads.
- Use persistent workers sharing one pool in threaded benchmarks, with matching
  worker counts for the `new/delete` comparison.
- Exclude pool and pointer-buffer setup from bulk benchmark timings and count
  successful allocation/free pairs.
- Move Linux CI builds and sanitiser jobs to GitHub-hosted runners.
- Refresh project, usage and architecture documentation.

### Upgrade Notes

- Rebuild applications linking the static library: pool constructor signatures
  have changed to support the optional alignment parameter.
- Invalid inputs now throw `std::invalid_argument` in both Debug and Release.
  Valid requests that exhaust capacity still return `nullptr`.
- Threaded benchmark names now include `threads:N`. Bulk and frame timings use
  revised setup boundaries, so compare results using the same benchmark version.
