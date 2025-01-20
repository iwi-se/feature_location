#include <concepts>
#include <exception>
#include <stdexcept>
#include <string>

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

auto factorial(std::unsigned_integral auto n) -> decltype(n)
{
  return (n == 0 ? 1 : unsigned_checked_product(n, factorial(n - 1)));
}
