#include <exception>
#include <concepts>
#include <string>

auto unsigned_checked_product(const unsigned_integral auto& a,
                              const unsigned_integral auto& b)
{
   auto x { static_cast<decltype(a)>(a * b) };
   if (a && x / a != b)
   {
      throw std::overflow_error(std::to_string(a) + " * " +
                                std::to_string(b)
                               );
   }
   return x;
}

template <typename T>
T factorial(T n) requires unsigned_integral<T>
{
   return (n == 0 ? 
              1 : 
              unsigned_checked_product(n,factorial<T>(n - 1))
          );
}
