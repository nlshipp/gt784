#****************************************************************************
#
# Copyright © 2006-2008 Broadcom Corporation
#
# 
# 
# This program is free software; you can redistribute it and/or modify 
# it under the terms of the GNU General Public License, version 2, as published by  
# the Free Software Foundation (the "GPL"). 
# 
#
# 
# This program is distributed in the hope that it will be useful,  
# but WITHOUT ANY WARRANTY; without even the implied warranty of  
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  
# GNU General Public License for more details. 
#  
# 
#  
#   
# 
# A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by 
# writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
# Boston, MA 02111-1307, USA. 
#
#
#****************************************************************************

#
# In most cases, you only need to modify this first section.
# Just add libraries to LIB_DIRS
#
LIB_DIRS = cms_boardctl cms_util cms_msg openssl


#
# We need to declare these directory names as phony targets,
# otherwise make will think the directories themselves are
# the targets and will never execute the cmds.
# See GNU Make, 4.6 
#
.PHONY : $(LIB_DIRS)
all    : $(LIB_DIRS)


#
# Cleaning rules
# See example in GNU Make, 4.6
#
clean: 
	for dir in $(LIB_DIRS); do \
		$(MAKE) -C $$dir clean; \
	done


#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
#
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))

include $(BUILD_DIR)/make.common

ifneq ($(strip $(BUILD_VODSL)),)
include $(BUILD_DIR)/make.voice
endif



##############################################################
# 
# Rules for inidividual libs
# libs can be compiled or not depending on the profile (which
# is modified via make menuconfig)
#
##############################################################


# always build this lib
cms_boardctl:
	$(MAKE) -C $@

# always build this lib
cms_util:
	$(MAKE) -C $@

# always build this lib
cms_msg:
	$(MAKE) -C $@


# DO_BUILD_OPENSSL is set in make.common
ifneq ($(strip $(DO_BUILD_OPENSSL)),)
openssl: sanity_check
	(tar xkfj openssl.tar.bz2 2> /dev/null || true)
	$(MAKE) -C openssl all install
else
openssl: sanity_check
	@echo "skipping openssl (not configured)"
endif


