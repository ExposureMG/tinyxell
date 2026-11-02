# tinyxell: builds both stage1 (with the libxenon toolchain) and the launcher (with xsdk's toolchain).
#
# Prerequisites:
#   - libxenon's xenon-gcc in $PATH (devkitPPC or the libxenon toolchain)
#   - xsdk built (make in the xsdk checkout), with its toolchain at $XSDK_TOOLCHAIN (default ~/xsdk-gcc)
#
# Set XSDK_ROOT to the xsdk checkout; it defaults to ../xsdk beside this repo.

XSDK_ROOT ?= ../xsdk
XSDK_TOOLCHAIN ?= $(HOME)/xsdk-gcc
BUILD = build

.PHONY: all stage1 launcher clean

all: stage1 launcher

stage1:
	$(MAKE) -C stage1

launcher: stage1
	@mkdir -p $(BUILD)
	cd $(BUILD) && cmake -G Ninja \
		-DCMAKE_TOOLCHAIN_FILE=$(abspath $(XSDK_ROOT))/cmake/xsdk.cmake \
		-DXSDK_TOOLCHAIN=$(XSDK_TOOLCHAIN) \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo \
		../launcher
	cmake --build $(BUILD)

clean:
	$(MAKE) -C stage1 clean
	rm -rf $(BUILD)
