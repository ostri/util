# util

Small classes shared by several projects, namespace `os3::util`.

## Contents

- `unique_id` - 64-bit snowflake id generator
- `sf_node` - process-wide snowflake node id, claimed once at startup
- `sf_number` - claims a free node id (0-1023) through an abstract unix socket
- `t2_calendar` - Target 2 (T2) business days
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

## T2 working days

`t2_calendar` follows the T2 calendar: Saturdays, Sundays, 1 January,
Good Friday, Easter Monday, 1 May, 25 December and 26 December are
closed. National holidays are not considered.

```cpp
#include "os3/util/t2-calendar.hpp"
#include <fmt/chrono.h>
#include <chrono>

using namespace std::chrono;
using os3::util::t2_calendar;

const auto may_day = year{2026} / May / 1;
const bool open    = t2_calendar::is_business_day(may_day); // false

const auto start  = year{2026} / October / 8;
const auto target = t2_calendar::add_business_days(start, 15);
if (target)
  fmt::print("{}\n", *target); // 2026-10-29

const auto back = t2_calendar::add_business_days(start, -3);
```

`add_business_days` returns `std::expected<year_month_day, err_data>`;
an invalid input date gives `t2_err_code::invalid_date`. A negative
count moves backwards, zero returns the date unchanged (even if it is
not a business day).
