################################################################################
#
# Makefile for libatomcpp.so
#
################################################################################

# Set the default target for just 'make' to all
.DEFAULT_GOAL := all

OUTPUT_NAME := libatomcpp.so

# Install directory name
HEADER_INSTALL_DIR:=/usr/local/include/atomcpp
LIB_INSTALL_DIR:=/usr/local/lib

SOURCE_DIR:=src
INCLUDE_DIR:=inc

BUILD_DIR:=build
THIRDPARTY_DIR:=third-party
THIRDPARTY_BUILD_DIR:=/usr/local
BREDIS_INCLUDE_DIR:=$(THIRDPARTY_DIR)/cpp-bredis

CPP_SRCS := $(wildcard $(SOURCE_DIR)/*.cc)
CPP_OBJS = $(addprefix $(BUILD_DIR)/lib/,$(notdir $(CPP_SRCS:.cc=.o)))
vpath %.c $(sort $(dir $(CPP_SRCS)))

HEADER_SRCS := $(wildcard $(INCLUDE_DIR)/*.h)
HEADER_OBJS = $(addprefix $(BUILD_DIR)/inc/,$(notdir $(HEADER_SRCS)))
vpath %.h $(sort $(dir $(HEADER_SRCS)))

TEST_DIR:=test
TEST_BINARY:=test_atom_cpp
TEST_SRCS := $(wildcard $(TEST_DIR)/*.cc) $(wildcard $(SOURCE_DIR)/*.cc)
TEST_OBJS = $(addprefix $(TEST_DIR)/$(BUILD_DIR)/,$(notdir $(TEST_SRCS:.cc=.o)))
vpath %.cc $(sort $(dir $(TEST_SRCS)))

# Check to see if we got a test filter
ifeq ($(TEST_FILTER),)
	TEST_FILTER:="*"
endif

# CFLAGS
CFLAGS := -std=c++11 -Wall -Werror -fPIC -I${INCLUDE_DIR} -I${THIRDPARTY_BUILD_DIR}/include/ -I${BREDIS_INCLUDE_DIR}/include/ -g

#LDFLAGS
LDFLAGS := -L${THIRDPARTY_BUILD_DIR}/lib -Wl,-rpath,${THIRDPARTY_BUILD_DIR}/lib -latom -lhiredis -lpthread -lboost_system -lboost_thread

$(BUILD_DIR)/lib/%.o: src/%.cc $(HEADER_OBJS) | $(BUILD_DIR)/lib
	@ echo "Compiling $<"
	@ $(CXX) -c $(CFLAGS) -o $@ $(filter %.cc,$^)

$(BUILD_DIR)/inc/%.h: %.h | $(BUILD_DIR)/inc
	@ echo "Copying $<"
	@ cp $< $@

$(BUILD_DIR)/lib/$(OUTPUT_NAME): $(CPP_OBJS) $(HEADER_OBJS) | $(BUILD_DIR)/lib
	@ echo "Linking $@"
	@ $(CXX) -shared $(filter %.o,$^) ${LDFLAGS} -o $@

$(BUILD_DIR)/inc: | $(BUILD_DIR)
	@ echo "Creating $@"
	@ mkdir $@

$(BUILD_DIR)/lib: | $(BUILD_DIR)
	@ echo "Creating $@"
	@ mkdir $@

$(BUILD_DIR):
	@ echo "Creating $@"
	@ mkdir $@

$(TEST_DIR)/$(BUILD_DIR):
	@ echo "Creating $@"
	@ mkdir $@

$(TEST_DIR)/$(BUILD_DIR)/%.o: test/%.cc $(HEADER_OBJS) | $(TEST_DIR)/$(BUILD_DIR)
	@ echo "Compiling $<"
	@ $(CXX) -c $(CFLAGS) -I$(TEST_DIR) -coverage -o $@ $(filter %.cc,$^)

$(TEST_DIR)/$(BUILD_DIR)/%.o: src/%.cc $(HEADER_OBJS) | $(TEST_DIR)/$(BUILD_DIR)
	@ echo "Compiling $<"
	@ $(CXX) -c $(CFLAGS) -I$(TEST_DIR) -coverage -O0 -fprofile-arcs -ftest-coverage -g -o $@ $(filter %.cc,$^)

$(TEST_DIR)/$(BUILD_DIR)/$(TEST_BINARY): $(TEST_OBJS) $(HEADER_OBJS) | $(TEST_DIR)/$(BUILD_DIR)
	@ echo "Linking $@"
	@ $(CXX) $(filter %.o,$^) -L${BUILD_DIR}/lib -Wl,-rpath,${BUILD_DIR}/lib -latom -lgtest_main -lgtest -lgmock $(LDFLAGS) -coverage -O0 -fprofile-arcs -ftest-coverage -g -o $@

.PHONY: all
all: $(BUILD_DIR)/lib/$(OUTPUT_NAME)

.PHONY: install
install: $(BUILD_DIR)/lib/$(OUTPUT_NAME)
	mkdir -p ${HEADER_INSTALL_DIR}
	cp -r $(BUILD_DIR)/inc/*.h ${HEADER_INSTALL_DIR}
	cp -r $(BUILD_DIR)/lib/*.so ${LIB_INSTALL_DIR}

.PHONY: test
test: $(TEST_DIR)/$(BUILD_DIR)/$(TEST_BINARY)
	./$(TEST_DIR)/$(BUILD_DIR)/$(TEST_BINARY) --gtest_filter=$(TEST_FILTER)
	mkdir -p ./$(TEST_DIR)/$(BUILD_DIR)/coverage/
	lcov -c --directory ./$(TEST_DIR)/$(BUILD_DIR)/ --output-file ./$(TEST_DIR)/$(BUILD_DIR)/coverage/main_coverage.info
	genhtml ./$(TEST_DIR)/$(BUILD_DIR)/coverage/main_coverage.info --output-directory ./$(TEST_DIR)/$(BUILD_DIR)/coverage/

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(TEST_DIR)/$(BUILD_DIR)
