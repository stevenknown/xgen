$(info "====---- START Target Machine Info Generation Makefile ----====")

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
	@echo "BUILD gentminfo.exe"
	@echo "EXEC:"
	$(CC) -std=c++0x -D$(TARG) $(GEN_OBJS) ../../com/$(COM_OUTPUT) $(CFLAGS) -o $@ -lstdc++ -lm  #-lgcov -lgcc
	@echo "SUCCESS TO GENERATE gentminfo.exe!!"

gen_objs: $(GEN_OBJS)
	@echo "EXEC:gen_objs: $(GEN_OBJS)"

$(COM_OUTPUT):
	@echo "BUILD $(COM_OUTPUT)"
	@echo "EXEC:"
	cd ../../com && make -f Makefile TARG=$(TARG) DEBUG=$(DEBUG) CFLAGS="$(CFLAGS)"

%.o:%.cpp
	@echo "BUILD $<"
	@echo "EXEC:"
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
	@find ./ -name "*.d" | xargs rm -f
	@find ./ -name "*.dot" | xargs rm -f
	@find ./ -name "*.elf" | xargs rm -f
	@find ./ -name "*.out" | xargs rm -f
	@find ./ -name "*.tmp" | xargs rm -f
	@find ./ -name "*.vcg" | xargs rm -f
	@find ./ -name "*.cxx" | xargs rm -f
	@find ./ -name "*.asm" | xargs rm -f

$(info "====---- END Target Machine Info Generation Makefile ----====")
