#include "Logger.hpp" 
	
void Logger::log(LogLevel level, const std::string& location, const std::string& message) {
    std::string level_str;
    const char* color = COLOR_RESET;
    switch (level) {
        case LOG_INFO:  level_str = "INFO";  color = COLOR_INFO;  break;
        case LOG_DEBUG: level_str = "DEBUG"; color = COLOR_DEBUG; break;
        case LOG_ERROR: level_str = "ERROR"; color = COLOR_ERROR; break;
    }
    std::cout << color << "[" << level_str << "][" << location << "] " << message << COLOR_RESET << std::endl;
}
