all:
	./scripts/build.sh

clean:
	rm -f CMakeCache.txt
	rm -rf CMakeFiles
	rm -rf cmake_install.cmake
	rm -rf */CMakeFiles
	rm -rf */cmake_install.cmake
	rm -rf */Makefile
	rm -rf build*
