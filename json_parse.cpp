#include <fstream>
#include <iostream>
#include "external/json.hpp"

using json = nlohmann::json;

int main() {
    std::ifstream f("config.json");
    json data = json::parse(f);

    std::string mode = data["mode"];
    int target_temp = data["target_temp"];
    int min = data["temp_range"]["min"];
    int max = data["temp_range"]["max"];

    std::cout << data << std::endl;
    std::cout << mode << std::endl;
    std::cout << target_temp << std::endl;
    std::cout << min << std::endl;
    std::cout << max << std::endl;
}
