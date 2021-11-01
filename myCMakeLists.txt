cmake_minimum_required(VERSION 3.5)
set (CMAKE_CXX_STANDARD 20)
project(spb_ai_champ)

add_custom_target(
    spb_ai_champ
    ALL
    COMMAND make -f build.mk BUILDTYPE=release CXX=g++ && mv release/main spb_ai_champ
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
