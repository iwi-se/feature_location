#ifndef UTILITY_HPP
#include <iostream>

template<typename... Args> void printDebug(bool print, Args... output)
{
  if (print)
  {
    ((std::cout << ... << output) << std::flush);
  }
}

#endif // UTILITY_HPP
