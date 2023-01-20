#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>

int main(int ArgC, char* ArgV[])
{
  std::cout << ArgC << std::endl;
  std::copy(
    ArgV, ArgV + ArgC, std::ostream_iterator<const char*>(std::cout, " "));
  std::cout << std::endl;

  std::string File{ArgV[2]};
  File.append("/").append("Dummy.cpp");

  std::ofstream Out{File, std::ios::trunc};
  Out << "#include <iostream>\nint func(int, char**)\n{\n  std::cerr << "
         "\"Dummy func() hit.\";\n  return 1;\n}\n";
}
