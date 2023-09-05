cmake_minimum_required(VERSION 3.26.3)
project(CheckCXX CXX)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/result.cmake"
  "set(CMAKE_CXX_COMPILER \"${CMAKE_CXX_COMPILER}\")\n"
  "\n"
  )