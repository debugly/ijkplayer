#include "logging.h"
#include <ctime>
#include <iostream>

#include <locale>
#include <string>
#include <codecvt>
#include <fstream>
#ifdef WIN32
#include <windows.h>
#endif
/**
* @brief   Gets the name of the log category from the enum value
*
* @param   The enum value of the category
* @return  The name of the category; returns the word UNKNOWN if not valid.
*/
const char* Log::TypeToString(const Type& type) {
	switch (type) {
	case LOG_TYPE_FATAL:
		return "FATAL";
	case LOG_TYPE_ERROR:
		return "ERROR";
	case LOG_TYPE_WARN:
		return "WARN ";
	case LOG_TYPE_INFO:
		return "INFO ";
	case LOG_TYPE_DEBUG:
		return "DEBUG";
	default:
		break;
	}
	return "UNKWN";
}

/**
* @brief   Initialises the file stream
*
* @param   fileName The location of the file to create/append to
* @return  True if the file was successfully initialised; false if already initialised
*/
bool Log::Initialise(const std::string& fileName) {
	Log& log = Log::get();

	if (!log.m_initialised) {
		log.m_fileName = fileName;

		//std::locale loc(std::locale(), new std::codecvt_utf8<char32_t>);
		std::locale loc("", NULL);

		log.m_stream.open(fileName.c_str(),
			std::ios_base::app | std::ios_base::out);
		log.m_stream.imbue(loc);

		log.m_initialised = true;
		Info("LOG INITIALISED");
		return true;
	}
	return false;
}

/**
* @brief   Finalises the file stream
*
* @return  True if the file was successfully finalised; false if not initialised
*/
bool Log::Finalise() {
	Log& log = Log::get();

	if (log.m_initialised) {
		//Info("LOG FINALISED");
		log.m_stream.close();
		return true;
	}
	return false;
}

/**
* @brief  Sets the debugging threshold
* This is the debugging threshold to use when reporting bugs, useful for having
* lots of debugging information that sometimes you just want to turn off.
*
* @param  type The given debugging threshold to use
*/
void Log::SetThreshold(const Type& type) {
	Log& log = Log::get();
	log.m_threshold = type;
}

/**
* @brief   Writes a Fatal Error to the log
*
* @param   message The message to log
* @return  True if the log was successful
*/
bool Log::Fatal(const std::string& message) {
	return Log::get().log(LOG_TYPE_FATAL, message);
}

/**
* @brief   Writes a Fatal Error to the log
*
* @param   format The format of the message
* @param   ... Variable arguments
* @return  True if the log was successful
*/
bool Log::Fatal(const char* format, ...) {
	va_list varArgs;
	va_start(varArgs, format);
	bool success = Log::get().log(LOG_TYPE_FATAL, format, varArgs);
	va_end(varArgs);
	return success;
}

/**
* @brief   Writes an Error to the log
*
* @param   message The message to log
* @return  True if the log was successful
*/
bool Log::Error(const std::string& message) {
	return Log::get().log(LOG_TYPE_ERROR, message);
}

/**
* @brief  Writes an Error to the log
*
* @param   format The format of the message
* @param   ... Variable arguments
* @return  True if the log was successful
*/
bool Log::Error(const char* format, ...) {
	va_list varArgs;
	va_start(varArgs, format);
	bool success = Log::get().log(LOG_TYPE_ERROR, format, varArgs);
	va_end(varArgs);
	return success;
}

/**
* @brief   Writes a warning to the log
*
* @param   message The message to log
* @return  True if the log was successful
*/
bool Log::Warn(const std::string& message) {
	return Log::get().log(LOG_TYPE_WARN, message);
}

/**
* @brief   Writes a warning to the log
*
* @param   format The format of the message
* @param   ... Variable arguments
* @return  True if the log was successful
*/
bool Log::Warn(const char* format, ...) {
	va_list varArgs;
	va_start(varArgs, format);
	bool success = Log::get().log(LOG_TYPE_WARN, format, varArgs);
	va_end(varArgs);
	return success;
}

/**
* @brief   Writes an information message to the log
*
* @param   message The message to log
* @return  True if the log was successful
*/
bool Log::Info(const std::string& message) {
	return Log::get().log(LOG_TYPE_INFO, message);
}

/**
* @brief   Writes an information message to the log
*
* @param   format The format of the message
* @param   ... Variable arguments
* @return  True if the log was successful
*/
bool Log::Info(const char* format, ...) {
	va_list varArgs;
	va_start(varArgs, format);
	bool success = Log::get().log(LOG_TYPE_INFO, format, varArgs);
	va_end(varArgs);
	return success;
}

/**
* @brief   Writes a Debug message to the log
*
* @param   message The message to log
* @return  True if the log was successful
*/
bool Log::Debug(const std::string& message) {
	return Log::get().log(LOG_TYPE_DEBUG, message);
}

/**
* @brief   Writes an Debug message to the log
*
* @param   format The format of the message
* @param   ... Variable arguments
* @return  True if the log was successful
*/
bool Log::Debug(const char* format, ...) {
	va_list varArgs;
	va_start(varArgs, format);
	bool success = Log::get().log(LOG_TYPE_DEBUG, format, varArgs);
	va_end(varArgs);
	return success;
}

/**
* @brief   Peeks at the top element of the function stack
*
* @return  The top element of the function stack
*/
std::string Log::Peek() {
	return Log::get().m_stack.back();
};

/**
* @brief   Pushes the function stack with the given message
*
* @param   input The message to store in the stack (typically the name of the
*          function)
* @return  True if the stack was successfully pushed
*/
bool Log::Push(const std::string& input) {
	if (!input.empty()) {
		Debug(input + " BEGIN");
		Log::get().m_stack.push_back(input);
		return true;
	}
	return false;
}

/**
* @brief   Pops the top element off the stack
*
* @return  The message just popped off the stack
*/
std::string Log::Pop() {
	Log& log = Log::get();
	if (!log.m_stack.empty()) {
		std::string temp(log.Peek());
		log.m_stack.pop_back();
		Debug(temp + " END");
		return temp;
	}
	return std::string();
}

/**
* @brief  Writes the stack to the log
*/
void Log::PrintStackTrace() {
	Log& log = Log::get();
	std::string temp = "---Stack Trace---\n";

	for (std::vector<std::string>::reverse_iterator i = log.m_stack.rbegin();
		i != log.m_stack.rend(); ++i) {
		temp += "| " + *i + "\n";
	}

	temp += "-----------------";
	log.write(temp.c_str());
}
/**
* @brief  Constructor
*/
Log::Log()
: m_threshold(LOG_TYPE_INFO),
m_fileName(),
m_stack(),
m_stream() {
}

/**
* @brief  Copy constructor
* Kept private in order to preserve singleton
*/
Log::Log(const Log&) {
}

/**
* @brief  Destructor
* Logs the shut down then closes the file stream
*/
Log::~Log() {
	Finalise();
}

/**
* @brief  Get the singleton instance
*/
Log& Log::get() {
	static Log log;
	return log;
}


/**
* @brief  Writes the specified message to the console and the log file
*
* @param  format The format of the message
* @param  ... Variable arguments
*/
void Log::write(const char* format, ...) {
	char buffer[512];

	va_list varArgs;
	va_start(varArgs, format);
#ifdef WIN32
	vsnprintf(buffer, sizeof(buffer), format, varArgs);
#else
	vsnprintf(buffer, sizeof(buffer), format, varArgs);
#endif
	va_end(varArgs);

	std::cout << buffer << std::endl; 
	m_stream << buffer << std::endl;
}



/**
* @brief   Logs the specified message with a timestamp and category prefix
* The constant TIMESTAMP_BUFFER_SIZE was calculated as the maximum number of
* characters required for the timestamp "[HH:MM:SS MM/DD/YY] ".
*
* @param   type The category of message to write based on the enum Log::Type
* @param   message The message to be sent
* @return  True if the log was successful
* @remark  Log time should be precise to milliseconds
*/
bool Log::log(const Type& type, const std::string& message) {
	if (type <= m_threshold) {
		static const int TIMESTAMP_BUFFER_SIZE = 23;
		char buffer[TIMESTAMP_BUFFER_SIZE];
#ifdef WIN32
		SYSTEMTIME win_time;
		GetLocalTime(&win_time);
		_snprintf(buffer, TIMESTAMP_BUFFER_SIZE,
			"%02d:%02d:%02d.%03d %02d/%02d", win_time.wHour, win_time.wMinute, win_time.wSecond, win_time.wMilliseconds, 
			win_time.wMonth, win_time.wDay);

#else
		time_t timestamp;
		time(&timestamp);
		strftime(buffer, sizeof(buffer), "%X %x", localtime(&timestamp));
#endif
		write("[%s] %s - %s", buffer, TypeToString(type), message.c_str());
		return true;
	}
	return false;
}

/**
* @brief   Logs the specified message with a timestamp and category prefix
* The constant TIMESTAMP_BUFFER_SIZE was calculated as the maximum number of
* characters required for the timestamp "[HH:MM:SS MM/DD/YY] ".
*
* @param   type The category of message to write based on the enum Log::Type
* @param   format The format of the message
* @param   ... Variable arguments
* @return  True if the log was successful
*/
bool Log::log(const Type& type, const char* format, const va_list& varArgs) {
	char buffer[1024];
#ifdef WIN32
	vsnprintf(buffer, sizeof(buffer), format, varArgs);
#else
	snprintf(buffer, sizeof(buffer), format, varArgs);
#endif
	return log(type, buffer);
}

/**
* @brief  Copy operator
* Kept private in order to preserve singleton
*/
Log& Log::operator=(const Log&) {
	return *this;
}
