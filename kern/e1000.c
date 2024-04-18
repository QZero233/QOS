#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/string.h>

// LAB 6: Your driver code here
struct e1000_device current_device;

// An array of all TDR
struct e1000_tx_desc* tdrs;

// An array of all RDR
struct e1000_rx_desc* rdrs;

// Write 4-byte data to certain reg of e1000
// The offset specified what reg to write
void inline e1000_reg_writel(int offset,uint32_t val){
    void* va=current_device.f.mapped_memory[0]+offset;
    volatile uint32_t* va32=(uint32_t*)va;

    *va32=val;
}


// Read 4-byte data from a certain reg of e1000
// The offset specified what reg to read
uint32_t inline e1000_reg_readl(int offset){
    void* va=current_device.f.mapped_memory[0]+offset;
    volatile uint32_t* va32=(uint32_t*)va;

    return *va32;   
}


// Read the value of RDT
uint32_t inline read_rdt(){
    return e1000_reg_readl(E1000_RDT);
}


// Try to receive a packer from e1000 device (It may fail if there is no packet to receive)
// If there is a packet to receive, then the data will be copied to the address given by parameter buf
// MUST make sure the length of buf at least equal to E1000_RD_BUFFER_SIZE
// Return the length of data, if succeeded
// Return -ERROR_RDR_EMPTY if there is no packet to receive
int e1000_try_receive_data(void* buf){
    // It will check the DD status of the next element of RDT, if it's set, then there is packet to receive
    // When finish copy, it will clear DD status and move RDT forward

    uint32_t next_rdt=read_rdt(); 

    // If it's the first time to receive, we just regard next as 0
    if(next_rdt == E1000_RD_NUM){
        next_rdt = 0;
    }else{
        next_rdt++;
        next_rdt %= E1000_RD_NUM;
    }

    struct e1000_rx_desc* rd=&rdrs[next_rdt];
    if(!(rd->status & E1000_RXD_STAT_DD)){
        // No packet to receive
        return -ERROR_RDR_EMPTY;
    }

    // Have packet to receive, begin to copy
    memcpy(buf,KADDR(rd->buffer_addr),rd->length);

    // Clear DD flag
    rd->status &= ~E1000_RXD_STAT_DD;

    // Move forward RDT
    e1000_reg_writel(E1000_RDT,next_rdt);

    return rd->length;
}

// Initialize the receive function of e1000 device
// Panic when error
void e1000_receive_init(){
    // Set MAC address
    // We use hard-coded MAC address 52:54:00:12:34:56
    // NOTE: Be very careful with the byte order; MAC addresses are written from lowest-order byte to highest-order byte

    // E1000_RA is the offset of RAL, so the offset of RAH = RA + 4
    e1000_reg_writel(E1000_RA,0x12005452);
    e1000_reg_writel(E1000_RA+4,0x5634+E1000_RAH_AV);

    // Set MTA to 0b as the manual says
    e1000_reg_writel(E1000_MTA,0);

    // Disable all interrupts by programming IMS
    e1000_reg_writel(E1000_IMS,0);

    // Serup RDR by doing the following steps:
    // 1.Allocate a page to store RDR (must make sure that E1000_RDR_SIZE <= PGSIZE)
    // 2.Set RDBAL and RDLEN register
    // 3.Initialize each RD (allocate buffer for each RD)
    // 4.Set RDH and RDT register

    // Step 1
    assert(E1000_RDR_SIZE <= PGSIZE);
    struct PageInfo* pp=page_alloc(ALLOC_ZERO);
    if(pp == NULL){
        panic("No free space for RDR");
    }

    // Step 2
    // Store the RDR address to global variable for later use
    rdrs=(struct e1000_rx_desc*)page2kva(pp);

    e1000_reg_writel(E1000_RDBAL,page2pa(pp));

    assert(E1000_RDR_SIZE % 128 == 0);
    e1000_reg_writel(E1000_RDLEN,E1000_RDR_SIZE);

    // Step 3
    // Make sure buffer size a factor of PGSIZE
    assert(PGSIZE % E1000_RD_BUFFER_SIZE == 0);

    int page_per_rd = PGSIZE / E1000_RD_BUFFER_SIZE;
    int pages=ROUNDUP(E1000_RD_NUM,page_per_rd);

    int current_rd_index=0;
    for(int i=0;i<pages;i++){
        pp = page_alloc(ALLOC_ZERO);
        if(pp == NULL){
            panic("No free page for RD buffer");
        }

        // Assign address for each rd
        physaddr_t base_pa=page2pa(pp);
        for(int j=0;j<page_per_rd;j++){
            rdrs[current_rd_index].buffer_addr = (uint64_t)(base_pa + j * E1000_RD_BUFFER_SIZE);
            // Also, clear the DD status flag
            rdrs[current_rd_index].status = 0;

            current_rd_index++;
        }
    }

    // Step 4.
    e1000_reg_writel(E1000_RDH,0);
    e1000_reg_writel(E1000_RDT,E1000_RD_NUM);


    // Program the RCTL register
    uint32_t rctl_val=E1000_RCTL_EN + E1000_RCTL_BSIZE_2048 + E1000_RCTL_SECRC;
    e1000_reg_writel(E1000_RCTL,rctl_val);
}

// Read the TDT register of e1000 device
// It will return the index of the tail of TDR
// (If the queue is not full, then the tdr[TDT] is always available)
uint32_t inline read_tdt(){
    return e1000_reg_readl(E1000_TDT);
}


// Check if the given TD is available
// Return true, if it's available
bool check_td_available(struct e1000_tx_desc* td){
    // If it's all 0-filled, then it's available
    uint64_t tdr=*((volatile uint64_t*)td);

    if(tdr == 0){
        return true;
    }

    // If DD bit is set, then it's available
    if(td->upper.fields.status & E1000_TXD_STAT_DD){
        return true;
    }

    return false;
}


// Move the TDT forward
// Note: If the queue is not full, then the tdr[TDT] is always available
// So if the next TD is not available, then TDT will not move forward
// Return 0, if move succeeded
// Return -ERROR_NO_FREE_TD, if the queue is full
int forward_tdt(){
    uint32_t current_tdt=read_tdt();
    current_tdt += 1;
    current_tdt %= E1000_TD_NUM;

    // Check if the next TD is available
    if(!check_td_available(&tdrs[current_tdt])){
        return -ERROR_NO_FREE_TD;
    }

    e1000_reg_writel(E1000_TDT,current_tdt);

    return 0;
}


// Try to allocate a TD
// If there is no available TD, it will return NULL
struct e1000_tx_desc* td_alloc(){
    uint32_t current_tdt=read_tdt();

    // Check if current tdt is available
    // If it's not available, then the queue must be full
    if(!check_td_available(&tdrs[current_tdt])){
        return NULL;
    }

    return &tdrs[current_tdt];
}


// Transmit data with legal length
// Length must be in the range of [E1000_TRANSMIT_DATA_LEN_MIN,E1000_TRANSMIT_DATA_LEN_MAX]
// WARNING: the address must be converted to physical address
// It should be done by walking through page table, instad of simply using PADDR marco
// Because the address may come from user space
// Return the TR index (index >= 0), if succeeded
// Return -ERROR_INVALID_LENGTH, if the length is illegal
// Return -ERROR_NO_FREE_TD, if there is no available TD
int e1000_transmit_data(physaddr_t addr,int len){
    if(len < E1000_TRANSMIT_DATA_LEN_MIN || len > E1000_TRANSMIT_DATA_LEN_MAX){
        return -ERROR_INVALID_LENGTH;
    }

    // Allocate a TD
    struct e1000_tx_desc* td=td_alloc();
    if(td == NULL){
        // No free TD
        return -ERROR_NO_FREE_TD;
    }

    // Set the values of TD

    td->buffer_addr=(uint64_t)addr;

    td->lower.flags.length=len;


    td->lower.flags.cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;

    // Move forward the TDT
    forward_tdt();

    return TD_INDEX(td);
}

// Perform the transmit initialization steps
void e1000_transmit_init(){
    //Allocate a page for transmit descriptor ring
    struct PageInfo* pp=page_alloc(ALLOC_ZERO);
    if(pp == NULL){
        panic("Can not alloc a free page for e1000 transmit descriptor ring");
    }

    // Save the address for later usage
    tdrs=(struct e1000_tx_desc*)page2kva(pp);

    // Initialize TDR
    current_device.tdr_base=tdrs;
    e1000_reg_writel(E1000_TDBAL,PADDR(tdrs));
    assert(E1000_TDR_SIZE % 128 == 0);
    e1000_reg_writel(E1000_TDLEN,E1000_TDR_SIZE);
    e1000_reg_writel(E1000_TDH,0);
    e1000_reg_writel(E1000_TDT,0);

    // Set control register TCTL
    uint32_t tctl_val=E1000_TCTL_EN | E1000_TCTL_PSP;
    tctl_val += e1000_tctl_ct(0x10);
    tctl_val += e1000_tctl_cold(0x40);
    e1000_reg_writel(E1000_TCTL,tctl_val);

    // Set TIPG register
    uint32_t tipg_val=e1000_tipg_lpgt(10)+e1000_tipg_lpgr1(4)+e1000_tipg_lpgr2(6);
    e1000_reg_writel(E1000_TIPG,tipg_val);    
}


// Initialize e1000 global variable
void e1000_init(struct pci_func* f){
    memset((void*)&current_device,0,sizeof(struct e1000_device));

    // NOTE: must use memcpy, can not just save the pointer
    // Because the f pointer passed to this function is pointed at a local variable
    memcpy(&current_device.f,f,sizeof(struct e1000_device));
}