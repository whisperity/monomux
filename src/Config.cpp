/* SPDX-License-Identifier: LGPL-3.0-only */
#include <sstream>
#include <string_view>

#include "Config.hpp"

namespace monomux
{

namespace
{

void printToggleFeature(std::ostream& OS, std::string_view Name, bool Enabled)
{
  OS << ' ' << (Enabled ? '+' : '-') << ' ' << Name << '\n';
}

} // namespace

std::string getHumanReadableConfiguration()
{
  std::ostringstream Buf;

  Buf << " * " << MONOMUX_BUILD_TYPE << " build\n";

#if MONOMUX_BUILD_SHARED_LIBS
  Buf << " * SHARED (dynamic) library\n";
#else /* !MONOMUX_BUILD_SHARED_LIBS */
#if MONOMUX_BUILD_UNITY
  Buf << " * UNITY library\n";
#else  /* !MONOMUX_BUILD_UNITY */
  Buf << " * STATIC library\n";
#endif /* MONOMUX_BUILD_UNITY */
#endif /* MONOMUX_BUILD_SHARED_LIBS */

  printToggleFeature(Buf,
                     "Embedding library support features",
                     config::EmbeddingLibraryFeatures);
  printToggleFeature(Buf, "Non-essential trace logs", config::NonEssentialLogs);

  std::string S = Buf.str();
  {
    // Clean up multiple subsequent newlines from the output.
    static constexpr std::string_view DoubleNewline = "\n\n";
    static constexpr std::string_view SingleNewline = "\n";
    std::string::size_type P = 0;
    while ((P = S.find(DoubleNewline.data())) != std::string::npos)
      S.replace(
        P, DoubleNewline.size(), SingleNewline.data(), SingleNewline.size());
  }
  return S;
}

} // namespace monomux
