// SPDX-License-Identifier: GPL-3.0-only
#include "catalog.h"
#include "text.h"

#include <limits>
#include <locale>
#include <sstream>

namespace projectcars
{
    namespace text
    {
        namespace
        {
            std::string ErrorMessage(const std::string& source, size_t line,
                                     const std::string& context, const std::string& detail)
            {
                std::ostringstream message;
                message.imbue(std::locale::classic());
                message << source << ':' << line << ": " << detail << " [context: ";
                const char* hex = "0123456789abcdef";
                for (size_t i = 0; i < context.size(); ++i)
                {
                    const unsigned char ch = static_cast<unsigned char>(context[i]);
                    if (ch < 32 || ch == 127)
                        message << "\\x" << hex[ch >> 4] << hex[ch & 15];
                    else
                        message << context[i];
                }
                message << ']';
                return message.str();
            }

            bool Space(char value)
            {
                return value == ' ' || value == '\t' || value == '\r' ||
                       value == '\v' || value == '\f';
            }

            bool Digit(char value)
            {
                return value >= '0' && value <= '9';
            }

            bool Decimal(const std::string& value)
            {
                size_t pos = 0;
                if (pos < value.size() && (value[pos] == '+' || value[pos] == '-'))
                    ++pos;
                size_t digits = 0;
                while (pos < value.size() && Digit(value[pos]))
                {
                    ++pos;
                    ++digits;
                }
                if (pos < value.size() && value[pos] == '.')
                {
                    ++pos;
                    while (pos < value.size() && Digit(value[pos]))
                    {
                        ++pos;
                        ++digits;
                    }
                }
                if (digits == 0)
                    return false;
                if (pos < value.size() && (value[pos] == 'e' || value[pos] == 'E'))
                {
                    ++pos;
                    if (pos < value.size() && (value[pos] == '+' || value[pos] == '-'))
                        ++pos;
                    const size_t begin = pos;
                    while (pos < value.size() && Digit(value[pos]))
                        ++pos;
                    if (pos == begin)
                        return false;
                }
                return pos == value.size();
            }

            bool NonzeroMantissa(const std::string& value)
            {
                for (size_t i = 0; i < value.size() && value[i] != 'e' && value[i] != 'E'; ++i)
                    if (value[i] >= '1' && value[i] <= '9')
                        return true;
                return false;
            }
        }

        Error::Error(const std::string& source, size_t line,
                     const std::string& context, const std::string& detail)
            : std::runtime_error(ErrorMessage(source, line, context, detail)),
              source_(source), line_(line), context_(context), detail_(detail) {}

        const std::string& Error::Source() const { return source_; }
        size_t Error::Line() const { return line_; }
        const std::string& Error::Context() const { return context_; }
        const std::string& Error::Detail() const { return detail_; }

        __declspec(noreturn) void Fail(const std::string& source, const Location& location,
                                      const std::string& detail)
        {
            throw Error(source, location.line, location.context, detail);
        }

        bool ReadLine(std::istream& input, const std::string& source,
                      size_t& lineNumber, Location& location)
        {
            std::string line;
            try
            {
                if (!std::getline(input, line))
                {
                    if (input.bad() || !input.eof())
                        throw Error(source, lineNumber + 1, "<input>", "input read failed");
                    return false;
                }
            }
            catch (const std::ios_base::failure& error)
            {
                if (!input.eof() || input.bad())
                    throw Error(source, lineNumber + 1, "<input>",
                                std::string("input read failed: ") + error.what());
                if (line.empty())
                    return false;
            }

            ++lineNumber;
            if (lineNumber == 1 && line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xef &&
                static_cast<unsigned char>(line[1]) == 0xbb &&
                static_cast<unsigned char>(line[2]) == 0xbf)
                line.erase(0, 3);
            location.line = lineNumber;
            location.context = line;
            return true;
        }

        std::vector<Token> Tokenize(const std::string& source, const Location& location)
        {
            const std::string& line = location.context;
            std::vector<Token> tokens;
            size_t pos = 0;
            while (pos < line.size())
            {
                if (Space(line[pos]))
                {
                    ++pos;
                    continue;
                }
                if (line[pos] == '#')
                    break;

                Token token;
                token.quoted = line[pos] == '"';
                if (token.quoted)
                {
                    const size_t begin = ++pos;
                    while (pos < line.size() && line[pos] != '"')
                    {
                        if (static_cast<unsigned char>(line[pos]) < 32 || line[pos] == 127)
                            Fail(source, location, "control character in quoted binding");
                        ++pos;
                    }
                    if (pos == line.size())
                        Fail(source, location, "unterminated quoted binding");
                    token.value = line.substr(begin, pos - begin);
                    ++pos;
                    if (pos < line.size() && !Space(line[pos]) && line[pos] != '#')
                        Fail(source, location, "characters after quoted binding");
                }
                else
                {
                    const size_t begin = pos;
                    while (pos < line.size() && !Space(line[pos]) && line[pos] != '#')
                    {
                        if (line[pos] == '"' || static_cast<unsigned char>(line[pos]) < 32 ||
                            line[pos] == 127)
                            Fail(source, location, "invalid character in unquoted token");
                        ++pos;
                    }
                    token.value = line.substr(begin, pos - begin);
                }
                tokens.push_back(token);
            }
            return tokens;
        }

        void Arity(const std::string& source, const Location& location,
                   const std::vector<Token>& tokens, size_t expected)
        {
            if (tokens.size() != expected)
            {
                std::ostringstream detail;
                detail.imbue(std::locale::classic());
                detail << tokens[0].value << " expects " << expected - 1
                       << " fields, got " << tokens.size() - 1;
                Fail(source, location, detail.str());
            }
        }

        void Identifier(const std::string& source, const Location& location,
                        const std::string& value)
        {
            if (value.empty())
                Fail(source, location, "empty identifier");
            for (size_t i = 0; i < value.size(); ++i)
            {
                const char ch = value[i];
                if (!Digit(ch) && !(ch >= 'A' && ch <= 'Z') &&
                    !(ch >= 'a' && ch <= 'z') && ch != '_' && ch != '-')
                    Fail(source, location, "invalid identifier '" + value +
                         "'; expected ASCII letters, digits, '_' or '-'");
            }
        }

        bool Finite(double value)
        {
            return value == value && value <= (std::numeric_limits<double>::max)() &&
                   value >= -(std::numeric_limits<double>::max)();
        }

        double Number(const std::string& source, const Location& location,
                      const std::string& value, const char* field)
        {
            const std::string detail = std::string(field) +
                " must be a finite, representable decimal number (got '" + value + "')";
            if (!Decimal(value))
                Fail(source, location, detail);

            std::istringstream input(value);
            input.imbue(std::locale::classic());
            double result = 0;
            input >> result;
            if (input.fail() || !input.eof() || !Finite(result) ||
                (result == 0 && NonzeroMantissa(value)))
                Fail(source, location, detail);
            return result;
        }

        void Nonnegative(const std::string& source, const Location& location,
                         double value, const char* field)
        {
            if (!Finite(value))
                Fail(source, location, std::string(field) + " must be finite");
            if (value < 0)
                Fail(source, location, std::string(field) + " must be nonnegative");
        }

        void Positive(const std::string& source, const Location& location,
                      double value, const char* field)
        {
            if (!Finite(value))
                Fail(source, location, std::string(field) + " must be finite");
            if (value <= 0)
                Fail(source, location, std::string(field) + " must be positive");
        }

        unsigned int Unsigned(const std::string& source, const Location& location,
                              const std::string& value, const char* field)
        {
            if (value.empty())
                Fail(source, location, std::string(field) + " must be an unsigned decimal integer");
            unsigned int result = 0;
            for (size_t i = 0; i < value.size(); ++i)
            {
                if (!Digit(value[i]))
                    Fail(source, location, std::string(field) + " must be an unsigned decimal integer");
                const unsigned int digit = static_cast<unsigned int>(value[i] - '0');
                if (result > ((std::numeric_limits<unsigned int>::max)() - digit) / 10)
                    Fail(source, location, std::string(field) +
                         " exceeds the 32-bit unsigned integer range");
                result = result * 10 + digit;
            }
            return result;
        }
    }
}
