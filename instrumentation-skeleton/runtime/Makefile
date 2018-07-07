# overwritten in setup.py
OBJDIR         ?= obj
LLVM_VERSION   ?= 3.8.0

PKG_CONFIG     := python3 ../setup.py pkg-config
BUILTIN_CFLAGS := `$(PKG_CONFIG) llvm-passes-builtin-$(LLVM_VERSION) --runtime-cflags`
LLVM_PREFIX	:= `$(PKG_CONFIG) llvm-$(LLVM_VERSION) --prefix`

#CC	:= gcc
CC      := $(LLVM_PREFIX)/bin/clang
AR	:= $(LLVM_PREFIX)/bin/llvm-ar
#CCFLAGS := -std=-O2 -fpic -Wall -Wextra -march=native $(BUILTIN_CFLAGS)
CCFLAGS := -flto -O2 -fpic -Wall -Wextra -march=native $(BUILTIN_CFLAGS)
#CCFLAGS := -O2 -fpic -Wall -Wextra -march=native $(BUILTIN_CFLAGS)
LIB      := libhmboundscheck.a
OBJS     := hmboundscheck.o
#LIB      := libdhash.a
#OBJS     := dhash.o

.PHONY: all clean

all: $(OBJDIR)/$(LIB)

$(OBJDIR)/$(LIB): $(addprefix $(OBJDIR)/,$(OBJS))
	$(AR) rcs $@ $^
#	$(CC) -shared -o $@ $^

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) -c $(CCFLAGS) -g -o $@ $<

$(OBJDIR):
	mkdir -p $@

clean:
	rm -rf $(OBJDIR)
