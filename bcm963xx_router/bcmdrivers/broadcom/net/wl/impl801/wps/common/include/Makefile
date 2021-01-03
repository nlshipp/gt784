#
# include/Makefile
#
# Copyright 2005, Broadcom, Inc.
#
# $Id: Makefile,v 1.1 2010/08/05 21:59:02 ywu Exp $
#

SRCBASE	= ..

TARGETS	= wpsvers.h


all release:
	bash wpsvers.sh
	mv wpsvers.h wps_version.h

clean:
	rm -rf ${TARGETS} *.prev


.PHONY: all release clean
