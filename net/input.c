#include "ns.h"

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.

	int r;

	while(true){
		// Allocate a page to store data
		union Nsipc* nsipc=(union Nsipc*)REQVA;
		if((r=sys_page_alloc(sys_getenvid(),(void*)REQVA,PTE_P | PTE_U | PTE_W)) < 0){
			panic("Failed to allocate page for recv %e\n",r);
		}

		// Try to receive a page
		while((r=sys_try_receive_packet(nsipc->pkt.jp_data)) == 0){
			// Yield to be CPU-friendly
			sys_yield();
		}

		nsipc->pkt.jp_len=r;

		// Send IPC message
		ipc_send(ns_envid,NSREQ_INPUT,nsipc,PTE_P | PTE_U | PTE_W);

		// Unmap tmp page
		sys_page_unmap(sys_getenvid(),(void*)REQVA);
	}
	
}
