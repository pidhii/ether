.PHONY: all clean cleanall Release Debug test fuzzy

all:
	@echo -e "\e[1mBuilding Debug configuration\e[0m"
	@mkdir -p Debug/build
	@cmake -D CMAKE_BUILD_TYPE=Debug \
				 -D CMAKE_INSTALL_PREFIX=`pwd`/Debug/install \
				 -B Debug/build \
				 -S .
	@$(MAKE) -C Debug/build install
	@echo -e "\n\e[1mBuilding Release configuration\e[0m"
	@mkdir -p Release/build
	@cmake -D CMAKE_BUILD_TYPE=Release \
				 -D CMAKE_INSTALL_PREFIX=`pwd`/Release/install \
				 -B Release/build \
				 -S .
	@$(MAKE) -C Release/build install

Release:
	@mkdir -p Release/build
	@cmake -D CMAKE_BUILD_TYPE=Release \
				 -D CMAKE_INSTALL_PREFIX=`pwd`/Release/install \
				 -B Release/build \
				 -S .
	@$(MAKE) -C Release/build install

Debug:
	@mkdir -p Debug/build
	@cmake -D CMAKE_BUILD_TYPE=Debug \
				 -D CMAKE_INSTALL_PREFIX=`pwd`/Debug/install \
				 -B Debug/build \
				 -S .
	@$(MAKE) -C Debug/build install

clean:
	@rm -rfv Release/build Debug/build

cleanall:
	@rm -rfv Release Debug

test: _test_Debug _test_Release
_test_%:
	@test -d $* && $(MAKE) -C $*/build test CTEST_OUTPUT_ON_FAILURE=1

fuzzy: t/test.eth
	@valgrind --leak-check=full ./Debug/install/bin/ether --log debug t/builtins.eth
	@valgrind --leak-check=full ./Debug/install/bin/ether --log debug t/test.eth <<< $$(echo -e "1\n2\n")
