#ifndef MBUF_H
#define MBUF_H
/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <odp.h>

#define MAX_NAME_LEN (100)
#define RTE_PKTMBUF_HEADROOM  (0)
#define SOCKET_ID_ANY 0
#define PKT_TX_VLAN_PKT      (1ULL << 57) /**< TX packet is a 802.1q VLAN packet. */

struct rte_mempool {
    uint32_t magic;
    uint32_t socket_id;
    uint32_t size;
    odp_atomic_u32_t count; //no odp_pool_count like function
    char name[MAX_NAME_LEN];
    odp_pool_param_t odp_buffer_pool_param;
    odp_pool_t odp_buffer_pool;
};
typedef rte_mempool rte_mempool_t;

struct rte_mbuf {
    uint32_t magic;
    rte_mempool_t *pool; /**< Pool from which mbuf was allocated. */
    void *buf_addr;           /**< Virtual address of segment buffer. */
    uint16_t buf_len;         /**< Length of segment buffer. */
    uint16_t data_off; /**data area's offset to the buf_addr, headroom is in between*/

    struct rte_mbuf *next;  /**< Next segment of scattered packet. */
    uint16_t data_len;      /**< Amount of data in segment buffer, not include headroom */

    /* these fields are valid for first segment only */
    uint8_t nb_segs;        /**< Number of segments. */
    uint8_t in_port;        /**< Input port. */
    uint32_t pkt_len;       /**< Total pkt len: sum of all segment data_len. */

    odp_atomic_u32_t refcnt;

    uint64_t ol_flags;
    union {
        uint64_t tx_offload;       /**< combined for easy fetch */
        struct {
            uint64_t l2_len:7; /**< L2 (MAC) Header Length. */
            uint64_t l3_len:9; /**< L3 (IP) Header Length. */
            uint64_t l4_len:8; /**< L4 (TCP/UDP) Header Length. */
            uint64_t tso_segsz:16; /**< TCP TSO segment size */

            /* fields for TX offloading of tunnels */
            uint64_t outer_l3_len:9; /**< Outer L3 (IP) Hdr Length. */
            uint64_t outer_l2_len:7; /**< Outer L2 (MAC) Hdr Length. */

            /* uint64_t unused:8; */
        };
    };
    uint16_t vlan_tci;
};
typedef struct rte_mbuf rte_mbuf_t;

void utl_rte_mempool_delete(rte_mempool_t * &pool);

rte_mempool_t * utl_rte_mempool_create(const char  *name,
                                      unsigned n, 
                                      unsigned elt_size,
                                      unsigned cache_size,
                                      uint32_t _id,
                                      uint32_t socket_id  );

rte_mempool_t * utl_rte_mempool_create_non_pkt(const char  *name,
                                               unsigned n, 
                                               unsigned elt_size,
                                               unsigned cache_size,
                                               uint32_t _id ,
                                               int socket_id);

unsigned int rte_mempool_count(rte_mempool_t *mp);


rte_mbuf_t* utl_rte_pktmbuf_add_after(rte_mbuf_t *m1, rte_mbuf_t *m2);

rte_mbuf_t* utl_rte_pktmbuf_add_after2(rte_mbuf_t *m1, rte_mbuf_t *m2);

void utl_rte_pktmbuf_add_last(rte_mbuf_t *m,rte_mbuf_t *m_last);
    
void rte_pktmbuf_free(rte_mbuf_t *m);

rte_mbuf_t *rte_pktmbuf_alloc(rte_mempool_t *mp);

char *rte_pktmbuf_append(rte_mbuf_t *m, uint16_t len);

char *rte_pktmbuf_adj(struct rte_mbuf *m, uint16_t len);

int rte_pktmbuf_trim(rte_mbuf_t *m, uint16_t len);

void rte_pktmbuf_attach(struct rte_mbuf *mi, struct rte_mbuf *md);

void rte_pktmbuf_free_seg(rte_mbuf_t *m);

uint16_t rte_mbuf_refcnt_update(rte_mbuf_t *m, int16_t value);

void rte_pktmbuf_dump(const struct rte_mbuf *m, unsigned dump_len);

int rte_mempool_sc_get(rte_mempool_t *mp, void **obj_p);

void rte_mempool_sp_put(rte_mempool_t *mp, void *obj);

int rte_mempool_get(rte_mempool_t *mp, void **obj_p);

void rte_mempool_put(rte_mempool_t *mp, void *obj);

void * rte_memcpy(void *dst, const void *src, size_t n);

void rte_exit(int exit_code, const char *format, ...);

unsigned rte_lcore_to_socket_id(unsigned lcore_id);

#define rte_pktmbuf_mtod(m, t) ((t)((char *)(m)->buf_addr + (m)->data_off))

#define rte_pktmbuf_pkt_len(m) ((m)->pkt_len)

#define rte_pktmbuf_data_len(m) ((m)->data_len)

uint64_t rte_rand(void);


/* return 0 for success -1 for failure
 * store odp_packet_t in *odp_packet_p
 */
int mbuf_to_odp_packet(rte_mbuf_t* mbuf, odp_packet_t* odp_packet_p);
int mbuf_to_odp_packet_tbl(rte_mbuf**pkts, odp_packet_t* odp_pkts, uint16_t pkt_nm);
static inline void rte_pktmbuf_refcnt_update(struct rte_mbuf *m, int16_t v) {;}
#endif
