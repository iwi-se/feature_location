#include <concepts>
#include <exception>
#include <iostream>
#include <source_location>
#include <string>

auto unsigned_checked_product(const unsigned_integral auto& a,
                              const unsigned_integral auto& b)
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
  Logger      log { std::to_string(n) };
  decltype(n) result { 1 };
  while (n > 1)
  {
    result = unsigned_checked_product(result, n--);
  }
  return result;
}
