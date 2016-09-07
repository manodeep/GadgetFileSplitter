### Set the default compiler -- options are icc/gcc/clang.
CC ?=gcc

### You should NOT edit below this line
DISTNAME:=GadgetFileSplitter
MAJOR:=0
MINOR:=0
PATCHLEVEL:=1
VERSION:=$(MAJOR).$(MINOR).$(PATCHLEVEL)
DO_CHECKS := 1
ifeq (clean,$(findstring clean,$(MAKECMDGOALS)))
  DO_CHECKS := 0
endif

ifeq (clena,$(findstring clena,$(MAKECMDGOALS)))
  DO_CHECKS := 0
endif

ifeq (celan,$(findstring celan,$(MAKECMDGOALS)))
  DO_CHECKS := 0
endif

ifeq (celna,$(findstring celna,$(MAKECMDGOALS)))
  DO_CHECKS := 0
endif

ifeq (distclean,$(findstring distclean,$(MAKECMDGOALS)))
  DO_CHECKS := 0
endif

ifeq (realclean,$(findstring realclean,$(MAKECMDGOALS)))
  DO_CHECKS := 0
endif

## Enable mpi mode if mpicc is set as the compiler
ifeq ($(CC), mpicc)
  OPTIONS +=-DUSE_MPI
endif


## Only set everything if the command is not "make clean"
ifeq ($(DO_CHECKS), 1)
  UNAME := $(shell uname)
  ## Colored text output
  ## Taken from: http://stackoverflow.com/questions/24144440/color-highlighting-of-makefile-warnings-and-errors
  ## Except, you have to use "echo -e" on linux and "echo" on Mac
  ECHO_COMMAND := echo -e
  ifeq ($(UNAME), Darwin)
    ECHO_COMMAND := echo
  endif
  ifeq ($(TRAVIS_OS_NAME), linux)
    ECHO_COMMAND := echo
  endif 

  ccred:=$(shell $(ECHO_COMMAND) "\033[0;31m")
  ccmagenta:=$(shell $(ECHO_COMMAND) "\033[0;35m")
  ccgreen:=$(shell $(ECHO_COMMAND) "\033[0;32m")
  ccblue:=$(shell $(ECHO_COMMAND) "\033[0;34m")
  ccreset:=$(shell $(ECHO_COMMAND) "\033[0;0m")
  boldfont:=$(shell $(ECHO_COMMAND) "\033[1m")
  ## end of colored text output

  ## First check make version. Versions of make older than 3.80 will crash
  ifneq (3.80,$(firstword $(sort $(MAKE_VERSION) 3.80)))
    ## Order-only attributes were added to make version 3.80
    $(warning $(ccmagenta)Please upgrade $(ccblue)make$(ccreset))
    ifeq ($(UNAME), Darwin)
      $(info on Mac+homebrew, use $(ccmagenta)"brew outdated xctool || brew upgrade xctool"$(ccreset))
      $(info Otherwise, install XCode command-line tools$ directly: $(ccmagenta)"xcode-select --install"$(ccreset))
      $(info This link: $(ccmagenta)"http://railsapps.github.io/xcode-command-line-tools.html"$(ccreset) has some more details)
    else
      $(info On Linux: Try some variant of $(ccmagenta)"sudo apt-get update && sudo apt-get upgrade"$(ccreset))
    endif
    $(error $(ccred)Project requires make >= 3.80 to compile.$(ccreset))
  endif
  #end of checks for make. 

  ## Make clang the default compiler on Mac
  ifeq ($(UNAME), Darwin)
    CC ?= clang
  endif

  # Now check if gcc is set to be the compiler but if clang is really under the hood.
  export CC_IS_CLANG ?= -1
  export CC_IS_GCC ?= -1
  ifeq ($(CC_IS_CLANG), -1)
    CC_VERSION := $(shell $(CC) --version 2>/dev/null)
    ifndef CC_VERSION
      $(error $(ccred)Could not find $$CC = ${CC}$(ccreset))
    endif

    ifeq (clang,$(findstring clang,$(CC_VERSION)))
      export CC_IS_CLANG := 1
      export CC_IS_GCC := 0
    endif

    export CC_VERSION
    export CC_IS_CLANG
  endif
  # Done with checking if clang is underneath gcc 

  ifeq ($(CC_IS_GCC), -1)
    ifeq (gcc,$(findstring gcc,$(CC_VERSION)))
      export CC_IS_GCC := 1
    endif
  endif
  # Additional check for real gcc

  export CC_IS_GCC_OR_CLANG := -1
  ifeq ($(CC_IS_GCC_OR_CLANG), -1)
    ifeq ($(CC_IS_GCC), 1)
      CC_IS_GCC_OR_CLANG := 1
    endif
    ifeq ($(CC_IS_CLANG), 1)
      CC_IS_GCC_OR_CLANG := 1
    endif

    ifeq ($(CC_IS_GCC_OR_CLANG), -1)
      CC_IS_GCC_OR_CLANG := 0
    endif 
    export CC_IS_GCC_OR_CLANG
  endif
  # # Unified variable showing that compiler is gcc or clang

  # CC is set at this point. In case the compiler on Mac is *not* clang under the hood
  # print an info message saying what to do in case of an error
  ifeq ($(UNAME), Darwin)
    ifneq ($(CC_IS_CLANG), 1)
      export CLANG_COMPILER_WARNING_PRINTED ?= 0
      ifeq ($(CLANG_COMPILER_WARNING_PRINTED), 0)
        $(warning Looks like $(ccmagenta)clang$(ccreset) (on Mac) is not set as the compiler. If you run into errors like $(ccred)"no such instruction: `vxorpd %xmm1, %xmm1,%xmm1'"$(ccreset), then please use $(ccmagenta)clang$(ccreset) as the compiler (directly invoke $(ccmagenta)"make"$(ccmagenta), NOT $(ccred)"make CC=gcc"$(ccreset)))
        export CLANG_COMPILER_WARNING_PRINTED := 1
      endif
    endif
  endif

  # ### The POSIX_SOURCE flag is required to get the definition of strtok_r
  CCFLAGS += -DVERSION=\"${VERSION}\"
  CCFLAGS += -std=c99 -m64 -g -Wsign-compare -Wall -Wextra -Wshadow -Wunused -fPIC -D_POSIX_SOURCE=200809L -D_GNU_SOURCE -D_DARWIN_C_SOURCE -O3 #-Ofast
  CCFLAGS += -Wno-unused-local-typedefs ## to suppress the unused typedef warning for the compile time assert for sizeof(struct config_options)

  ifneq ($(CC), mpicc)
    ifeq (icc,$(findstring icc,$(CC)))
      CCFLAGS += -xhost -opt-prefetch -opt-prefetch-distance=16 #-vec-report6
    endif
  endif

  ### GCC is slightly more complicated. CC might be called gcc but it might be clang underneath
  ### compiler specific flags for gcc
  ifeq ($(CC_IS_GCC), 1)
    ## Real gcc here
    CCFLAGS += -ftree-vectorize -funroll-loops -fprefetch-loop-arrays --param simultaneous-prefetches=4 #-ftree-vectorizer-verbose=6 -fopt-info-vec-missed #-fprofile-use -fprofile-correction #-fprofile-generate
    # Use the clang assembler on Mac.
    ifeq ($(UNAME), Darwin)
      CCFLAGS += -Wa,-q
      export CLANG_ASM_WARNING_PRINTED ?= 0
      ifeq ($(CLANG_ASM_WARNING_PRINTED), 0)
        $(warning $(ccmagenta) WARNING: gcc on Mac does not support intrinsics. Attempting to use the clang assembler $(ccreset))
        $(warning $(ccmagenta) If you see the error message $(ccred) "/opt/local/bin/as: assembler (/opt/local/bin/clang) not installed" $(ccmagenta) then try the following fix $(ccreset))
        $(warning $(ccmagenta) Either install clang ($(ccgreen)for Macports use, "sudo port install clang-3.8"$(ccmagenta)) or add option $(ccgreen)"-mno-avx"$(ccreset) to "CCFLAGS" in $(ccmagenta)"common.mk"$(ccreset))
        export CLANG_ASM_WARNING_PRINTED := 1
      endif # warning printed
    endif
  endif

  ifeq ($(CC_IS_GCC_OR_CLANG), 1) 
    CCFLAGS += -funroll-loops
    CCFLAGS += -march=native -fno-strict-aliasing
    CCFLAGS += -Wformat=2  -Wpacked  -Wnested-externs -Wpointer-arith  -Wredundant-decls  -Wfloat-equal -Wcast-qual
    CCFLAGS +=  -Wcast-align -Wmissing-declarations -Wmissing-prototypes  -Wnested-externs -Wstrict-prototypes  #-D_POSIX_C_SOURCE=2 -Wpadded -Wconversion
    CLINK += -lm 
  endif #gcc or clang

  # ## Everything is checked and ready. Print out the variables.
  export MAKEFILE_VARS_PRINTED ?= 0
  ifeq ($(MAKEFILE_VARS_PRINTED), 0)
    MAKEFILE_VARS := MAKE CC CLINK 
    # I want the equal sign in the info print out later to be aligned
    # However, the variables themselves can be longer than the tab character
    # Therefore, I am going to split the variables into "small" and "long"
    # sets of variables. Ugly, but works. I get the aligned print at the end.
    BIG_MAKEFILE_VARS := CCFLAGS OPTIONS
    tabvar:= $(shell $(ECHO_COMMAND) "\t")
    $(info )
    $(info $(ccmagenta)$(boldfont)-------COMPILE SETTINGS------------$(ccreset))
    $(foreach var, $(MAKEFILE_VARS), $(info $(tabvar) $(boldfont)$(var)$(ccreset)$(tabvar)$(tabvar) = ["$(ccblue)$(boldfont)${${var}}$(ccreset)"]))
    # this line is identical to the previous except for one less tab character. 
    $(foreach var, $(BIG_MAKEFILE_VARS), $(info $(tabvar) $(boldfont)$(var)$(ccreset)$(tabvar) = ["$(ccblue)$(boldfont)${${var}}$(ccreset)"]))
    $(info $(ccmagenta)$(boldfont)-------END OF COMPILE SETTINGS------------$(ccreset))
    $(info )
    $(info )
    export MAKEFILE_VARS_PRINTED := 1
    ##$(info $$var is [${var}])
  endif

endif ## make clean is not under effect (the if condition starts all the way at the top with variable DO_CHECKS)
