# Makefile - makefile for the gbn protocol
#
# gbn - implementation of the go-back-n protocol
# by Rafael P. Laufer <rlaufer@cs.ucla.edu>
# Copyright (C) 2006 Rafael P. Laufer
# Modified 2007 Jason Ryder
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

CC              = gcc
LD              = gcc
AR              = ar

CFLAGS          = -Wall -ansi -D_GNU_SOURCE 
LFLAGS          = -Wall -ansi

.c.o:
	$(CC) $(CFLAGS) -c $@

process: p2p.c process.c

clean:
	rm -f *.o process
