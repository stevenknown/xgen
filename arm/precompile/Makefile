GEN_OBJS +=\
arm_gen_targ_mach_resource.o

COM_OBJS +=\
../../com/ltype.o \
../../com/smempool.o \
../../com/strbuf.o \
../../com/diagnostic.o \
../../com/bs.o

gentminfo.exe: gen_objs com_objs
ifeq ($(TARGET),)
	$(error "Miss TARGET")
endif
ifeq ($(CFLAGS),) 
	$(error "Miss CFLAGS") 
endif
	$(info CFLAGS=$(CFLAGS),TARGET=$(TARGET))
	gcc -D$(TARGET) $(GEN_OBJS) $(COM_OBJS) $(CFLAGS) -o $@ -lstdc++ -lm  #-lgcov -lgcc
	@echo "success!!"

%.o:%.cpp
ifeq ($(TARGET),)
	$(error "Miss TARGET") 
endif
ifeq ($(CFLAGS),) 
	$(error "Miss CFLAGS") 
endif
	$(info CFLAGS=$(CFLAGS),TARGET=$(TARGET))
	@echo "build $<"
	$(INFER) gcc  -D$(TARGET) $(CFLAGS) -c $< -o $@

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
