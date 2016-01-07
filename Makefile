
LIB_DIR = ./lib
BIN_DIR = ./bin
INC_DIR = ./include

C = clang
CFLAGS = -O3 -g -fPIC -I$(INC_DIR)

L = clang
LFLAGS = -L$(LIB_DIR)

AR = ar
ARFLAGS = rcs
#DEFINES=-D 

LIB_SRCS := $(shell find NatNetC -mindepth 1 -maxdepth 4 -name "*.c")
LIB_OBJS = $(LIB_SRCS:.c=.o)
LIBNAME = NatNetC

DEMO_SRCS = NatNetCLI/NatNetMain.c
DEMO_OBJS = $(DEMO_SRCS:.c=.o)
DEMONAME = nn_demo

TEST_SRCS := $(shell find test -mindepth 1 -maxdepth 4 -name "*.c")
TEST_OBJS = $(TEST_SRCS:.c=.o)
TESTNAME = nn_test

SRCS = $(LIB_SRCS) $(DEMO_SRCS) $(TEST_SRCS)
OBJS = $(LIB_OBJS) $(DEMO_OBJS) $(TEST_OBJS)

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
DLLNAME = lib$(LIBNAME).so
DLLFLAGS = -shared -Wl,-soname,$(DLLNAME)
BIN_LIBPATH = -L./lib
BIN_LIBS = -lpthread
else ifeq ($(UNAME), Darwin)
DLLNAME = lib$(LIBNAME).dylib
DLLFLAGS = -dynamiclib -install_name $(LIB_DIR)/$(DLLNAME) -current_version 1.0
BIN_LIBPATH = -L./lib
BIN_LIBS = -lpthread
endif

all: static dll

message:
	@echo "To compile do:"
	@echo "make static (build static library"$()")"
	@echo "make dll    (build dynamic library)"
	@echo "make demo   (build "$(DEMO_SRCS)")"
	@echo "make test   (build "$(TEST_SRCS)")"

# include dependency file
-include .depend

dirs:
	@mkdir -p lib
	@mkdir -p bin

static: dirs $(LIB_OBJS)
	$(AR) $(ARFLAGS) $(LIB_DIR)/lib$(LIBNAME).a $(LIB_OBJS)
	
dll: dirs $(LIB_OBJS)
	$(C) $(DLLFLAGS) $(LIB_OBJS) -o $(LIB_DIR)/$(DLLNAME)
	
demo: dirs static $(DEMO_OBJS)
	$(C) $(DEMO_OBJS) $(BIN_LIBPATH) $(BIN_LIBS) $(LIB_DIR)/lib$(LIBNAME).a -o $(BIN_DIR)/$(DEMONAME)
	
test: dirs static $(TEST_OBJS)
	$(C) $(TEST_OBJS) $(BIN_LIBPATH) $(BIN_LIBS) $(LIB_DIR)/lib$(LIBNAME).a -o $(BIN_DIR)/$(TESTNAME)
	
.c.o:
	$(C) $(CFLAGS) -c $< -o $@
	
clean:
	rm -f $(OBJS)

clobber: clean
	rm -f $(LIB_DIR)/* $(BIN_DIR)/*

depend:
	mkdep $(CXXFLAGS) $(SRCS)

.PHONY: clean clobber message
