.PHONY: all clean cleanall Release Debug test fuzzy

ifeq ($(CMAKE),)
$(info Set CMAKE-variable to change CMake version.)
CMAKE=cmake
endif
$(info Using cmake ($(shell cmake --version)))

all:
	@echo -e "\e[1mBuilding Debug configuration\e[0m"
	@mkdir -p Debug/build
	@$(CMAKE) -D CMAKE_BUILD_TYPE=Debug \
				    -D CMAKE_INSTALL_PREFIX=`pwd`/Debug/install \
				    -D ENABLE_ARB=NO \
				    -B Debug/build \
				    -S .
	@$(MAKE) -C Debug/build install
	@echo -e "\n\e[1mBuilding Release configuration\e[0m"
	@mkdir -p Release/build
	@$(CMAKE) -D CMAKE_BUILD_TYPE=Release \
				    -D CMAKE_INSTALL_PREFIX=`pwd`/Release/install \
				    -D BUILD_SHARED_LIBRARY=ON \
				    -D ENABLE_ARB=NO \
				    -B Release/build \
				    -S .
	@$(MAKE) -C Release/build install

Release:
	@mkdir -p Release/build
	@$(CMAKE) -D CMAKE_BUILD_TYPE=Release \
				    -D CMAKE_INSTALL_PREFIX=`pwd`/Release/install \
				    -D BUILD_SHARED_LIBRARY=ON \
				    -D ENABLE_ARB=NO \
				    -B Release/build \
				    -S .
	@$(MAKE) -C Release/build install

Debug:
	@mkdir -p Debug/build
	@$(CMAKE) -D CMAKE_BUILD_TYPE=Debug \
				    -D CMAKE_INSTALL_PREFIX=`pwd`/Debug/install \
				    -D ENABLE_ARB=NO \
				    -B Debug/build \
				    -S .
	@$(MAKE) -C Debug/build install

clean:
	@rm -rfv Release/build Debug/build

cleanall:
	@rm -rfv Release Debug

test: test_Debug test_Release
.ONESHELL:
test_%:
	source ./env.sh $*/install
	test -d $*
	$(MAKE) -C $*/build test #CTEST_OUTPUT_ON_FAILURE=1

fuzzy: t/test.eth
	valgrind --leak-check=full ./Debug/install/bin/ether --log=debug -Lt t/builtins.eth
	valgrind --leak-check=full ./Debug/install/bin/ether --log=debug -Lt t/test.eth <<<$(shell echo -e "1\n2\n")
