#include <concepts>

template <typename T>
T factorial(T n) requires unsigned_integral<T>
{
   return (n == 0 ? 
              1 : 
              n * factorial<T>(n - 1));
}
