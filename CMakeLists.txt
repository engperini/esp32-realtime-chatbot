cmake_minimum_required(VERSION 3.16)

# TARGET FIXO
set(IDF_TARGET esp32s3)

# FLASH SIZE FIXO (ANTES DO PROJECT)
set(CONFIG_ESPTOOLPY_FLASH_SIZE "8MB")
set(CONFIG_ESPTOOLPY_FLASH_SIZE_8MB y)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(xiao_realtime_chatbot)


