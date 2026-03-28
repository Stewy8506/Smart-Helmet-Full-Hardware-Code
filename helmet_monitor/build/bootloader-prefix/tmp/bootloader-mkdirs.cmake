# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/esp/esp-idf-v5.2/components/bootloader/subproject"
  "D:/shared/programming/Projects/IITM_Gwalior/RTOS/MOD_1/helmet_monitor/build/bootloader"
  "D:/shared/programming/Projects/IITM_Gwalior/RTOS/MOD_1/helmet_monitor/build/bootloader-prefix"
  "D:/shared/programming/Projects/IITM_Gwalior/RTOS/MOD_1/helmet_monitor/build/bootloader-prefix/tmp"
  "D:/shared/programming/Projects/IITM_Gwalior/RTOS/MOD_1/helmet_monitor/build/bootloader-prefix/src/bootloader-stamp"
  "D:/shared/programming/Projects/IITM_Gwalior/RTOS/MOD_1/helmet_monitor/build/bootloader-prefix/src"
  "D:/shared/programming/Projects/IITM_Gwalior/RTOS/MOD_1/helmet_monitor/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/shared/programming/Projects/IITM_Gwalior/RTOS/MOD_1/helmet_monitor/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/shared/programming/Projects/IITM_Gwalior/RTOS/MOD_1/helmet_monitor/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
