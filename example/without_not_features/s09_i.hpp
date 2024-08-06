#include <concepts>

auto factorial(unsigned_integral auto n)
{
  decltype(n) result { 1 };
  while (n > 1)
  {
    result *= n--;
  }
  return result;
}
