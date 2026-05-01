#include <iostream>
#include <vector>
#include <string>

int main() {
    std::vector<std::string> messages = {"Hello", "from", "VeroCC", "C++!"};
    for (const auto &m : messages) std::cout << m << " ";
    std::cout << "\n";
    return 0;
}
