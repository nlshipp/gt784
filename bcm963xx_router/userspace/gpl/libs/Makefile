
# To add a lib, add it to LIB_DIRS, and write the target rule in the
# last section of this Makefile
#
LIB_DIRS = sqlite 

#
# We need to declare these directory names as phony targets,
# otherwise make will think the directories themselves are
# the targets and will never execute the cmds.
# See GNU Make, 4.6 
#
.PHONY : $(LIB_DIRS)

all : $(LIB_DIRS)


#
# Cleaning rules
# See example in GNU Make, 4.6
#
clean:
	for dir in $(LIB_DIRS); do \
		if [ -d $$dir ] ; then \
			if [ -f $$dir/Makefile ] ; then \
				$(MAKE) -C $$dir clean; \
			fi \
		fi \
	done


#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
# You do not need to modify this part.
#
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))

include $(BUILD_DIR)/make.common

export BUILD_DIR



##############################################################
# 
# Rules for inidividual apps
# libs can be compiled or not depending on the profile (which
# is modified via make menuconfig)
#
##############################################################

ifneq ($(strip $(BUILD_DLNA)),)
sqlite: sanity_check
	$(MAKE) -C sqlite dynamic
else
sqlite: sanity_check
	@echo "skipping SQLite (not configured)"
endif
