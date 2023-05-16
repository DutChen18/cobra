CXX := clang++
CXXFLAGS := -Wall -Wextra -std=c++11 -Iinclude
LDFLAGS := 
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
else ifeq ($(config), distr)
	CXXFLAGS += -Ofast -flto -march=native
	LDFLAGS += -Ofast -flto -march=native
else
$(error "unknown config $(config)")
endif

ifdef san
	ifeq ($(san), addr)
		CXXFLAGS += -fsanitize=address,undefined
		LDFLAGS += -fsanitize=address,undefined
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

SRC_DIR := src
OBJ_DIR := build
DEP_DIR := build
# SRC_FILES = $(shell find $(SRC_DIR) -type f -name "*.cc")
SRC_FILES := src/event_loop.cc src/exception.cc src/executor.cc src/main.cc src/context.cc src/socket.cc src/http/error.cc src/http/header.cc src/http/parse.cc src/http/util.cc src/http/stringify.cc src/process.cc src/fd.cc src/path.cc
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cc,$(OBJ_DIR)/%.o,$(SRC_FILES))
DEP_FILES := $(patsubst $(SRC_DIR)/%.cc,$(DEP_DIR)/%.d,$(SRC_FILES))
NAME := webserv

FUZZ_NAME := webserv_fuzz

all: $(NAME)

fuzz: CXXFLAGS += -DCOBRA_FUZZ -fsanitize=fuzzer
fuzz: LDFLAGS += -fsanitize=fuzzer
fuzz: $(FUZZ_NAME)

$(FUZZ_NAME): $(OBJ_FILES)
	$(CXX) -o $(FUZZ_NAME) -Iinclude fuzz/request.cc $(OBJ_FILES) $(LDFLAGS) $(CXXFLAGS) -MMD

$(NAME): $(OBJ_FILES)
	$(CXX) -o $@ $^ $(LDFLAGS) 

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc Makefile
	mkdir -p $(@D)
	$(CXX) -o $@ $< $(CXXFLAGS) -c -MMD

clean:
	rm -rf $(OBJ_DIR)
	rm -rf $(DEP_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean
	${MAKE} all

-include $(DEP_FILES)
.PHONY: all clean fclean re fuzz
