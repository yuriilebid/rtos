file(REMOVE_RECURSE
  "bootloader/bootloader.bin"
  "bootloader/bootloader.elf"
  "bootloader/bootloader.map"
  "config/sdkconfig.cmake"
  "config/sdkconfig.h"
  "project_elf_src.c"
  "rtos2.bin"
  "rtos2.map"
  "CMakeFiles/app"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/app.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
