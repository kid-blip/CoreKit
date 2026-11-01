#include <iostream>
#include "CoreKit.hpp"

int main() {
    // 1. Initialize the task table
    CoreKit::Table tasks;
    tasks.addColumn("ID");
    tasks.addColumn("Task");
    tasks.addColumn("Priority");
    tasks.addColumn("Completed");

    // 2. Populate rows
    tasks.addRow({1, "Write custom OS bootloader", 3, false});
    tasks.addRow({2, "Produce breakcore track", 2, false});
    tasks.addRow({3, "Fix FORScan hex bytes", 3, true});

    // 3. Print table output to terminal
    std::cout << "--- All Tasks ---" << std::endl;
    tasks.print();

    // 4. Filter and sort
    CoreKit::Table active = tasks.filter([](const auto& row) {
        return row.get<bool>("Completed") == false;
    });
    active.sortBy("Priority", false);

    std::cout << "\n--- Active Tasks by Priority ---" << std::endl;
    active.print();

    return 0;
}
