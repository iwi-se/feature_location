#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <fstream>
#include <memory>
#include <set>
#include <functional>

using namespace std;

constexpr char sep { '\t' };
constexpr char blank { ' ' };

using size_t = string::size_type;

struct occurence_t
{   size_t m_id_parent { },
           m_position { };
};

template <>
struct std::less<occurence_t>
{
   bool operator()(const occurence_t& lhs,const occurence_t& rhs) const
   {
      return lhs.m_id_parent < rhs.m_id_parent &&
             lhs.m_position < rhs.m_position;

   }
};

using occurences_t = set<occurence_t>;

struct extended_string_t
{
   size_t m_id_self { };
   occurences_t m_occurences;
   string m_string { };

};

using extended_strings_t = vector<extended_string_t>;

struct all_strings_t
{
   extended_strings_t m_strings { };
   bool add(string s);
   void add(extended_strings_t& copy_m_strings,string s,
            size_t id_parent1,size_t pos1,
            size_t id_parent2,size_t pos2);
   bool find_substrings(extended_strings_t& copy_m_strings,
                        const extended_string_t& first,
                        const extended_string_t& second);
   bool process(size_t from,size_t to);
   void export_csv(ofstream& out) const;
   void export_dot(ofstream& out) const;
};

bool all_strings_t::add(string s)
{
   if (s.length() == 0 || 
       ranges::find_if(m_strings,[s](extended_string_t xs)
                         { return xs.m_string == s; } ) !=
          m_strings.end()
      )
   return false;
   extended_string_t xs { m_strings.size(),{ },s };
   xs.m_occurences.insert({ m_strings.size(),0 });
   m_strings.push_back(xs);
   return true;
};

void all_strings_t::add(extended_strings_t& copy_m_strings,string s,
                        size_t id_parent1,size_t pos1,
                        size_t id_parent2,size_t pos2)
{
   if (id_parent1 >= m_strings.size() || id_parent2 >= m_strings.size())
      throw out_of_range("ID "s + to_string(id_parent1) + "is out of range "s +
                         "all_strings_t::add(string,size_t,size_t,size_t,size_t)."s);
   auto position { ranges::find_if(copy_m_strings,
                      [s](extended_string_t xs)
                      { return xs.m_string == s; } )
                 };
   if (position == copy_m_strings.end())
   {
      extended_string_t xs { copy_m_strings.size(),{ },s };
      xs.m_occurences.insert({id_parent1,pos1 });
      xs.m_occurences.insert({id_parent2,pos2 });
      copy_m_strings.push_back(xs);
   }
   else
   {
      position->m_occurences.insert({ id_parent1,pos1 });
      position->m_occurences.insert({ id_parent2,pos2 });
   }
}

bool all_strings_t::find_substrings(extended_strings_t& copy_m_strings,
                                    const extended_string_t& first,
                                    const extended_string_t& second)
{
//   cout << "[" << first.m_string << "]\t[" << second.m_string << "]" << endl;
   bool stop { true };
   if (first.m_string.size() > second.m_string.size())
      throw invalid_argument ("first.m_string.length() > second.m_string.length()"
                              " in all_strings_t::find_substrings("
                              "const extendend_string_t&,const_extended_string_t&).");
   for (auto row { 0 };row <= first.m_string.length() - 1;++row)
   {
      for (auto col { 0 };col <= second.m_string.length() - 1;++col)
      {
         size_t diagonal { };
         if ((row == 0 || col == 0) || (row > 0 && col > 0 && first.m_string.at(row - 1) != second.m_string.at(col - 1)))
         while (row + diagonal < first.m_string.size() &&
                col + diagonal < second.m_string.size() &&
                first.m_string.at(row + diagonal) == second.m_string.at(col + diagonal))
         {
            ++diagonal;
         }
         if (diagonal)
         {
            auto substring { second.m_string.substr(col,diagonal) };
            /*cout << substring 
                     << " id1 " << first.m_id_self << " pos " << row << " " << first.m_string
                     << " id2 " << second.m_id_self << " pos " << col  << " " << second.m_string
                     << endl; */
            add(copy_m_strings,substring,first.m_id_self,row,second.m_id_self,col);
            stop = false;
         }
      }
   }
   return stop;
}

bool all_strings_t::process(size_t from,size_t to)
{
   bool stop { true };
   if (from > to || from >= m_strings.size())
      throw range_error("Illegal range ["s + to_string(from) +
                        ".."s + to_string(to) +
                        " in all_strings_t::process(size_t,size_t)."s);
   auto copy_m_strings { m_strings };
   for (auto i { from };i < to;++i)
   {
//      for (auto j { i + 1};j <= to;++j)
      for (auto j { i };j <= to;++j)
      {
         auto id1 { i },
              id2 { j };
         if (m_strings.at(i).m_string.length() > m_strings.at(j).m_string.length())
           swap(id1,id2);
         auto& first { m_strings.at(id1) };
         auto& second { m_strings.at(id2) };
         if (find_substrings(copy_m_strings,first,second))
            stop = false;
      }
   }
   m_strings = copy_m_strings;
   return stop;
}

void all_strings_t::export_csv(ofstream& out) const
{
   out << "ID" << sep << "SIZE" << sep << "S" << sep << "#OCC" << sep << "OCC" << endl;
   for (const auto& e : m_strings)
   {
      out << e.m_id_self << sep << e.m_string.size() << sep 
          << e.m_string << sep << e.m_occurences.size();
      for (auto& o : e.m_occurences)
      {
         out << sep << o.m_id_parent << sep << o.m_position;
      }
      out << endl;
   }
}

void all_strings_t::export_dot(ofstream& out) const
{
   out << "digraph substrings\n{" << endl;
   for (const auto& e : m_strings)
   {
      out << sep << e.m_id_self << " [ label = \"" << e.m_string << "\" ]\n";
      for (auto& o : e.m_occurences)
      {
        if (e.m_id_self != o.m_id_parent)
        {
           out << sep << o.m_id_parent <<  " -> { " << e.m_id_self << " }\n";
        }
      }
   }
   out << '}' << endl;
}

int main(int argc,char* args[])
{
   if (argc != 3)
      throw invalid_argument("argc != 3 in main()");
   ifstream input { args[1] };
   if (!input)
      throw invalid_argument("File \""s + args[1] + " not found!"s);
   ofstream csv { string { args[2] } + ".csv" };
   if (!csv)
      throw invalid_argument("File \""s + args[2] + ".csv cannot be opened!"s);
   ofstream dot { string { args[2] } + ".viz" };
   if (!dot)
      throw invalid_argument("File \""s + args[2] + ".viz cannot be opened!"s);
   all_strings_t all_strings;
   while (input)
   {
      string s;
      getline(input,s);
      if (input)
         all_strings.add(s);
   }
   while (all_strings.process(0,all_strings.m_strings.size() - 1));
   all_strings.export_csv(csv);
   all_strings.export_dot(dot);
}
