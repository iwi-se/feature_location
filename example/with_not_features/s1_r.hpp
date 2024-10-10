#include <concepts>

auto factorial(std::unsigned_integral auto n) -> decltype(n)
{
  return (n == 0 ? 1 : n * factorial(n - 1));
}
