#include "mbuf.h"

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

struct trex_odp_buffer_handle {
    odp_buffer_t odp_buffer;
};

#define SHM_PKT_POOL_SIZE      (512*2048)
#define SHM_PKT_POOL_BUF_SIZE  (1856)

#define ROUND_UP(var, align) (((var) + ((align) - 1)) & ~((align) - 1))
#define ROUND_UP_TREX_ODP_BUFFER_HANDLE(align) ROUND_UP(sizeof(struct trex_odp_buffer_handle), align)
#define RTE_MBUF_FROM_BADDR(ba)     (((rte_mbuf_t *)(ba)) - 1)
//alignment of rte_mempool_t
#define MEMPOOL_ALIGN (16)

/* MAGICA: allocated rte_mempool_t 
 * MAGICF: free rte_mempool_t
 */
#define MAGICA 0xBBCCDDAA
#define MAGICF 0xBBCCDDFF

#define MAX_SOCKETS_SUPPORTED (4) //should keep in line with defined in bp_sim.h
#define MAX_POOL_NUM (10)

static odp_pool_t mempool_pool;
static odp_pool_t packet_pool_arr[MAX_SOCKETS_SUPPORTED];


 __attribute__((constructor)) static void pal_constructor(void)
{
    odp_pool_param_t pool_param;
    uint32_t i;
    
    odp_pool_param_init(&pool_param);
    pool_param.buf.num = MAX_SOCKETS_SUPPORTED*MAX_POOL_NUM;
    pool_param.buf.size = ROUND_UP_TREX_ODP_BUFFER_HANDLE(MEMPOOL_ALIGN) + sizeof(rte_mempool_t);
    pool_param.buf.align = MEMPOOL_ALIGN;
    pool_param.type = ODP_POOL_BUFFER;
    mempool_pool = odp_pool_create("mempool_pool", &pool_param);
    if(mempool_pool == ODP_POOL_INVALID) {
	printf("%s failed to create buffer pool for mempool\n", __FUNCTION__);
	rte_exit(EXIT_FAILURE, "exit here %s: %d", __FUNCTION__, __LINE__);
    }

    for(i = 0; i < MAX_SOCKETS_SUPPORTED; i++) {
	char name[MAX_NAME_LEN];
	odp_pool_param_t pkt_pool_param;

	snprintf(name, MAX_NAME_LEN, "pkt-%d", i);
	odp_pool_param_init(&pkt_pool_param);
	pkt_pool_param.pkt.seg_len = SHM_PKT_POOL_BUF_SIZE;
	pkt_pool_param.pkt.len = SHM_PKT_POOL_BUF_SIZE;
	pkt_pool_param.pkt.num = SHM_PKT_POOL_SIZE / SHM_PKT_POOL_BUF_SIZE;
	pkt_pool_param.type = ODP_POOL_PACKET;

	packet_pool_arr[i] = odp_pool_create(name, &pkt_pool_param);
	if (packet_pool_arr[i] == ODP_POOL_INVALID) {
	    printf("%s failed to create odp packet pool\n", __FUNCTION__);
	    rte_exit(EXIT_FAILURE, "exit here %s: %d", __FUNCTION__, __LINE__);
	}
    }
}

rte_mempool_t * utl_rte_mempool_create_non_pkt(const char  *name,
                                               uint32_t n, 
                                               uint32_t elt_size,
                                               uint32_t cache_size,
                                               uint32_t _id ,
                                               int socket_id)
{
    odp_buffer_t odp_buffer;
    struct trex_odp_buffer_handle* head;
    rte_mempool_t* mempool = NULL;
    int ent_size = 0;

    if(odp_unlikely(socket_id >= MAX_SOCKETS_SUPPORTED)) {
	printf("%s invalid argument socket_id %d\n", __FUNCTION__, socket_id);
	rte_exit(EXIT_FAILURE, "exit here %s: %d", __FUNCTION__, __LINE__);
    }

    odp_buffer = odp_buffer_alloc(mempool_pool);
    if(odp_unlikely(odp_buffer == ODP_BUFFER_INVALID)) {
	printf("%s alloc failed\n", __FUNCTION__);
	rte_exit(EXIT_FAILURE, "exit here %s: %d", __FUNCTION__, __LINE__);
    }
    
    head = (struct trex_odp_buffer_handle*)odp_buffer_addr(odp_buffer);
    head->odp_buffer = odp_buffer;
    mempool = (rte_mempool_t*)((uint8_t*)head + ROUND_UP_TREX_ODP_BUFFER_HANDLE(MEMPOOL_ALIGN));
    mempool->magic = MAGICA;

    snprintf(mempool->name, MAX_NAME_LEN, "%s-%d", name, socket_id);
    mempool->socket_id = socket_id;
    odp_atomic_init_u32(&mempool->count, 0);

    /* buffer structure: |rounded struct trex_odp_buffer_handle|user area|
     * for packet buffer user area: |rte_mbuf|headroom|packet|tailroom|
     * for packet buffer caller has cover rte_mbuf and headroom in elt_size
     */
    ent_size = ROUND_UP_TREX_ODP_BUFFER_HANDLE(cache_size) + elt_size;
    
    odp_pool_param_init(&(mempool->odp_buffer_pool_param));
    mempool->odp_buffer_pool_param.buf.num = n;
    mempool->odp_buffer_pool_param.buf.size = ent_size;
    mempool->odp_buffer_pool_param.buf.align = cache_size;
    mempool->odp_buffer_pool_param.type = ODP_POOL_BUFFER;
    mempool->odp_buffer_pool = odp_pool_create(mempool->name, &(mempool->odp_buffer_pool_param));
    if (odp_unlikely(mempool->odp_buffer_pool == ODP_POOL_INVALID)) {
	printf("%s failed to create odp buffer pool\n", __FUNCTION__);
	rte_exit(EXIT_FAILURE, "exit here %s: %d", __FUNCTION__, __LINE__);
    }

    return mempool;
}

rte_mempool_t * utl_rte_mempool_create(const char  *name,
                                       uint32_t n, 
                                       uint32_t elt_size,
                                       uint32_t cache_size,
                                       uint32_t _id,
                                       uint32_t socket_id )
{
    rte_mempool_t* mempool = NULL;

    /*caller should have already covered rte_mbuf_t in elt_size*/
    mempool = utl_rte_mempool_create_non_pkt(name, n, elt_size, cache_size, _id, socket_id);
    if(odp_unlikely(mempool == NULL)) {
	printf("%s failed to create mempool\n", __FUNCTION__);
	rte_exit(EXIT_FAILURE, "exit here %s: %d", __FUNCTION__, __LINE__);
    }

    return mempool;
}

static inline void utl_mempool_check(rte_mempool_t* mp)
{
    if(odp_unlikely(mp->magic != MAGICA)) {
	printf("%s mempool check failed\n", __FUNCTION__);
	rte_exit(EXIT_FAILURE, "exit here %s: %d", __FUNCTION__, __LINE__);
    }
}

static inline void utl_buffer_check(struct trex_odp_buffer_handle* head)
{
    if(odp_unlikely(!odp_buffer_is_valid(head->odp_buffer))) {
	printf("%s odp buffer check failed\n", __FUNCTION__);
	rte_exit(EXIT_FAILURE, "exit here %s: %d", __FUNCTION__, __LINE__);
    }
}

static inline void utl_mbuf_check(const rte_mbuf_t* mbuf)
{
    if(odp_unlikely(mbuf->magic != MAGICA)) {
	printf("%s mbuf check failed\n", __FUNCTION__);
	rte_exit(EXIT_FAILURE, "exit here %s: %d", __FUNCTION__, __LINE__);
    }
}

void utl_rte_mempool_delete(rte_mempool_t* &pool)
{
    struct trex_odp_buffer_handle* head;

    utl_mempool_check(pool);
    odp_pool_destroy(pool->odp_buffer_pool);
    pool->magic = MAGICF;
    head = (struct trex_odp_buffer_handle*)((uint8_t*)pool - ROUND_UP_TREX_ODP_BUFFER_HANDLE(MEMPOOL_ALIGN));
    odp_buffer_free(head->odp_buffer);
    pool = NULL;
}

uint32_t rte_mempool_count(rte_mempool_t* mp)
{
    utl_mempool_check(mp);    
    return odp_atomic_load_u32(&mp->count);
}

int rte_mempool_sc_get(rte_mempool_t* mp, void **obj_p)
{
    odp_buffer_t odp_buffer;
    struct trex_odp_buffer_handle* buf_head;
    
    utl_mempool_check(mp);
    odp_buffer = odp_buffer_alloc(mp->odp_buffer_pool);
    if(odp_unlikely(odp_buffer == ODP_BUFFER_INVALID)){
	printf("%s alloc failed\n", __FUNCTION__);
	return -1; //keep consistent with dpdk synonymous func
    }
    
    buf_head = (struct trex_odp_buffer_handle*)odp_buffer_addr(odp_buffer);
    buf_head->odp_buffer = odp_buffer;
    *obj_p = (uint8_t*)buf_head + ROUND_UP_TREX_ODP_BUFFER_HANDLE(mp->odp_buffer_pool_param.buf.align);
    odp_atomic_inc_u32(&mp->count);
    
    return 0;
}

void rte_mempool_sp_put(rte_mempool_t *mp, void *obj)
{
    struct trex_odp_buffer_handle* head;

    head = (struct trex_odp_buffer_handle*)((uint8_t*)obj - ROUND_UP_TREX_ODP_BUFFER_HANDLE(mp->odp_buffer_pool_param.buf.align));
    utl_buffer_check(head);
    
    odp_buffer_free(head->odp_buffer);
    odp_atomic_dec_u32(&mp->count);
}

int rte_mempool_get(rte_mempool_t *mp, void **obj_p)
{
    return(rte_mempool_sc_get(mp, obj_p));
}

void rte_mempool_put(rte_mempool_t *mp, void *obj)
{
    rte_mempool_sp_put(mp, obj);
}

rte_mbuf_t *rte_pktmbuf_alloc(rte_mempool_t *mp)
{
    rte_mbuf_t* mbuf;
    uint32_t total_len;
    uint32_t buf_len;

    utl_mempool_check(mp);
    if(odp_unlikely(rte_mempool_get(mp, (void**)&mbuf) < 0)){
	printf("%s alloc failed\n", __FUNCTION__);
	return NULL;
    }

    total_len = mp->odp_buffer_pool_param.buf.size -
	ROUND_UP_TREX_ODP_BUFFER_HANDLE(mp->odp_buffer_pool_param.buf.align);
    if(odp_unlikely(total_len < sizeof(rte_mbuf_t))) {
	printf("%s invalid length\n", __FUNCTION__);
	return NULL;
    }
    memset((uint8_t*)mbuf, 0, total_len);
    buf_len = total_len - sizeof(rte_mbuf_t);

    mbuf->magic = MAGICA;
    mbuf->pool = mp;
    /* start of buffer is just after mbuf structure */
    mbuf->buf_addr = (uint8_t *)mbuf + sizeof(rte_mbuf_t);
    mbuf->buf_len = (uint16_t)buf_len;

    /* keep some headroom between start of buffer and data */
    mbuf->data_off = (RTE_PKTMBUF_HEADROOM <= mbuf->buf_len) ?
	RTE_PKTMBUF_HEADROOM : mbuf->buf_len;
    mbuf->data_len = 0;
    
    mbuf->next = NULL;
    mbuf->pkt_len = 0;
    mbuf->nb_segs = 1;
    mbuf->in_port = 0xff;
    odp_atomic_init_u32(&mbuf->refcnt, 1);

    return mbuf;
}

void rte_pktmbuf_free(rte_mbuf_t *mbuf)
{
    rte_mbuf_t* mbuf_next;

    utl_mbuf_check(mbuf);

    while(mbuf != NULL) {
	mbuf_next = mbuf->next;
	rte_pktmbuf_free_seg(mbuf);
	mbuf = mbuf_next;
    }
}

void rte_pktmbuf_detach(rte_mbuf_t *mbuf)
{
    mbuf->buf_addr = (uint8_t*)mbuf + sizeof(rte_mbuf_t);
    mbuf->buf_len = (uint16_t)(mbuf->pool->odp_buffer_pool_param.buf.size -
			       ROUND_UP_TREX_ODP_BUFFER_HANDLE(mbuf->pool->odp_buffer_pool_param.buf.align) -
			       sizeof(rte_mbuf_t));

    mbuf->data_off = (RTE_PKTMBUF_HEADROOM <= mbuf->buf_len) ?
	RTE_PKTMBUF_HEADROOM : mbuf->buf_len;
    mbuf->data_len = 0;
}

static void rte_mbuf_free_seg_internal(rte_mbuf_t* mbuf)
{
    utl_mbuf_check(mbuf);

    mbuf->magic = MAGICF;
    mbuf->next = NULL;
    rte_mempool_put(mbuf->pool, (void*)mbuf);
}

void rte_pktmbuf_free_seg(rte_mbuf_t *mbuf)
{
    utl_mbuf_check(mbuf);

    if(odp_atomic_fetch_dec_u32(&mbuf->refcnt) == 1) {
	rte_mbuf_t* md = RTE_MBUF_FROM_BADDR(mbuf->buf_addr);

	/* indirect mbuf
	 * although trex does not use such 
	 */
	if(md != mbuf) {
	    rte_pktmbuf_detach(mbuf);
	    if(rte_mbuf_refcnt_update(md, -1) == 0) {
		rte_mbuf_free_seg_internal(md);
	    }
	}

	rte_mbuf_free_seg_internal(mbuf);
    }
}

/* return udpated value not old value
 * this is consistent with dpdk synonymous function
 * pal/linux synosymous function has different behavior
 */
uint16_t rte_mbuf_refcnt_update(rte_mbuf_t *mbuf, int16_t value)
{
    uint32_t old;

    /* odp atomic act on unsigned int
     */
    if(value >= 0) {
	old = odp_atomic_fetch_add_u32(&mbuf->refcnt, (uint32_t)value);
    } else {
	old = odp_atomic_fetch_sub_u32(&mbuf->refcnt, (uint32_t)(-value));
    }

    return(old + value);
}

rte_mbuf_t* utl_rte_pktmbuf_add_after(rte_mbuf_t* mbuf1,rte_mbuf_t* mbuf2)
{
    rte_mbuf_refcnt_update(mbuf2,1);
    mbuf1->next=mbuf2;

    mbuf1->pkt_len += mbuf2->data_len;
    mbuf1->nb_segs = mbuf2->nb_segs + 1;
    return (mbuf1);
    
}

rte_mbuf_t* utl_rte_pktmbuf_add_after2(rte_mbuf_t* mbuf1,rte_mbuf_t* mbuf2)
{
    mbuf1->next=mbuf2;
    mbuf1->pkt_len += mbuf2->data_len;
    mbuf1->nb_segs = mbuf2->nb_segs + 1;
    return (mbuf1);
}

void utl_rte_pktmbuf_add_last(rte_mbuf_t* mbuf,rte_mbuf_t* m_last)
{
    //there could be 2 cases supported 
    //1. one mbuf 
    //2. two mbug where last is indirect 

    if ( mbuf->next == NULL ) {
	utl_rte_pktmbuf_add_after2(mbuf,m_last);
    }else{
	mbuf->next->next=m_last;
	mbuf->pkt_len += m_last->data_len;
	mbuf->nb_segs = 3;
    }
}

static inline rte_mbuf_t *rte_pktmbuf_lastseg(rte_mbuf_t *mbuf)
{
    rte_mbuf_t* mbuf_tmp = mbuf;
    utl_mbuf_check(mbuf_tmp);
    while (mbuf_tmp->next != NULL)
	mbuf_tmp = mbuf_tmp->next;

    return(mbuf_tmp);
}

static inline uint16_t rte_pktmbuf_headroom(const rte_mbuf_t *mbuf)
{
	return mbuf->data_off;
}

static inline uint16_t rte_pktmbuf_tailroom(const rte_mbuf_t* mbuf)
{
	return (uint16_t)(mbuf->buf_len - rte_pktmbuf_headroom(mbuf) -
			  mbuf->data_len);
}

char* rte_pktmbuf_append(rte_mbuf_t* mbuf, uint16_t len)
{
	void *tail;
	rte_mbuf_t* mbuf_last;
	utl_mbuf_check(mbuf);

	mbuf_last = rte_pktmbuf_lastseg(mbuf);
	if (len > rte_pktmbuf_tailroom(mbuf_last))
		return NULL;

	/* pal/linux seems wrong in this synonymous func
	 */
	tail = (char*) mbuf_last->buf_addr +
	    mbuf_last->data_off + mbuf_last->data_len;
	mbuf_last->data_len = (uint16_t)(mbuf_last->data_len + len);
	mbuf->pkt_len  = (mbuf->pkt_len + len);
	return (char*) tail;    
}

char *rte_pktmbuf_adj(rte_mbuf_t* mbuf, uint16_t len)
{
    utl_mbuf_check(mbuf);
    if (odp_unlikely(len > mbuf->data_len))
	return NULL;

    mbuf->data_len = (uint16_t)(mbuf->data_len - len);
    mbuf->data_off += len;
    mbuf->pkt_len  = (mbuf->pkt_len - len);
    return (char *)mbuf->buf_addr + mbuf->data_off;
}

int rte_pktmbuf_trim(rte_mbuf_t* mbuf, uint16_t len)
{
	rte_mbuf_t* mbuf_last;

	utl_mbuf_check(mbuf);
	mbuf_last = rte_pktmbuf_lastseg(mbuf);
	if (odp_unlikely(len > mbuf_last->data_len))
		return -1;

	mbuf_last->data_len = (uint16_t)(mbuf_last->data_len - len);
	mbuf->pkt_len  = (mbuf->pkt_len - len);
	return 0;
}

void rte_pktmbuf_attach(rte_mbuf_t* mbufi, rte_mbuf_t* mbufd)
{
	rte_mbuf_refcnt_update(mbufd, 1);
	mbufi->buf_addr = mbufd->buf_addr;
	mbufi->buf_len = mbufd->buf_len;
	mbufi->data_off = mbufd->data_off;
	mbufi->next = NULL;
	mbufi->pkt_len = mbufi->data_len;
	mbufi->nb_segs = 1;
}

static inline void rte_hexdump(FILE *f, const char* title, const void* buf, uint32_t len)
{
    uint32_t i, out, ofs;
    const uint8_t* data = (uint8_t*)buf;
#define LINE_LEN 80    
    char line[LINE_LEN];    /* space needed 8+16*3+3+16 == 75 */

    fprintf(f, "%s at [%p], len=%u\n", (title) ? title : "  Dump data", data, len);
    ofs = 0;
    while (ofs < len) {
        /* format the line in the buffer, then use printf to output to screen */
        out = snprintf(line, LINE_LEN, "%08X:", ofs);
        for (i = 0; ((ofs + i) < len) && (i < 16); i++)
            out += snprintf(line+out, LINE_LEN - out, " %02X", (data[ofs+i] & 0xff));
        for(; i <= 16; i++)
            out += snprintf(line+out, LINE_LEN - out, " | ");
        for(i = 0; (ofs < len) && (i < 16); i++, ofs++) {
            unsigned char c = data[ofs];
            if ( (c < ' ') || (c > '~'))
                c = '.';
            out += snprintf(line+out, LINE_LEN - out, "%c", c);
        }
        fprintf(f, "%s\n", line);
    }
    fflush(f);
}

void rte_pktmbuf_dump(FILE *f, const rte_mbuf_t* mbuf, uint32_t dump_len)
{
    uint32_t len;
    uint32_t nb_segs;

    utl_mbuf_check(mbuf);

    fprintf(f, "dump mbuf at 0x%p, buf_addr=%lx, buf_len=%u\n",
	    mbuf, (uint64_t)mbuf->buf_addr, (uint32_t)mbuf->buf_len);
    fprintf(f, "  pkt_len=%u, nb_segs=%u, in_port=%u\n",
	    mbuf->pkt_len, (uint32_t)mbuf->nb_segs, (uint32_t)mbuf->in_port);
    nb_segs = mbuf->nb_segs;

    while (mbuf && nb_segs != 0) {
	utl_mbuf_check(mbuf);
	    
	fprintf(f, "  segment at 0x%p, data=0x%p, data_len=%u\n",
		mbuf, rte_pktmbuf_mtod(mbuf, void*), (uint32_t)mbuf->data_len);
	len = dump_len;
	if (len > mbuf->data_len)
	    len = mbuf->data_len;
	if (len != 0)
	    rte_hexdump(f, NULL, rte_pktmbuf_mtod(mbuf, void*), len);
	dump_len -= len;
	mbuf = mbuf->next;
	nb_segs--;
    }
}

void * rte_memcpy(void *dst, const void *src, size_t n)
{
    return memcpy(dst, src, n);
}


void rte_exit(int exit_code, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
    exit(exit_code);
}

uint32_t rte_lcore_to_socket_id(uint32_t lcore_id)
{
    return 0;
}


uint64_t rte_rand(void)
{
    return rand();
}

/* return 0 for success -1 for failure
 * store odp_packet_t in *odp_packet_p
 */
int mbuf_to_odp_packet(rte_mbuf_t* mbuf, odp_packet_t* odp_packet_p)
{
    odp_packet_t pkt;
    odp_pool_t odp_packet_pool;
    uint8_t* buf;
    uint8_t* data_addr;
    int pkt_len;

    utl_mbuf_check(mbuf);
    odp_packet_pool = packet_pool_arr[mbuf->pool->socket_id];
    if(odp_unlikely(odp_packet_pool == ODP_POOL_INVALID)) {
	printf("%s invalid odp pakcet pool\n", __FUNCTION__);
	return -1;
    }

    pkt_len = mbuf->pkt_len;
    if(odp_unlikely(pkt_len <= 0)) {
	printf("%s invalid packet len %d\n", __FUNCTION__, pkt_len);
	return -1;
    }
    
    pkt = odp_packet_alloc(odp_packet_pool, pkt_len);
    if (odp_unlikely(pkt == ODP_PACKET_INVALID)) {
	printf("%s failed to alloc odp packet\n", __FUNCTION__);
	return -1;
    }
    
    buf = (uint8_t*)odp_packet_data(pkt);
    odp_packet_l2_offset_set(pkt, 0);
    
    while(mbuf != NULL && pkt_len > 0) {
	data_addr = (uint8_t*)mbuf->buf_addr + mbuf->data_off;
	memcpy(buf, data_addr, mbuf->data_len);
	buf += mbuf->data_len;
	pkt_len -= mbuf->data_len;
	mbuf = mbuf->next;
    }
    
    *odp_packet_p = pkt;
    
    return 0;
}

int mbuf_to_odp_packet_tbl(rte_mbuf**pkts, odp_packet_t* odp_pkts, uint16_t nb_pkts) {
    int i=0;
    for (i=0; i<nb_pkts; i++) {
        mbuf_to_odp_packet(pkts[i], &odp_pkts[i]);        
    } 
    return i;
}

