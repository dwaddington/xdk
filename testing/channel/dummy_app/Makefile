include ../../../mk/global.mk
## Compiler, tools and options
CC      =  gcc
CXX     =  g++
LINK    =  $(CXX)
OPT     =  -g -O3
CCFLAGS = $(OPT) -Wall  
LDFLAGS = $(OPT)


# Libraries
LIBS = $(XDK_LIBS) $(BOOST_LIBS) -L$(XDK_BASE)/lib/libexo -L$(XDK_BASE)/lib/libcomponent -lexo -lcomponent -lpthread -lnuma -ldl -lrt -lm -ltinyxml

# Includes.
LIBEXO_INC_DIR := ../../../lib/libexo

INC = -I. -I$(XDK_BASE)/lib/libexo/ -I$(XDK_BASE)/lib/libcomponent/component -I$(XDK_BASE)/lib/libcomponent/ -I$(XDK_BASE)/lib/libcommon -I$(XDK_BASE)/lib/libexo/exo/component -I$(XDK_BASE)/drivers/x540-nic/ -I$(XDK_BASE) -I$(XDK_BASE)/include -I$(LIBEXO_INC_DIR)  -I$(XDK_BASE)/net

%.cc.o: %.cc
	$(CXX) -c $(CCFLAGS) $(INC) $< -o $@

%.c.o: %.c
	$(CC) -c -fPIC $(CCFLAGS) $(INC) $< -o $@

# Build all targets
all: dummy_app

TEST = dummy_app
TEST_SRC = dummy_app.cc $(XDK_BASE)/drivers/x540-nic/x540/xml_config_parser.cc

# Double pattern substition to get the object files right.
TEST_OBJ := $(patsubst %.cc, %.cc.o, $(TEST_SRC))
TEST_OBJ := $(patsubst %.c, %.c.o, $(TEST_OBJ))

$(TEST): $(TEST_OBJ)
	$(LINK) -o $@ $(TEST_OBJ) $(LDFLAGS) $(LIBS) $(INC) -Wl,-rpath=$(XDK_BASE)/lib/libexo

################################################################################

clean-bin: 
	rm -f $(TEST_OBJ) $(TEST)

clean: clean-bin

.PHONY: clean all

