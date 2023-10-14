#include <execution>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "search_server.h"
#include "log_duration.h"
#include "process_queries.h"
#include "test_example_functions.h"

int main() {
    std::cout << "Running tests" << std::endl;
    TestSearchServer();
    std::cout << "All tests passed!" << std::endl;
}