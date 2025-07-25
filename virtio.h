#pragma once

uint32_t virtio_reg_read32(unsigned offset);
uint64_t virtio_reg_read64(unsigned offset);
void virtio_reg_write32(unsigned offset, uint32_t value);
void virtio_reg_fetch_and_or32(unsigned offset, uint32_t value);
extern struct virtio_virtq *blk_request_vq;
extern struct virtio_blk_req *blk_req;
extern paddr_t blk_req_paddr;
extern unsigned blk_capacity;
void virtio_blk_init(void);
struct virtio_virtq *virtq_init(unsigned index);
void virtq_kick(struct virtio_virtq *vq, int desc_index);
bool virtq_is_busy(struct virtio_virtq *vq);
void read_write_disk(void *buf, unsigned sector, int is_write);