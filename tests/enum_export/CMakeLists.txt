cmake_minimum_required(VERSION 3.25)
project(EnumExportContract LANGUAGES CXX)
include("${CMAKE_CURRENT_LIST_DIR}/../../tools/EnumExport.cmake")
visionrt_add_enum_exporter(enum_export_contract contract.cpp)
