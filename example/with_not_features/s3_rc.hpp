#include <concepts>
#include <exception>
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

auto factorial(unsigned_integral auto n)
{
  return (n == 0 ? 1 : unsigned_checked_product(n, factorial<T>(n - 1)));
}
