CXX := /usr/bin/clang++
CXXFLAGS := -std=c++17 -Wall -fcolor-diagnostics -fansi-escape-codes -g

SRC_SHARED := shared/*.cpp
SRC_CLIENT := client/*.cpp
SRC_SERVER := server/*.cpp

OUT_CLIENT_DIR := client/build
OUT_CLIENT_EXEC := $(OUT_CLIENT_DIR)/client

OUT_SERVER_DIR := server/build
OUT_SERVER_EXEC := $(OUT_SERVER_DIR)/server

build-client:
	mkdir -p $(OUT_CLIENT_DIR)
	$(CXX) $(CXXFLAGS) $(SRC_SHARED) $(SRC_CLIENT) -o $(OUT_CLIENT_EXEC)

build-server:
	mkdir -p $(OUT_SERVER_DIR)
	$(CXX) $(CXXFLAGS) $(SRC_SHARED) $(SRC_SERVER) -o $(OUT_SERVER_EXEC)

clean-client:
	rm -rf $(OUT_CLIENT_DIR)

clean-server:
	rm -rf $(OUT_SERVER_DIR)

.PHONY: build-client build-server clean-server clean-client
