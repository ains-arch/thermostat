#include <gpiod.hpp>
#include <iostream>
#include <ostream>

int main() {
    std::cout << "Hello, thermostat!" << std::endl;

    // instantiate a new chip object by opening the gpiochip0 device file
    std::cout << "opening GPIO chip" << std::endl;
    auto chip = gpiod::chip("/dev/gpiochip0");
    std::cout << "GPIO chip opened" << std::endl;
    
    // get info about the chip
    std::cout << ".get_info about the chip" << std::endl;
    std::cout << chip.get_info()  << std::endl;
    std::cout << "end .get_info" << std::endl;
    
    // wrap unsigned int to represent the relevant line offset
    auto pin = gpiod::line::offset(22);

    // get info about the line
    std::cout << ".get_line_info" << std::endl;
    std::cout << chip.get_line_info(pin)  << std::endl;
    std::cout << "end .get_line_info" << std::endl;
    
    // get file descriptor associated with this chip
    // std::cout << ".fd (file descriptor) for chip" << std::endl;
    // std::cout << chip.fd() << std::endl;
    // std::cout << "end .fd" << std::endl;
    
    // line config options for use in line requests and reconfiguration
    auto config = gpiod::line_config();

    auto pinSettings = gpiod::line_settings();
    pinSettings.set_output_value(gpiod::line::value::ACTIVE); // high
    pinSettings.set_direction(gpiod::line::direction::OUTPUT); // driving

    config.add_line_settings(gpiod::line::offset(22), pinSettings);

    auto request = chip.prepare_request();
    request.set_line_config(config);
    request.do_request();;
    
    std::cout << "Done!" << std::endl;
    return 0;
}
