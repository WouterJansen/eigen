# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.26

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

# Produce verbose output by default.
VERBOSE = 1

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
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/sbronder/open_source/eigen

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/sbronder/open_source/eigen/build

# Include any dependencies generated for this target.
include test/CMakeFiles/umeyama_7.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include test/CMakeFiles/umeyama_7.dir/compiler_depend.make

# Include the progress variables for this target.
include test/CMakeFiles/umeyama_7.dir/progress.make

# Include the compile flags for this target's objects.
include test/CMakeFiles/umeyama_7.dir/flags.make

test/CMakeFiles/umeyama_7.dir/umeyama.cpp.o: test/CMakeFiles/umeyama_7.dir/flags.make
test/CMakeFiles/umeyama_7.dir/umeyama.cpp.o: /home/sbronder/open_source/eigen/test/umeyama.cpp
test/CMakeFiles/umeyama_7.dir/umeyama.cpp.o: test/CMakeFiles/umeyama_7.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/sbronder/open_source/eigen/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object test/CMakeFiles/umeyama_7.dir/umeyama.cpp.o"
	cd /home/sbronder/open_source/eigen/build/test && /mnt/sw/nix/store/6560mkp838syd8jpp6gdyyisczwcvs67-gcc-11.4.0/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT test/CMakeFiles/umeyama_7.dir/umeyama.cpp.o -MF CMakeFiles/umeyama_7.dir/umeyama.cpp.o.d -o CMakeFiles/umeyama_7.dir/umeyama.cpp.o -c /home/sbronder/open_source/eigen/test/umeyama.cpp

test/CMakeFiles/umeyama_7.dir/umeyama.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/umeyama_7.dir/umeyama.cpp.i"
	cd /home/sbronder/open_source/eigen/build/test && /mnt/sw/nix/store/6560mkp838syd8jpp6gdyyisczwcvs67-gcc-11.4.0/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/sbronder/open_source/eigen/test/umeyama.cpp > CMakeFiles/umeyama_7.dir/umeyama.cpp.i

test/CMakeFiles/umeyama_7.dir/umeyama.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/umeyama_7.dir/umeyama.cpp.s"
	cd /home/sbronder/open_source/eigen/build/test && /mnt/sw/nix/store/6560mkp838syd8jpp6gdyyisczwcvs67-gcc-11.4.0/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/sbronder/open_source/eigen/test/umeyama.cpp -o CMakeFiles/umeyama_7.dir/umeyama.cpp.s

# Object files for target umeyama_7
umeyama_7_OBJECTS = \
"CMakeFiles/umeyama_7.dir/umeyama.cpp.o"

# External object files for target umeyama_7
umeyama_7_EXTERNAL_OBJECTS =

test/umeyama_7: test/CMakeFiles/umeyama_7.dir/umeyama.cpp.o
test/umeyama_7: test/CMakeFiles/umeyama_7.dir/build.make
test/umeyama_7: test/CMakeFiles/umeyama_7.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/sbronder/open_source/eigen/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable umeyama_7"
	cd /home/sbronder/open_source/eigen/build/test && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/umeyama_7.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/CMakeFiles/umeyama_7.dir/build: test/umeyama_7
.PHONY : test/CMakeFiles/umeyama_7.dir/build

test/CMakeFiles/umeyama_7.dir/clean:
	cd /home/sbronder/open_source/eigen/build/test && $(CMAKE_COMMAND) -P CMakeFiles/umeyama_7.dir/cmake_clean.cmake
.PHONY : test/CMakeFiles/umeyama_7.dir/clean

test/CMakeFiles/umeyama_7.dir/depend:
	cd /home/sbronder/open_source/eigen/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/sbronder/open_source/eigen /home/sbronder/open_source/eigen/test /home/sbronder/open_source/eigen/build /home/sbronder/open_source/eigen/build/test /home/sbronder/open_source/eigen/build/test/CMakeFiles/umeyama_7.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/CMakeFiles/umeyama_7.dir/depend
