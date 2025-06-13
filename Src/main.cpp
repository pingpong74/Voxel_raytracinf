#include "application.h"

#include <cstdlib>
#include <stdexcept>
#include <iostream>

int main() {
    Application app{};

    try {
        app.run();
    }
    catch (const std::runtime_error& error) {
        std::cerr << "Error: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}