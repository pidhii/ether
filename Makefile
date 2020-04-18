.PHONY: all clean cleanall Release Debug test

all:
	@echo -e "\e[1mBuilding Debug configuration\e[0m"
	@mkdir -p Debug/build
	@cmake -D CMAKE_BUILD_TYPE=Debug \
				 -D CMAKE_INSTALL_PREFIX=`pwd`/Debug/install \
				 -B Debug/build \
				 -S .
	@$(MAKE) -C Debug/build install
	@$(MAKE) -C Debug/build test
	@echo -e "\n\e[1mBuilding Release configuration\e[0m"
	@mkdir -p Release/build
	@cmake -D CMAKE_BUILD_TYPE=Release \
				 -D CMAKE_INSTALL_PREFIX=`pwd`/Release/install \
				 -B Release/build \
				 -S .
	@$(MAKE) -C Release/build install
	@$(MAKE) -C Release/build test


Release:
	@mkdir -p Release/build
	@cmake -D CMAKE_BUILD_TYPE=Release \
				 -D CMAKE_INSTALL_PREFIX=`pwd`/Release/install \
				 -B Release/build \
				 -S .
	@$(MAKE) -C Release/build install
	@$(MAKE) -C Release/build test

Debug:
	@mkdir -p Debug/build
	@cmake -D CMAKE_BUILD_TYPE=Debug \
				 -D CMAKE_INSTALL_PREFIX=`pwd`/Debug/install \
				 -B Debug/build \
				 -S .
	@$(MAKE) -C Debug/build install
	@$(MAKE) -C Debug/build test

clean:
	@rm -rfv Release/build Debug/build

cleanall:
	@rm -rfv Release Debug

test: Debug t/test.eth
	@valgrind --leak-check=full ./Debug/install/bin/ether --log debug t/builtins.eth
	@valgrind --leak-check=full ./Debug/install/bin/ether --log debug t/test.eth <<< $$(echo -e "1\n2\n")

