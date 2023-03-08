#include <iostream>
#include <ctime>
#include <mutex>
#include <string>




enum class LogLevel { INFO, WARNING, ERROR, READ, WRITE };

class Logger {
public:
    static Logger& get() {
        static Logger instance;
        return instance;
    }

    void info(const std::string& message, bool newLine = false) {
        log("INFO", message, "\033[32m", newLine); // green text
    }

    void warning(const std::string& message, bool newLine = false) {
        log("WARNING", message, "\033[33m", newLine); // yellow text
    }

    void error(const std::string& message, bool newLine = false) {
        log("ERROR", message, "\033[31m", newLine); // red text
    }

    void read(const std::string& message, bool newLine = false) {
        log("READ", message, "\033[36m", newLine); // cyan text
    }

    void write(const std::string& message, bool newLine = false) {
        log("WRITE", message, "\033[35m", newLine); // magenta text
    }
    
    void processInfo(int numElements, const std::string& optionalInfo = "") {
        process(numElements, optionalInfo);
        std::string logStr = std::string("Processing ") + std::to_string(numElements) + " elements. (" + optionalInfo + ")";
        info(logStr);
    }
    void processRead(int numElements, const std::string& optionalInfo = "") {
        process(numElements, optionalInfo);
        std::string logStr = std::string("Processing ") + std::to_string(numElements) + " elements. (" + optionalInfo + ")";
        read(logStr);
    }
    void processWrite(int numElements, const std::string& optionalInfo = "") {
        process(numElements, optionalInfo);
        std::string logStr = std::string("Processing ") + std::to_string(numElements) + " elements. (" + optionalInfo + ")";
        write(logStr);
    }

    void updateProcess(int numProcessedElements = 1, const std::string& optionalInfo = "") {
        std::lock_guard<std::mutex> lock(mutex);
        if (!loadingBarActive) {
            return;
        }
        numProcessed += numProcessedElements;
        int progress = (int)((float)numProcessed / totalElements * 100);
        std::cout << "\r[" << getTimeStr() << "] " << optionalInfo << " [";
        int pos = barWidth * progress / 100;
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) {
                std::cout << "=";
            }
            else if (i == pos) {
                std::cout << ">";
            }
            else {
                std::cout << " ";
            }
        }
        std::cout << "] " << progress << "%" << std::flush;
        if (numProcessed == totalElements) {
            endLoading();
        }
    }

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    LogLevel level;
    std::mutex mutex;
    const int barWidth = 30;
    bool loadingBarActive = false;
    int numProcessed = 0;
    int totalElements = 0;

    std::string getTimeStr() {
        std::time_t now = std::time(nullptr);
        std::tm timeinfo;
        localtime_s(&timeinfo, &now);
        char timeStr[20];
        std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
        return timeStr;
    }

    void process(int numElements, const std::string& optionalInfo = "") {
        std::lock_guard<std::mutex> lock(mutex);
        loadingBarActive = true;
        numProcessed = 0;
        totalElements = numElements;
    }

    void endLoading() {
        if (!loadingBarActive) {
            return;
        }
        loadingBarActive = false;
        std::cout << std::endl;
    }

    void log(const std::string& levelStr, const std::string& message, const std::string& color, bool newLine = false) {
        std::lock_guard<std::mutex> lock(mutex);
        if (newLine) {
            std::cout << std::endl;
        }
        if (loadingBarActive) {
            std::cout << std::endl;
        }
        if (level == LogLevel::ERROR || level == LogLevel::WARNING || level == LogLevel::INFO) {
            std::cout << "["
                << getTimeStr()
                << color // set text color
                << " " << levelStr
                << "\033[0m" // reset color
                << "] "
                << message
                << std::endl;
        }
    }
};