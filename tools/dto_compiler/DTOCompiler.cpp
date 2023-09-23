#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>

#include "lex.hpp"

namespace
{

void printInvocation(std::ostream& OS)
{
  OS << R"EOF(
Usage: dto_compiler <input_file>

    input_file: The input file, written in DTO DSL, to parse and generate output
                from.
)EOF";
}

} // namespace

int main(int ArgC, char* ArgV[])
{
  std::cout << ArgC << std::endl;
  std::copy(
    ArgV, ArgV + ArgC, std::ostream_iterator<const char*>(std::cout, " "));
  std::cout << std::endl;

  // std::string File{ArgV[2]};
  // File.append("/").append("Dummy.cpp");

  // std::cout << File << std::endl;

  // std::ofstream Out{File, std::ios::trunc};
  // Out << "#include <iostream>\nint func(int, char**)\n{\n  std::cerr << "
  //        "\"Dummy func() hit.\";\n  return 1;\n}\n";

  std::string InputBuffer;
  {
    std::ifstream Input{ArgV[1]};
    if (!Input.is_open())
    {
      std::cerr << "ERROR! Failed to open input file: '" << ArgV[1] << '\''
                << std::endl;
      return EXIT_FAILURE;
    }

    std::stringstream Buf;
    Buf << Input.rdbuf();
    InputBuffer = Buf.str();
  }

  std::cout << "Input string:\n" << InputBuffer << std::endl;

  using namespace monomux::tools::dto_compiler;
  lexer L{InputBuffer};

  token T{};
  while ((T = L.lex()) != token::EndOfFile)
  {
    std::cout << to_string(L.get_token_info_raw()) << std::endl;

    if (T == token::SyntaxError)
    {
      std::cerr << "ERROR!" << std::endl;
      return EXIT_FAILURE;
    }
  }
}
