#include "ns.h"

extern union Nsipc nsipcbuf;

int i=0;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver

	union Nsipc* nsipc=(union Nsipc*)(REQVA);
	while(true){
		int val=ipc_recv(NULL,(void*)REQVA,NULL);

		if(val != NSREQ_OUTPUT){
			continue;
		}

		// What to do when TDR is full?
		// Here is a temporaty solution: wait until there is an available TD
		int r;
		while((r=sys_transmit_packet((void*)nsipc->pkt.jp_data,nsipc->pkt.jp_len)) == -E_DEVICE_BUSY){
			sys_yield();
		}

		if(r < 0)
			panic("Notwork transmit error %e for %d\n",r,i);
		
		i++;
	}
	
}
