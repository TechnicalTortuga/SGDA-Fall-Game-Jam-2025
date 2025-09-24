#include "Logger.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger()
    : minLogLevel_(LogLevel::DEBUG), initialized_(false) {
    Initialize();
}

Logger::~Logger() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

void Logger::Initialize() {
    if (initialized_) return;

    try {
        // Create log directory if it doesn't exist
        std::filesystem::path logDir = "editorlogs";
        std::filesystem::create_directories(logDir);

        // Open log file with timestamp
        std::string logPath = GetLogFilePath();
        logFile_.open(logPath, std::ios::out | std::ios::app);

        if (!logFile_.is_open()) {
            std::cerr << "Failed to open log file: " << logPath << std::endl;
            return;
        }

        initialized_ = true;

        // Log initialization
        Log(LogLevel::INFO, "Logger initialized - logging to: " + logPath);

    } catch (const std::exception& e) {
        std::cerr << "Logger initialization failed: " << e.what() << std::endl;
    }
}

std::string Logger::GetLogFilePath() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "editorlogs/paintstrike_editor_"
       << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S")
       << ".log";

    return ss.str();
}

std::string Logger::GetTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string Logger::LogLevelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

void Logger::Log(LogLevel level, const std::string& message, const char* file, int line) {
    if (level < minLogLevel_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    std::string timestamp = GetTimestamp();
    std::string levelStr = LogLevelToString(level);

    // Format: [TIMESTAMP] [LEVEL] message (file:line)
    std::stringstream formattedMessage;
    formattedMessage << "[" << timestamp << "] [" << levelStr << "] " << message;

    if (file && line > 0) {
        formattedMessage << " (" << file << ":" << line << ")";
    }

    std::string finalMessage = formattedMessage.str();

    // Always output to console
    if (level >= LogLevel::WARNING) {
        std::cerr << finalMessage << std::endl;
    } else {
        std::cout << finalMessage << std::endl;
    }

    // Output to file if initialized
    if (initialized_ && logFile_.is_open()) {
        logFile_ << finalMessage << std::endl;
        logFile_.flush(); // Ensure immediate write
    }
}
