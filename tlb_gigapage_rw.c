/* Copyright 2019 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#include <metal/csr.h>
// older Freedom-e SDK doesn't have above header, use the following define instead

/*!
 * @brief Read a given CSR register without checking validity of CSR offset
 * @param crs Register label or hex value offset to read from
 * @param value Variable name of uintprt_t type to get the value
 */
#define METAL_CPU_GET_CSR(reg, value)                                          \
    __asm__ volatile("csrr %0, " #reg : "=r"(value));

/*!
 * @brief Write to a given CSR register without checking validity of CSR offset
 * @param crs Register label or hex value offset to write to
 * @param value Variable name of uintprt_t type to set the value
 */
#define METAL_CPU_SET_CSR(reg, value)                                          \
    __asm__ volatile("csrw " #reg ", %0" : : "r"(value));


/*!
 * @brief Flush both L1 & L2 TLB
 */
#define METAL_CPU_TLB_FLUSH_ALL()                                          \
    __asm__ __volatile__ ("sfence.vma" : : : "memory");


#define METAL_MSTATUS_MPRV   0x00020000UL
#define METAL_MSTATUS_SUM    0x00040000UL
#define METAL_MSTATUS_MPP    0x00001800UL
#define METAL_TLB_PAGESIZE_SHIFT	12
#define METAL_WORDADDR_SHIFT			2


 __attribute__ ((noinline)) __attribute__ ((aligned (4))) void isr_handle() {
	// read mepc, mcause, mstatus, mtval and exit the program
	__asm__ __volatile__ ("csrr t0, mepc; csrr t0, mcause; csrr t0, mstatus; csrr t0, mtval;");
	exit(10);
}


void tlb_gigapage_readwrite_test() {
	unsigned long pte[512] __attribute__ ((aligned (4096)));
	unsigned long buf[10];
	unsigned long val, val1;
	unsigned long *p;

	// initialize PTE entry
	memset(pte,0,sizeof(unsigned long)*512);

	// set exception handler to isr_handle()
	METAL_CPU_SET_CSR(mtvec,isr_handle );
	//__asm__ __volatile__ ("mv t0, %0; csrw mtvec,t0;" : : "r" (isr_handle) : );


	// prepare test data, assume located at PA=0x8xxx.xxxx
	// write buf[0] from 0x8xxx.xxxx address, then later read from 0x4xxx.xxxx
	// if TLB setup correctly, both result should be the same
	buf[0] = 0xdeafbeef;

	// configure PMP for S/U mode access
	// pmpaddr0 is word address, should not excess max available range, or else it will wrap around
	// setup 0- 2.5GB, TOR, no lock, RXW set, accessible from PMP-0, PMP permission will not apply to M mode
	val = 0xA0000000 >> METAL_WORDADDR_SHIFT;
	METAL_CPU_SET_CSR(pmpaddr0,val);
	METAL_CPU_SET_CSR(pmpcfg0,0xf);


	// define 3 gigapages using word address
	// access right: 0xDF , set DA UXWRV bits
	//             : 0xD7 , set DA U WRV bits
	//             : 0xDB , set DA UX RV bits
	

	// this gigapage allow same code located in @ 0x2xxx.xxxx be execuated in U mode
	// VA=0x0000.0000 - 0x3fff.ffff -> PA=0x0000.0000 - 0x3fff.ffff
	pte[0]= (0x80000000 >>METAL_WORDADDR_SHIFT) | 0xDF;
	
	
	// the following two gigapages will map to same PA
	// read from/write to these two VAs will result of read from/write to the same PA
	
	// VA=0x4000.0000 - 0x7fff.ffff -> PA=0x8000.0000 - 0xbfff.ffff
	// Change the property, 0xD7 -> 0xDF because 0x40400000 is the code execution region.
	// pte[1]= (0x80000000 >>METAL_WORDADDR_SHIFT) | 0xD7;
	pte[1]= (0x40000000 >>METAL_WORDADDR_SHIFT) | 0xDF;
	// VA=0x8000.0000 - 0xbfff.ffff -> PA=0x8000.0000 - 0xbfff.ffff
	pte[2]= (0x80000000 >>METAL_WORDADDR_SHIFT) | 0xD7;


	// set SATP.PPN
	p = (unsigned long *)pte;
	val = (unsigned long) p >> METAL_TLB_PAGESIZE_SHIFT;
	// set mode to Sv39, ASID=0. ASID other than zero is currently not supported
	val = 0x8000000000000000 | val;
	METAL_CPU_SET_CSR(satp,val );
	METAL_CPU_GET_CSR(satp,val1);
	if (val1 != val) {
		// exit with error code 1
		exit(1);
	}

	//flush L1 & Unified TLB
	METAL_CPU_TLB_FLUSH_ALL();


	// modify MPRV affect load/store privilege, instruction not affected
	// MPRV=0, load/store based on current mode
	// MPRV=1, load/store based on mode set in MPP
	// U5/U7/U84 doesn't support uepc, uret, to change U mode, need to set MPRV/MPP and mret
	// Set to a known state MPV=1, SUM=0, MPP=U mode
	METAL_CPU_GET_CSR(mstatus,val);
	// set MPRV=0
	val = ( val & (~ (METAL_MSTATUS_MPRV) ) );
	// set SUM=0
	val = ( val & (~ (METAL_MSTATUS_SUM) ) );
	// set MPP=U mode
	val = ( val & ( ~ METAL_MSTATUS_MPP ) ) ;
	METAL_CPU_SET_CSR(mstatus,val );

	
	// switch to U mode and continue execute code below
	__asm__ __volatile__ ("la t0, 1f; csrw mepc, t0; mret;  1:" );
	


	// The following executed code in U mode, if TLB did not setup correctly, 
	// Instruction page fault exception will be triggered. isr_handle() will be called
	// the first test is read test, to do this setup pointer p to access buf, which
	// located @ VA=0x8xxx.xxxx, from different VA (which is 0x4xxx.xxxx)
	val = (unsigned long) buf ;
	val = ( (val & 0x7fffffff) | 0x00000000);
	p = (unsigned long *) val;

	if (p[0]!=buf[0] ) {
		// if TLB setup correctly, should not be here
		// first read test failed, exit program with error code =2
		exit (2);
	} else {
		// pass the 1st read test, now continue do the write test
		// write to VA=0x4xxx.xxxx should reflect to read from VA=0x8xxx.xxxx
		p[0]=0xbeefdeaf;
		if (p[0]!=buf[0] ) {
			// if TLB setup correctly, should not be here
			// write test failed, exit program with error code =3
			exit(3);
		} else {
			// write to 0x4xxx.0000 match the read from 0x8xxx.xxxx
			// pass the write test exit the program normally
			return;
		}

	}

}




int main(void)
{
	tlb_gigapage_readwrite_test();
	return 0;
}



