# Copyright 2019 SiFive, Inc #
# SPDX-License-Identifier: Apache-2.0 #

PROGRAM ?= tlb_gigapage_rw

$(PROGRAM): $(wildcard *.c) $(wildcard *.h) $(wildcard *.S)

override CFLAGS += 	-Xlinker --defsym=__stack_size=0x38000 -Xlinker --defsym=__heap_size=0x38000

clean:
	rm -f $(PROGRAM) $(PROGRAM).hex
