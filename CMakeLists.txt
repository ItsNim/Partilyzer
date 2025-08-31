cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(partilyzer)

spiffs_create_partition_image(storage1 storage FLASH_IN_PROJECT)
