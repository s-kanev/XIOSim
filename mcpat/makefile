TAR = mcpat

.PHONY: dbg opt depend clean clean_dbg clean_opt

all: lib

libd: $(TAR).mk obj_dbg_lib
	@$(MAKE) TAG=dbg TGT=lib -C . -f $(TAR).mk

lib: $(TAR).mk obj_opt_lib
	@$(MAKE) TAG=opt TGT=lib -C . -f $(TAR).mk

dbg: $(TAR).mk obj_dbg
	@$(MAKE) TAG=dbg TGT=exe -C . -f $(TAR).mk

opt: $(TAR).mk obj_opt
	@$(MAKE) TAG=opt TGT=exe -C . -f $(TAR).mk

obj_dbg_lib:
	mkdir -p obj_dbg

obj_opt_lib:
	mkdir -p obj_opt

obj_dbg:
	mkdir -p $@

obj_opt:
	mkdir -p $@

clean: clean_dbg clean_opt

clean_dbg: obj_dbg
	@$(MAKE) TAG=dbg -C . -f $(TAR).mk clean
	rm -rf $<

clean_opt: obj_opt
	@$(MAKE) TAG=opt -C . -f $(TAR).mk clean
	rm -rf $<

