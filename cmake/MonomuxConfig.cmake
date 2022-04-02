set(MONOMUX_NON_ESSENTIAL_LOGS ON CACHE BOOL
  "If set, the built binary will contain some additional log outputs that are needed for verbose debugging of the project. Turn off to cut down further on the binary size for production."
  )

configure_file(cmake/Config.in.h include/monomux/Config.h)
