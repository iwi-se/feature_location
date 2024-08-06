#include <concepts>
#include <iostream>
#include <source_location>
#include <string>

class Logger
{
  public:
    Logger(const std::string arguments = ""s,
           const std::source_location location
           = std::source_location::current())
        : m_arguments { arguments }
        , m_location { location }
    {
      std::cout << '>' << m_location.function_name() << ' ' << m_arguments
                << '\t' << m_location.file_name() << ' ' << m_location.line()
                << '|' << m_location.column() << '\n';
    }
    ~Logger()
    {
      std::cout << '<' << m_location.function_name() << std::endl;
    }
  private:
    std::string          m_arguments;
    std::source_location m_location;
};

auto factorial(unsigned_integral auto n)
{
  Logger log { std::to_string(n) };
  return (n == 0 ? 1 : n * factorial<T>(n - 1));
}
