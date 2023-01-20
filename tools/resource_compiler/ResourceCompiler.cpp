/* SPDX-License-Identifier: GPL-3.0-only */
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "monomux/Log.hpp"
#include "monomux/system/CurrentPlatform.hpp"

namespace
{

void printInvocation(std::ostream& OS)
{
  OS << R"EOF(
Usage: resource_compiler <root_dir> <header_copy_location> <output_cpp>
           [name=file [name=file]]...

    root_dir: The root directory where the **SOURCE CODE** for
              resource_compiler is available.
              NOTE: This project is a build-time tool and not a user-facing
              binary, so the source code must ALWAYS be available.

    header_copy_location: The location where the header file that allows client
                          code to interface with the resources system should be
                          copied to.

    output_cpp: Path to the generated output file. Will be overwritten!

    name=file: For each resource to be embedded, the built 'name' of the
               resource, followed by the path of the source file to embed.
)EOF";
  OS << std::endl;
}

#ifdef MONOMUX_PLATFORM_UNIX
constexpr char PathSeparator = '/';
#endif /* MONOMUX_PLATFORM_UNIX */

struct GenerationConfiguration
{
  struct Resource
  {
    std::string UserName;
    std::string MachineName;
    std::string InputPath;
  };

  std::string HeaderCopyLocation;
  std::vector<Resource> Resources;

  void addResource(std::string Key, std::string Path);
};

std::vector<char> readFile(std::string File);
void copyFileRaw(std::string From, std::string To);
void generateImplementationFile(std::string Source,
                                std::string Target,
                                const GenerationConfiguration& Cfg);

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): It's fine if this crashes.
int main(int ArgC, char* ArgV[])
{
  if (ArgC < 3)
  {
    std::cerr << "ERROR: Must specify at least 3 arguments!\n";
    printInvocation(std::cerr);
    return EXIT_FAILURE;
  }

  GenerationConfiguration Cfg;

  for (std::size_t I = 4; I < static_cast<std::size_t>(ArgC); ++I)
  {
    const std::string_view Arg = ArgV[I];
    std::cout << Arg << std::endl;
    const auto FirstEqualSign = Arg.find('=');
    if (FirstEqualSign == std::string_view::npos)
    {
      std::cerr << "ERROR: Invalid resource specifier: '" << Arg << "'\n";
      printInvocation(std::cerr);
      return EXIT_FAILURE;
    }

    Cfg.addResource(std::string{Arg.substr(0, FirstEqualSign)},
                    std::string{Arg.substr(FirstEqualSign + 1)});
  }

  const std::string RootPath = ArgV[1];
  const std::string HeaderOutPath = ArgV[2];
  Cfg.HeaderCopyLocation = HeaderOutPath;
  copyFileRaw(RootPath + PathSeparator + "EmbeddedResources.hpp",
              HeaderOutPath);

  const std::string OutputPath = ArgV[3];
  generateImplementationFile(
    RootPath + PathSeparator + "EmbeddedResources.cpp", OutputPath, Cfg);
}

namespace
{

std::vector<char> readFile(std::string File)
{
  std::ifstream Input{File, std::ios::in | std::ios::binary};
  if (!Input.is_open())
  {
    std::cerr << "ERROR! Failed to open resource file: ' " << File << '\''
              << std::endl;
    throw File;
  }

  std::vector<char> Ret{std::istreambuf_iterator<char>(Input),
                        std::istreambuf_iterator<char>{}};
  return Ret;
}

void copyFileRaw(std::string From, std::string To)
{
  std::ifstream Input{From, std::ios::in};
  if (!Input.is_open())
  {
    std::cerr << "ERROR! Failed to open input file: ' " << From << '\''
              << std::endl;
    throw From;
  }

  std::ofstream Output{To, std::ios::out | std::ios::trunc};
  if (!Output.is_open())
  {
    std::cerr << "ERROR! Failed to open output file: '" << To << '\''
              << std::endl;
    throw To;
  }

  std::string Line;
  while (std::getline(Input, Line, '\n'))
    Output << Line << '\n';
}

using ActionMap = std::map<std::string, std::function<void()>>;

bool tryLineAsReplaceDirective(const std::string& Line, const ActionMap& Acts)
{
  static constexpr std::string_view ReplaceDirective =
    "EMBEDDED_RESOURCES_REPLACE_THIS_WITH";
  const auto FirstNonWhitespacePos = Line.find_first_not_of(" \t");
  const auto ReplaceMacroPos =
    Line.find(ReplaceDirective, FirstNonWhitespacePos);

  if (ReplaceMacroPos != FirstNonWhitespacePos ||
      FirstNonWhitespacePos == std::string::npos)
    return false;
  if (Line.substr(ReplaceMacroPos + ReplaceDirective.size(), 2) != "(\"")
    return false;

  const auto ReplaceDirectivePos =
    ReplaceMacroPos + ReplaceDirective.size() + std::strlen("(\"");
  const auto ReplaceDirectiveEndPos = Line.find("\")", ReplaceDirectivePos);

  const std::string_view ReplaceDirectiveArg = std::string_view{Line}.substr(
    ReplaceDirectivePos, ReplaceDirectiveEndPos - ReplaceDirectivePos);

  if (const auto It = Acts.find(std::string{ReplaceDirectiveArg});
      It != Acts.end())
  {
    It->second();
    return true;
  }

  std::cerr << "ERROR! Encountered unknown replacement directive '"
            << ReplaceDirectiveArg << "' in input text" << std::endl;
  throw std::string{ReplaceDirectiveArg};
}

void generateImplementationFile(std::string Source,
                                std::string Target,
                                const GenerationConfiguration& Cfg)
{
  std::ifstream Input{Source, std::ios::in};
  if (!Input.is_open())
  {
    std::cerr << "ERROR! Failed to open input template: ' " << Source << '\''
              << std::endl;
    throw Source;
  }

  std::ofstream Output{Target, std::ios::out | std::ios::trunc};
  if (!Output.is_open())
  {
    std::cerr << "ERROR! Failed to open output file: '" << Target << '\''
              << std::endl;
    throw Target;
  }

  static ActionMap ReplaceActions;
  ReplaceActions.try_emplace("RealHeaderInclude", [&Output, &Cfg]() {
    std::ostringstream OS;
    OS << "#include " << '"' << Cfg.HeaderCopyLocation << '"';
    Output << OS.str() << '\n';
    // std::cerr << "\tREPLACE line: " << OS.str() << '\n';
  });
  ReplaceActions.try_emplace("DataDirectives", [&Output, &Cfg]() {
    std::ostringstream OS;
    for (std::size_t I = 0; I < Cfg.Resources.size(); ++I)
    {
      const GenerationConfiguration::Resource& R = Cfg.Resources.at(I);
      std::cerr << '[' << I + 1 << '/' << Cfg.Resources.size() << ']' << ' '
                << "Compiling " << R.UserName << " from " << R.InputPath
                << std::endl;
      std::vector<char> Buffer = readFile(R.InputPath);

      OS.str("");
      OS << "RESOURCE_BUFFER(" << R.MachineName << ", " << Buffer.size()
         << ") = ";
      const std::uint8_t LeftMargin = OS.str().size() + 1;
      static constexpr std::uint8_t MaxCol = 16;
      static constexpr std::uint8_t Midpoint = MaxCol / 2;
      static const std::uint8_t MidpointSeparatorLength = std::strlen("    ");

      Output << std::string(LeftMargin - std::strlen("/* "), ' ') << "/*  "
             << std::dec;
      [&Output]() {
        static const std::uint8_t Digits =
          monomux::log::Logger::digits(MaxCol, 16) - 1;
        static const std::uint8_t Distance = std::strlen("' , '");
        for (std::uint8_t I = 0; I < MaxCol; ++I)
        {
          Output << std::setw(Digits) << std::setfill('0') << std::hex
                 << static_cast<int>(I);

          Output << std::string(I == MaxCol - 1 ? 1 : Distance - Digits + 1,
                                ' ');
          if (I + 1 == Midpoint)
            Output << std::string(MidpointSeparatorLength - 1, ' ');
        }
      }();
      Output << " */\n";

      Output << OS.str();

      const std::size_t MaxRowHexDigits =
        monomux::log::Logger::digits(Buffer.size(), 16);
      const std::size_t MaxRowHexDigitsPrintedLen =
        std::strlen("/* 0x") + MaxRowHexDigits + std::strlen(" */  ");

      // Print the bytes, but only to the output, not to cerr...
      Output << '{';
      std::uint8_t Col = 0;
      for (std::size_t ByteIndex = 0; ByteIndex < Buffer.size(); ++ByteIndex)
      {
        char C = Buffer[ByteIndex];

        if (C == '\'')
          Output << "'\\''"
                 << ",";
        else if (C == '\\')
          Output << "'\\\\'"
                 << ",";
        else if (C == '\n')
          Output << "'\\n'"
                 << ",";
        else if (C == '\t')
          Output << "'\\t'"
                 << ",";
        else if (std::isprint(C))
          Output << '\'' << C << '\'' << " ,";
        else
          Output << "0x" << std::hex << std::setw(2) << std::setfill('0')
                 << (static_cast<std::uint8_t>(C) & UINT8_MAX) << ',';

        ++Col;
        if (Col % MaxCol == 0)
        {
          Col = 0;
          Output << '\n'
                 << std::string(LeftMargin - MaxRowHexDigitsPrintedLen - 1,
                                ' ');
          Output << "/* 0x" << std::hex
                 << std::setw(static_cast<int>(MaxRowHexDigits))
                 << std::setfill('0') << (ByteIndex + 1) << " */  " << ' ';
        }
        else if (Col % Midpoint == 0)
          Output << std::string(MidpointSeparatorLength, ' ');
        else
          Output << ' ';
      }

      Output << " };\n\n";
      // std::cerr << "\tADDED   line: " << OS.str() << "...;\n";
    }
  });
  ReplaceActions.try_emplace("EntryEmplaces", [&Output, &Cfg]() {
    std::ostringstream OS;
    for (const GenerationConfiguration::Resource& R : Cfg.Resources)
    {
      OS.str("");
      OS << "RESOURCE_INIT(" << '"' << R.UserName << '"' << ", "
         << R.MachineName << ");";
      Output << OS.str() << '\n';
      // std::cerr << "\tADDED   line: " << OS.str() << '\n';
    }
  });

  std::string Line;
  while (std::getline(Input, Line))
  {
    // std::cerr << "\tParsing line: " << Line << '\n';
    if (tryLineAsReplaceDirective(Line, ReplaceActions))
      continue;
    Output << Line << '\n';
  }
}

void GenerationConfiguration::addResource(std::string Key, std::string Path)
{
  std::ostringstream MachineName;
  MachineName << "Resource_";
  for (const char C : Key)
  {
    switch (C)
    {
      case '?':
      case '!':
      case '|':
      case '/':
      case '\\':
      case '\'':
      case '"':
      case '-':
      case '+':
      case '*':
      case '=':
      case '.':
      case ':':
      case ',':
      case ';':
      case '(':
      case ')':
      case '[':
      case ']':
      case '{':
      case '}':
      case '<':
      case '>':
        MachineName << '_';
        break;
      default:
        MachineName << C;
        break;
    }
  }

  Resource R{std::move(Key), MachineName.str(), std::move(Path)};
  Resources.emplace_back(std::move(R));
}

} // namespace
