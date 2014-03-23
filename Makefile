CXX = g++
CXXFLAGS = -std=c++0x -g -Wall 
#INCLUDES := 
LD := g++
LDFLAGS := -lboost_program_options-mt -lboost_thread-mt -lboost_system-mt -lboost_filesystem-mt -lboost_regex
SOURCES := $(shell find . -name '*.cpp' -print | sort)
OBJECTS := $(SOURCES:.cpp=.o)
TARGETS = FileIndexer FileWorker WordIndex

%.o:%.cpp %.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $<

all: program


program: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)
	chmod 755 $@

clean:
	rm -f $(TARGETS) $(OBJECTS)