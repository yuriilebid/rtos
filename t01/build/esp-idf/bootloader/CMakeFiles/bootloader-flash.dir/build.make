# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.18

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Disable VCS-based implicit rules.
% : %,v


# Disable VCS-based implicit rules.
% : RCS/%


# Disable VCS-based implicit rules.
% : RCS/%,v


# Disable VCS-based implicit rules.
% : SCCS/s.%


# Disable VCS-based implicit rules.
% : s.%


.SUFFIXES: .hpux_make_needs_suffix_list


# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /Users/ylebid/.espressif/python_env/idf4.1_py2.7_env/lib/python2.7/site-packages/cmake/data/CMake.app/Contents/bin/cmake

# The command to remove a file.
RM = /Users/ylebid/.espressif/python_env/idf4.1_py2.7_env/lib/python2.7/site-packages/cmake/data/CMake.app/Contents/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/ylebid/Desktop/rtos/t01

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/ylebid/Desktop/rtos/t01/build

# Utility rule file for bootloader-flash.

# Include the progress variables for this target.
include esp-idf/bootloader/CMakeFiles/bootloader-flash.dir/progress.make

esp-idf/bootloader/CMakeFiles/bootloader-flash:
	cd /Users/ylebid/esp/esp-idf/components/bootloader && /Users/ylebid/.espressif/python_env/idf4.1_py2.7_env/lib/python2.7/site-packages/cmake/data/CMake.app/Contents/bin/cmake -D IDF_PATH="/Users/ylebid/esp/esp-idf" -D ESPTOOLPY="/Users/ylebid/.espressif/python_env/idf4.1_py2.7_env/bin/python /Users/ylebid/esp/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32" -D ESPTOOL_ARGS="write_flash @flash_bootloader_args" -D WORKING_DIRECTORY="/Users/ylebid/Desktop/rtos/t01/build" -P /Users/ylebid/esp/esp-idf/components/esptool_py/run_esptool.cmake

bootloader-flash: esp-idf/bootloader/CMakeFiles/bootloader-flash
bootloader-flash: esp-idf/bootloader/CMakeFiles/bootloader-flash.dir/build.make

.PHONY : bootloader-flash

# Rule to build all files generated by this target.
esp-idf/bootloader/CMakeFiles/bootloader-flash.dir/build: bootloader-flash

.PHONY : esp-idf/bootloader/CMakeFiles/bootloader-flash.dir/build

esp-idf/bootloader/CMakeFiles/bootloader-flash.dir/clean:
	cd /Users/ylebid/Desktop/rtos/t01/build/esp-idf/bootloader && $(CMAKE_COMMAND) -P CMakeFiles/bootloader-flash.dir/cmake_clean.cmake
.PHONY : esp-idf/bootloader/CMakeFiles/bootloader-flash.dir/clean

esp-idf/bootloader/CMakeFiles/bootloader-flash.dir/depend:
	cd /Users/ylebid/Desktop/rtos/t01/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/ylebid/Desktop/rtos/t01 /Users/ylebid/esp/esp-idf/components/bootloader /Users/ylebid/Desktop/rtos/t01/build /Users/ylebid/Desktop/rtos/t01/build/esp-idf/bootloader /Users/ylebid/Desktop/rtos/t01/build/esp-idf/bootloader/CMakeFiles/bootloader-flash.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : esp-idf/bootloader/CMakeFiles/bootloader-flash.dir/depend

