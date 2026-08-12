# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Espressif/frameworks/esp-idf-v5.3.1/components/bootloader/subproject"
  "F:/IoT/esp32 IDF code/firmware/build/bootloader"
  "F:/IoT/esp32 IDF code/firmware/build/bootloader-prefix"
  "F:/IoT/esp32 IDF code/firmware/build/bootloader-prefix/tmp"
  "F:/IoT/esp32 IDF code/firmware/build/bootloader-prefix/src/bootloader-stamp"
  "F:/IoT/esp32 IDF code/firmware/build/bootloader-prefix/src"
  "F:/IoT/esp32 IDF code/firmware/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "F:/IoT/esp32 IDF code/firmware/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "F:/IoT/esp32 IDF code/firmware/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
