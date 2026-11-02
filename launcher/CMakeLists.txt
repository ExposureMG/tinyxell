# tinyxell launcher: an xsdk title that loads a libxenon homebrew ELF and hands it off to tinyxell's stage1 through the
# hypervisor. Build with xsdk's cmake toolchain after building stage1 (which uses the libxenon toolchain).
cmake_minimum_required(VERSION 3.25)
project(tinyxell C)

add_executable(tinyxell
	main.c
	hv.c
)
set_target_properties(tinyxell PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
target_compile_options(tinyxell PRIVATE -Wall -Wextra -Werror)
target_include_directories(tinyxell PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../include
	${CMAKE_CURRENT_SOURCE_DIR}/../stage1/build
	${CMAKE_CURRENT_SOURCE_DIR}
)
xsdk_add_xex(tinyxell TITLE_ID 0x54584C48 NAME "tinyxell")
