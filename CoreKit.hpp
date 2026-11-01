#ifndef CORE_KIT_HPP
#define CORE_KIT_HPP

#include <iostream>
#include <string>
#include <vector>
#include <variant>
#include <map>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <thread>
#include <future>
#include <functional>
#include <stdexcept>

namespace CoreKit {

using Value = std::variant<std::monostate, int, double, bool, std::string>;

class Table {
private:
    // Структура Column должна быть объявлена первой для корректной видимости в RowProxy
    struct Column {
        std::string name;
        std::vector<Value> data;
    };

    std::vector<Column> columns_;
    size_t row_count_ = 0;

public:
    // Представление строки для доступа по имени колонок при предикатной фильтрации
    class RowProxy {
    private:
        const std::vector<Column>& columns_;
        size_t row_idx_;
    public:
        RowProxy(const std::vector<Column>& cols, size_t r) : columns_(cols), row_idx_(r) {}

        template<typename T>
        T get(const std::string& colName) const {
            for (const auto& col : columns_) {
                if (col.name == colName) {
                    if (row_idx_ >= col.data.size()) {
                        throw std::out_of_range("Row index out of range");
                    }
                    try {
                        return std::get<T>(col.data[row_idx_]);
                    } catch (...) {
                        throw std::runtime_error("Type mismatch or missing value (NA) in column: " + colName);
                    }
                }
            }
            throw std::out_of_range("Column not found: " + colName);
        }

        const Value& getVariant(const std::string& colName) const {
            for (const auto& col : columns_) {
                if (col.name == colName) {
                    return col.data[row_idx_];
                }
            }
            throw std::out_of_range("Column not found: " + colName);
        }
    };

private:
    int findCol(const std::string& name) const {
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (columns_[i].name == name) return static_cast<int>(i);
        }
        return -1;
    }

    std::vector<double> getNumericData(int col_idx) const {
        std::vector<double> result;
        for (const auto& val : columns_[col_idx].data) {
            if (std::holds_alternative<int>(val)) {
                result.push_back(static_cast<double>(std::get<int>(val)));
            } else if (std::holds_alternative<double>(val)) {
                result.push_back(std::get<double>(val));
            }
        }
        return result;
    }

    double getQuantile(std::vector<double>& sorted_nums, double q) const {
        if (sorted_nums.empty()) return 0.0;
        double pos = q * (sorted_nums.size() - 1);
        size_t idx = static_cast<size_t>(pos);
        double frac = pos - idx;
        if (idx + 1 < sorted_nums.size()) {
            return sorted_nums[idx] * (1.0 - frac) + sorted_nums[idx + 1] * frac;
        }
        return sorted_nums[idx];
    }

    Value parseCell(const std::string& str) const {
        if (str.empty() || str == "NA" || str == "null") {
            return std::monostate{};
        }
        if (str == "true") return true;
        if (str == "false") return false;

        try {
            size_t idx = 0;
            int i_val = std::stoi(str, &idx);
            if (idx == str.size()) return i_val;
        } catch (...) {}

        try {
            size_t idx = 0;
            double d_val = std::stod(str, &idx);
            if (idx == str.size()) return d_val;
        } catch (...) {}

        return str;
    }

public:
    void addColumn(const std::string& name) {
        columns_.push_back({name, {}});
    }

    void addRow(std::initializer_list<Value> values) {
        size_t i = 0;
        for (const auto& val : values) {
            columns_[i].data.push_back(val);
            ++i;
        }
        ++row_count_;
    }

    void addRowUnchecked(const std::vector<Value>& values) {
        for (size_t i = 0; i < columns_.size(); ++i) {
            columns_[i].data.push_back(values[i]);
        }
        ++row_count_;
    }

    size_t rowCount() const { return row_count_; }
    size_t colCount() const { return columns_.size(); }

    // Красивый вывод таблицы
    void print() const {
        for (const auto& col : columns_) {
            std::cout << std::setw(14) << col.name << " | ";
        }
        std::cout << "\n";
        for (size_t i = 0; i < columns_.size(); ++i) {
            std::cout << "---------------+--";
        }
        std::cout << "\n";

        for (size_t r = 0; r < row_count_; ++r) {
            for (const auto& col : columns_) {
                std::cout << std::setw(14);
                const auto& val = col.data[r];
                std::visit([](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, std::monostate>) {
                        std::cout << "NA";
                    } else if constexpr (std::is_same_v<T, bool>) {
                        std::cout << (arg ? "true" : "false");
                    } else {
                        std::cout << arg;
                    }
                }, val);
                std::cout << " | ";
            }
            std::cout << "\n";
        }
    }

    // --- Описательная статистика ---
    double mean(const std::string& colName) const {
        int idx = findCol(colName);
        if (idx == -1) return 0.0;
        auto nums = getNumericData(idx);
        if (nums.empty()) return 0.0;
        return std::accumulate(nums.begin(), nums.end(), 0.0) / nums.size();
    }

    // Умная многопоточность: авто-определение ядер и чанкинг через std::future
    double parallelMean(const std::string& colName) const {
        int idx = findCol(colName);
        if (idx == -1) return 0.0;
        
        auto nums = getNumericData(idx);
        if (nums.empty()) return 0.0;

        unsigned int hardware_threads = std::thread::hardware_concurrency();
        size_t num_threads = (hardware_threads > 0) ? hardware_threads : 2;

        if (nums.size() < 1000 || num_threads == 1) {
            return std::accumulate(nums.begin(), nums.end(), 0.0) / nums.size();
        }

        size_t chunk_size = nums.size() / num_threads;
        std::vector<std::future<double>> futures;

        for (size_t i = 0; i < num_threads; ++i) {
            size_t start = i * chunk_size;
            size_t end = (i == num_threads - 1) ? nums.size() : start + chunk_size;

            futures.push_back(std::async(std::launch::async, [start, end, &nums]() {
                double partial_sum = 0.0;
                for (size_t j = start; j < end; ++j) {
                    partial_sum += nums[j];
                }
                return partial_sum;
            }));
        }

        double total_sum = 0.0;
        for (auto& fut : futures) {
            total_sum += fut.get();
        }

        return total_sum / nums.size();
    }

    double median(const std::string& colName) const {
        int idx = findCol(colName);
        if (idx == -1) return 0.0;
        auto nums = getNumericData(idx);
        if (nums.empty()) return 0.0;

        std::sort(nums.begin(), nums.end());
        size_t n = nums.size();
        if (n % 2 == 0) {
            return (nums[n / 2 - 1] + nums[n / 2]) / 2.0;
        }
        return nums[n / 2];
    }

    double variance(const std::string& colName) const {
        int idx = findCol(colName);
        if (idx == -1) return 0.0;
        auto nums = getNumericData(idx);
        if (nums.size() <= 1) return 0.0;

        double m = mean(colName);
        double sq_sum = 0.0;
        for (double val : nums) {
            sq_sum += (val - m) * (val - m);
        }
        return sq_sum / (nums.size() - 1);
    }

    double standardDeviation(const std::string& colName) const {
        return std::sqrt(variance(colName));
    }

    // --- Очистка данных ---
    void fillMissing(const std::string& colName, double fallbackValue) {
        int idx = findCol(colName);
        if (idx == -1) return;

        for (auto& val : columns_[idx].data) {
            if (std::holds_alternative<std::monostate>(val)) {
                val = fallbackValue;
            }
        }
    }

    void fillMissingWithMean(const std::string& colName) {
        double m = mean(colName);
        fillMissing(colName, m);
    }

    void removeOutliers(const std::string& colName) {
        int idx = findCol(colName);
        if (idx == -1 || row_count_ == 0) return;

        auto nums = getNumericData(idx);
        if (nums.size() < 4) return;

        std::sort(nums.begin(), nums.end());
        double q1 = getQuantile(nums, 0.25);
        double q3 = getQuantile(nums, 0.75);
        double iqr = q3 - q1;

        double lower_bound = q1 - 1.5 * iqr;
        double upper_bound = q3 + 1.5 * iqr;

        std::vector<size_t> valid_indices;
        for (size_t r = 0; r < row_count_; ++r) {
            const auto& val = columns_[idx].data[r];
            double num_val = 0.0;
            bool is_valid_num = false;

            if (std::holds_alternative<int>(val)) {
                num_val = static_cast<double>(std::get<int>(val));
                is_valid_num = true;
            } else if (std::holds_alternative<double>(val)) {
                num_val = std::get<double>(val);
                is_valid_num = true;
            }

            if (!is_valid_num || (num_val >= lower_bound && num_val <= upper_bound)) {
                valid_indices.push_back(r);
            }
        }

        for (auto& col : columns_) {
            std::vector<Value> filtered_data;
            filtered_data.reserve(valid_indices.size());
            for (size_t r_idx : valid_indices) {
                filtered_data.push_back(col.data[r_idx]);
            }
            col.data = std::move(filtered_data);
        }
        row_count_ = valid_indices.size();
    }

    // --- Корреляционный анализ ---
    double pearsonCorrelation(const std::string& colName1, const std::string& colName2) const {
        int idx1 = findCol(colName1);
        int idx2 = findCol(colName2);
        if (idx1 == -1 || idx2 == -1) return 0.0;

        auto nums1 = getNumericData(idx1);
        auto nums2 = getNumericData(idx2);
        if (nums1.size() != nums2.size() || nums1.empty()) return 0.0;

        double m1 = mean(colName1);
        double m2 = mean(colName2);

        double numerator = 0.0;
        double denom1 = 0.0;
        double denom2 = 0.0;

        for (size_t i = 0; i < nums1.size(); ++i) {
            double diff1 = nums1[i] - m1;
            double diff2 = nums2[i] - m2;
            numerator += diff1 * diff2;
            denom1 += diff1 * diff1;
            denom2 += diff2 * diff2;
        }

        if (denom1 == 0.0 || denom2 == 0.0) return 0.0;
        return numerator / (std::sqrt(denom1) * std::sqrt(denom2));
    }

    // --- Оконные функции ---
    std::vector<double> rollingSum(const std::string& colName, size_t windowSize) const {
        int idx = findCol(colName);
        std::vector<double> result;
        if (idx == -1 || windowSize == 0) return result;

        auto nums = getNumericData(idx);
        if (nums.size() < windowSize) return result;

        result.reserve(nums.size() - windowSize + 1);
        double window_sum = 0.0;

        for (size_t i = 0; i < nums.size(); ++i) {
            window_sum += nums[i];
            if (i >= windowSize) {
                window_sum -= nums[i - windowSize];
            }
            if (i >= windowSize - 1) {
                result.push_back(window_sum);
            }
        }
        return result;
    }

    std::vector<double> rollingMean(const std::string& colName, size_t windowSize) const {
        std::vector<double> result;
        if (windowSize == 0) return result;
        
        auto sums = rollingSum(colName, windowSize);
        result.reserve(sums.size());
        for (double s : sums) {
            result.push_back(s / windowSize);
        }
        return result;
    }

    // --- Сортировка и фильтрация ---
    void sortBy(const std::string& colName, bool ascending = true) {
        int idx = findCol(colName);
        if (idx == -1 || row_count_ <= 1) return;

        std::vector<size_t> indices(row_count_);
        std::iota(indices.begin(), indices.end(), 0);

        const auto& col_data = columns_[idx].data;
        std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
            if (ascending) {
                return col_data[a] < col_data[b];
            } else {
                return col_data[b] < col_data[a];
            }
        });

        for (auto& col : columns_) {
            std::vector<Value> sorted_data;
            sorted_data.reserve(row_count_);
            for (size_t i : indices) {
                sorted_data.push_back(col.data[i]);
            }
            col.data = std::move(sorted_data);
        }
    }

    std::vector<Value> unique(const std::string& colName) const {
        int idx = findCol(colName);
        std::vector<Value> result;
        if (idx == -1) return result;

        for (const auto& val : columns_[idx].data) {
            if (std::find(result.begin(), result.end(), val) == result.end()) {
                result.push_back(val);
            }
        }
        return result;
    }

    Table filter(std::function<bool(const RowProxy&)> predicate) const {
        Table result;
        for (const auto& col : columns_) {
            result.addColumn(col.name);
        }

        for (size_t r = 0; r < row_count_; ++r) {
            RowProxy proxy(columns_, r);
            if (predicate(proxy)) {
                std::vector<Value> row_vals;
                for (const auto& col : columns_) {
                    row_vals.push_back(col.data[r]);
                }
                result.addRowUnchecked(row_vals);
            }
        }
        return result;
    }

    // --- GroupBy и Агрегация ---
    class GroupByResult {
    private:
        std::map<Value, std::vector<size_t>> groups_;
        const std::vector<Column>& columns_;
        std::string group_col_;

        const Value& getVal(const std::string& colName, size_t r) const {
            for (const auto& col : columns_) {
                if (col.name == colName) return col.data[r];
            }
            throw std::runtime_error("Column not found: " + colName);
        }

    public:
        GroupByResult(const std::vector<Column>& cols, const std::string& groupCol) 
            : columns_(cols), group_col_(groupCol) {
            int idx = -1;
            for (size_t i = 0; i < cols.size(); ++i) {
                if (cols[i].name == groupCol) { idx = static_cast<int>(i); break; }
            }
            if (idx == -1) return;
            for (size_t r = 0; r < cols[idx].data.size(); ++r) {
                groups_[cols[idx].data[r]].push_back(r);
            }
        }

        Table mean(const std::string& targetCol) const {
            Table res;
            res.addColumn(group_col_);
            res.addColumn(targetCol + "_mean");

            for (const auto& [key, indices] : groups_) {
                double sum = 0.0;
                size_t count = 0;
                for (size_t r : indices) {
                    const auto& val = getVal(targetCol, r);
                    if (std::holds_alternative<int>(val)) {
                        sum += std::get<int>(val);
                        count++;
                    } else if (std::holds_alternative<double>(val)) {
                        sum += std::get<double>(val);
                        count++;
                    }
                }
                double m = (count > 0) ? (sum / count) : 0.0;
                res.addRowUnchecked({key, m});
            }
            return res;
        }

        Table sum(const std::string& targetCol) const {
            Table res;
            res.addColumn(group_col_);
            res.addColumn(targetCol + "_sum");

            for (const auto& [key, indices] : groups_) {
                double total = 0.0;
                for (size_t r : indices) {
                    const auto& val = getVal(targetCol, r);
                    if (std::holds_alternative<int>(val)) {
                        total += std::get<int>(val);
                    } else if (std::holds_alternative<double>(val)) {
                        total += std::get<double>(val);
                    }
                }
                res.addRowUnchecked({key, total});
            }
            return res;
        }
    };

    GroupByResult groupBy(const std::string& colName) const {
        return GroupByResult(columns_, colName);
    }

    // --- Модуль I/O (CSV / JSON) ---
    void toCSV(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) return;

        for (size_t i = 0; i < columns_.size(); ++i) {
            file << columns_[i].name;
            if (i + 1 < columns_.size()) file << ",";
        }
        file << "\n";

        for (size_t r = 0; r < row_count_; ++r) {
            for (size_t i = 0; i < columns_.size(); ++i) {
                const auto& val = columns_[i].data[r];
                std::visit([&file](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, std::monostate>) {
                        file << "NA";
                    } else if constexpr (std::is_same_v<T, bool>) {
                        file << (arg ? "true" : "false");
                    } else {
                        file << arg;
                    }
                }, val);

                if (i + 1 < columns_.size()) file << ",";
            }
            file << "\n";
        }
    }

    bool fromCSV(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return false;

        std::string line;
        if (!std::getline(file, line)) return false;

        columns_.clear();
        row_count_ = 0;
        {
            std::stringstream ss(line);
            std::string col_name;
            while (std::getline(ss, col_name, ',')) {
                if (!col_name.empty() && col_name.back() == '\r') col_name.pop_back();
                addColumn(col_name);
            }
        }

        while (std::getline(file, line)) {
            if (line.empty() || line == "\r") continue;
            if (!line.empty() && line.back() == '\r') line.pop_back();

            std::stringstream ss(line);
            std::string cell;
            std::vector<Value> row_values;
            while (std::getline(ss, cell, ',')) {
                if (!cell.empty() && cell.back() == '\r') cell.pop_back();
                row_values.push_back(parseCell(cell));
            }

            if (row_values.size() == columns_.size()) {
                for (size_t i = 0; i < columns_.size(); ++i) {
                    columns_[i].data.push_back(row_values[i]);
                }
                ++row_count_;
            }
        }
        return true;
    }

    void toJSON(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) return;

        file << "[\n";
        for (size_t r = 0; r < row_count_; ++r) {
            file << "  {\n";
            for (size_t i = 0; i < columns_.size(); ++i) {
                file << "    \"" << columns_[i].name << "\": ";
                const auto& val = columns_[i].data[r];
                
                std::visit([&file](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, std::monostate>) {
                        file << "null";
                    } else if constexpr (std::is_same_v<T, std::string>) {
                        file << "\"" << arg << "\"";
                    } else if constexpr (std::is_same_v<T, bool>) {
                        file << (arg ? "true" : "false");
                    } else {
                        file << arg;
                    }
                }, val);

                if (i + 1 < columns_.size()) file << ",";
                file << "\n";
            }
            file << "  }" << (r + 1 < row_count_ ? "," : "") << "\n";
        }
        file << "]\n";
    }
};

} // namespace CoreKit

#endif // CORE_KIT_HPP
