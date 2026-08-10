cmake_minimum_required(VERSION 3.16)

# Pin the app version so ESP-IDF does not shell out to `git describe`
# (avoids failures when the repo has no commits yet / nested submodule probes).
set(PROJECT_VER "0.1.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(kbdrelay)
