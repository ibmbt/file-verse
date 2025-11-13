CXX = g++
CXXFLAGS = -std=c++17 -I./source/include
SOURCES = source/core/bscs24043.cpp \
          source/core/fs_format.cpp \
          source/core/fs_init.cpp \
          source/core/file_operations.cpp \
          source/core/directory_operations.cpp \
          source/core/user_management.cpp \
          source/core/info_operations.cpp

testing: $(SOURCES)
	$(CXX) $(CXXFLAGS) -o testing $(SOURCES)

clean:
	rm -f testing

.PHONY: clean