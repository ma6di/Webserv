#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <string>
#include <ctime>

#define COLOR_RESET   "\033[0m"
#define COLOR_INFO    "\033[32m" // Green
#define COLOR_DEBUG   "\033[36m" // Cyan
#define COLOR_ERROR   "\033[31m" // Red


enum LogLevel { LOG_INFO, LOG_DEBUG, LOG_ERROR };

class Logger {
public:
    static void log(LogLevel level, const std::string& location, const std::string& message);
};

#endif
