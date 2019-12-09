#pragma once

#include <functional>
#include <sstream>
#include <string>

#include "string.h"

#include "ivi-logging-common.h"

namespace logging {

template <size_t I = 0, typename Func, typename... TupleTypes, typename... CallArgumentTypes>
typename std::enable_if<I == sizeof...(TupleTypes)>::type for_each_in_tuple_(
    std::tuple<TupleTypes...>& tpl, Func func, CallArgumentTypes&... args)
{
    UNUSED(tpl);
    UNUSED(func);
    UNUSED(args...);
}

template <size_t I = 0, typename Func, typename... TupleTypes, typename... CallArgumentTypes>
    typename std::enable_if < I<sizeof...(TupleTypes)>::type for_each_in_tuple_(
                                  std::tuple<TupleTypes...>& tpl, Func func, CallArgumentTypes&... args)
{
    func(std::get<I>(tpl), args...);
    for_each_in_tuple_<I + 1>(tpl, func, args...);
}

inline std::string pointerToString(const void* p)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "0x%zX", (size_t)p);
    return buffer;
}

class StringBuilder
{
public:
    template <typename Type> StringBuilder& operator<<(const Type& v)
    {
        m_stream << v;
        return *this;
    }

    StringBuilder& operator<<(const uint8_t& v)
    {
        m_stream << (int)v;
        return *this;
    }

    StringBuilder& operator<<(const int8_t& v)
    {
        m_stream << (int)v;
        return *this;
    }

    operator std::string() { return m_stream.str(); }

    std::string str() const { return m_stream.str(); }

private:
    std::stringstream m_stream;
};

#define log_with_context(_context_, severity)                                                          \
    for (auto dummy = &(_context_); (dummy != nullptr) && dummy->isEnabled(severity); dummy = nullptr) \
    (_context_).createLog(severity, __FILE__, __LINE__, __PRETTY_FUNCTION__)

#ifndef log_error

#define log_with_severity(severity) log_with_context(getDefaultContext(), severity)

/**
 * Generate a log with "fatal" severity
 */
#define log_fatal() log_with_context(getDefaultContext(), logging::LogLevel::Fatal)

/**
 * Generate a log with "error" severity
 */
#define log_error() log_with_context(getDefaultContext(), logging::LogLevel::Error)

/**
 * Generate a log with "verbose" severity
 */
#define log_verbose() log_with_context(getDefaultContext(), logging::LogLevel::Verbose)

/**
 * Generate a log with "info" severity
 */
#define log_info() log_with_context(getDefaultContext(), logging::LogLevel::Info)

/**
 * Generate a log with "warning" severity
 */
#define log_warn() log_with_context(getDefaultContext(), logging::LogLevel::Warning)
#define log_warning() log_warn()

/**
 * Generate a log with "debug" severity
 */
#define log_debug() log_with_context(getDefaultContext(), logging::LogLevel::Debug)

/**
 * Defines the identifiers of an application. This macro should be used at one
 * place in every application.
 */
#define LOG_DEFINE_APP_IDS(appID, appDescription) logging::AppLogContext s_appLogContext(appID, appDescription)

/**
 * Create a LogContext with the given ID (4 characters in case of DLT support)
 * and description
 */
#define LOG_DECLARE_CONTEXT(contextName, contextShortID, contextDescription) \
    LogContext contextName(contextShortID, contextDescription)

/**
 * Create a new context and define is as default context for the current scope
 */
#define LOG_DECLARE_DEFAULT_CONTEXT(context, contextID, contextDescription) \
    LOG_DECLARE_CONTEXT(context, contextID, contextDescription);            \
    LOG_SET_DEFAULT_CONTEXT(context)

/**
 * Import the given context, which should be exported by another module
 */
#define LOG_IMPORT_CONTEXT(contextName) extern LogContext contextName

/**
 * Set the given context as default for the current scope
 */
#define LOG_SET_DEFAULT_CONTEXT(context) \
    static std::function<LogContext&()> getDefaultContext = []() -> LogContext& { return context; }

/**
 * Import the given context and set it as default for the current scope
 */
#define LOG_IMPORT_DEFAULT_CONTEXT(context) \
    LOG_IMPORT_CONTEXT(context);            \
    LOG_SET_DEFAULT_CONTEXT(context)

/**
 * Set the given context as default for the current class
 */
#define LOG_SET_CLASS_CONTEXT(context) \
    static inline LogContext& getDefaultContext() { return context; }

/**
 *
 */
#define LOG_DECLARE_DEFAULT_LOCAL_CONTEXT(contextShortID, contextDescription)   \
    std::function<LogContext&()> getDefaultContext = []() -> LogContext& {      \
        static LogContext __defaultContext(contextShortID, contextDescription); \
        return __defaultContext;                                                \
    }

/**
 * Set a new context as default context for a class. To be used inside the class
 * definition.
 */
#define LOG_DECLARE_CLASS_CONTEXT(contextShortID, contextDescription)              \
    static LogContext& getDefaultContext()                                         \
    {                                                                              \
        static LogContext __defaultLogContext(contextShortID, contextDescription); \
        return __defaultLogContext;                                                \
    }

#endif

template <typename... Types> struct TypeSet
{
};

template <class, class> class LogContextT;

template <template <class...> class ContextTypesClass, class... ContextTypes,
    template <class...> class ContextDataTypesClass, class... LogDataTypes>
class LogContextT<ContextTypesClass<ContextTypes...>, ContextDataTypesClass<LogDataTypes...>> : public LogContextCommon
{
    struct setParentContextFunctor
    {
        template <typename T, typename ParentType> void operator()(T&& t, ParentType& parent)
        {
            t.setParentContext(parent);
        }
    };

    struct isEnabledFunctor
    {
        isEnabledFunctor(bool& b) : m_isEnabled(b) {}
        template <typename T> void operator()(T&& t, LogLevel level) { m_isEnabled |= t.isEnabled(level); }
        bool& m_isEnabled;
    };

    struct writeFormattedFunctor
    {
        template <typename T, typename... Args> void operator()(T&& t, const char* format, Args... args)
        {
            if (t.isEnabled()) // We need to check each context here to ensure that
                // we don't send data to a disabled context
                t.writeFormatted(format, args...);
        }
    };

    struct registerContextFunctor
    {
        template <typename T> void operator()(T&& t) { t.registerContext(); }
    };

    struct initDataFunctor
    {
        template <typename T>
        void operator()(T&& t,
            LogContextT<ContextTypesClass<ContextTypes...>, ContextDataTypesClass<LogDataTypes...>>& context,
            LogInfo& log)
        {
            if (t.isEnabled()) // We need to check each context here to ensure that
                // we don't send data to a disabled context
                t.init(context, log);
        }
    };

    struct streamFunctor
    {
        template <typename T, typename Type> void operator()(T&& t, const Type& v)
        {
            if (t.isEnabled()) // We need to check each context here to ensure that
                // we don't send data to a disabled context
                t << v;
        }
    };

public:
    /**
     * Use that typedef to extend a LogContext type by adding a backend to it.
     */
    template <typename ExtraContextType, typename ExtraDataType>
    using Extension = typename LogContextT<ContextTypesClass<ContextTypes..., ExtraContextType>,
        ContextDataTypesClass<LogDataTypes..., ExtraDataType>>::LogContextT;

    class LogData : LogInfo
    {
        template <size_t I = 0, typename... CallArgumentTypes>
        typename std::enable_if<I == sizeof...(ContextTypes)>::type for_each_init(std::tuple<LogDataTypes...>& tpl,
            LogContextT<ContextTypesClass<ContextTypes...>, ContextDataTypesClass<LogDataTypes...>>& context,
            CallArgumentTypes&... args)
        {
            UNUSED(context);
            UNUSED(tpl);
            UNUSED(args...);
        }

        template <size_t I = 0, typename... CallArgumentTypes>
            typename std::enable_if
            < I<sizeof...(ContextTypes)>::type for_each_init(std::tuple<LogDataTypes...>& tpl,
                  LogContextT<ContextTypesClass<ContextTypes...>, ContextDataTypesClass<LogDataTypes...>>& context,
                  CallArgumentTypes&... args)
        {
            std::get<I>(tpl).init(std::get<I>(context.m_contexts), args...);
            for_each_init<I + 1>(tpl, context, args...);
        }

    public:
        LogData(LogContextT<ContextTypesClass<ContextTypes...>, ContextDataTypesClass<LogDataTypes...>>& context,
            LogLevel level, const char* fileName, int lineNumber, const char* prettyFunction)
            : LogInfo(level, fileName, lineNumber, prettyFunction), m_context(context)
        {
            for_each_init(m_contexts, context, *this);
        }

        ~LogData() {}

        LogData& write() { return *this; }

        template <typename Arg1, typename... Args>
        LogData& write(const Arg1& firstArg, const Args&... remainingArguments)
        {
            if (m_context.isEnabled(getLogLevel())) {
                *this << firstArg;
                return write(remainingArguments...);
            }
            return *this;
        }

        template <typename... Args> LogData& writeFormatted(const char* format, Args... args)
        {
            for_each_in_tuple_(m_contexts, writeFormattedFunctor(), format, args...);
            return *this;
        }

        template <typename Type> LogData& operator<<(const Type& v)
        {
            for_each_in_tuple_(m_contexts, streamFunctor(), v);
            return *this;
        }

        /**
         * Used to support std::endl, std::ends, etc...
         */
        LogData& operator<<(LogData& (*functionPtr)(LogData&))
        {
            functionPtr(*this);
            return *this;
        }

        std::tuple<LogDataTypes...> m_contexts;
        LogContextT<ContextTypesClass<ContextTypes...>, ContextDataTypesClass<LogDataTypes...>>& m_context;
    };

    LogContextT(const std::string& id, const std::string& contextDescription) : LogContextCommon(id, contextDescription)
    {
        for_each_in_tuple_(m_contexts, setParentContextFunctor(), *this);
    }

    LogData createLog(LogLevel level, const char* fileName, int lineNumber, const char* prettyFunction)
    {
        return LogData(*this, level, fileName, lineNumber, prettyFunction);
    }

    bool isEnabled(LogLevel level)
    {
        checkContext();
        bool enabled = false;
        for_each_in_tuple_(m_contexts, isEnabledFunctor(enabled), level);
        return enabled;
    }

    void checkContext()
    {
        if (!m_bRegistered) {
            setDefaultAPPIDSIfNeeded();
            for_each_in_tuple_(m_contexts, registerContextFunctor());
            m_bRegistered = true;
        }
    }

    std::tuple<ContextTypes...> m_contexts;
    bool m_bRegistered = false;
};

/**
 * Use this context definition to completely disable the logging
 */
typedef LogContextT<TypeSet<>, TypeSet<>> NoLoggingLogContext;
} // namespace logging

#include "ivi-logging-null.h"
#include "ivi-logging-types.h"

#include "ivi-logging-config.h"
