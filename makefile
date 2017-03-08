CXXFLAGS += -masm=intel
CXXFLAGS += -MD -MP
CXXFLAGS += -std=gnu++17
CXXFLAGS += -Wall -Wextra
CXXFLAGS += -gdwarf-4
CXXFLAGS += -funwind-tables -fasynchronous-unwind-tables
CXXFLAGS += -fnon-call-exceptions 
CXXFLAGS += -mcld
CXXFLAGS += -mpreferred-stack-boundary=4

INCLUDE := -Iinclude
LIBS := 

OUTPUT := libjwdpmi.a

SRCDIR := src
OUTDIR := bin
OBJDIR := obj
SRC := $(wildcard $(SRCDIR)/*.cpp)
OBJ := $(SRC:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
DEP := $(SRC:$(SRCDIR)/%.cpp=$(OBJDIR)/%.d)

.PHONY: all clean

all: $(OBJDIR) $(OUTDIR) $(OUTDIR)/$(OUTPUT)

clean:
	-rm -rf obj/* bin/*

$(OUTDIR): 
	-mkdir $(OUTDIR)

$(OBJDIR):
	-mkdir $(OBJDIR)

$(OUTDIR)/$(OUTPUT): $(OBJ)
	ar cru $@ $(OBJ) $(LIBS)
	ranlib $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -o $@ -MF $(@:.o=.d) $(INCLUDE) -c $< $(PIPECMD)

ifneq ($(MAKECMDGOALS),clean)
  -include $(DEP)
endif
