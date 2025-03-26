#include <iostream>
#include <vector>
#include <string>
using namespace std;

int main()
{
   vector<string> v;
   v.push_back("EINS");
   v.push_back("zwei");
   v.push_back("DrEi");
   for (auto e : v)
      cout << e << endl;
   for (size_t i { 0 }; i < v.size(); ++i)
      v.push_back(v.at(i) + "LOOP");
   for (auto e : v)
      cout << e << endl;
}
