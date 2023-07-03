CXX			:= clang++
WARNINGS	:= -Wall -Wextra
ANALYZER	:=
#SANITIZERS	:= -fsanitize=address,undefined,integer
SANITIZERS	:= -fsanitize=address,undefined
CXXFLAGS	:= -std=c++20 $(WARNINGS) $(ANALYZER) $(SANITIZERS) -I../include \
			-Isupport -MMD -MP -DFT_TEST -O0 -g3 -DCOBRA_DEBUG -I. -DCOBRA_TEST

OBJ_DIR		:= build

COBRA_SRC	:= ../src
COBRA_FILES	:= $(shell find $(COBRA_SRC) -type f -name '*.cc')
COBRA_OBJS	:= $(patsubst $(COBRA_SRC)/%.cc,$(OBJ_DIR)/%.o,$(COBRA_FILES))
COBRA_DEPS	:= $(patsubst $(COBRA_SRC)/%.cc,$(OBJ_DIR)/%.d,$(COBRA_FILES))


CXXFILES	:= $(shell find . -type f -name '*.cc')
HEADER_DEPS	:= $(CXXFILES:.cc=.d)
TSTFILES	:= $(CXXFILES:.cc=.out)

BIN_DIR		:= bin

SILENT		:= @

ifdef verbose
	SILENT	:=
endif

ifdef std
	CXXFLAGS += -DFT_TEST_STD
endif

.PHONY:	all
all: run

.PHONY: analyze
analyze:
	${MAKE} clean
	${MAKE} testdrivers ANALYZER="-fanalyzer"

.PHONY: run
run: $(TSTFILES)
	-@rc=0; count=0; res=0; \
	for file in $(TSTFILES); do \
		timeout 5s ./$$file; \
		res="$$?"; \
		if [ "$$res" -eq 0 ]; \
		then printf PASSED; \
		else printf FAILED; \
		fi; \
		echo "	$$file"; \
		rc=`expr $$rc + $$(($$res != 0))`; count=`expr $$count + 1`; \
	done; \
	echo; echo "Executed $$count tests	Failed tests $$rc"

.PHONY: testdrivers
testdrivers: $(TSTFILES)

-include $(HEADER_DEPS)
%.out: %.cc $(COBRA_OBJS) Makefile
	$(SILENT)$(CXX) -o $@ $< $(COBRA_OBJS) $(CXXFLAGS)

-include $(COBRA_DEPS)
$(OBJ_DIR)/%.o: $(COBRA_SRC)/%.cc Makefile
	@mkdir -p $(@D)
	$(SILENT)$(CXX) -o $@ $< $(CXXFLAGS) -c -MMD

.PHONY: clean
clean:
	rm -f $(TSTFILES) $(HEADER_DEPS) $(COBRA_OBJS) $(COBRA_DEPS)

.PHONY: fclean
fclean:
	${MAKE} clean

.PHONY: re
re:
	${MAKE} fclean
	${MAKE}
