#include <kern/pci.h>
#include <kern/e1000_hw.h>


#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

// The size of each RD buffer (in bytes)
// For convenience, it must be a factor of PGSIZE
#define E1000_RD_BUFFER_SIZE 2048
#define E1000_RCTL_BSIZE_2048 (00 << 16)

// Size of each RD (receive descriptor)
#define E1000_RD_SIZE sizeof(struct e1000_rx_desc)

// Number of RD in RDR (receive descriptor ring)
// Must be a multiple of 8
#define E1000_RD_NUM 256

// The size of RDR in byte
// Must be a multiple of 128
#define E1000_RDR_SIZE (E1000_RD_SIZE*E1000_RD_NUM)

// Return the index of a given TD
#define TD_INDEX(td) ((struct e1000_tx_desc*)td-tdrs)

// Boundary length of transmit data
#define E1000_TRANSMIT_DATA_LEN_MIN 0
#define E1000_TRANSMIT_DATA_LEN_MAX 16288

// Define some error codes
#define ERROR_NO_FREE_TD 1
#define ERROR_INVALID_LENGTH 2
#define ERROR_RDR_EMPTY 3

// Some operation macro on e1000 registers
#define e1000_tctl_ct(val) (val << 4)
#define e1000_tctl_cold(val) (val << 12)

#define e1000_tipg_lpgt(val) (val)
#define e1000_tipg_lpgr1(val) (val << 10)
#define e1000_tipg_lpgr2(val) (val << 20)

// Size of each TD (transmit descriptor) in byte
#define E1000_TD_SIZE sizeof(struct e1000_tx_desc)

// The number of TDs in TDR
// Must be a multiple of 8
#define E1000_TD_NUM 32

// The size of TDR in byte
// Must be 128-byte aligned
#define E1000_TDR_SIZE (E1000_TD_SIZE*E1000_TD_NUM)


// Structure to describe an e1000 device
struct e1000_device
{   
    // The PCI func of the device
    struct pci_func f;

    // The base address (linear address) of TDR (Transmit Descriptior Ring) 
    // Must be 16-byte aligned
    // Since our page size is 4096, so the start address of a page is always 16-byte aligned
    volatile void* tdr_base;

    // The base address (linear address) of RDR (Receive Descriptor Ring)
    volatile void* rdr_base;
};

// Assume that JOS only support one e1000 device, so we use a global variable to store it
extern struct e1000_device current_device;

void e1000_reg_writel(int offset,uint32_t val);
uint32_t e1000_reg_readl(int offset);

int e1000_try_receive_data(void* buf);
int e1000_transmit_data(physaddr_t addr,int len);

void e1000_receive_init();
void e1000_transmit_init();

void e1000_init(struct pci_func* f);

#endif  // SOL >= 6
