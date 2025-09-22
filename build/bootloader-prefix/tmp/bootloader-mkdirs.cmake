# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/eniss/esp/v5.1.4/esp-idf/components/bootloader/subproject"
  "/home/eniss/esp-proje/sd kart/build/bootloader"
  "/home/eniss/esp-proje/sd kart/build/bootloader-prefix"
  "/home/eniss/esp-proje/sd kart/build/bootloader-prefix/tmp"
  "/home/eniss/esp-proje/sd kart/build/bootloader-prefix/src/bootloader-stamp"
  "/home/eniss/esp-proje/sd kart/build/bootloader-prefix/src"
  "/home/eniss/esp-proje/sd kart/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/eniss/esp-proje/sd kart/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/eniss/esp-proje/sd kart/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
