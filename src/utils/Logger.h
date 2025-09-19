#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

class Logger {
public:
    static void Init(const std::string& logFile = "paintsplash.log");
    static void Shutdown();

    static void Log(LogLevel level, const std::string& message, const char* file = nullptr, int line = 0);
    static void SetLogLevel(LogLevel level);

private:
    static std::ofstream logFile_;
    static LogLevel currentLevel_;
    static bool initialized_;

    static std::string GetTimestamp();
    static std::string LevelToString(LogLevel level);
    static void WriteToFile(const std::string& message);
    static void WriteToConsole(LogLevel level, const std::string& message);
};

// Convenience macros
#define LOG_DEBUG(message) Logger::Log(LogLevel::DEBUG, message, __FILE__, __LINE__)
#define LOG_INFO(message) Logger::Log(LogLevel::INFO, message, __FILE__, __LINE__)
#define LOG_WARNING(message) Logger::Log(LogLevel::WARNING, message, __FILE__, __LINE__)
#define LOG_ERROR(message) Logger::Log(LogLevel::ERROR, message, __FILE__, __LINE__)
#define LOG_FATAL(message) Logger::Log(LogLevel::FATAL, message, __FILE__, __LINE__)
