```markdown
# CoreKit

Header-only C++17 data analysis library in the style of Pandas. No external dependencies, no CMake requirements.

---

## Quick Start (Task Manager in 1 Minute)

Copy the following snippet into `main.cpp` alongside `CoreKit.hpp` and build:

```bash
g++ -std=c++17 -pthread main.cpp -o core_app
./core_app

```

```cpp
#include <iostream>
#include "CoreKit.hpp"

int main() {
    CoreKit::Table tasks;
    tasks.addColumn("ID");
    tasks.addColumn("Task");
    tasks.addColumn("Priority");
    tasks.addColumn("Completed");

    tasks.addRow({1, "Write custom OS bootloader", 3, false});
    tasks.addRow({2, "Produce breakcore track", 2, false});
    tasks.addRow({3, "Fix FORScan hex bytes", 3, true});

    std::cout << "--- All Tasks ---" << std::endl;
    tasks.print();

    CoreKit::Table active = tasks.filter([](const auto& row) {
        return row.get<bool>("Completed") == false;
    });
    active.sortBy("Priority", false);

    std::cout << "\n--- Active Tasks by Priority ---" << std::endl;
    active.print();

    return 0;
}

```

---

## Key Advantages

* **Zero External Dependencies**: Operates strictly on the C++17 standard library (STL). No Eigen, Boost, or third-party crates required.
* **Header-Only Integration**: Just drop `CoreKit.hpp` into your project source directory and start using it immediately without CMake or build configuration tweaks.
* **Heterogeneous Data Storage**: Store mixed types within the same table column via `std::variant<std::monostate, int, double, bool, std::string>`.
* **Built-in Analytical Tools**: Out-of-the-box support for lambda filtering, sorting, grouping with aggregations, window functions, and statistical analysis.
* **Native File I/O**: Parse and serialize tables directly to and from CSV and JSON formats without external parsing libraries.

---

## Practical Guide & Features

CoreKit gives you a simple, intuitive dataframe engine without forcing you to deal with third-party dependencies or complex build configurations. Here is a breakdown of what you can do with it.

### 1. Creating Tables & Adding Data

Define your structure with columns and insert rows containing mixed types (integers, doubles, booleans, and strings). Missing values can be represented using `std::monostate{}` or `"NA"`.

```cpp
CoreKit::Table df;
df.addColumn("Name");
df.addColumn("Age");
df.addColumn("Score");

df.addRow({"Alice", 24, 91.5});
df.addRow({"Bob", 30, std::monostate{}});

```

### 2. Filtering Rows with Custom Rules

Pass a simple lambda function to filter your table. The `RowProxy` object grants clean, type-safe access to column data by name.

```cpp
auto highScorers = df.filter([](const auto& row) {
    return row.get<double>("Score") > 90.0;
});

```

### 3. Sorting & Unique Values

Order your dataset by any column in ascending or descending order, or retrieve distinct elements and their frequency counts.

```cpp
df.sortBy("Score", false);
std::vector<CoreKit::Value> uniqueNames = df.unique("Name");
std::map<CoreKit::Value, size_t> counts = df.valueCounts("Department");

```

### 4. Grouping & Aggregating Data

Split dataset rows by categorical keys and calculate aggregate metrics like sum or mean for specific numeric target columns.

```cpp
CoreKit::Table deptSalaries = df.groupBy("Department").mean("Salary");
CoreKit::Table regionalSales = df.groupBy("Region").sum("Sales");

```

### 5. Statistics & Multithreading

Compute descriptive statistics instantly, or use multithreaded processing for large datasets.

```cpp
double avg = df.mean("Score");
double med = df.median("Score");
double var = df.variance("Score");
double stdDev = df.standardDeviation("Score");
double parallelAvg = df.parallelMean("LargeSensorData");

```

### 6. Time-Series & Window Functions

Analyze sequential or time-series data using sliding window calculations in linear time.

```cpp
std::vector<double> rSum = df.rollingSum("Temperature", 3);
std::vector<double> rMean = df.rollingMean("Temperature", 3);

```

### 7. Correlation Analysis

Find linear relationships between two numeric columns using Pearson correlation coefficient.

```cpp
double correlation = df.pearsonCorrelation("StudyHours", "ExamScore");

```

### 8. Cleaning Up Messy Datasets

Fix missing cells or remove statistical noise from datasets with minimal effort.

```cpp
df.fillMissingWithMean("Score");
df.removeOutliers("Score");

```

### 9. Working with Files (CSV & JSON)

Read datasets directly from disk or save processed analytical results for external tools.

```cpp
CoreKit::Table data;
data.fromCSV("dataset.csv");
data.toCSV("output.csv");
data.toJSON("output.json");

```

---

## License

Distributed under the MIT License. Feel free to use, modify, and distribute it in your own projects.

```

```
