# util

Small classes shared by several projects, namespace `os3::util`.

## Contents

- `unique_id` - 64-bit snowflake id generator
- `sf_node` - process-wide snowflake node id, claimed once at startup
- `sf_number` - claims a free node id (0-1023) through an abstract unix socket
- `err_data` - error payload (`code`, `msg`) used with `std::expected`

## Build

```bash
cmake --preset ninja-debug && cmake --build build/debug && ctest --preset debug
```

Presets: `ninja-debug` (sanitizers), `ninja-release`, `ninja-profile`,
`ninja-coverage` (target `coverage`).

## Use

```cmake
CPMAddPackage(NAME util GITHUB_REPOSITORY ostri/util GIT_TAG main)
target_link_libraries(app PRIVATE os3::util)
```
