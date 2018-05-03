# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

include_directories(src/main/jni/player)
include_directories(ffmpeg/include)
link_directories(ffmpeg/lib)

add_library( # Sets the name of the library.
                 SilentPlayer

                 # Sets the library as a shared library.
                 SHARED

                 # Provides a relative path to your source file(s).
                 # Associated headers in the same location as their source
                 # file are automatically included.
                 src/main/jni/player/FFPlayer.c
                 )

target_include_directories(SilentPlayer PRIVATE
                           src/main/jni)

target_link_libraries( SilentPlayer

                       libswscale.a
                       libavformat.a
                       libavcodec.a
                       libavutil.a
                       libswresample.a
                       libx264.a

                       # Links the target library to the log library
                       # included in the NDK.
                       log
                       jnigraphics
                       z
                       android
                       GLESv2
                       )

