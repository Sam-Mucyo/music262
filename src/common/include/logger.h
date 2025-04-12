#pragma once

#include <iostream>
#include <memory>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>

/**
 * @brief Logger class that provides a thread-safe logging interface using
 * spdlog
 *
 * This class wraps the spdlog library to provide a consistent, easy-to-use
 * interface for logging throughout the application. It ensures that logs are
 * thread-safe and can be output to both console and files with proper
 * formatting.
 */
class Logger {
public:
  /**
   * @brief Initialize the logger with default settings
   *
   * This method sets up the spdlog logger with console and file sinks.
   * It should be called once at the start of the application.
   *
   * @param appName The name of the application, used for the logger name and
   * logfile
   */
  static void init(const std::string &appName = "music_app") {
    try {
      // Create a console sink with color output
      auto console_sink =
          std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      console_sink->set_level(spdlog::level::debug);

      // Create a file sink with rotation (5MB max size, 3 rotated files)
      auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
          appName + ".log", 5 * 1024 * 1024, 3);
      file_sink->set_level(spdlog::level::trace);

      // Create a logger with both sinks
      auto logger = std::make_shared<spdlog::logger>(
          appName, spdlog::sinks_init_list{console_sink, file_sink});

      // Set the default logger
      spdlog::set_default_logger(logger);

      // Set global log level
      spdlog::set_level(spdlog::level::debug);

      // Set pattern: [timestamp] [level] [logger name] message
      spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");

      spdlog::info("Logger initialized");
    } catch (const spdlog::spdlog_ex &ex) {
      std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
  }

  /**
   * @brief Set the global log level
   *
   * This method sets the minimum level of messages that will be logged.
   * Messages below this level will be ignored.
   *
   * @param level The log level to set
   */
  static void setLevel(spdlog::level::level_enum level) {
    spdlog::set_level(level);
  }
};

// Convenience macros for logging
// These make the logging calls more concise and consistent

/**
 * @brief Log a trace message (most detailed level)
 */
#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)

/**
 * @brief Log a debug message
 */
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)

/**
 * @brief Log an info message (normal operation)
 */
#define LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)

/**
 * @brief Log a warning message (potential problem)
 */
#define LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)

/**
 * @brief Log an error message (operation failed)
 */
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)

/**
 * @brief Log a critical message (severe error)
 */
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)
