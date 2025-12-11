/* VirtIO Network Driver */

#include "../../include/types.h"
#include "../../include/console.h"

#define VIRTIO_NET_F_CSUM       (1 << 0)
#define VIRTIO_NET_F_GUEST_CSUM (1 << 1)
#define VIRTIO_NET_F_MAC        (1 << 5)
#define VIRTIO_NET_F_GSO        (1 << 6)
#define VIRTIO_NET_F_GUEST_TSO4 (1 << 7)
#define VIRTIO_NET_F_GUEST_TSO6 (1 << 8)
#define VIRTIO_NET_F_GUEST_ECN  (1 << 9)
#define VIRTIO_NET_F_GUEST_UFO  (1 << 10)
#define VIRTIO_NET_F_HOST_TSO4  (1 << 11)
#define VIRTIO_NET_F_HOST_TSO6  (1 << 12)
#define VIRTIO_NET_F_HOST_ECN   (1 << 13)
#define VIRTIO_NET_F_HOST_UFO   (1 << 14)
#define VIRTIO_NET_F_MRG_RXBUF  (1 << 15)
#define VIRTIO_NET_F_STATUS     (1 << 16)
#define VIRTIO_NET_F_CTRL_VQ    (1 << 17)
#define VIRTIO_NET_F_CTRL_RX    (1 << 18)
#define VIRTIO_NET_F_CTRL_VLAN  (1 << 19)
#define VIRTIO_NET_F_GUEST_ANNOUNCE (1 << 21)

#define VIRTIO_NET_S_LINK_UP    1
#define VIRTIO_NET_S_ANNOUNCE   2

#define MAX_PACKET_SIZE 1514
#define RX_RING_SIZE    128
#define TX_RING_SIZE    128

typedef struct virtio_net_hdr {
    u8 flags;
    u8 gso_type;
    u16 hdr_len;
    u16 gso_size;
    u16 csum_start;
    u16 csum_offset;
} __attribute__((packed)) virtio_net_hdr_t;

typedef struct virtio_net_packet {
    virtio_net_hdr_t hdr;
    u8 data[MAX_PACKET_SIZE];
} virtio_net_packet_t;

typedef struct virtio_net_device {
    u64 base_addr;
    u8 mac_addr[6];
    u32 status;
    bool link_up;
    void* pci_dev;
    
    virtio_net_packet_t rx_ring[RX_RING_SIZE];
    virtio_net_packet_t tx_ring[TX_RING_SIZE];
    u32 rx_head;
    u32 rx_tail;
    u32 tx_head;
    u32 tx_tail;
} virtio_net_device_t;

static virtio_net_device_t* virtio_net_dev = NULL;

static i32 virtio_net_read(void* device, u64 offset, u64 size, void* buffer) {
    virtio_net_device_t* dev = (virtio_net_device_t*)device;
    
    if (!dev || !buffer) {
        return ERR_INVALID;
    }
    
    if (!dev->link_up) {
        return ERR_AGAIN;
    }
    
    if (dev->rx_head == dev->rx_tail) {
        return 0;
    }
    
    u32 idx = dev->rx_head % RX_RING_SIZE;
    virtio_net_packet_t* pkt = &dev->rx_ring[idx];
    
    u64 copy_size = size < MAX_PACKET_SIZE ? size : MAX_PACKET_SIZE;
    for (u64 i = 0; i < copy_size; i++) {
        ((u8*)buffer)[i] = pkt->data[i];
    }
    
    dev->rx_head++;
    
    return (i32)copy_size;
}

static i32 virtio_net_write(void* device, u64 offset, u64 size, const void* buffer) {
    virtio_net_device_t* dev = (virtio_net_device_t*)device;
    
    if (!dev || !buffer) {
        return ERR_INVALID;
    }
    
    if (!dev->link_up) {
        return ERR_AGAIN;
    }
    
    if (size > MAX_PACKET_SIZE) {
        return ERR_INVALID;
    }
    
    u32 idx = dev->tx_tail % TX_RING_SIZE;
    if ((dev->tx_tail - dev->tx_head) >= TX_RING_SIZE) {
        return ERR_BUSY;
    }
    
    virtio_net_packet_t* pkt = &dev->tx_ring[idx];
    pkt->hdr.flags = 0;
    pkt->hdr.gso_type = 0;
    pkt->hdr.hdr_len = 0;
    pkt->hdr.gso_size = 0;
    pkt->hdr.csum_start = 0;
    pkt->hdr.csum_offset = 0;
    
    for (u64 i = 0; i < size; i++) {
        pkt->data[i] = ((const u8*)buffer)[i];
    }
    
    dev->tx_tail++;
    
    return (i32)size;
}

static i32 virtio_net_ioctl(void* device, u32 cmd, void* arg) {
    virtio_net_device_t* dev = (virtio_net_device_t*)device;
    
    if (!dev) {
        return ERR_INVALID;
    }
    
    switch (cmd) {
        case 0x01:
            if (arg) {
                for (u32 i = 0; i < 6; i++) {
                    ((u8*)arg)[i] = dev->mac_addr[i];
                }
                return ERR_SUCCESS;
            }
            return ERR_INVALID;
            
        case 0x02:
            return dev->link_up ? 1 : 0;
            
        default:
            return ERR_INVALID;
    }
}

static device_ops_t virtio_net_ops = {
    .read = virtio_net_read,
    .write = virtio_net_write,
    .ioctl = virtio_net_ioctl,
    .read_block = NULL,
    .write_block = NULL,
    .ioctl_block = NULL,
    .open = NULL,
    .close = NULL,
    .probe = NULL,
    .remove = NULL
};

void virtio_net_init(void* pci_dev) {
    console_print("Initializing VirtIO network device...\n");
    
    virtio_net_dev = (virtio_net_device_t*)malloc(sizeof(virtio_net_device_t));
    if (!virtio_net_dev) {
        console_print("  Failed to allocate device structure\n");
        return;
    }
    
    virtio_net_dev->pci_dev = pci_dev;
    virtio_net_dev->base_addr = 0;
    virtio_net_dev->status = VIRTIO_NET_S_LINK_UP;
    virtio_net_dev->link_up = true;
    
    virtio_net_dev->mac_addr[0] = 0x52;
    virtio_net_dev->mac_addr[1] = 0x54;
    virtio_net_dev->mac_addr[2] = 0x00;
    virtio_net_dev->mac_addr[3] = 0x12;
    virtio_net_dev->mac_addr[4] = 0x34;
    virtio_net_dev->mac_addr[5] = 0x56;
    
    virtio_net_dev->rx_head = 0;
    virtio_net_dev->rx_tail = 0;
    virtio_net_dev->tx_head = 0;
    virtio_net_dev->tx_tail = 0;
    
    i32 major = device_register("virtio-net", DEVICE_NETWORK, &virtio_net_ops, virtio_net_dev);
    if (major < 0) {
        console_print("  Failed to register network device\n");
        free(virtio_net_dev);
        virtio_net_dev = NULL;
        return;
    }
    
    console_print("  VirtIO network device registered (major=");
    console_print_dec(major);
    console_print(", MAC=");
    for (u32 i = 0; i < 6; i++) {
        console_print_hex(virtio_net_dev->mac_addr[i]);
        if (i < 5) console_print(":");
    }
    console_print(")\n");
    
    vfs_create_device_node("/dev/eth0", S_IFCHR | 0660, major, 0);
}
