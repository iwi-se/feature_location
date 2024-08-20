#include <concepts>

template<std::unsigned_integral T> T factorial(T n)
{
  return (n == 0 ? 1 : n * factorial(n - 1));
}
