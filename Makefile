CXX := clang++
CXXFLAGS := -Wall -Wextra -std=c++20 -Iinclude
LDFLAGS := -lssl -lcrypto
# CXXFLAGS = -Wall -Wextra -std=c++11 -Iinclude -fsanitize=thread -g3
# LDFLAGS = -fsanitize=thread
# CXXFLAGS = -Wall -Wextra -std=c++11 -Iinclude -Ofast -march=native -flto
# LDFLAGS = -flto

ifdef pedantic
	CXXFLAGS += -pedantic
endif

ifndef nerror
	CXXFLAGS += -Werror
endif

ifndef config 
	config := debug
endif

ifeq ($(config), debug)
	CXXFLAGS += -DCOBRA_DEBUG -fno-inline -g3 -O0
	LDFLAGS += -DCOBRA_DEBUG -fno-inline -g3 -O0
	ifndef san
		san := addr
	endif
else ifeq ($(config), release)
	CXXFLAGS += -g3 -O2 -DCOBRA_DEBUG
	LDFLAGS += -g3 -O2 -DCOBRA_DEBUG
else ifeq ($(config), profile)
	CXXFLAGS += -g3 -Ofast -flto -march=native -pg
	LDFLAGS += -g3 -Ofast -flto -march=native -pg
else ifeq ($(config), distr)
	CXXFLAGS += -Ofast -flto -march=native
	LDFLAGS += -Ofast -flto -march=native
else
$(error "unknown config $(config)")
endif

ifdef san
	ifeq ($(san), addr)
		CXXFLAGS += -fsanitize=address,undefined -fsanitize-trap=all
		LDFLAGS += -fsanitize=address,undefined -fsanitize-trap=all
	else ifeq ($(san), mem)
		CXXFLAGS += -fsanitize=memory,undefined -fsanitize-memory-track-origins
		LDFLAGS += -fsanitize=memory,undefined -fsanitize-memory-track-origins
	else ifeq ($(san), thread)
		CXXFLAGS += -fsanitize=thread,undefined
		LDFLAGS += -fsanitize=thread,undefined
	else
	$(error "unknown sanitizer $(san)")
	endif
endif

ifndef fuzz_target

else ifeq ($(fuzz_target), config)
	CXXFLAGS += -DCOBRA_FUZZ_CONFIG -DCOBRA_FUZZ -fsanitize=fuzzer
	LDFLAGS += -fsanitize=fuzzer
else ifeq ($(fuzz_target), request)
	CXXFLAGS += -DCOBRA_FUZZ_REQUEST -DCOBRA_FUZZ -fsanitize=fuzzer
	LDFLAGS += -fsanitize=fuzzer
else ifeq ($(fuzz_target), uri)
	CXXFLAGS += -DCOBRA_FUZZ_URI -DCOBRA_FUZZ -fsanitize=fuzzer
	LDFLAGS += -fsanitize=fuzzer
else ifeq ($(fuzz_target), inflate)
	CXXFLAGS += -DCOBRA_FUZZ_INFLATE -DCOBRA_FUZZ -fsanitize=fuzzer
	LDFLAGS += -fsanitize=fuzzer
else
$(error "unknown fuzz target $(fuzz_target)")
endif

SRC_DIR := src
OBJ_DIR := build
DEP_DIR := build
# SRC_FILES = $(shell find $(SRC_DIR) -type f -name "*.cc")
SRC_FILES := src/main.cc src/asyncio/executor.cc src/exception.cc src/asyncio/event_loop.cc src/exception.cc src/file.cc src/net/address.cc src/net/stream.cc src/http/parse.cc src/process.cc src/http/message.cc src/http/writer.cc src/http/uri.cc src/http/util.cc src/http/handler.cc src/http/server.cc src/config.cc src/fastcgi.cc src/serde.cc src/asyncio/mutex.cc src/fuzz_config.cc src/fuzz_request.cc src/fuzz_uri.cc src/fuzz_inflate.cc
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cc,$(OBJ_DIR)/%.o,$(SRC_FILES))
DEP_FILES := $(patsubst $(SRC_DIR)/%.cc,$(DEP_DIR)/%.d,$(SRC_FILES))
NAME := webserv

FUZZ_NAME := webserv_fuzz

all: $(NAME)

fuzz: CXXFLAGS += -DCOBRA_FUZZ -fsanitize=fuzzer
fuzz: LDFLAGS += -fsanitize=fuzzer
fuzz: $(NAME)

$(FUZZ_NAME): $(OBJ_FILES)
	$(CXX) -o $(FUZZ_NAME) -Iinclude fuzz/main.cc $(OBJ_FILES) $(LDFLAGS) $(CXXFLAGS) -MMD

$(NAME): $(NAME).out
	mv $(NAME).out $(NAME)

$(NAME).out: $(OBJ_FILES)
	$(CXX) -o $@ $^ $(LDFLAGS) 

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc Makefile
	mkdir -p $(@D)
	$(CXX) -o $@ $< $(CXXFLAGS) -c -MMD

#fmt:
#	docker run -v $(PWD):/src xianpengshen/clang-tools:16 clang-format -i $(SRC_FILES) $(shell find include/ -type f -name '*.hh')

clean:
	rm -rf $(OBJ_DIR)
	rm -rf $(DEP_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean
	${MAKE} all

-include $(DEP_FILES)
.PHONY: all clean fclean re fuzz fmt
