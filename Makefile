COMPILE := g++
CXXFLAGS := -MMD -fPIC -Iinc

SRCDIR := src
BUILDDIR := .
TARGET := liblbHttpd.so

SERVERSDIR := servers
SERVERSBUILDDIR := .
SERVERSTARGET := wsEcho

GTESTDIR := gtest
GTESTBUILDDIR := .
GTESTTARGET := serverTests

LBENCODINGPATH := ../liblbEncoding
LBENCODINGINC := -I $(LBENCODINGPATH)/inc
LBENCODINGLD := -L$(LBENCODINGPATH) -llbEncoding

# List of all .cpp source files.
CPP = $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/ws/*.cpp)
SERVERSCPP = $(wildcard $(SERVERSDIR)/wsEcho/*.cpp)
GTESTCPP = $(wildcard $(GTESTDIR)/*.cpp)

# All .o files go to build dir.
OBJ = $(CPP:%.cpp=$(BUILDDIR)/%.o)
SERVERSOBJ = $(SERVERSCPP:%.cpp=$(SERVERSBUILDDIR)/%.o)
GTESTOBJ = $(GTESTCPP:%.cpp=$(GTESTBUILDDIR)/%.o)

# gcc will create these .d files containing dependencies.
DEP = $(OBJ:%.o=%.d)
SERVERSDEP = $(SERVERSOBJ:%.o=%.d)
GTESTDEP = $(GTESTOBJ:%.o=%.d)

debug: DEBUG = -g -DDEBUG
debug: all

all: $(TARGET) $(SERVERSTARGET) $(GTESTTARGET)

$(TARGET): $(OBJ)
	$(COMPILE) -shared -lmicrohttpd $(LBENCODINGLD) -o $(TARGET) $(OBJ)

$(SERVERSTARGET): $(SERVERSOBJ)
	$(COMPILE) -o $(SERVERSTARGET) $(LBENCODINGLD) -L$(BUILDDIR) -llbHttpd $(SERVERSOBJ)

$(GTESTTARGET): $(GTESTOBJ) $(TARGET)
	$(COMPILE) -Wl,-rpath,$(BUILDDIR) $(LBENCODINGLD) -L$(BUILDDIR) -lgtest -llbHttpd -o $(GTESTTARGET)  $(GTESTOBJ)

# Include all .d files
-include $(DEP)
-include $(SERVERSDEP)
-include $(GTESTDEP)

$(BUILDDIR)/$(SRCDIR)/%.o : $(SRCDIR)/%.cpp
	mkdir -p $(@D)
	$(COMPILE) $(DEBUG) $(LBENCODINGINC) -c $(CXXFLAGS) -o $@ $<

$(SERVERSBUILDDIR)/$(SERVERSDIR)/%.o : $(SERVERSDIR)/%.cpp
	mkdir -p $(@D)
	$(COMPILE) $(DEBUG) $(LBENCODINGINC) -c $(CXXFLAGS) $(CURLINC) -o $@ $<

$(GTESTBUILDDIR)/$(GTESTDIR)/%.o : $(GTESTDIR)/%.cpp
	mkdir -p $(@D)
	$(COMPILE) $(DEBUG) $(LBENCODINGINC) -c $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(DEP) $(OBJ) $(TARGET)
	rm -f $(SERVERSDEP) $(SERVERSOBJ) $(SERVERSTARGET)
	rm -f $(GTESTDEP) $(GTESTOBJ) $(GTESTTARGET)
