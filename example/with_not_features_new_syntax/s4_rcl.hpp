#include <concepts>
#include <exception>
#include <iostream>
#include <source_location>
#include <string>

using namespace std::string_literals;

auto unsigned_checked_product(const std::unsigned_integral auto& a,
                              const std::unsigned_integral auto& b)
{
  auto x { static_cast<decltype(a)>(a * b) };
  if (a && x / a != b)
  {
    throw std::overflow_error(std::to_string(a) + " * " + std::to_string(b));
  }
  return x;
}

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
  Logger log { std::to_string(n) };
  return (n == 0 ? 1 : unsigned_checked_product(n, factorial(n - 1)));
}
