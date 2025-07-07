#include "utils/log.h"


INITIALIZE_EASYLOGGINGPP


/*
int SetLogConfig(const std::string &log_dir, const std::string &log_name) {
  const std::string &dir = log_dir;
  el::Configurations defaultConf;
  // To set GLOBAL configurations you may use
  el::Loggers::addFlag(el::LoggingFlag::StrictLogFileSizeCheck);
  defaultConf.setGlobally(el::ConfigurationType::Format,
                          "%datetime %fbase:%line %msg");
  defaultConf.setGlobally(el::ConfigurationType::ToFile, "true");
  defaultConf.setGlobally(el::ConfigurationType::ToStandardOutput, "false");
  defaultConf.setGlobally(el::ConfigurationType::MaxLogFileSize,
                          "209715200");  // 200MB
  std::string filename;
  std::string prefix = log_name + ".";
  filename =
      prefix + el::LevelHelper::convertToString(el::Level::Debug) + ".log";
  defaultConf.set(el::Level::Debug, el::ConfigurationType::Filename,
                  dir + "/" + filename);

  filename =
      prefix + el::LevelHelper::convertToString(el::Level::Error) + ".log";
  defaultConf.set(el::Level::Error, el::ConfigurationType::Filename,
                  dir + "/" + filename);

  filename =
      prefix + el::LevelHelper::convertToString(el::Level::Info) + ".log";
  defaultConf.set(el::Level::Info, el::ConfigurationType::Filename,
                  dir + "/" + filename);

  filename =
      prefix + el::LevelHelper::convertToString(el::Level::Trace) + ".log";
  defaultConf.set(el::Level::Trace, el::ConfigurationType::Filename,
                  dir + "/" + filename);

  filename =
      prefix + el::LevelHelper::convertToString(el::Level::Warning) + ".log";
  defaultConf.set(el::Level::Warning, el::ConfigurationType::Filename,
                  dir + "/" + filename);

  el::Loggers::reconfigureLogger("default", defaultConf);
  el::Helpers::installPreRollOutCallback(
      [](const char *filename, std::size_t size) {
        // SHOULD NOT LOG ANYTHING HERE BECAUSE LOG FILE IS CLOSED!
        std::cout << "************** Rolling out [" << filename
                  << "] because it reached [" << size << " bytes]" << std::endl;
        std::time_t t = std::time(nullptr);
        char mbstr[100];
        if (std::strftime(mbstr, sizeof(mbstr), "%F-%T", std::localtime(&t))) {
          std::cout << mbstr << '\n';
        }
        std::stringstream ss;
        ss << "mv " << filename << " " << filename << "-" << mbstr;
        system(ss.str().c_str());
      });

  LOG(INFO) << "easylogging initialized with log directory: " << dir
            << " and log name: " << log_name;
  return 0;
}
*/

int SetLogConfig(const std::string &log_dir, const std::string &log_name) {
  const std::string &dir = log_dir;
  el::Configurations defaultConf;

  el::Loggers::addFlag(el::LoggingFlag::StrictLogFileSizeCheck);

  defaultConf.setGlobally(el::ConfigurationType::Format, "%datetime [%level] %fbase:%line %msg");
  defaultConf.setGlobally(el::ConfigurationType::ToFile, "true");

#ifdef DEBUG_
  defaultConf.setGlobally(el::ConfigurationType::ToStandardOutput, "true");
#else
  defaultConf.setGlobally(el::ConfigurationType::ToStandardOutput, "false");
#endif

  defaultConf.setGlobally(el::ConfigurationType::MaxLogFileSize,
                          "209715200");  // 200MB

  std::string full_log_path = dir + "/" + log_name + ".log";

  // 设置所有日志等级写入同一个文件
  defaultConf.setGlobally(el::ConfigurationType::Filename, full_log_path);

  el::Loggers::reconfigureLogger("default", defaultConf);

  el::Helpers::installPreRollOutCallback(
      [](const char *filename, std::size_t size) {
        // DON'T LOG HERE
        std::cout << "************** Rolling out [" << filename
                  << "] because it reached [" << size << " bytes]" << std::endl;
        std::time_t t = std::time(nullptr);
        char mbstr[100];
        if (std::strftime(mbstr, sizeof(mbstr), "%F-%T", std::localtime(&t))) {
          std::cout << mbstr << '\n';
        }
        std::stringstream ss;
        ss << "mv " << filename << " " << filename << "-" << mbstr;
        system(ss.str().c_str());
      });

  LOG(INFO) << "easylogging initialized with log file: " << full_log_path;
  return 0;
}

