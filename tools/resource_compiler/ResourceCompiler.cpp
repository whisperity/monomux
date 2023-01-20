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

    std::optional<std::size_t> Size;
  };

  std::string HeaderCopyLocation;
  std::vector<Resource> Resources;

  void addResource(std::string Key, std::string Path);
};

std::vector<char> readFile(std::string File);
void copyFileRaw(std::string From, std::string To);
void generateImplementationFile(std::string Source,
                                std::string Target,
                                GenerationConfiguration& Cfg);

} // namespace

int main(int ArgC, char* ArgV[])
{
  if (ArgC < 3)
  {
    std::cerr << "ERROR: Must specify at least 3 arguments!\n";
    printInvocation(std::cerr);
    return EXIT_FAILURE;
  }

  GenerationConfiguration Cfg;

  std::cout << ArgC << std::endl;
  std::copy(
    ArgV, ArgV + ArgC, std::ostream_iterator<const char*>(std::cout, " "));
  std::cout << std::endl;

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

bool tryLineAsReplaceDirective(const std::string& Line,
                               const GenerationConfiguration& Cfg,
                               const ActionMap& Acts)
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
                                GenerationConfiguration& Cfg)
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
    for (GenerationConfiguration::Resource& R : Cfg.Resources)
    {
      std::vector<char> Buffer = readFile(R.InputPath);
      R.Size.emplace(Buffer.size());

      OS.str("");
      OS << "RESOURCE_BUFFER(" << R.MachineName << ", " << Buffer.size()
         << ") = ";
      Output << OS.str();

      // Print the bytes, but only to the output, not to cerr...
      Output << '{' << std::hex;

      for (char C : Buffer)
        Output << "0x" << static_cast<int>(C) << ", ";

      Output << std::dec << "};" << '\n';

      // std::cerr << "\tADDED   line: " << OS.str() << "...;\n";
    }
  });
  ReplaceActions.try_emplace("EntryEmplaces", [&Output, &Cfg]() {
    std::ostringstream OS;
    for (const GenerationConfiguration::Resource& R : Cfg.Resources)
    {
      if (!R.Size)
      {
        std::cerr << "ERROR! Cannot embed data pointer for resource of unknown "
                     "size! Perhaps file '"
                  << R.UserName << "' ('" << R.InputPath << "') failed to load?"
                  << std::endl;
        throw std::string{R.UserName};
      }

      OS.str("");
      OS << "RESOURCE_INIT(" << '"' << R.UserName << '"' << ", "
         << R.MachineName << ", " << *R.Size << ");";
      Output << OS.str() << '\n';
      // std::cerr << "\tADDED   line: " << OS.str() << '\n';
    }
  });

  std::string Line;
  while (std::getline(Input, Line))
  {
    // std::cerr << "\tParsing line: " << Line << '\n';
    if (tryLineAsReplaceDirective(Line, Cfg, ReplaceActions))
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
    if (C == PathSeparator)
      MachineName << '_';
    else
      MachineName << C;
  }

  Resource R{std::move(Key), MachineName.str(), std::move(Path)};
  Resources.emplace_back(std::move(R));
}

} // namespace
