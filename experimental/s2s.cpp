// Compile with g++ -std=c++2b s2s.cpp
// 
#include <iostream>
#include <string>
#include <algorithm>
#include <ranges>
#include <stdexcept>

using namespace std;

constexpr bool logging { true };

constexpr char opening_parenthesis { '(' };
constexpr char closing_parenthesis { ')' };
constexpr char separator { '\t' };

using st = string::size_type;

string trim_parentheses(string s)
{
   if (s.length() == 0)
   {
      return s;
   }
   auto open { ranges::count(s,opening_parenthesis) },
        closed { ranges::count(s,closing_parenthesis) };

   if constexpr (logging)
   {
      cout << "s = " << s << '\n'
           << '#' << opening_parenthesis << " = " << open << '\n'
           << '#' << closing_parenthesis << " = " << closed 
           << endl;
   }
   
   while (open > closed)
   {
      if (s.at(0) == opening_parenthesis)
      {
         s.erase(0,1);
         --open;
      }
      else if (s.at(s.length() - 1) == opening_parenthesis)
      {
         s.erase(s.length() - 1,1);
         --open;
      }
      else 
      {
         throw domain_error("Missing "s + opening_parenthesis + 
                            "at beginning of " + s + '.');
      }
   }
   while (closed > open)
   {
      if (s.at(s.length() - 1) == closing_parenthesis)
      {
         s.erase(s.length() - 1,1);
         --closed;
      }
      else if (s.at(0) == closing_parenthesis)
      {
         s.erase(0,1);
         --closed;
      }
      else
      {
         throw domain_error("Missing "s + closing_parenthesis + 
                            "at end of " + s + '.');
      }
   }
   return s;
}
string lcs(string s1,string s2)
{
   if constexpr (logging)
   { 
      cout << "Initially" << '\n'
           << "s1 = " << s1 << '\n'
           << "s2 = " << s2 << endl;
   }


   if (s1.length() > s2.length())
   {
      swap(s1,s2);
   }

   if constexpr (logging)
   { 
      cout << "After swapping" << '\n'
           << "s1 = " << s1 << '\n'
           << "s2 = " << s2 << endl;
   }

   st found_index { },
      found_length { };

   for (st index { }; index < s1.length(); ++index)
   {
      for (st len { 1 }; len < s1.length() - index + 1; ++len)
      {
         if constexpr (logging)
         {
            cout << "s1.length() = " << s1.length() << '\n'
                 << "index = " << index << " len = " << len
                 << " s1.substr() = " << s1.substr(index,len) << endl;
         }
         if (s2.contains(s1.substr(index,len)))
         {
            if (len > found_length)
            {
               found_index = index;
               found_length = len;
               if constexpr (logging)
               {
                  cout << "Memorized found_index = " << found_index
                       << " found_length = " << found_length 
                       << endl;
               }
            }
         }
      }
   }
   return trim_parentheses(s1.substr(found_index,found_length));
}

string intersect(string s1,string s2)
{
   string result { },
          lc_seq { };
   while ((lc_seq = lcs(s1,s2)) != ""s)   
   {
      if constexpr(logging)
      {
         cout << "s1 = " << s1 
              << " s2 = " << s2 
              << " lc_seq = " << lc_seq
              << endl;
      }
      if (result.length() > 0)
      {
         result += separator;
      }
      result += lc_seq;
      s1.erase(s1.find(lc_seq),lc_seq.length());
      s2.erase(s2.find(lc_seq),lc_seq.length());
      if constexpr(logging)
      {
         cout << "result = " << result
              << " s1 = " << s1
              << " s2 = " << s2
              << endl;
      }
   }
   return result;
}

string unite(string s1,string s2)
{
   return "UNIFICATION";
}

string subtract(string s1,string s2)
{
   return "DIFFERENCE";
}

int main()
{
   // cout << lcs("ABC|def","*ABef+") << endl;
   // cout << trim_parentheses("(No( parenthe)))ses.)") << endl; // throws
   // cout << lcs("(nota(b))","(a(b))") << endl;
   // cout << lcs("","") << endl;
   cout << intersect("(nota(notb))","(nota(b))") << endl;
   // cout << intersect("","") << endl;
}
