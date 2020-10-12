GTEST_DIR = ./gtest/googletest
GTEST_LIB_DIR = ./$(OBJ_DIR)
GTEST_LIBS = $(OBJ_DIR)/libgtest.a $(OBJ_DIR)/libgtest_main.a
GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h \
				$(GTEST_DIR)/include/gtest/internal/*.h
GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)

SRC_DIR = ./src
HEADER_DIR = ./include
TEST_DIR = ./test
DATA_DIR = ./data
OBJ_DIR= ./obj
BIN_DIR = ./bin
HEADERS = $(HEADER_DIR)/*.h 

CPPFLAGS += -isystem $(GTEST_DIR)/include
CXXFLAGS += -Wall -O3 -pthread -std=c++17 -fno-omit-frame-pointer
PMEMFLAGS = -lpmem

EHASH_TEST = $(BIN_DIR)/ehash_test
YCSB_TEST = $(BIN_DIR)/ycsb
TESTS = $(EHASH_TEST) $(YCSB_TEST)

MAKEDIR = mkdir -p

all : $(GTEST_LIBS) $(TESTS)
ycsb: $(YCSB_TEST)
ehash: $(EHASH_TEST)

$(OBJ_DIR)/gtest-all.o : $(GTEST_SRCS_) | $(OBJ_DIR)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c $(GTEST_DIR)/src/gtest-all.cc -o $@

$(OBJ_DIR)/gtest_main.o : $(GTEST_SRCS_) | $(OBJ_DIR)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c $(GTEST_DIR)/src/gtest_main.cc -o $@

$(OBJ_DIR)/libgtest.a : $(OBJ_DIR)/gtest-all.o | $(OBJ_DIR)
	$(AR) $(ARFLAGS) $@ $^

$(OBJ_DIR)/libgtest_main.a : $(OBJ_DIR)/gtest-all.o $(OBJ_DIR)/gtest_main.o | $(OBJ_DIR)
	$(AR) $(ARFLAGS) $@ $^

$(OBJ_DIR)/data_page.o: $(SRC_DIR)/data_page.cpp $(HEADERS) | $(OBJ_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -I $(HEADER_DIR) -o $@
	
$(OBJ_DIR)/pm_ehash.o: $(SRC_DIR)/pm_ehash.cpp $(HEADERS) | $(OBJ_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -I $(HEADER_DIR) -o $@

$(OBJ_DIR)/ehash_test.o : $(TEST_DIR)/ehash_test.cpp  $(HEADERS) $(GTEST_HEADERS) | $(OBJ_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -I $(HEADER_DIR) -o $@

$(EHASH_TEST) : $(OBJ_DIR)/pm_ehash.o $(OBJ_DIR)/data_page.o $(OBJ_DIR)/ehash_test.o $(GTEST_LIBS) | $(DATA_DIR) $(BIN_DIR) 
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -L$(GTEST_LIB_DIR) -lgtest_main -lpthread $^ $(PMEMFLAGS) -o $@

$(OBJ_DIR)/ycsb.o: $(TEST_DIR)/ycsb.cpp $(HEADERS) | $(OBJ_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -I $(HEADER_DIR) -o $@

$(YCSB_TEST): $(OBJ_DIR)/ycsb.o $(OBJ_DIR)/pm_ehash.o $(OBJ_DIR)/data_page.o | $(OBJ_DIR) $(BIN_DIR) 
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -lpthread $^ $(PMEMFLAGS) -o $@

$(OBJ_DIR):
	$(MAKEDIR) obj

$(BIN_DIR):
	$(MAKEDIR) bin

.PHONY: clean cleano cleand
clean :
	$(RM) $(GTEST_LIBS) $(TESTS) $(OBJ_DIR)/*.o data/*

cleanobj :
	$(RM) $(GTEST_LIBS) $(OBJ_DIR)/*.o

cleandata : 
	$(RM) ../data/*
