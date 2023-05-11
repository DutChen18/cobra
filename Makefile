CXX = clang++
CXXFLAGS = -Wall -Wextra -std=c++11 -Iinclude -fsanitize=address,undefined -g3
LDFLAGS = -fsanitize=address,undefined
# CXXFLAGS = -Wall -Wextra -std=c++11 -Iinclude -fsanitize=thread,undefined -g3
# LDFLAGS = -fsanitize=thread,undefined
# CXXFLAGS = -Wall -Wextra -std=c++11 -Iinclude -Ofast -march=native
# CXXFLAGS = -Wall -Wextra -std=c++11 -Iinclude -g3

ifdef pedantic
	CXXFLAGS += -pedantic
endif

ifndef nerror
	CXXFLAGS += -Werror
endif

SRC_DIR = src
OBJ_DIR = build
DEP_DIR = build
# SRC_FILES = $(shell find $(SRC_DIR) -type f -name "*.cc")
SRC_FILES = src/event_loop.cc src/exception.cc src/executor.cc src/main.cc src/context.cc src/socket.cc src/http_error.cc src/http_header.cc src/http.cc
OBJ_FILES = $(patsubst $(SRC_DIR)/%.cc,$(OBJ_DIR)/%.o,$(SRC_FILES))
DEP_FILES = $(patsubst $(SRC_DIR)/%.cc,$(DEP_DIR)/%.d,$(SRC_FILES))
NAME = webserv

all: $(NAME)

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
	$(MAKE) all

-include $(DEP_FILES)
.PHONY: all clean fclean re
