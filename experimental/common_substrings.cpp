#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <fstream>

using namespace std;

using size_t = string::size_type;

struct string_info_t
{
   size_t id_self { },
          id_parent { },
          pos { },
          len { };
};

using occurences_t = vector<string_info_t>;

struct all_strings_t
{
   vector<string> m_strings;
   occurences_t m_occurences;
   bool add(string s);
   void add(string s,size_t id_parent,size_t pos,size_t len);
   void find_substrings(size_t from,size_t to);
};

bool all_strings_t::add(string s)
{
   if (ranges::find(m_strings,s) != m_strings.end())
      return false;
   m_strings.push_back(s);
   return true;
};

void all_strings_t::add(string s,size_t id_parent,size_t pos,size_t len)
{
   if (id_parent >= m_strings.size())
      throw out_of_range("ID "s + to_string(id_parent) + "is out of range "s +
                         "all_strings_t::add(string,size_t,size_t,size_t)."s);
   size_t id_self { };
   auto position { ranges::find(m_strings,s) };
   if (position == m_strings.end())
   {
      m_strings.push_back(s);
      id_self = m_strings.size() - 1;
   }
   else
      id_self = position - m_strings.begin();
   m_occurences.push_back({ id_self,id_parent,pos,len });
}

void all_strings_t::find_substrings(size_t from,size_t to)
{
   if (from > to || from >= m_strings.size())
      throw range_error("Illegal range ["s + to_string(from) +
                        ".."s + to_string(to) + "."s);
}

all_strings_t all_strings;

int main(int argc,char* args[])
{
   if (argc != 2)
      throw invalid_argument("argc != 2 in main()");
   ifstream input { args[1] };
   if (!input)
      throw invalid_argument("File \""s + args[1] + " not found!"s);
   all_strings_t all_strings;
   while (!input)
   {
      string s;
      getline(input,s);
      if (input)
         all_strings.add(s);
   }
}



