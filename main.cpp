#include <iostream>
#include <stdexcept>

#include "app/App.h"

int main() {
    try {
        App app;
        app.run();
    } catch (const std::runtime_error& e) {
        std::cerr << "[SPATIUM] Fatal error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
