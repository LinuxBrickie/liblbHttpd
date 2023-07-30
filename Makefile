COMPILE := g++
CXXFLAGS := -MMD -fPIC -Iinc

SRCDIR := src
BUILDDIR := .
TARGET := liblbHttpd.so

GTESTDIR := gtest
GTESTBUILDDIR := .
GTESTTARGET := serverTests

LBENCODINGPATH := ../liblbEncoding
LBENCODINGINC := -I $(LBENCODINGPATH)/inc
LBENCODINGLD := -L$(LBENCODINGPATH) -llbEncoding

# List of all .cpp source files.
CPP = $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/ws/*.cpp)
GTESTCPP = $(wildcard $(GTESTDIR)/*.cpp)

# All .o files go to build dir.
OBJ = $(CPP:%.cpp=$(BUILDDIR)/%.o)
GTESTOBJ = $(GTESTCPP:%.cpp=$(GTESTBUILDDIR)/%.o)

# gcc will create these .d files containing dependencies.
DEP = $(OBJ:%.o=%.d)
GTESTDEP = $(GTESTOBJ:%.o=%.d)

debug: DEBUG = -g -DDEBUG
debug: all

all: $(TARGET) $(GTESTTARGET)

$(TARGET): $(OBJ)
	$(COMPILE) -shared -lmicrohttpd $(LBENCODINGLD) -o $(TARGET) $(OBJ)

$(GTESTTARGET): $(GTESTOBJ) $(TARGET)
	$(COMPILE) -Wl,-rpath,$(BUILDDIR) $(LBENCODINGLD) -L$(BUILDDIR) -lgtest -llbHttpd -o $(GTESTTARGET)  $(GTESTOBJ)

# Include all .d files
-include $(DEP)
-include $(GTESTDEP)

$(BUILDDIR)/$(SRCDIR)/%.o : $(SRCDIR)/%.cpp
	mkdir -p $(@D)
	$(COMPILE) $(DEBUG) $(LBENCODINGINC) -c $(CXXFLAGS) -o $@ $<

$(GTESTBUILDDIR)/$(GTESTDIR)/%.o : $(GTESTDIR)/%.cpp
	mkdir -p $(@D)
	$(COMPILE) $(DEBUG) -c $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(DEP) $(OBJ) $(TARGET)
	rm -f $(GTESTDEP) $(GTESTOBJ) $(GTESTTARGET)
