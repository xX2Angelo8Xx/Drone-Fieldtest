#include <iostream>
#include "lcd_handler.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <line1> <line2>" << std::endl;
        return 1;
    }
    
    LCDHandler lcd;
    
    if (!lcd.init()) {
        // Non-blocking - continue if LCD fails
        return 0;
    }
    
    lcd.displayMessage(argv[1], argv[2]);
    return 0;
}