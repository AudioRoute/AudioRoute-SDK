# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

include_directories(src/main/jni)
include_directories(../audioroute/src/main/jni)

add_library( # Sets the name of the library.
             hostsample

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             src/main/jni/LiveEffectEngine.cpp
             src/main/jni/jni_bridge.cpp

                           )
# Set the path to the Oboe directory.
set (OBOE_DIR ../externals/oboe)

# Add the Oboe library as a subdirectory in your project.
add_subdirectory (${OBOE_DIR} ./externals/oboe)

# Specify the path to the Oboe header files.
include_directories (${OBOE_DIR}/include)


add_library( audiomodule SHARED IMPORTED)
string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_LCASE)

set_target_properties( audiomodule PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../audioroute/build/intermediates/cmake/${CMAKE_BUILD_TYPE_LCASE}/obj/${ANDROID_ABI}/libaudiomodule.so )

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log
               )


# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
                       hostsample

                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib}
                       OpenSLES
                       android
                       atomic
                       log
                       oboe
                       audiomodule
                       )
