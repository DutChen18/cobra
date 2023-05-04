CXX = c++
CXXFLAGS = -Wall -Wextra -std=c++11 -Iinclude

ifdef pedantic
	CXXFLAGS += -pedantic
endif

ifndef nerror
	CXXFLAGS += -Werror
endif

SRC_DIR = src
OBJ_DIR = build
DEP_DIR = build
SRC_FILES = $(shell find $(SRC_DIR) -type f -name "*.cc")
OBJ_FILES = $(patsubst $(SRC_DIR)/%.cc,$(OBJ_DIR)/%.o,$(SRC_FILES))
DEP_FILES = $(patsubst $(SRC_DIR)/%.cc,$(DEP_DIR)/%.d,$(SRC_FILES))
NAME = webserv

all: $(NAME)

$(NAME): $(OBJ_FILES)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc
	mkdir -p $(@D)
	$(CXX) -o $@ $< $(CXXFLAGS) -c -MMD

clean:
	rm -f $(OBJ_DIR)
	rm -f $(DEP_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean
	$(MAKE) all

-include $(DEP_FILES)
.PHONY: all clean fclean re
