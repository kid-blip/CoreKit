// ==========================================
// BLOCK 1: Includes, Namespace, and Type Aliases
// ==========================================
#ifndef CORE_KIT_HPP
#define CORE_KIT_HPP

#include <vector>
#include <string>
#include <string_view>
#include <variant>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <thread>
#include <future>
#include <iomanip>

namespace CoreKit {

// Heterogeneous cell type supporting multiple standard types and empty states (NA)
using Value = std::variant<std::monostate, int, double, bool, std::string>;

// Forward declarations
class Table;
class GroupByProxy;


// ==========================================
// BLOCK 2: Column Structure Definition
// ==========================================
struct Column {
    std::string name;
    std::vector<Value> data;
};


// ==========================================
// BLOCK 3: Table Class - Setup & Row/Column Modifiers
// ==========================================
class Table {
public:
    Table() : row_count_(0) {}

    void addColumn(std::string_view name) {
        columns_.push_back({std::string(name), std::vector<Value>(row_count_, std::monostate{})});
    }

    void addRow(std::initializer_list<Value> values) {
        size_t colIdx = 0;
        for (const auto& val : values) {
            if (colIdx >= columns_.size()) {
                columns_.push_back({"Col_" + std::to_string(colIdx), std::vector<Value>(row_count_, std::monostate{})});
            }
            columns_[colIdx].data.push_back(val);
            colIdx++;
        }
        for (size_t i = colIdx; i < columns_.size(); ++i) {
            columns_[i].data.push_back(std::monostate{});
        }
        row_count_++;
    }

    void addRowVector(const std::vector<Value>& values) {
        size_t colIdx = 0;
        for (const auto& val : values) {
            if (colIdx >= columns_.size()) {
                columns_.push_back({"Col_" + std::to_string(colIdx), std::vector<Value>(row_count_, std::monostate{})});
            }
            columns_[colIdx].data.push_back(val);
            colIdx++;
        }
        for (size_t i = colIdx; i < columns_.size(); ++i) {
            columns_[i].data.push_back(std::monostate{});
        }
        row_count_++;
    }

    size_t rowCount() const { return row_count_; }
    size_t columnCount() const { return columns_.size(); }

    size_t findColumnIndex(std::string_view colName) const {
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (columns_[i].name == colName) return i;
        }
        throw std::runtime_error("Column not found: " + std::string(colName));
    }


    // ==========================================
    // BLOCK 4: Table Class - Sorting, Filtering & Distinct Values
    // ==========================================

    void sortBy(std::string_view colName, bool ascending = true) {
        size_t colIdx = findColumnIndex(colName);
        std::vector<size_t> indices(row_count_);
        std::iota(indices.begin(), indices.end(), 0);

        const auto& colData = columns_[colIdx].data;
        std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
            const auto& valA = colData[a];
            const auto& valB = colData[b];
            if (valA.index() != valB.index()) {
                return valA.index() < valB.index();
            }
            bool result = false;
            std::visit([&](const auto& vA) {
                using T = std::decay_t<decltype(vA)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    result = false;
                } else {
                    const auto& vB = std::get<T>(valB);
                    result = vA < vB;
                }
            }, valA);
            return ascending ? result : !result;
        });

        for (auto& col : columns_) {
            std::vector<Value> sorted_data;
            sorted_data.resize(row_count_);
            for (size_t i = 0; i < row_count_; ++i) {
                sorted_data[i] = std::move(col.data[indices[i]]);
            }
            col.data = std::move(sorted_data);
        }
    }

    template<typename Predicate>
    Table filter(Predicate pred) const;

    std::vector<Value> unique(std::string_view colName) const {
        size_t colIdx = findColumnIndex(colName);
        std::vector<Value> result;
        for (const auto& val : columns_[colIdx].data) {
            if (std::find(result.begin(), result.end(), val) == result.end()) {
                result.push_back(val);
            }
        }
        return result;
    }

    std::map<Value, size_t> valueCounts(std::string_view colName) const {
        size_t colIdx = findColumnIndex(colName);
        std::map<Value, size_t> counts;
        for (const auto& val : columns_[colIdx].data) {
            counts[val]++;
        }
        return counts;
    }

    GroupByProxy groupBy(std::string_view colName);


    // ==========================================
    // BLOCK 5: Table Class - Statistics, Multithreading & Window Functions
    // ==========================================

    double mean(std::string_view colName) const {
        size_t colIdx = findColumnIndex(colName);
        double sum = 0.0;
        size_t count = 0;
        for (const auto& val : columns_[colIdx].data) {
            if (std::holds_alternative<int>(val)) {
                sum += std::get<int>(val);
                count++;
            } else if (std::holds_alternative<double>(val)) {
                sum += std::get<double>(val);
                count++;
            }
        }
        if (count == 0) return 0.0;
        return sum / count;
    }

    double sum(std::string_view colName) const {
        size_t colIdx = findColumnIndex(colName);
        double total = 0.0;
        for (const auto& val : columns_[colIdx].data) {
            if (std::holds_alternative<int>(val)) {
                total += std::get<int>(val);
            } else if (std::holds_alternative<double>(val)) {
                total += std::get<double>(val);
            }
        }
        return total;
    }

    double median(std::string_view colName) const {
        size_t colIdx = findColumnIndex(colName);
        std::vector<double> vals;
        for (const auto& val : columns_[colIdx].data) {
            if (std::holds_alternative<int>(val)) vals.push_back(std::get<int>(val));
            else if (std::holds_alternative<double>(val)) vals.push_back(std::get<double>(val));
        }
        if (vals.empty()) return 0.0;
        std::sort(vals.begin(), vals.end());
        size_t mid = vals.size() / 2;
        if (vals.size() % 2 == 0) {
            return (vals[mid - 1] + vals[mid]) / 2.0;
        }
        return vals[mid];
    }

    double variance(std::string_view colName) const {
        double m = mean(colName);
        size_t colIdx = findColumnIndex(colName);
        double sq_sum = 0.0;
        size_t count = 0;
        for (const auto& val : columns_[colIdx].data) {
            double v = 0.0;
            bool valid = false;
            if (std::holds_alternative<int>(val)) { v = std::get<int>(val); valid = true; }
            else if (std::holds_alternative<double>(val)) { v = std::get<double>(val); valid = true; }
            if (valid) {
                sq_sum += (v - m) * (v - m);
                count++;
            }
        }
        if (count <= 1) return 0.0;
        return sq_sum / (count - 1);
    }

    double standardDeviation(std::string_view colName) const {
        return std::sqrt(variance(colName));
    }

    double parallelMean(std::string_view colName) const {
        size_t colIdx = findColumnIndex(colName);
        const auto& data = columns_[colIdx].data;
        if (data.empty()) return 0.0;

        unsigned int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 2;
        if (data.size() < 1000 || num_threads == 1) {
            return mean(colName);
        }

        size_t chunk_size = data.size() / num_threads;
        std::vector<std::future<std::pair<double, size_t>>> futures;

        for (unsigned int i = 0; i < num_threads; ++i) {
            size_t start = i * chunk_size;
            size_t end = (i == num_threads - 1) ? data.size() : start + chunk_size;
            futures.push_back(std::async(std::launch::async, [&data, start, end]() {
                double local_sum = 0.0;
                size_t local_count = 0;
                for (size_t j = start; j < end; ++j) {
                    const auto& val = data[j];
                    if (std::holds_alternative<int>(val)) {
                        local_sum += std::get<int>(val);
                        local_count++;
                    } else if (std::holds_alternative<double>(val)) {
                        local_sum += std::get<double>(val);
                        local_count++;
                    }
                }
                return std::make_pair(local_sum, local_count);
            }));
        }

        double total_sum = 0.0;
        size_t total_count = 0;
        for (auto& f : futures) {
            auto res = f.get();
            total_sum += res.first;
            total_count += res.second;
        }

        if (total_count == 0) return 0.0;
        return total_sum / total_count;
    }

    std::vector<double> rollingSum(std::string_view colName, size_t window) const {
        size_t colIdx = findColumnIndex(colName);
        const auto& data = columns_[colIdx].data;
        std::vector<double> result;
        if (window == 0 || data.empty()) return result;

        double current_sum = 0.0;
        for (size_t i = 0; i < data.size(); ++i) {
            double val = 0.0;
            bool valid = false;
            if (std::holds_alternative<int>(data[i])) { val = std::get<int>(data[i]); valid = true; }
            else if (std::holds_alternative<double>(data[i])) { val = std::get<double>(data[i]); valid = true; }

            if (valid) {
                current_sum += val;
            }
            if (i >= window) {
                double old_val = 0.0;
                if (std::holds_alternative<int>(data[i - window])) old_val = std::get<int>(data[i - window]);
                else if (std::holds_alternative<double>(data[i - window])) old_val = std::get<double>(data[i - window]);
                current_sum -= old_val;
            }
            result.push_back(current_sum);
        }
        return result;
    }

    std::vector<double> rollingMean(std::string_view colName, size_t window) const {
        auto sums = rollingSum(colName, window);
        std::vector<double> result;
        result.reserve(sums.size());
        for (size_t i = 0; i < sums.size(); ++i) {
            size_t current_window = (i + 1 < window) ? (i + 1) : window;
            result.push_back(sums[i] / current_window);
        }
        return result;
    }

    double pearsonCorrelation(std::string_view col1, std::string_view col2) const {
        size_t idx1 = findColumnIndex(col1);
        size_t idx2 = findColumnIndex(col2);
        const auto& d1 = columns_[idx1].data;
        const auto& d2 = columns_[idx2].data;

        size_t n = std::min(d1.size(), d2.size());
        double sum1 = 0, sum2 = 0, sum1Sq = 0, sum2Sq = 0, pSum = 0;
        size_t count = 0;

        for (size_t i = 0; i < n; ++i) {
            double v1 = 0, v2 = 0;
            bool ok1 = false, ok2 = false;
            if (std::holds_alternative<int>(d1[i])) { v1 = std::get<int>(d1[i]); ok1 = true; }
            else if (std::holds_alternative<double>(d1[i])) { v1 = std::get<double>(d1[i]); ok1 = true; }

            if (std::holds_alternative<int>(d2[i])) { v2 = std::get<int>(d2[i]); ok2 = true; }
            else if (std::holds_alternative<double>(d2[i])) { v2 = std::get<double>(d2[i]); ok2 = true; }

            if (ok1 && ok2) {
                sum1 += v1;
                sum2 += v2;
                sum1Sq += v1 * v1;
                sum2Sq += v2 * v2;
                pSum += v1 * v2;
                count++;
            }
        }

        if (count == 0) return 0.0;
        double num = pSum - (sum1 * sum2 / count);
        double den = std::sqrt((sum1Sq - (sum1 * sum1 / count)) * (sum2Sq - (sum2 * sum2 / count)));
        if (den == 0) return 0.0;
        return num / den;
    }

    void fillMissingWithMean(std::string_view colName) {
        size_t colIdx = findColumnIndex(colName);
        double m = mean(colName);
        for (auto& val : columns_[colIdx].data) {
            if (std::holds_alternative<std::monostate>(val)) {
                val = m;
            }
        }
    }

    void removeOutliers(std::string_view colName) {
        size_t colIdx = findColumnIndex(colName);
        std::vector<double> vals;
        for (const auto& val : columns_[colIdx].data) {
            if (std::holds_alternative<int>(val)) vals.push_back(std::get<int>(val));
            else if (std::holds_alternative<double>(val)) vals.push_back(std::get<double>(val));
        }
        if (vals.size() < 4) return;
        std::sort(vals.begin(), vals.end());
        double q1 = vals[vals.size() / 4];
        double q3 = vals[(vals.size() * 3) / 4];
        double iqr = q3 - q1;
        double lower = q1 - 1.5 * iqr;
        double upper = q3 + 1.5 * iqr;

        std::vector<size_t> valid_indices;
        for (size_t i = 0; i < row_count_; ++i) {
            const auto& val = columns_[colIdx].data[i];
            bool keep = true;
            if (std::holds_alternative<int>(val)) {
                double v = std::get<int>(val);
                if (v < lower || v > upper) keep = false;
            } else if (std::holds_alternative<double>(val)) {
                double v = std::get<double>(val);
                if (v < lower || v > upper) keep = false;
            }
            if (keep) valid_indices.push_back(i);
        }

        for (auto& col : columns_) {
            std::vector<Value> filtered_data;
            filtered_data.resize(valid_indices.size());
            for (size_t i = 0; i < valid_indices.size(); ++i) {
                filtered_data[i] = std::move(col.data[valid_indices[i]]);
            }
            col.data = std::move(filtered_data);
        }
        row_count_ = valid_indices.size();
    }


    // ==========================================
    // BLOCK 6: Table Class - File I/O & Terminal Printing
    // ==========================================

    void fromCSV(std::string_view filename) {
        std::ifstream file{std::string(filename)};
        if (!file.is_open()) throw std::runtime_error("Could not open file for reading");
        std::string line;
        if (!std::getline(file, line)) return;

        columns_.clear();
        row_count_ = 0;

        std::stringstream ss(line);
        std::string colName;
        while (std::getline(ss, colName, ',')) {
            addColumn(colName);
        }

        while (std::getline(file, line)) {
            std::stringstream row_ss(line);
            std::string cell;
            std::vector<Value> rowVals;
            while (std::getline(row_ss, cell, ',')) {
                if (cell.empty() || cell == "NA") {
                    rowVals.push_back(std::monostate{});
                } else {
                    try {
                        size_t idx = 0;
                        int iVal = std::stoi(cell, &idx);
                        if (idx == cell.size()) {
                            rowVals.push_back(iVal);
                        } else {
                            double dVal = std::stod(cell, &idx);
                            if (idx == cell.size()) {
                                rowVals.push_back(dVal);
                            } else {
                                rowVals.push_back(cell);
                            }
                        }
                    } catch (...) {
                        rowVals.push_back(cell);
                    }
                }
            }
            addRowVector(rowVals);
        }
    }

    void toCSV(std::string_view filename) const {
        std::ofstream file{std::string(filename)};
        if (!file.is_open()) throw std::runtime_error("Could not open file for writing");

        for (size_t i = 0; i < columns_.size(); ++i) {
            file << columns_[i].name;
            if (i + 1 < columns_.size()) file << ",";
        }
        file << "\n";

        for (size_t r = 0; r < row_count_; ++r) {
            for (size_t c = 0; c < columns_.size(); ++c) {
                std::visit([&file](const auto& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, std::monostate>) {
                        file << "NA";
                    } else if constexpr (std::is_same_v<T, bool>) {
                        file << (arg ? "true" : "false");
                    } else {
                        file << arg;
                    }
                }, columns_[c].data[r]);
                if (c + 1 < columns_.size()) file << ",";
            }
            file << "\n";
        }
    }

    void toJSON(std::string_view filename) const {
        std::ofstream file{std::string(filename)};
        if (!file.is_open()) throw std::runtime_error("Could not open file for writing");

        file << "[\n";
        for (size_t r = 0; r < row_count_; ++r) {
            file << "  {\n";
            for (size_t c = 0; c < columns_.size(); ++c) {
                file << "    \"" << columns_[c].name << "\": ";
                std::visit([&file](const auto& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, std::monostate>) {
                        file << "null";
                    } else if constexpr (std::is_same_v<T, bool>) {
                        file << (arg ? "true" : "false");
                    } else if constexpr (std::is_same_v<T, std::string>) {
                        file << "\"" << arg << "\"";
                    } else {
                        file << arg;
                    }
                }, columns_[c].data[r]);
                if (c + 1 < columns_.size()) file << ",";
                file << "\n";
            }
            file << "  }" << (r + 1 < row_count_ ? "," : "") << "\n";
        }
        file << "]\n";
    }

    void print() const {
        if (columns_.empty()) {
            std::cout << "[Empty Table]\n";
            return;
        }

        for (size_t i = 0; i < columns_.size(); ++i) {
            std::cout << std::setw(15) << columns_[i].name << " | ";
        }
        std::cout << "\n";

        for (size_t i = 0; i < columns_.size(); ++i) {
            std::cout << std::string(17, '-');
        }
        std::cout << "\n";

        for (size_t r = 0; r < row_count_; ++r) {
            for (size_t c = 0; c < columns_.size(); ++c) {
                std::cout << std::setw(15);
                std::visit([](const auto& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, std::monostate>) {
                        std::cout << "NA";
                    } else if constexpr (std::is_same_v<T, bool>) {
                        if (arg) std::cout << "true"; else std::cout << "false";
                    } else {
                        std::cout << arg;
                    }
                }, columns_[c].data[r]);
                std::cout << " | ";
            }
            std::cout << "\n";
        }
    }

    std::vector<Column> columns_;
    size_t row_count_;
};


// ==========================================
// BLOCK 7: RowProxy Class (Type-Safe Row Access)
// ==========================================
class RowProxy {
public:
    RowProxy(const Table& table, size_t rowIndex) : table_(table), rowIndex_(rowIndex) {}

    template<typename T>
    T get(std::string_view colName) const {
        size_t colIdx = table_.findColumnIndex(colName);
        const auto& val = table_.columns_[colIdx].data[rowIndex_];
        if constexpr (std::is_same_v<T, int>) {
            if (std::holds_alternative<int>(val)) return std::get<int>(val);
            if (std::holds_alternative<double>(val)) return static_cast<int>(std::get<double>(val));
        } else if constexpr (std::is_same_v<T, double>) {
            if (std::holds_alternative<double>(val)) return std::get<double>(val);
            if (std::holds_alternative<int>(val)) return static_cast<double>(std::get<int>(val));
        } else if constexpr (std::is_same_v<T, bool>) {
            if (std::holds_alternative<bool>(val)) return std::get<bool>(val);
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (std::holds_alternative<std::string>(val)) return std::get<std::string>(val);
        }
        throw std::runtime_error("Type mismatch or unsupported type in RowProxy::get");
    }

private:
    const Table& table_;
    size_t rowIndex_;
};


// ==========================================
// BLOCK 8: GroupByProxy Class (Aggregation Support)
// ==========================================
class GroupByProxy {
public:
    GroupByProxy(Table& table, std::string_view groupCol) : table_(table), groupCol_(groupCol) {}

    Table mean(std::string_view targetCol) const {
        std::unordered_map<Value, std::pair<double, size_t>> groups;
        size_t groupColIdx = table_.findColumnIndex(groupCol_);
        size_t targetColIdx = table_.findColumnIndex(targetCol);

        for (size_t i = 0; i < table_.rowCount(); ++i) {
            Value key = table_.columns_[groupColIdx].data[i];
            const auto& val = table_.columns_[targetColIdx].data[i];
            double v = 0.0;
            bool valid = false;
            if (std::holds_alternative<int>(val)) { v = std::get<int>(val); valid = true; }
            else if (std::holds_alternative<double>(val)) { v = std::get<double>(val); valid = true; }

            if (valid) {
                groups[key].first += v;
                groups[key].second += 1;
            }
        }

        Table result;
        result.addColumn(groupCol_);
        result.addColumn(std::string(targetCol) + "_mean");

        for (const auto& [key, p] : groups) {
            if (p.second > 0) {
                result.addRowVector({key, p.first / p.second});
            }
        }
        return result;
    }

    Table sum(std::string_view targetCol) const {
        std::unordered_map<Value, double> groups;
        size_t groupColIdx = table_.findColumnIndex(groupCol_);
        size_t targetColIdx = table_.findColumnIndex(targetCol);

        for (size_t i = 0; i < table_.rowCount(); ++i) {
            Value key = table_.columns_[groupColIdx].data[i];
            const auto& val = table_.columns_[targetColIdx].data[i];
            double v = 0.0;
            bool valid = false;
            if (std::holds_alternative<int>(val)) { v = std::get<int>(val); valid = true; }
            else if (std::holds_alternative<double>(val)) { v = std::get<double>(val); valid = true; }

            if (valid) {
                groups[key] += v;
            }
        }

        Table result;
        result.addColumn(groupCol_);
        result.addColumn(std::string(targetCol) + "_sum");

        for (const auto& [key, total] : groups) {
            result.addRowVector({key, total});
        }
        return result;
    }

private:
    Table& table_;
    std::string groupCol_;
};


// ==========================================
// BLOCK 9: Deferred Inline Method Implementations
// ==========================================
template<typename Predicate>
inline Table Table::filter(Predicate pred) const {
    Table result;
    for (const auto& col : columns_) {
        result.addColumn(col.name);
    }

    for (size_t i = 0; i < row_count_; ++i) {
        RowProxy proxy(*this, i);
        if (pred(proxy)) {
            std::vector<Value> rowVals;
            rowVals.reserve(columns_.size());
            for (const auto& col : columns_) {
                rowVals.push_back(col.data[i]);
            }
            result.addRowVector(rowVals);
        }
    }
    return result;
}

inline GroupByProxy Table::groupBy(std::string_view colName) {
    findColumnIndex(colName);
    return GroupByProxy(*this, colName);
}

} // namespace CoreKit

#endif // CORE_KIT_HPP
