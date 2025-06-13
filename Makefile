
.PHONY: all download_deps build_luamake

all: download_deps build_luamake
	3rd/luamake/luamake build --release

build_luamake:
	@echo "Building luamake..."
	git submodule update --init --recursive
	cd 3rd/luamake && compile/build.sh

download_deps: build_luamake
	3rd/luamake/luamake lua compile/download_deps.lua
