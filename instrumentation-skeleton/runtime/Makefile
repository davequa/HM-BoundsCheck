# overwritten in setup.py
OBJDIR         ?= obj
LLVM_VERSION   ?= 3.8.0

PKG_CONFIG     := python3 ../setup.py pkg-config
BUILTIN_CFLAGS := `$(PKG_CONFIG) llvm-passes-builtin-$(LLVM_VERSION) --runtime-cflags`

CC      := gcc
CCFLAGS := -O2 -fpic -Wall -Wextra -march=native $(BUILTIN_CFLAGS)
LIB      := libcount.so
OBJS     := count.o

.PHONY: all clean

all: $(OBJDIR)/$(LIB)

$(OBJDIR)/$(LIB): $(addprefix $(OBJDIR)/,$(OBJS))
	$(CC) -shared  -o $@ $^

$(OBJDIR)/%.o: %.cxx | $(OBJDIR)
	$(CC) -c $(CCFLAGS) -o $@ $<

$(OBJDIR):
	mkdir -p $@

clean:
	rm -rf $(OBJDIR)
