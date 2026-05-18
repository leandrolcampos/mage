add_mage_object_library(
  MageFooAddOne
  SRCS
    AddOne.cpp
  BUILDS
    HOST
    GPU
)

add_mage_object_library(
  MageFooAddTwo
  SRCS
    AddTwo.cpp
  BUILDS
    HOST
    GPU
  DEPENDS
    MageFooAddOne
)

add_mage_library(
  MageFoo
  BUILDS
    HOST
    GPU
  DEPENDS
    MageFooAddTwo
)

add_mage_bitcode_library(
  MageFooBitcode
  BUILDS
    GPU
  DEPENDS
    MageFooAddTwo
)
