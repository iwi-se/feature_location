#include <concepts>

template<std::unsigned_integral T> T factorial(T n)
{
  decltype(n) result { 1 };
  while (n > 1)
  {
    result *= n--;
  }
  return result;
}
