#pragma once

#include <string>
#include <fstream>
#include <mutex>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static Logger& GetInstance();

    void Log(LogLevel level, const std::string& message, const char* file = nullptr, int line = 0);
    void SetLogLevel(LogLevel level) { minLogLevel_ = level; }
    LogLevel GetLogLevel() const { return minLogLevel_; }

    // Convenience methods
    void Debug(const std::string& message, const char* file = nullptr, int line = 0) {
        Log(LogLevel::DEBUG, message, file, line);
    }

    void Info(const std::string& message, const char* file = nullptr, int line = 0) {
        Log(LogLevel::INFO, message, file, line);
    }

    void Warning(const std::string& message, const char* file = nullptr, int line = 0) {
        Log(LogLevel::WARNING, message, file, line);
    }

    void Error(const std::string& message, const char* file = nullptr, int line = 0) {
        Log(LogLevel::ERROR, message, file, line);
    }

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void Initialize();
    std::string GetLogFilePath() const;
    std::string GetTimestamp() const;
    std::string LogLevelToString(LogLevel level) const;

    std::ofstream logFile_;
    std::mutex mutex_;
    LogLevel minLogLevel_;
    bool initialized_;
};

// Global logging macros for convenience
#define LOG_DEBUG(message) Logger::GetInstance().Debug(message, __FILE__, __LINE__)
#define LOG_INFO(message) Logger::GetInstance().Info(message, __FILE__, __LINE__)
#define LOG_WARNING(message) Logger::GetInstance().Warning(message, __FILE__, __LINE__)
#define LOG_ERROR(message) Logger::GetInstance().Error(message, __FILE__, __LINE__)
