cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hello_world)

spiffs_create_partition_image(storage1 fonts FLASH_IN_PROJECT)
