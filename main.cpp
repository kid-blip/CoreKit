#include <iostream>
#include "CoreKit.hpp"

int main() {
    std::cout << "=== Расширенная демонстрация CoreKit (GroupBy & Sorting) ===" << std::endl << std::endl;

    // 1. Создаем таблицу
    CoreKit::Table df;
    df.addColumn("Department");
    df.addColumn("Employee");
    df.addColumn("Salary");
    df.addColumn("Score");

    // 2. Заполняем данные
    df.addRow({"IT", "Alice", 3500.0, 92.5});
    df.addRow({"HR", "Bob", 2800.0, 81.0});
    df.addRow({"IT", "Charlie", 4200.0, 95.0});
    df.addRow({"Sales", "David", 3100.0, 88.5});
    df.addRow({"HR", "Eva", 2950.0, 85.0});
    df.addRow({"Sales", "Frank", 3400.0, 90.0});
    df.addRow({"IT", "Grace", 4000.0, 89.0});

    std::cout << "--- Исходная таблица сотрудников ---" << std::endl;
    df.print();
    std::cout << "\n";

    // 3. Тест сортировки: упорядочим таблицу по зарплате (Salary) по убыванию
    std::cout << "--- Сортировка по зарплате (по убыванию) ---" << std::endl;
    df.sortBy("Salary", false);
    df.print();
    std::cout << "\n";

    // 4. Тест уникальных значений с безопасным обходом variant
    std::cout << "--- Список уникальных отделов (Department) ---" << std::endl;
    auto departments = df.unique("Department");
    for (const auto& dept : departments) {
        std::cout << " - ";
        std::visit([](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                std::cout << "NA";
            } else if constexpr (std::is_same_v<T, bool>) {
                std::cout << (arg ? "true" : "false");
            } else {
                std::cout << arg;
            }
        }, dept);
        std::cout << "\n";
    }
    std::cout << "\n";

    // 5. Тест GroupBy и Агрегации: находим среднюю зарплату по отделам
    std::cout << "--- Средняя зарплата по отделам (GroupBy Department -> Mean Salary) ---" << std::endl;
    CoreKit::Table avg_salaries = df.groupBy("Department").mean("Salary");
    avg_salaries.print();

    std::cout << "\n=== Все задачи успешно выполнены! ===" << std::endl;
    return 0;
}
