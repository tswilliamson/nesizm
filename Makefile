#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(FXCGSDK)),)
export FXCGSDK := $(realpath ../../)
endif

include $(FXCGSDK)/toolchain/prizm_rules


#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
TARGET		:=	$(notdir $(CURDIR))
BUILD		:=	$(CONFIG)
SOURCES		:=	src src/scope_timer src/gfx src/asm src/mappers
DATA		:=	data  
INCLUDES	:=  src

#---------------------------------------------------------------------------------
# options for code and add-in generation
#---------------------------------------------------------------------------------

MKG3AFLAGS := -n basic:NESizm

OPTIMIZATION = -O2

# add -S -fverbose-asm for assembly output
CBASEFLAGS	= $(OPTIMIZATION) \
		  -Wall \
		  -funroll-loops \
		  -fno-trapping-math \
		  -fno-trapv \
		  -fno-threadsafe-statics \
		  -fno-unwind-tables \
		  -Wno-switch \
		  -Wno-stringop-truncation \
		  -Wno-class-memaccess \
		  -Wno-narrowing \
		  -Wno-format-overflow \
		  -mpretend-cmove \
		  -fdelayed-branch \
		  $(MACHDEP) $(INCLUDE) $(DEFINES)
		  
CFLAGS	=	$(CBASEFLAGS) \
		  -std=c99

CXXFLAGS	=  $(CBASEFLAGS) \
		  -fpermissive \
		  -fno-rtti \
		  -fno-exceptions \
		  -fno-threadsafe-statics \
		  -fno-use-cxa-get-exception-ptr \
		  -std=c++11

ASFLAGS	=	$(CFLAGS) 

LDFLAGS	=  -Xlinker -Map=$(CURDIR)/output.map $(MACHDEP) $(OPTIMIZATION) -T$(FXCGSDK)/toolchain/prizm.x -Wl,-static -g

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
SYSTEMLIBS :=  -lc -lfxcg -lgcc
LIBS	:=	-lzx7 -lcalctype -lsnd -lptune2_simple $(SYSTEMLIBS)

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT		:=	$(CURDIR)/$(BUILD)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
					$(SFILES:.S=.o)

#---------------------------------------------------------------------------------
# build a list of include paths
#---------------------------------------------------------------------------------
export INCLUDE	:=	$(foreach dir,$(INCLUDES), -iquote $(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD) -I$(LIBFXCG_INC) -I$(UTILS_INC)

#---------------------------------------------------------------------------------
# build a list of library paths
#---------------------------------------------------------------------------------
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
					-L$(FXCGSDK)/utils/lib \
					-L$(LIBFXCG_LIB) 
					
export OUTPUT		:=	$(CURDIR)/$(BUILD)/$(TARGET)
export OUTPUT_FINAL		:=	$(CURDIR)/$(TARGET)
.PHONY: $(BUILD) clean

#---------------------------------------------------------------------------------
$(BUILD):
	-mkdir $@
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
export CYGWIN := nodosfilewarning
clean:
	$(call rmdir,$(BUILD))
	$(call rm,$(OUTPUT).bin)
	$(call rm,$(OUTPUT_FINAL).g3a)
	$(call rm,$(OUTPUT_FINAL)_cg10.g3a)

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT_FINAL)_cg10.g3a: $(OUTPUT).bin
	$(MKG3A) $(MKG3AFLAGS) -i uns:../unselected_cg10.bmp -i sel:../selected_cg10.bmp $(OUTPUT).bin $@

$(OUTPUT_FINAL).g3a: $(OUTPUT_FINAL)_cg10.g3a
	$(MKG3A) $(MKG3AFLAGS) -i uns:../unselected.bmp -i sel:../selected.bmp $(OUTPUT).bin $@
	
.DEFAULT_GOAL := $(OUTPUT_FINAL).g3a

$(OUTPUT).bin: $(OFILES) 

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------