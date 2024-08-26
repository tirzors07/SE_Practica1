# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/tirzo/esp/v5.3/esp-idf/components/bootloader/subproject"
  "C:/SE/Practica1/build/bootloader"
  "C:/SE/Practica1/build/bootloader-prefix"
  "C:/SE/Practica1/build/bootloader-prefix/tmp"
  "C:/SE/Practica1/build/bootloader-prefix/src/bootloader-stamp"
  "C:/SE/Practica1/build/bootloader-prefix/src"
  "C:/SE/Practica1/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/SE/Practica1/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/SE/Practica1/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
