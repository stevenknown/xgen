GEN_OBJS+=arm_gen_targ_mach_resource.o

include ../../com/Makefile.inc
include ../../Makefile.option

$(info "CC=$(CC),TARG=$(TARG),CFLAGS=$(CFLAGS),WIN=$(WIN),WSL=$(WSL)")

ifeq ($(CC),)
	$(error "MISS CC")
endif
ifeq ($(TARG),)
	$(error "MISS TARG")
endif
ifeq ($(CFLAGS),) 
	$(error "MISS CFLAGS") 
endif

gentminfo.exe: gen_objs $(COM_OUTPUT)
	$(CC) -std=c++0x -D$(TARG) $(GEN_OBJS) ../../com/$(COM_OUTPUT) $(CFLAGS) -o $@ -lstdc++ -lm  #-lgcov -lgcc
	@echo "SUCCESS!!"

gen_objs: $(GEN_OBJS)

$(COM_OUTPUT):
	@echo "BUILD $(COM_OUTPUT)"
	cd ../../com && make -f Makefile TARG=$(TARG) DEBUG=$(DEBUG)

%.o:%.cpp
	$(info CFLAGS=$(CFLAGS),TARG=$(TARG))
	@echo "BUILD $(CC) -D$(TARG) $(CFLAGS) -c $< -o $@"
	$(INFER) $(CC) -D$(TARG) $(CFLAGS) -c $< -o $@

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
