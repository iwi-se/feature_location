#include "expression.hpp"

bool operator==(const BasePlusRelativePath &lhs, const BasePlusRelativePath &rhs)
{
  return lhs.relative == rhs.relative;
}

bool operator<(const BasePlusRelativePath &lhs, const BasePlusRelativePath &rhs)
{
  return lhs.relative < rhs.relative;
}
