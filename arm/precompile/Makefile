ifeq ($(WIN), true)
  $(info "On Windows")
  #Add predefined macro if build host is Windows.
  CFLAGS += -D_ON_WINDOWS_

  #Set HOST_CC compiler  
  ifeq ($(WSL), true)
    CFLAGS += -D_USE_GCC_
    ifndef HOST_CC
      HOST_CC := $(shell which clang++ > /dev/null)
      ifndef HOST_CC
        HOST_CC = $(if $(shell which clang),clang,gcc.exe)
      endif
    endif
  else
    #There is no CC compiler avaiable without WSL environment.
  endif 
else
  $(info "On Linux or Unix")
  CFLAGS += -D_USE_GCC_
  ifndef HOST_CC
    HOST_CC := $(shell which clang++ > /dev/null)
    ifndef HOST_CC
      HOST_CC = $(if $(shell which clang),clang,gcc)
    endif
  else
    $(info "HOST_CC is defined="$(HOST_CC))
  endif
endif
$(info "HOST_CC=$(HOST_CC),TARGET=$(TARGET),CFLAGS=$(CFLAGS),WIN=$(WIN),WSL=$(WSL)")

GEN_OBJS +=\
arm_gen_targ_mach_resource.o

COM_OBJS +=\
../../com/ltype.o \
../../com/smempool.o \
../../com/strbuf.o \
../../com/diagnostic.o \
../../com/bs.o\
../../com/comf.o

gentminfo.exe: gen_objs com_objs
ifeq ($(HOST_CC),)
	$(error "Miss HOST_CC")
endif
ifeq ($(TARGET),)
	$(error "Miss TARGET")
endif
ifeq ($(CFLAGS),) 
	$(error "Miss CFLAGS") 
endif
	$(HOST_CC) -std=c++0x -D$(TARGET) $(GEN_OBJS) $(COM_OBJS) $(CFLAGS) -o $@ -lstdc++ -lm  #-lgcov -lgcc
	@echo "success!!"

%.o:%.cpp
ifeq ($(HOST_CC),)
	$(error "Miss HOST_CC")
endif
ifeq ($(TARGET),)
	$(error "Miss TARGET") 
endif
ifeq ($(CFLAGS),)
	$(error "Miss CFLAGS") 
endif
	$(info CFLAGS=$(CFLAGS),TARGET=$(TARGET))
	@echo "CMD>>$(INFER) $(HOST_CC) -D$(TARGET) $(CFLAGS) -c $< -o $@"
	$(INFER) $(HOST_CC) -D$(TARGET) $(CFLAGS) -c $< -o $@

gen_objs: $(GEN_OBJS)
com_objs: $(COM_OBJS)

clean:
	@rm -f ../../com/ltype.o
	@rm -f ../../com/smempool.o
	@rm -f ../../com/strbuf.o
	@rm -f ../../com/bs.o
	@find ./ -name "*.exe" | xargs rm -f
	@find ./ -name "*.gcda" | xargs rm -f
	@find ./ -name "*.gcno" | xargs rm -f
	@find ./ -name "*.o" | xargs rm -f
	@find ./ -name "*.dot" | xargs rm -f
	@find ./ -name "*.elf" | xargs rm -f
	@find ./ -name "*.out" | xargs rm -f
	@find ./ -name "*.tmp" | xargs rm -f
	@find ./ -name "*.vcg" | xargs rm -f
	@find ./ -name "*.cxx" | xargs rm -f
	@find ./ -name "*.asm" | xargs rm -f
