ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro)
endif
 
all: 
	$(MAKE) -C debugstub || { exit 1;}
	$(MAKE) -C tcp_comms || { exit 1;}
 
#-------------------------------------------------------------------------------
lib:
#-------------------------------------------------------------------------------
	mkdir lib
 
#-------------------------------------------------------------------------------
clean:
#-------------------------------------------------------------------------------
	@$(MAKE) -C debugstub clean
	@$(MAKE) -C tcp_comms clean

