#include "Logger.h"
#include "PathUtils.h"

std::ofstream Logger::logFile_;
LogLevel Logger::currentLevel_ = LogLevel::INFO;
bool Logger::initialized_ = false;

void Logger::Init(const std::string& logFile)
{
    if (initialized_) return;

    // If no log file specified, create one next to the executable
    std::string actualLogFile = logFile;
    if (actualLogFile.empty() || actualLogFile == "paintsplash.log") {
        // Get executable directory using our utility
        std::string exeDir = Utils::GetExecutableDir();

        // Create timestamped log file in executable directory
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << exeDir << "/paintsplash_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".log";
        actualLogFile = ss.str();
    }

    logFile_.open(actualLogFile, std::ios::out | std::ios::trunc);
    if (!logFile_.is_open()) {
        std::cerr << "[LOGGER WARNING] Could not open log file: " << actualLogFile << ". Logging to file will be disabled, but the game will continue.\n";
        // Do not set initialized_ to true, but allow the game to continue
        currentLevel_ = LogLevel::DEBUG;
        return;
    }
    currentLevel_ = LogLevel::DEBUG;  // Enable DEBUG logging for development
    initialized_ = true;

    LOG_INFO("Logger initialized with DEBUG level enabled - logging to: " + actualLogFile);
    std::cout << "[LOGGER] Log file created at: " << actualLogFile << std::endl;
}

void Logger::Shutdown()
{
    if (!initialized_) return;

    LOG_INFO("Logger shutting down");
    if (logFile_.is_open()) {
        logFile_.close();
    }
    initialized_ = false;
}

void Logger::Log(LogLevel level, const std::string& message, const char* file, int line)
{
    if (level < currentLevel_) return;

    std::string timestamp = GetTimestamp();
    std::string levelStr = LevelToString(level);

    std::stringstream logMessage;
    logMessage << "[" << timestamp << "] [" << levelStr << "] ";

    if (file && level >= LogLevel::WARNING) {
        // Extract filename from path
        std::string filename = file;
        size_t lastSlash = filename.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            filename = filename.substr(lastSlash + 1);
        }
        logMessage << filename << ":" << line << " - ";
    }

    logMessage << message;

    std::string finalMessage = logMessage.str();

    WriteToFile(finalMessage);
    WriteToConsole(level, finalMessage);
}

void Logger::SetLogLevel(LogLevel level)
{
    currentLevel_ = level;
}

std::string Logger::GetTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return ss.str();
}

std::string Logger::LevelToString(LogLevel level)
{
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

void Logger::WriteToFile(const std::string& message)
{
    if (logFile_.is_open()) {
        logFile_ << message << std::endl;
        logFile_.flush(); // Ensure immediate write to file
    } else if (initialized_) {
        // If file was supposed to be open but isn't, try to reopen it
        std::string logFile = "paintsplash.log";
        logFile_.open(logFile, std::ios::out | std::ios::app);
        if (logFile_.is_open()) {
            logFile_ << message << std::endl;
            logFile_.flush();
        }
    }
}

void Logger::WriteToConsole(LogLevel level, const std::string& message)
{
    switch (level) {
        case LogLevel::DEBUG:
            std::cout << "\033[36m" << message << "\033[0m" << std::endl; // Cyan
            break;
        case LogLevel::INFO:
            std::cout << "\033[32m" << message << "\033[0m" << std::endl; // Green
            break;
        case LogLevel::WARNING:
            std::cout << "\033[33m" << message << "\033[0m" << std::endl; // Yellow
            break;
        case LogLevel::ERROR:
        case LogLevel::FATAL:
            std::cerr << "\033[31m" << message << "\033[0m" << std::endl; // Red
            break;
    }
}
