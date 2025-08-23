#pragma once
#include <atomic>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <chrono>
#include <filesystem>

namespace utils {

class Logger {
public:
    enum class Level { Debug = 0, Info = 1, Warn = 2, Error = 3, Off = 4 };

    // Singleton access
    static Logger& get() {
        static Logger instance;
        return instance;
    }

    // Configuration (call at program start or from Simulator ctor)
    void setLevel(Level lvl) { level_.store(lvl, std::memory_order_relaxed); }
    Level level() const { return level_.load(std::memory_order_relaxed); }

    // If set, logs are mirrored to std::cout / std::cerr in addition to file (default true)
    void setAlsoConsole(bool v) { alsoConsole_.store(v, std::memory_order_relaxed); }

    // If set, timestamps are written in UTC; otherwise local time (default: local)
    void setUseUTC(bool v) { useUTC_.store(v, std::memory_order_relaxed); }

    // Open/rotate the log file. Creates parent dirs if needed.
    // Pass empty string to disable file output.
    bool setOutputFile(const std::string& path, bool append = true) {
        std::lock_guard<std::mutex> lock(m_);
        if (file_.is_open()) file_.close();
        if (path.empty()) {
            filePath_.reset();
            return true;
        }
        try {
            std::filesystem::path p(path);
            if (p.has_parent_path()) {
                std::error_code ec;
                std::filesystem::create_directories(p.parent_path(), ec);
            }
            file_.open(path, append ? (std::ios::app) : (std::ios::trunc));
            if (!file_) return false;
            filePath_ = path;
            return true;
        } catch (...) {
            return false;
        }
    }

    // ---- Logging functions ----
    template <typename... Ts>
    void debug(Ts&&... xs)  { log(Level::Debug, std::forward<Ts>(xs)...); }

    template <typename... Ts>
    void info(Ts&&... xs)   { log(Level::Info,  std::forward<Ts>(xs)...); }

    template <typename... Ts>
    void warn(Ts&&... xs)   { log(Level::Warn,  std::forward<Ts>(xs)...); }

    template <typename... Ts>
    void error(Ts&&... xs)  { log(Level::Error, std::forward<Ts>(xs)...); }

    template <typename... Ts>
    void reportError(Ts&&... xs) {
        if (level() == Level::Off) {
            std::ostringstream body;
            (void)std::initializer_list<int>{ ((body << std::forward<Ts>(xs)), 0)... };
            std::cerr << "Error: " << body.str() << std::endl;
        } else {
            error(std::forward<Ts>(xs)...);
        }
    }

    template <typename... Ts>
    void reportWarn(Ts&&... xs) {
        if (level() == Level::Off) {
            std::ostringstream body;
            (void)std::initializer_list<int>{ ((body << std::forward<Ts>(xs)), 0)... };
            std::cerr << "Warning: " << body.str() << std::endl;
        } else {
            warn(std::forward<Ts>(xs)...);
        }
    }

    // Small scoped helper: logs start/end with elapsed ms
    class Scope {
    public:
        Scope(std::string what, Level lvl = Level::Debug)
            : what_(std::move(what)), lvl_(lvl),
              start_(std::chrono::steady_clock::now()) {
            Logger::get().log(lvl_, "[BEGIN] ", what_);
        }
        ~Scope() {
            using namespace std::chrono;
            auto ms = duration_cast<milliseconds>(steady_clock::now() - start_).count();
            Logger::get().log(lvl_, "[END]   ", what_, " (", ms, " ms)");
        }
    private:
        std::string what_;
        Level lvl_;
        std::chrono::steady_clock::time_point start_;
    };

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    template <typename... Ts>
    void log(Level msgLvl, Ts&&... xs) {
        const char* color = "";
        const char* reset = "\033[0m";

        switch (msgLvl) {
        case Level::Debug: color = "\033[36m"; break; // cyan
        case Level::Info:  color = "\033[32m"; break; // green
        case Level::Warn:  color = "\033[33m"; break; // yellow
        case Level::Error: color = "\033[31m"; break; // red
        default:           color = "";         break;
}
        if (msgLvl < level_.load(std::memory_order_relaxed)) return;

        // Build message body first
        std::ostringstream body;
        (void)std::initializer_list<int>{ ( (body << std::forward<Ts>(xs)), 0 )... };

        // Compose full line with metadata
        std::ostringstream line;
        line << timeStamp() << " " << levelToText(msgLvl)
             << " [tid " << std::this_thread::get_id() << "] "
             << body.str() << "\n";
        auto s = line.str();

        const bool toConsole = alsoConsole_.load(std::memory_order_relaxed);

        // Output atomically
        std::lock_guard<std::mutex> lock(m_);
        if (file_.is_open()) {
            file_ << s;
            file_.flush();
        }
        if (toConsole) {
            std::ostream& os = (msgLvl >= Level::Warn) ? std::cerr : std::cout;
            os << color << s << reset;
        }
    }

    std::string timeStamp() const {
        using namespace std::chrono;
        const auto now = system_clock::now();
        std::time_t t = system_clock::to_time_t(now);
        std::tm tm{};
        if (useUTC_.load(std::memory_order_relaxed)) {
#if defined(_WIN32)
            gmtime_s(&tm, &t);
#else
            gmtime_r(&t, &tm);
#endif
        } else {
#if defined(_WIN32)
            localtime_s(&tm, &t);
#else
            localtime_r(&t, &tm);
#endif
        }
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        // + milliseconds
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;
        ss << "." << std::setw(3) << std::setfill('0') << ms;
        return ss.str();
    }

    static std::string_view levelToText(Level l) {
        switch (l) {
            case Level::Debug: return "DEBUG";
            case Level::Info:  return "INFO ";
            case Level::Warn:  return "WARN ";
            case Level::Error: return "ERROR";
            case Level::Off:   return "OFF  ";
        }
        return "INFO ";
    }

    std::atomic<Level> level_{Level::Info};
    std::atomic<bool> alsoConsole_{true};
    std::atomic<bool> useUTC_{false};

    std::optional<std::string> filePath_;
    std::ofstream file_;
    mutable std::mutex m_;
};

} // namespace utils
