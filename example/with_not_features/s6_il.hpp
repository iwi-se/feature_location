#include <concepts>
#include <iostream>
#include <source_location>
#include <string>

using namespace std::string_literals;

class Logger
{
  public:
    Logger(const std::string arguments = "",
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

template<std::unsigned_integral T> T factorial(T n)
{
  Logger      log { std::to_string(n) };
  decltype(n) result { 1 };
  while (n > 1)
  {
    result *= n--;
  }
  return result;
}
