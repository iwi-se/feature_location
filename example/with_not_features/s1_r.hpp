#include <concepts>

auto factorial(unsigned_integral auto n)
{
  return (n == 0 ? 1 : n * factorial<T>(n - 1));
}
