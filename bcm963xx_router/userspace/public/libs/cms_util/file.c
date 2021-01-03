/***********************************************************************
 *
 *  Copyright (c) 2007  Broadcom Corporation
 *  All Rights Reserved
 *
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
 * 
 ************************************************************************/

#include "cms.h"
#include "cms_util.h"
#include "oal.h"

UBOOL8 cmsFil_isFilePresent(const char *filename)
{
   return (oalFil_isFilePresent(filename));
}


SINT32 cmsFil_getSize(const char *filename)
{
   return (oalFil_getSize(filename));
}


CmsRet cmsFil_copyToBuffer(const char *filename, UINT8 *buf, UINT32 *bufSize)
{
   return (oalFil_copyToBuffer(filename, buf, bufSize));
}



