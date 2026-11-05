// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <istream>
#include <stdexcept>
#include <string>
#include <vector>

namespace projectcars
{
    namespace text
    {
        struct Token
        {
            std::string value;
            bool quoted;
        };

        struct Location
        {
            size_t line;
            std::string context;
        };

        class Error : public std::runtime_error
        {
        public:
            Error(const std::string& source, size_t line,
                  const std::string& context, const std::string& detail);

            const std::string& Source() const;
            size_t Line() const;
            const std::string& Context() const;
            const std::string& Detail() const;

        private:
            std::string source_;
            size_t line_;
            std::string context_;
            std::string detail_;
        };

        __declspec(noreturn) void Fail(const std::string& source, const Location& location,
                                      const std::string& detail);
        bool ReadLine(std::istream& input, const std::string& source,
                      size_t& lineNumber, Location& location);
        std::vector<Token> Tokenize(const std::string& source, const Location& location);
        void Arity(const std::string& source, const Location& location,
                   const std::vector<Token>& tokens, size_t expected);
        void Identifier(const std::string& source, const Location& location,
                        const std::string& value);
        bool Finite(double value);
        double Number(const std::string& source, const Location& location,
                      const std::string& value, const char* field);
        void Nonnegative(const std::string& source, const Location& location,
                         double value, const char* field);
        void Positive(const std::string& source, const Location& location,
                      double value, const char* field);
        unsigned int Unsigned(const std::string& source, const Location& location,
                              const std::string& value, const char* field);
    }
}
