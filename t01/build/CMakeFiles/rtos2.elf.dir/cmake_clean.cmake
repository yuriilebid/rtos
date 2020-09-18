file(REMOVE_RECURSE
  "bootloader/bootloader.bin"
  "bootloader/bootloader.elf"
  "bootloader/bootloader.map"
  "config/sdkconfig.cmake"
  "config/sdkconfig.h"
  "project_elf_src.c"
  "rtos2.bin"
  "rtos2.map"
  "CMakeFiles/rtos2.elf.dir/project_elf_src.c.obj"
  "project_elf_src.c"
  "rtos2.elf"
  "rtos2.elf.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang C)
  include(CMakeFiles/rtos2.elf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
