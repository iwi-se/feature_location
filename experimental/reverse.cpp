#include <string>
#include <iostream>
#include <algorithm>
#include <ranges>
using namespace std;

int main()
{
   string s;
   while (cin)
   {
      getline(cin,s);
      if (cin)
      {
         ranges::reverse(s);
         cout << s << endl;
      }
   }
}
