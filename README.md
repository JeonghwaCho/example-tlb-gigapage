# example-tlb-gigapage

Demonstrates how to setup Giga pages with U series core's TLB MMU.
<br />
<br />
  

## Assumption

* Code is compiled and linked to PA=0x0000.0000-0x3fff.ffff address space
* Data is compiled and linked to PA=0x8000.0000-0xbfff.ffff address space
* only one core will run the test (if it's multicore system)
<br />
<br />

## Overview

Here is high level flow diagram illustrate how to setup gigapge TLB entry.
<br />
<br />
<img src="https://github.com/sifive/example-tlb-gigapage/blob/master/TLB-Gigapage.svg" width="800" />
<br />
<br />


## Description

This is a simple test that setup 3 TLB pages on a single CPU (hart). Such CPU\
will enter user mode and perform simple read/write tests.
<br />

For a multicore system, only one CPU will run the test and the rest of the cores\
will execute WFI instruction and stay in the secondary_main().
<br />

For example, a U74MC system contains 5 harts (CPUs), 1xS76 and 4xU74. Hart 1 \
will be used to run the test. To setup hart 1 to run the test, __metal_boot_hart\
in the linker script need to set to 1.
<br />
<br />

| hart id | Core Type | Description |
| :---    | :---      | :---        |
| 0 | S76 | No S mode supported.  This CPU ill execute wfi stay in secondary_main() |
| 1 | U74 | S mode with MMU supported. This CPU sets up translation and run the test |
| 2 | U74 | S mode with MMU supported. This CPU will execute wfi stay in secondary_main() |
| 3 | U74 | S mode with MMU supported. This CPU will execute wfi stay in secondary_main() |
| 4 | U74 | S mode with MMU supported. This CPU will execute wfi stay in secondary_main() |
<br />

3x gigapages will be setup and the memory map is illustrated as below.\
<br />
<br />

| Gigapage | Virtual address range | Physical address range | Permission |
| :---     | :---                  | :---                   | :---       |
| 1 | 0x0000.0000 - 0x3fff.ffff | 0x0000.0000 - 0x3fff.ffff | DA UXWRV |
| 2 | 0x4000.0000 - 0x7fff.ffff | 0x8000.0000 - 0xbfff.ffff | DA U WRV |
| 3 | 0x8000.0000 - 0xbfff.ffff | 0x8000.0000 - 0xbfff.ffff | DA U WRV |
<br />
<br />

The 1st gigapage, VA=0x0000.0000 - 0x3fff.ffff, is used to map to the code located \
@ PA=0x2xxx.xxxx. The 2nd and the 3rd gigapage, VA=0x4000.0000 - 0x7fff.ffff  \
and VA=0x8000.0000 - 0xbfff.ffff , are used to map to PA=0x8000.0000 - 0xbfff.ffff. \
This is where the data is located. Both 2nd and 3rd gigapages are map to the same PA.\
This imply that read/write to these two pages will be translated actions to the same \
physical address.
<br />
<br />
<img src="https://github.com/sifive/example-tlb-gigapage/blob/master/TLB-Gigapage-Overview.svg" width="720" />
<br />
<br />

A test buffer, named buf, an array of 10 integers, is created and located at PA=0x8xxx.xxxx \
. It has initial value of buf[0]=0xdeafbeef. If read it from VA=0x4xxx.xxxx or VA=0x8xxx.xxxx \
will return 0xdeadbeef. 
<br />
<br />
<img src="https://github.com/sifive/example-tlb-gigapage/blob/master/TLB-Read-test.svg" width="720" />
<br />
<br />

If write value 0xbeefdeaf to VA=0x4xxx.xxxx, read it from 0x8xxxx.xxxx will be the \
same.
<br />
<br />
<img src="https://github.com/sifive/example-tlb-gigapage/blob/master/TLB-Write-test.svg" width="720" />
<br />
<br />



## Exit code and meaning

| Exit Code | Meaning |
| :---      | :---    |
| 0         | read/write test passed and program exit normally |
| 1         | Fail to enable TLB or set PPN base address |
| 2         | Read test failed |
| 3         | Read test pass, Write test failed |



