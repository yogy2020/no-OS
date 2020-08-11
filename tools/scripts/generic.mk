#needed by iio
export NO-OS

#------------------------------------------------------------------------------
#                              UTIL FUNCTIONS
#------------------------------------------------------------------------------
# recursive wildcard
rwildcard = $(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

#------------------------------------------------------------------------------
#                          COMMON INITIALIZATION
#------------------------------------------------------------------------------

ifneq ($(if $(findstring iio, $(LIBRARIES)), 1),)

IIO_DIR		= $(NO-OS)/libraries/iio
IIO_LIB		= $(IIO_DIR)/libiio.a

#Make and clean rules for iio
CLEAN_IIO	= $(MAKE) -C $(IIO_DIR) clean
$(IIO_LIB):
	$(MAKE) -C $(IIO_DIR)

ifeq (aducm3029,$(strip $(PLATFORM)))
LIBS_DIRS	+= "$(IIO_DIR)"
LIBS		+= $(notdir $(IIO_DIR))
INCLUDE_DIRS	+= $(IIO_DIR)
else
INC_PATHS += -I"$(IIO_DIR)"
LIB_PATHS += -L"$(IIO_DIR)"
LIBS += -liio
endif

endif

LIB_TARGETS += $(IIO_LIB)

# Get all .c and .h files from SRC_DIRS
SRCS     += $(foreach dir, $(SRC_DIRS), $(call rwildcard, $(dir),*.c))
INCS     += $(foreach dir, $(SRC_DIRS), $(call rwildcard, $(dir),*.h))

# Remove ignored files
SRCS     := $(filter-out $(IGNORE_FILES),$(SRCS))
INCS     := $(filter-out $(IGNORE_FILES),$(INCS))

#------------------------------------------------------------------------------
#                          INCLUDE SPECIFIC MAKEFILES
#------------------------------------------------------------------------------

ifeq (aducm3029,$(strip $(PLATFORM)))
#Aducm3029 makefile
include ../../tools/scripts/aducm.mk

else
#Xilnx and altera makefiles
ifeq ($(OS), Windows_NT)
include ../../tools/scripts/windows.mk
else
include ../../tools/scripts/linux.mk
endif

endif
