list(APPEND libmonomuxCore_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/MovableAtomic.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/SmallIndexMap.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/TaggedPointer.hpp
  )
set(libmonomuxCore_HEADERS "${libmonomuxCore_HEADERS}" PARENT_SCOPE)
