clean:
	@find ./ -name "*.o" | xargs rm -f
	@find ./ -name "*.a" | xargs rm -f
	@find ./ -name "*.dot" | xargs rm -f
	@find ./ -name "*.exe" | xargs rm -f
	@find ./ -name "*.elf" | xargs rm -f
	@find ./ -name "*.out" | xargs rm -f
	@find ./ -name "*.tmp" | xargs rm -f
	@find ./ -name "*.vcg" | xargs rm -f
	@find ./ -name "*.cxx" | xargs rm -f
	@find ./ -name "*.asm" | xargs rm -f
	@find ./ -name "*.swp" | xargs rm -f
	@find ./ -name "*.swo" | xargs rm -f
	@find ./ -name "*.log" | xargs rm -f
	@find ./ -name "*.LOGLOG" | xargs rm -f
	@find ./ -name "LOGLOG" | xargs rm -f

