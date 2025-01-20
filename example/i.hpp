#include <concepts>

auto factorial(std::unsigned_integral auto n)
{
  decltype(n) result { 1 };
  while (n > 1)
  {
    result *= n--;
  }
  return result;
}
