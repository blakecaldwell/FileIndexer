CXX = g++
CXXFLAGS = -std=c++0x -g -Wall 
INCLUDES := -I/home/blake/boost/include
LD := g++
LDFLAGS := -L/home/blake/boost/lib -W1,-rpath,/home/blake/boost/lib -lboost_program_options -lboost_thread -lboost_system -lboost_filesystem -lboost_regex
SOURCES := $(shell find . -name '*.cpp' -print | sort)
OBJECTS := $(SOURCES:.cpp=.o)
TARGETS = FileIndexer FileWorker WordIndex

%.o:%.cpp %.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $<

all: program


program: $(OBJECTS)
	$(LD) -o $@ $(OBJECTS) $(LDFLAGS)
	chmod 755 $@

clean:
	rm -f $(TARGETS) $(OBJECTS)
