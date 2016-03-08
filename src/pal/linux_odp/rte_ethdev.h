/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_ETHDEV_H_
#define _RTE_ETHDEV_H_

#include <rte_pci.h>
/**
 * @file
 *
 * RTE Ethernet Device API
 *
 * The Ethernet Device API is composed of two parts:
 *
 * - The application-oriented Ethernet API that includes functions to setup
 *   an Ethernet device (configure it, setup its RX and TX queues and start it),
 *   to get its MAC address, the speed and the status of its physical link,
 *   to receive and to transmit packets, and so on.
 *
 * - The driver-oriented Ethernet API that exports a function allowing
 *   an Ethernet Poll Mode Driver (PMD) to simultaneously register itself as
 *   an Ethernet device driver and as a PCI driver for a set of matching PCI
 *   [Ethernet] devices classes.
 *
 * By default, all the functions of the Ethernet Device API exported by a PMD
 * are lock-free functions which assume to not be invoked in parallel on
 * different logical cores to work on the same target object.  For instance,
 * the receive function of a PMD cannot be invoked in parallel on two logical
 * cores to poll the same RX queue [of the same port]. Of course, this function
 * can be invoked in parallel by different logical cores on different RX queues.
 * It is the responsibility of the upper level application to enforce this rule.
 *
 * If needed, parallel accesses by multiple logical cores to shared queues
 * shall be explicitly protected by dedicated inline lock-aware functions
 * built on top of their corresponding lock-free functions of the PMD API.
 *
 * In all functions of the Ethernet API, the Ethernet device is
 * designated by an integer >= 0 named the device port identifier.
 *
 * At the Ethernet driver level, Ethernet devices are represented by a generic
 * data structure of type *rte_eth_dev*.
 *
 * Ethernet devices are dynamically registered during the PCI probing phase
 * performed at EAL initialization time.
 * When an Ethernet device is being probed, an *rte_eth_dev* structure and
 * a new port identifier are allocated for that device. Then, the eth_dev_init()
 * function supplied by the Ethernet driver matching the probed PCI
 * device is invoked to properly initialize the device.
 *
 * The role of the device init function consists of resetting the hardware,
 * checking access to Non-volatile Memory (NVM), reading the MAC address
 * from NVM etc.
 *
 * If the device init operation is successful, the correspondence between
 * the port identifier assigned to the new device and its associated
 * *rte_eth_dev* structure is effectively registered.
 * Otherwise, both the *rte_eth_dev* structure and the port identifier are
 * freed.
 *
 * The functions exported by the application Ethernet API to setup a device
 * designated by its port identifier must be invoked in the following order:
 *     - rte_eth_dev_configure()
 *     - rte_eth_tx_queue_setup()
 *     - rte_eth_rx_queue_setup()
 *     - rte_eth_dev_start()
 *
 * Then, the network application can invoke, in any order, the functions
 * exported by the Ethernet API to get the MAC address of a given device, to
 * get the speed and the status of a device physical link, to receive/transmit
 * [burst of] packets, and so on.
 *
 * If the application wants to change the configuration (i.e. call
 * rte_eth_dev_configure(), rte_eth_tx_queue_setup(), or
 * rte_eth_rx_queue_setup()), it must call rte_eth_dev_stop() first to stop the
 * device and then do the reconfiguration before calling rte_eth_dev_start()
 * again. The tramsit and receive functions should not be invoked when the
 * device is stopped.
 *
 * Please note that some configuration is not stored between calls to
 * rte_eth_dev_stop()/rte_eth_dev_start(). The following configuration will
 * be retained:
 *
 *     - flow control settings
 *     - receive mode configuration (promiscuous mode, hardware checksum mode,
 *       RSS/VMDQ settings etc.)
 *     - VLAN filtering configuration
 *     - MAC addresses supplied to MAC address array
 *     - flow director filtering mode (but not filtering rules)
 *     - NIC queue statistics mappings
 *
 * Any other configuration will not be stored and will need to be re-entered
 * after a call to rte_eth_dev_start().
 *
 * Finally, a network application can close an Ethernet device by invoking the
 * rte_eth_dev_close() function.
 *
 * Each function of the application Ethernet API invokes a specific function
 * of the PMD that controls the target device designated by its port
 * identifier.
 * For this purpose, all device-specific functions of an Ethernet driver are
 * supplied through a set of pointers contained in a generic structure of type
 * *eth_dev_ops*.
 * The address of the *eth_dev_ops* structure is stored in the *rte_eth_dev*
 * structure by the device init function of the Ethernet driver, which is
 * invoked during the PCI probing phase, as explained earlier.
 *
 * In other words, each function of the Ethernet API simply retrieves the
 * *rte_eth_dev* structure associated with the device port identifier and
 * performs an indirect invocation of the corresponding driver function
 * supplied in the *eth_dev_ops* structure of the *rte_eth_dev* structure.
 *
 * For performance reasons, the address of the burst-oriented RX and TX
 * functions of the Ethernet driver are not contained in the *eth_dev_ops*
 * structure. Instead, they are directly stored at the beginning of the
 * *rte_eth_dev* structure to avoid an extra indirect memory access during
 * their invocation.
 *
 * RTE ethernet device drivers do not use interrupts for transmitting or
 * receiving. Instead, Ethernet drivers export Poll-Mode receive and transmit
 * functions to applications.
 * Both receive and transmit functions are packet-burst oriented to minimize
 * their cost per packet through the following optimizations:
 *
 * - Sharing among multiple packets the incompressible cost of the
 *   invocation of receive/transmit functions.
 *
 * - Enabling receive/transmit functions to take advantage of burst-oriented
 *   hardware features (L1 cache, prefetch instructions, NIC head/tail
 *   registers) to minimize the number of CPU cycles per packet, for instance,
 *   by avoiding useless read memory accesses to ring descriptors, or by
 *   systematically using arrays of pointers that exactly fit L1 cache line
 *   boundaries and sizes.
 *
 * The burst-oriented receive function does not provide any error notification,
 * to avoid the corresponding overhead. As a hint, the upper-level application
 * might check the status of the device link once being systematically returned
 * a 0 value by the receive function of the driver for a given number of tries.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * A structure used to retrieve link-level information of an Ethernet port.
 */
struct rte_eth_link {
	uint16_t link_speed;      /**< ETH_LINK_SPEED_[10, 100, 1000, 10000] */
	uint16_t link_duplex;     /**< ETH_LINK_[HALF_DUPLEX, FULL_DUPLEX] */
	uint8_t  link_status : 1; /**< 1 -> link up, 0 -> link down */
}__attribute__((aligned(8)));     /**< aligned for atomic64 read/write */

#define ETH_LINK_SPEED_AUTONEG  0       /**< Auto-negotiate link speed. */
#define ETH_LINK_SPEED_10       10      /**< 10 megabits/second. */
#define ETH_LINK_SPEED_100      100     /**< 100 megabits/second. */
#define ETH_LINK_SPEED_1000     1000    /**< 1 gigabits/second. */
#define ETH_LINK_SPEED_10000    10000   /**< 10 gigabits/second. */
#define ETH_LINK_SPEED_10G      10000   /**< alias of 10 gigabits/second. */
#define ETH_LINK_SPEED_20G      20000   /**< 20 gigabits/second. */
#define ETH_LINK_SPEED_40G      40000   /**< 40 gigabits/second. */

#define ETH_LINK_AUTONEG_DUPLEX 0       /**< Auto-negotiate duplex. */
#define ETH_LINK_HALF_DUPLEX    1       /**< Half-duplex connection. */
#define ETH_LINK_FULL_DUPLEX    2       /**< Full-duplex connection. */

/**
 * A structure used to configure the ring threshold registers of an RX/TX
 * queue for an Ethernet port.
 */
struct rte_eth_thresh {
	uint8_t pthresh; /**< Ring prefetch threshold. */
	uint8_t hthresh; /**< Ring host threshold. */
	uint8_t wthresh; /**< Ring writeback threshold. */
};

/**
 *  Simple flags are used for rte_eth_conf.rxmode.mq_mode.
 */
#define ETH_MQ_RX_RSS_FLAG  0x1
#define ETH_MQ_RX_DCB_FLAG  0x2
#define ETH_MQ_RX_VMDQ_FLAG 0x4

/**
 *  A set of values to identify what method is to be used to route
 *  packets to multiple queues.
 */
enum rte_eth_rx_mq_mode {
	/** None of DCB,RSS or VMDQ mode */
	ETH_MQ_RX_NONE = 0,

	/** For RX side, only RSS is on */
	ETH_MQ_RX_RSS = ETH_MQ_RX_RSS_FLAG,
	/** For RX side,only DCB is on. */
	ETH_MQ_RX_DCB = ETH_MQ_RX_DCB_FLAG,
	/** Both DCB and RSS enable */
	ETH_MQ_RX_DCB_RSS = ETH_MQ_RX_RSS_FLAG | ETH_MQ_RX_DCB_FLAG,

	/** Only VMDQ, no RSS nor DCB */
	ETH_MQ_RX_VMDQ_ONLY = ETH_MQ_RX_VMDQ_FLAG,
	/** RSS mode with VMDQ */
	ETH_MQ_RX_VMDQ_RSS = ETH_MQ_RX_RSS_FLAG | ETH_MQ_RX_VMDQ_FLAG,
	/** Use VMDQ+DCB to route traffic to queues */
	ETH_MQ_RX_VMDQ_DCB = ETH_MQ_RX_VMDQ_FLAG | ETH_MQ_RX_DCB_FLAG,
	/** Enable both VMDQ and DCB in VMDq */
	ETH_MQ_RX_VMDQ_DCB_RSS = ETH_MQ_RX_RSS_FLAG | ETH_MQ_RX_DCB_FLAG |
				 ETH_MQ_RX_VMDQ_FLAG,
};

/**
 * for rx mq mode backward compatible
 */
#define ETH_RSS                       ETH_MQ_RX_RSS
#define VMDQ_DCB                      ETH_MQ_RX_VMDQ_DCB
#define ETH_DCB_RX                    ETH_MQ_RX_DCB

/**
 * A set of values to identify what method is to be used to transmit
 * packets using multi-TCs.
 */
enum rte_eth_tx_mq_mode {
	ETH_MQ_TX_NONE    = 0,  /**< It is in neither DCB nor VT mode. */
	ETH_MQ_TX_DCB,          /**< For TX side,only DCB is on. */
	ETH_MQ_TX_VMDQ_DCB,	/**< For TX side,both DCB and VT is on. */
	ETH_MQ_TX_VMDQ_ONLY,    /**< Only VT on, no DCB */
};

/**
 * for tx mq mode backward compatible
 */
#define ETH_DCB_NONE                ETH_MQ_TX_NONE
#define ETH_VMDQ_DCB_TX             ETH_MQ_TX_VMDQ_DCB
#define ETH_DCB_TX                  ETH_MQ_TX_DCB

/**
 * A structure used to configure the RX features of an Ethernet port.
 */
struct rte_eth_rxmode {
	/** The multi-queue packet distribution mode to be used, e.g. RSS. */
	enum rte_eth_rx_mq_mode mq_mode;
	uint32_t max_rx_pkt_len;  /**< Only used if jumbo_frame enabled. */
	uint16_t split_hdr_size;  /**< hdr buf size (header_split enabled).*/
	uint8_t header_split : 1, /**< Header Split enable. */
		hw_ip_checksum   : 1, /**< IP/UDP/TCP checksum offload enable. */
		hw_vlan_filter   : 1, /**< VLAN filter enable. */
		hw_vlan_strip    : 1, /**< VLAN strip enable. */
		hw_vlan_extend   : 1, /**< Extended VLAN enable. */
		jumbo_frame      : 1, /**< Jumbo Frame Receipt enable. */
		hw_strip_crc     : 1, /**< Enable CRC stripping by hardware. */
		enable_scatter   : 1; /**< Enable scatter packets rx handler */
};

/**
 * A structure used to configure the Receive Side Scaling (RSS) feature
 * of an Ethernet port.
 * If not NULL, the *rss_key* pointer of the *rss_conf* structure points
 * to an array holding the RSS key to use for hashing specific header
 * fields of received packets. The length of this array should be indicated
 * by *rss_key_len* below. Otherwise, a default random hash key is used by
 * the device driver.
 *
 * The *rss_key_len* field of the *rss_conf* structure indicates the length
 * in bytes of the array pointed by *rss_key*. To be compatible, this length
 * will be checked in i40e only. Others assume 40 bytes to be used as before.
 *
 * The *rss_hf* field of the *rss_conf* structure indicates the different
 * types of IPv4/IPv6 packets to which the RSS hashing must be applied.
 * Supplying an *rss_hf* equal to zero disables the RSS feature.
 */
struct rte_eth_rss_conf {
	uint8_t *rss_key;    /**< If not NULL, 40-byte hash key. */
	uint8_t rss_key_len; /**< hash key length in bytes. */
	uint64_t rss_hf;     /**< Hash functions to apply - see below. */
};

/* Supported RSS offloads */
/* for 1G & 10G */
#define ETH_RSS_IPV4_SHIFT                    0
#define ETH_RSS_IPV4_TCP_SHIFT                1
#define ETH_RSS_IPV6_SHIFT                    2
#define ETH_RSS_IPV6_EX_SHIFT                 3
#define ETH_RSS_IPV6_TCP_SHIFT                4
#define ETH_RSS_IPV6_TCP_EX_SHIFT             5
#define ETH_RSS_IPV4_UDP_SHIFT                6
#define ETH_RSS_IPV6_UDP_SHIFT                7
#define ETH_RSS_IPV6_UDP_EX_SHIFT             8
/* for 40G only */
#define ETH_RSS_NONF_IPV4_UDP_SHIFT           31
#define ETH_RSS_NONF_IPV4_TCP_SHIFT           33
#define ETH_RSS_NONF_IPV4_SCTP_SHIFT          34
#define ETH_RSS_NONF_IPV4_OTHER_SHIFT         35
#define ETH_RSS_FRAG_IPV4_SHIFT               36
#define ETH_RSS_NONF_IPV6_UDP_SHIFT           41
#define ETH_RSS_NONF_IPV6_TCP_SHIFT           43
#define ETH_RSS_NONF_IPV6_SCTP_SHIFT          44
#define ETH_RSS_NONF_IPV6_OTHER_SHIFT         45
#define ETH_RSS_FRAG_IPV6_SHIFT               46
#define ETH_RSS_FCOE_OX_SHIFT                 48
#define ETH_RSS_FCOE_RX_SHIFT                 49
#define ETH_RSS_FCOE_OTHER_SHIFT              50
#define ETH_RSS_L2_PAYLOAD_SHIFT              63

/* for 1G & 10G */
#define ETH_RSS_IPV4                    (1 << ETH_RSS_IPV4_SHIFT)
#define ETH_RSS_IPV4_TCP                (1 << ETH_RSS_IPV4_TCP_SHIFT)
#define ETH_RSS_IPV6                    (1 << ETH_RSS_IPV6_SHIFT)
#define ETH_RSS_IPV6_EX                 (1 << ETH_RSS_IPV6_EX_SHIFT)
#define ETH_RSS_IPV6_TCP                (1 << ETH_RSS_IPV6_TCP_SHIFT)
#define ETH_RSS_IPV6_TCP_EX             (1 << ETH_RSS_IPV6_TCP_EX_SHIFT)
#define ETH_RSS_IPV4_UDP                (1 << ETH_RSS_IPV4_UDP_SHIFT)
#define ETH_RSS_IPV6_UDP                (1 << ETH_RSS_IPV6_UDP_SHIFT)
#define ETH_RSS_IPV6_UDP_EX             (1 << ETH_RSS_IPV6_UDP_EX_SHIFT)
/* for 40G only */
#define ETH_RSS_NONF_IPV4_UDP           (1ULL << ETH_RSS_NONF_IPV4_UDP_SHIFT)
#define ETH_RSS_NONF_IPV4_TCP           (1ULL << ETH_RSS_NONF_IPV4_TCP_SHIFT)
#define ETH_RSS_NONF_IPV4_SCTP          (1ULL << ETH_RSS_NONF_IPV4_SCTP_SHIFT)
#define ETH_RSS_NONF_IPV4_OTHER         (1ULL << ETH_RSS_NONF_IPV4_OTHER_SHIFT)
#define ETH_RSS_FRAG_IPV4               (1ULL << ETH_RSS_FRAG_IPV4_SHIFT)
#define ETH_RSS_NONF_IPV6_UDP           (1ULL << ETH_RSS_NONF_IPV6_UDP_SHIFT)
#define ETH_RSS_NONF_IPV6_TCP           (1ULL << ETH_RSS_NONF_IPV6_TCP_SHIFT)
#define ETH_RSS_NONF_IPV6_SCTP          (1ULL << ETH_RSS_NONF_IPV6_SCTP_SHIFT)
#define ETH_RSS_NONF_IPV6_OTHER         (1ULL << ETH_RSS_NONF_IPV6_OTHER_SHIFT)
#define ETH_RSS_FRAG_IPV6               (1ULL << ETH_RSS_FRAG_IPV6_SHIFT)
/* FCOE relevant should not be used */
#define ETH_RSS_FCOE_OX                 (1ULL << ETH_RSS_FCOE_OX_SHIFT)
#define ETH_RSS_FCOE_RX                 (1ULL << ETH_RSS_FCOE_RX_SHIFT)
#define ETH_RSS_FCOE_OTHER              (1ULL << ETH_RSS_FCOE_OTHER_SHIFT)
#define ETH_RSS_L2_PAYLOAD              (1ULL << ETH_RSS_L2_PAYLOAD_SHIFT)

#define ETH_RSS_IP ( \
		ETH_RSS_IPV4 | \
		ETH_RSS_IPV6 | \
		ETH_RSS_NONF_IPV4_OTHER | \
		ETH_RSS_FRAG_IPV4 | \
		ETH_RSS_NONF_IPV6_OTHER | \
		ETH_RSS_FRAG_IPV6)
#define ETH_RSS_UDP ( \
		ETH_RSS_IPV4 | \
		ETH_RSS_IPV6 | \
		ETH_RSS_IPV4_UDP | \
		ETH_RSS_IPV6_UDP | \
		ETH_RSS_IPV6_UDP_EX | \
		ETH_RSS_NONF_IPV4_UDP | \
		ETH_RSS_NONF_IPV6_UDP)
/**< Mask of valid RSS hash protocols */
#define ETH_RSS_PROTO_MASK ( \
		ETH_RSS_IPV4 | \
		ETH_RSS_IPV4_TCP | \
		ETH_RSS_IPV6 | \
		ETH_RSS_IPV6_EX | \
		ETH_RSS_IPV6_TCP | \
		ETH_RSS_IPV6_TCP_EX | \
		ETH_RSS_IPV4_UDP | \
		ETH_RSS_IPV6_UDP | \
		ETH_RSS_IPV6_UDP_EX | \
		ETH_RSS_NONF_IPV4_UDP | \
		ETH_RSS_NONF_IPV4_TCP | \
		ETH_RSS_NONF_IPV4_SCTP | \
		ETH_RSS_NONF_IPV4_OTHER | \
		ETH_RSS_FRAG_IPV4 | \
		ETH_RSS_NONF_IPV6_UDP | \
		ETH_RSS_NONF_IPV6_TCP | \
		ETH_RSS_NONF_IPV6_SCTP | \
		ETH_RSS_NONF_IPV6_OTHER | \
		ETH_RSS_FRAG_IPV6 | \
		ETH_RSS_L2_PAYLOAD)

/*
 * Definitions used for redirection table entry size.
 * Some RSS RETA sizes may not be supported by some drivers, check the
 * documentation or the description of relevant functions for more details.
 */
#define ETH_RSS_RETA_SIZE_64  64
#define ETH_RSS_RETA_SIZE_128 128
#define ETH_RSS_RETA_SIZE_512 512
#define RTE_RETA_GROUP_SIZE   64

/* Definitions used for VMDQ and DCB functionality */
#define ETH_VMDQ_MAX_VLAN_FILTERS   64 /**< Maximum nb. of VMDQ vlan filters. */
#define ETH_DCB_NUM_USER_PRIORITIES 8  /**< Maximum nb. of DCB priorities. */
#define ETH_VMDQ_DCB_NUM_QUEUES     128 /**< Maximum nb. of VMDQ DCB queues. */
#define ETH_DCB_NUM_QUEUES          128 /**< Maximum nb. of DCB queues. */

/* DCB capability defines */
#define ETH_DCB_PG_SUPPORT      0x00000001 /**< Priority Group(ETS) support. */
#define ETH_DCB_PFC_SUPPORT     0x00000002 /**< Priority Flow Control support. */

/* Definitions used for VLAN Offload functionality */
#define ETH_VLAN_STRIP_OFFLOAD   0x0001 /**< VLAN Strip  On/Off */
#define ETH_VLAN_FILTER_OFFLOAD  0x0002 /**< VLAN Filter On/Off */
#define ETH_VLAN_EXTEND_OFFLOAD  0x0004 /**< VLAN Extend On/Off */

/* Definitions used for mask VLAN setting */
#define ETH_VLAN_STRIP_MASK   0x0001 /**< VLAN Strip  setting mask */
#define ETH_VLAN_FILTER_MASK  0x0002 /**< VLAN Filter  setting mask*/
#define ETH_VLAN_EXTEND_MASK  0x0004 /**< VLAN Extend  setting mask*/
#define ETH_VLAN_ID_MAX       0x0FFF /**< VLAN ID is in lower 12 bits*/

/* Definitions used for receive MAC address   */
#define ETH_NUM_RECEIVE_MAC_ADDR  128 /**< Maximum nb. of receive mac addr. */

/* Definitions used for unicast hash  */
#define ETH_VMDQ_NUM_UC_HASH_ARRAY  128 /**< Maximum nb. of UC hash array. */

/* Definitions used for VMDQ pool rx mode setting */
#define ETH_VMDQ_ACCEPT_UNTAG   0x0001 /**< accept untagged packets. */
#define ETH_VMDQ_ACCEPT_HASH_MC 0x0002 /**< accept packets in multicast table . */
#define ETH_VMDQ_ACCEPT_HASH_UC 0x0004 /**< accept packets in unicast table. */
#define ETH_VMDQ_ACCEPT_BROADCAST   0x0008 /**< accept broadcast packets. */
#define ETH_VMDQ_ACCEPT_MULTICAST   0x0010 /**< multicast promiscuous. */

/* Definitions used for VMDQ mirror rules setting */
#define ETH_VMDQ_NUM_MIRROR_RULE     4 /**< Maximum nb. of mirror rules. . */

#define ETH_VMDQ_POOL_MIRROR    0x0001 /**< Virtual Pool Mirroring. */
#define ETH_VMDQ_UPLINK_MIRROR  0x0002 /**< Uplink Port Mirroring. */
#define ETH_VMDQ_DOWNLIN_MIRROR 0x0004 /**< Downlink Port Mirroring. */
#define ETH_VMDQ_VLAN_MIRROR    0x0008 /**< VLAN Mirroring. */

/**
 * A structure used to configure VLAN traffic mirror of an Ethernet port.
 */
struct rte_eth_vlan_mirror {
	uint64_t vlan_mask; /**< mask for valid VLAN ID. */
	uint16_t vlan_id[ETH_VMDQ_MAX_VLAN_FILTERS];
	/** VLAN ID list for vlan mirror. */
};

/**
 * A structure used to configure traffic mirror of an Ethernet port.
 */
struct rte_eth_vmdq_mirror_conf {
	uint8_t rule_type_mask; /**< Mirroring rule type mask we want to set */
	uint8_t dst_pool; /**< Destination pool for this mirror rule. */
	uint64_t pool_mask; /**< Bitmap of pool for pool mirroring */
	struct rte_eth_vlan_mirror vlan; /**< VLAN ID setting for VLAN mirroring */
};

/**
 * A structure used to configure 64 entries of Redirection Table of the
 * Receive Side Scaling (RSS) feature of an Ethernet port. To configure
 * more than 64 entries supported by hardware, an array of this structure
 * is needed.
 */
struct rte_eth_rss_reta_entry64 {
	uint64_t mask;
	/**< Mask bits indicate which entries need to be updated/queried. */
	uint8_t reta[RTE_RETA_GROUP_SIZE];
	/**< Group of 64 redirection table entries. */
};

/**
 * This enum indicates the possible number of traffic classes
 * in DCB configratioins
 */
enum rte_eth_nb_tcs {
	ETH_4_TCS = 4, /**< 4 TCs with DCB. */
	ETH_8_TCS = 8  /**< 8 TCs with DCB. */
};

/**
 * This enum indicates the possible number of queue pools
 * in VMDQ configurations.
 */
enum rte_eth_nb_pools {
	ETH_8_POOLS = 8,    /**< 8 VMDq pools. */
	ETH_16_POOLS = 16,  /**< 16 VMDq pools. */
	ETH_32_POOLS = 32,  /**< 32 VMDq pools. */
	ETH_64_POOLS = 64   /**< 64 VMDq pools. */
};

/* This structure may be extended in future. */
struct rte_eth_dcb_rx_conf {
	enum rte_eth_nb_tcs nb_tcs; /**< Possible DCB TCs, 4 or 8 TCs */
	uint8_t dcb_queue[ETH_DCB_NUM_USER_PRIORITIES];
	/**< Possible DCB queue,4 or 8. */
};

struct rte_eth_vmdq_dcb_tx_conf {
	enum rte_eth_nb_pools nb_queue_pools; /**< With DCB, 16 or 32 pools. */
	uint8_t dcb_queue[ETH_DCB_NUM_USER_PRIORITIES];
	/**< Possible DCB queue,4 or 8. */
};

struct rte_eth_dcb_tx_conf {
	enum rte_eth_nb_tcs nb_tcs; /**< Possible DCB TCs, 4 or 8 TCs. */
	uint8_t dcb_queue[ETH_DCB_NUM_USER_PRIORITIES];
	/**< Possible DCB queue,4 or 8. */
};

struct rte_eth_vmdq_tx_conf {
	enum rte_eth_nb_pools nb_queue_pools; /**< VMDq mode, 64 pools. */
};

/**
 * A structure used to configure the VMDQ+DCB feature
 * of an Ethernet port.
 *
 * Using this feature, packets are routed to a pool of queues, based
 * on the vlan id in the vlan tag, and then to a specific queue within
 * that pool, using the user priority vlan tag field.
 *
 * A default pool may be used, if desired, to route all traffic which
 * does not match the vlan filter rules.
 */
struct rte_eth_vmdq_dcb_conf {
	enum rte_eth_nb_pools nb_queue_pools; /**< With DCB, 16 or 32 pools */
	uint8_t enable_default_pool; /**< If non-zero, use a default pool */
	uint8_t default_pool; /**< The default pool, if applicable */
	uint8_t nb_pool_maps; /**< We can have up to 64 filters/mappings */
	struct {
		uint16_t vlan_id; /**< The vlan id of the received frame */
		uint64_t pools;   /**< Bitmask of pools for packet rx */
	} pool_map[ETH_VMDQ_MAX_VLAN_FILTERS]; /**< VMDq vlan pool maps. */
	uint8_t dcb_queue[ETH_DCB_NUM_USER_PRIORITIES];
	/**< Selects a queue in a pool */
};

struct rte_eth_vmdq_rx_conf {
	enum rte_eth_nb_pools nb_queue_pools; /**< VMDq only mode, 8 or 64 pools */
	uint8_t enable_default_pool; /**< If non-zero, use a default pool */
	uint8_t default_pool; /**< The default pool, if applicable */
	uint8_t enable_loop_back; /**< Enable VT loop back */
	uint8_t nb_pool_maps; /**< We can have up to 64 filters/mappings */
	uint32_t rx_mode; /**< Flags from ETH_VMDQ_ACCEPT_* */
	struct {
		uint16_t vlan_id; /**< The vlan id of the received frame */
		uint64_t pools;   /**< Bitmask of pools for packet rx */
	} pool_map[ETH_VMDQ_MAX_VLAN_FILTERS]; /**< VMDq vlan pool maps. */
};

/**
 * A structure used to configure the TX features of an Ethernet port.
 */
struct rte_eth_txmode {
	enum rte_eth_tx_mq_mode mq_mode; /**< TX multi-queues mode. */

	/* For i40e specifically */
	uint16_t pvid;
	uint8_t hw_vlan_reject_tagged : 1,
		/**< If set, reject sending out tagged pkts */
		hw_vlan_reject_untagged : 1,
		/**< If set, reject sending out untagged pkts */
		hw_vlan_insert_pvid : 1;
		/**< If set, enable port based VLAN insertion */
};

/**
 * A structure used to configure an RX ring of an Ethernet port.
 */
struct rte_eth_rxconf {
	struct rte_eth_thresh rx_thresh; /**< RX ring threshold registers. */
	uint16_t rx_free_thresh; /**< Drives the freeing of RX descriptors. */
	uint8_t rx_drop_en; /**< Drop packets if no descriptors are available. */
	uint8_t rx_deferred_start; /**< Do not start queue with rte_eth_dev_start(). */
};

#define ETH_TXQ_FLAGS_NOMULTSEGS 0x0001 /**< nb_segs=1 for all mbufs */
#define ETH_TXQ_FLAGS_NOREFCOUNT 0x0002 /**< refcnt can be ignored */
#define ETH_TXQ_FLAGS_NOMULTMEMP 0x0004 /**< all bufs come from same mempool */
#define ETH_TXQ_FLAGS_NOVLANOFFL 0x0100 /**< disable VLAN offload */
#define ETH_TXQ_FLAGS_NOXSUMSCTP 0x0200 /**< disable SCTP checksum offload */
#define ETH_TXQ_FLAGS_NOXSUMUDP  0x0400 /**< disable UDP checksum offload */
#define ETH_TXQ_FLAGS_NOXSUMTCP  0x0800 /**< disable TCP checksum offload */
#define ETH_TXQ_FLAGS_NOOFFLOADS \
		(ETH_TXQ_FLAGS_NOVLANOFFL | ETH_TXQ_FLAGS_NOXSUMSCTP | \
		 ETH_TXQ_FLAGS_NOXSUMUDP  | ETH_TXQ_FLAGS_NOXSUMTCP)
/**
 * A structure used to configure a TX ring of an Ethernet port.
 */
struct rte_eth_txconf {
	struct rte_eth_thresh tx_thresh; /**< TX ring threshold registers. */
	uint16_t tx_rs_thresh; /**< Drives the setting of RS bit on TXDs. */
	uint16_t tx_free_thresh; /**< Drives the freeing of TX buffers. */
	uint32_t txq_flags; /**< Set flags for the Tx queue */
	uint8_t tx_deferred_start; /**< Do not start queue with rte_eth_dev_start(). */
};

/**
 * This enum indicates the flow control mode
 */
enum rte_eth_fc_mode {
	RTE_FC_NONE = 0, /**< Disable flow control. */
	RTE_FC_RX_PAUSE, /**< RX pause frame, enable flowctrl on TX side. */
	RTE_FC_TX_PAUSE, /**< TX pause frame, enable flowctrl on RX side. */
	RTE_FC_FULL      /**< Enable flow control on both side. */
};

/**
 * A structure used to configure Ethernet flow control parameter.
 * These parameters will be configured into the register of the NIC.
 * Please refer to the corresponding data sheet for proper value.
 */
struct rte_eth_fc_conf {
	uint32_t high_water;  /**< High threshold value to trigger XOFF */
	uint32_t low_water;   /**< Low threshold value to trigger XON */
	uint16_t pause_time;  /**< Pause quota in the Pause frame */
	uint16_t send_xon;    /**< Is XON frame need be sent */
	enum rte_eth_fc_mode mode;  /**< Link flow control mode */
	uint8_t mac_ctrl_frame_fwd; /**< Forward MAC control frames */
	uint8_t autoneg;      /**< Use Pause autoneg */
};

/**
 * A structure used to configure Ethernet priority flow control parameter.
 * These parameters will be configured into the register of the NIC.
 * Please refer to the corresponding data sheet for proper value.
 */
struct rte_eth_pfc_conf {
	struct rte_eth_fc_conf fc; /**< General flow control parameter. */
	uint8_t priority;          /**< VLAN User Priority. */
};

/**
 *  Memory space that can be configured to store Flow Director filters
 *  in the board memory.
 */
enum rte_fdir_pballoc_type {
	RTE_FDIR_PBALLOC_64K = 0,  /**< 64k. */
	RTE_FDIR_PBALLOC_128K,     /**< 128k. */
	RTE_FDIR_PBALLOC_256K,     /**< 256k. */
};

/**
 *  Select report mode of FDIR hash information in RX descriptors.
 */
enum rte_fdir_status_mode {
	RTE_FDIR_NO_REPORT_STATUS = 0, /**< Never report FDIR hash. */
	RTE_FDIR_REPORT_STATUS, /**< Only report FDIR hash for matching pkts. */
	RTE_FDIR_REPORT_STATUS_ALWAYS, /**< Always report FDIR hash. */
};

enum rte_fdir_mode {
    RTE_FDIR_MODE_NONE      = 0, /**< Disable FDIR support. */
    RTE_FDIR_MODE_SIGNATURE,     /**< Enable FDIR signature filter mode. */
    RTE_FDIR_MODE_PERFECT,       /**< Enable FDIR perfect filter mode. */
    RTE_FDIR_MODE_PERFECT_MAC_VLAN, /**< Enable FDIR filter mode - MAC VLAN. */
    RTE_FDIR_MODE_PERFECT_TUNNEL,   /**< Enable FDIR filter mode - tunnel. */
};
/**
 * UDP tunneling configuration.
 */
struct rte_eth_udp_tunnel {
	uint16_t udp_port;
	uint8_t prot_type;
};

/**
 *  Possible l4type of FDIR filters.
 */
enum rte_l4type {
	RTE_FDIR_L4TYPE_NONE = 0,       /**< None. */
	RTE_FDIR_L4TYPE_UDP,            /**< UDP. */
	RTE_FDIR_L4TYPE_TCP,            /**< TCP. */
	RTE_FDIR_L4TYPE_SCTP,           /**< SCTP. */
};

/**
 *  Select IPv4 or IPv6 FDIR filters.
 */
enum rte_iptype {
	RTE_FDIR_IPTYPE_IPV4 = 0,     /**< IPv4. */
	RTE_FDIR_IPTYPE_IPV6 ,        /**< IPv6. */
};

/**
 *  A structure used to define a FDIR packet filter.
 */
struct rte_fdir_filter {
	uint16_t flex_bytes; /**< Flex bytes value to match. */
	uint16_t vlan_id; /**< VLAN ID value to match, 0 otherwise. */
	uint16_t port_src; /**< Source port to match, 0 otherwise. */
	uint16_t port_dst; /**< Destination port to match, 0 otherwise. */
	union {
		uint32_t ipv4_addr; /**< IPv4 source address to match. */
		uint32_t ipv6_addr[4]; /**< IPv6 source address to match. */
	} ip_src; /**< IPv4/IPv6 source address to match (union of above). */
	union {
		uint32_t ipv4_addr; /**< IPv4 destination address to match. */
		uint32_t ipv6_addr[4]; /**< IPv6 destination address to match */
	} ip_dst; /**< IPv4/IPv6 destination address to match (union of above). */
	enum rte_l4type l4type; /**< l4type to match: NONE/UDP/TCP/SCTP. */
	enum rte_iptype iptype; /**< IP packet type to match: IPv4 or IPv6. */
};

/**
 *  A structure used to configure FDIR masks that are used by the device
 *  to match the various fields of RX packet headers.
 *  @note The only_ip_flow field has the opposite meaning compared to other
 *  masks!
 */
struct rte_fdir_masks {
	/** When set to 1, packet l4type is \b NOT relevant in filters, and
	   source and destination port masks must be set to zero. */
	uint8_t only_ip_flow;
	/** If set to 1, vlan_id is relevant in filters. */
	uint8_t vlan_id;
	/** If set to 1, vlan_prio is relevant in filters. */
	uint8_t vlan_prio;
	/** If set to 1, flexbytes is relevant in filters. */
	uint8_t flexbytes;
	/** If set to 1, set the IPv6 masks. Otherwise set the IPv4 masks. */
	uint8_t set_ipv6_mask;
	/** When set to 1, comparison of destination IPv6 address with IP6AT
	    registers is meaningful. */
	uint8_t comp_ipv6_dst;
	/** Mask of Destination IPv4 Address. All bits set to 1 define the
	    relevant bits to use in the destination address of an IPv4 packet
	    when matching it against FDIR filters. */
	uint32_t dst_ipv4_mask;
	/** Mask of Source IPv4 Address. All bits set to 1 define
	    the relevant bits to use in the source address of an IPv4 packet
	    when matching it against FDIR filters. */
	uint32_t src_ipv4_mask;
	/** Mask of Source IPv6 Address. All bits set to 1 define the
	    relevant BYTES to use in the source address of an IPv6 packet
	    when matching it against FDIR filters. */
	uint16_t dst_ipv6_mask;
	/** Mask of Destination IPv6 Address. All bits set to 1 define the
	    relevant BYTES to use in the destination address of an IPv6 packet
	    when matching it against FDIR filters. */
	uint16_t src_ipv6_mask;
	/** Mask of Source Port. All bits set to 1 define the relevant
	    bits to use in the source port of an IP packets when matching it
	    against FDIR filters. */
	uint16_t src_port_mask;
	/** Mask of Destination Port. All bits set to 1 define the relevant
	    bits to use in the destination port of an IP packet when matching it
	    against FDIR filters. */
	uint16_t dst_port_mask;
};

/**
 *  A structure used to report the status of the flow director filters in use.
 */
struct rte_eth_fdir {
	/** Number of filters with collision indication. */
	uint16_t collision;
	/** Number of free (non programmed) filters. */
	uint16_t free;
	/** The Lookup hash value of the added filter that updated the value
	   of the MAXLEN field */
	uint16_t maxhash;
	/** Longest linked list of filters in the table. */
	uint8_t maxlen;
	/** Number of added filters. */
	uint64_t add;
	/** Number of removed filters. */
	uint64_t remove;
	/** Number of failed added filters (no more space in device). */
	uint64_t f_add;
	/** Number of failed removed filters. */
	uint64_t f_remove;
};

/**
 * A structure used to enable/disable specific device interrupts.
 */
struct rte_intr_conf {
	/** enable/disable lsc interrupt. 0 (default) - disable, 1 enable */
	uint16_t lsc;
};


/**
 * A structure used to retrieve the contextual information of
 * an Ethernet device, such as the controlling driver of the device,
 * its PCI context, etc...
 */

/**
 * RX offload capabilities of a device.
 */
#define DEV_RX_OFFLOAD_VLAN_STRIP  0x00000001
#define DEV_RX_OFFLOAD_IPV4_CKSUM  0x00000002
#define DEV_RX_OFFLOAD_UDP_CKSUM   0x00000004
#define DEV_RX_OFFLOAD_TCP_CKSUM   0x00000008
#define DEV_RX_OFFLOAD_TCP_LRO     0x00000010

/**
 * TX offload capabilities of a device.
 */
#define DEV_TX_OFFLOAD_VLAN_INSERT 0x00000001
#define DEV_TX_OFFLOAD_IPV4_CKSUM  0x00000002
#define DEV_TX_OFFLOAD_UDP_CKSUM   0x00000004
#define DEV_TX_OFFLOAD_TCP_CKSUM   0x00000008
#define DEV_TX_OFFLOAD_SCTP_CKSUM  0x00000010
#define DEV_TX_OFFLOAD_TCP_TSO     0x00000020
#define DEV_TX_OFFLOAD_UDP_TSO     0x00000040

struct rte_eth_dev_info {
	struct rte_pci_device *pci_dev; /**< Device PCI information. */
	const char *driver_name; /**< Device Driver name. */
	unsigned int if_index; /**< Index to bound host interface, or 0 if none.
		Use if_indextoname() to translate into an interface name. */
	uint32_t min_rx_bufsize; /**< Minimum size of RX buffer. */
	uint32_t max_rx_pktlen; /**< Maximum configurable length of RX pkt. */
	uint16_t max_rx_queues; /**< Maximum number of RX queues. */
	uint16_t max_tx_queues; /**< Maximum number of TX queues. */
	uint32_t max_mac_addrs; /**< Maximum number of MAC addresses. */
	uint32_t max_hash_mac_addrs;
	/** Maximum number of hash MAC addresses for MTA and UTA. */
	uint16_t max_vfs; /**< Maximum number of VFs. */
	uint16_t max_vmdq_pools; /**< Maximum number of VMDq pools. */
	uint32_t rx_offload_capa; /**< Device RX offload capabilities. */
	uint32_t tx_offload_capa; /**< Device TX offload capabilities. */
	uint16_t reta_size;
	/**< Device redirection table size, the total number of entries. */
	struct rte_eth_rxconf default_rxconf; /**< Default RX configuration */
	struct rte_eth_txconf default_txconf; /**< Default TX configuration */
	uint16_t vmdq_queue_base; /**< First queue ID for VMDQ pools. */
	uint16_t vmdq_queue_num;  /**< Queue number for VMDQ pools. */
	uint16_t vmdq_pool_base;  /**< First ID of VMDQ pools. */
};

/** Maximum name length for extended statistics counters */
#define RTE_ETH_XSTATS_NAME_SIZE 64

/**
 * An Ethernet device extended statistic structure
 *
 * This structure is used by ethdev->eth_xstats_get() to provide
 * statistics that are not provided in the generic rte_eth_stats
 * structure.
 */
struct rte_eth_xstats {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	uint64_t value;
};

struct rte_eth_dev;

/** @internal Structure to keep track of registered callbacks */

#define TCP_UGR_FLAG 0x20
#define TCP_ACK_FLAG 0x10
#define TCP_PSH_FLAG 0x08
#define TCP_RST_FLAG 0x04
#define TCP_SYN_FLAG 0x02
#define TCP_FIN_FLAG 0x01
#define TCP_FLAG_ALL 0x3F

/**
 *  A structure used to define an ethertype filter.
 */
struct rte_ethertype_filter {
	uint16_t ethertype;  /**< little endian. */
	uint8_t priority_en; /**< compare priority enable. */
	uint8_t priority;
};

/**
 *  A structure used to define an syn filter.
 */
struct rte_syn_filter {
	uint8_t hig_pri; /**< 1 means higher pri than 2tuple, 5tupe,
			      and flex filter, 0 means lower pri. */
};

/**
 *  A structure used to define a 2tuple filter.
 */
struct rte_2tuple_filter {
	uint16_t dst_port;        /**< big endian. */
	uint8_t protocol;
	uint8_t tcp_flags;
	uint16_t priority;        /**< used when more than one filter matches. */
	uint8_t dst_port_mask:1,  /**< if mask is 1b, means not compare. */
		protocol_mask:1;
};

/**
 *  A structure used to define a flex filter.
 */
struct rte_flex_filter {
	uint16_t len;
	uint32_t dwords[32];  /**< flex bytes in big endian. */
	uint8_t mask[16];     /**< if mask bit is 1b, do not compare
				   corresponding byte in dwords. */
	uint8_t priority;
};

/**
 *  A structure used to define a 5tuple filter.
 */
struct rte_5tuple_filter {
	uint32_t dst_ip;         /**< destination IP address in big endian. */
	uint32_t src_ip;         /**< source IP address in big endian. */
	uint16_t dst_port;       /**< destination port in big endian. */
	uint16_t src_port;       /**< source Port big endian. */
	uint8_t protocol;        /**< l4 protocol. */
	uint8_t tcp_flags;       /**< tcp flags. */
	uint16_t priority;       /**< seven evels (001b-111b), 111b is highest,
				      used when more than one filter matches. */
	uint8_t dst_ip_mask:1,   /**< if mask is 1b, do not compare dst ip. */
		src_ip_mask:1,   /**< if mask is 1b, do not compare src ip. */
		dst_port_mask:1, /**< if mask is 1b, do not compare dst port. */
		src_port_mask:1, /**< if mask is 1b, do not compare src port. */
		protocol_mask:1; /**< if mask is 1b, do not compare protocol. */
};



#ifdef RTE_NIC_BYPASS

enum {
	RTE_BYPASS_MODE_NONE,
	RTE_BYPASS_MODE_NORMAL,
	RTE_BYPASS_MODE_BYPASS,
	RTE_BYPASS_MODE_ISOLATE,
	RTE_BYPASS_MODE_NUM,
};

#define	RTE_BYPASS_MODE_VALID(x)	\
	((x) > RTE_BYPASS_MODE_NONE && (x) < RTE_BYPASS_MODE_NUM)

enum {
	RTE_BYPASS_EVENT_NONE,
	RTE_BYPASS_EVENT_START,
	RTE_BYPASS_EVENT_OS_ON = RTE_BYPASS_EVENT_START,
	RTE_BYPASS_EVENT_POWER_ON,
	RTE_BYPASS_EVENT_OS_OFF,
	RTE_BYPASS_EVENT_POWER_OFF,
	RTE_BYPASS_EVENT_TIMEOUT,
	RTE_BYPASS_EVENT_NUM
};

#define	RTE_BYPASS_EVENT_VALID(x)	\
	((x) > RTE_BYPASS_EVENT_NONE && (x) < RTE_BYPASS_MODE_NUM)

enum {
	RTE_BYPASS_TMT_OFF,     /* timeout disabled. */
	RTE_BYPASS_TMT_1_5_SEC, /* timeout for 1.5 seconds */
	RTE_BYPASS_TMT_2_SEC,   /* timeout for 2 seconds */
	RTE_BYPASS_TMT_3_SEC,   /* timeout for 3 seconds */
	RTE_BYPASS_TMT_4_SEC,   /* timeout for 4 seconds */
	RTE_BYPASS_TMT_8_SEC,   /* timeout for 8 seconds */
	RTE_BYPASS_TMT_16_SEC,  /* timeout for 16 seconds */
	RTE_BYPASS_TMT_32_SEC,  /* timeout for 32 seconds */
	RTE_BYPASS_TMT_NUM
};

#define	RTE_BYPASS_TMT_VALID(x)	\
	((x) == RTE_BYPASS_TMT_OFF || \
	((x) > RTE_BYPASS_TMT_OFF && (x) < RTE_BYPASS_TMT_NUM))

typedef void (*bypass_init_t)(struct rte_eth_dev *dev);
typedef int32_t (*bypass_state_set_t)(struct rte_eth_dev *dev, uint32_t *new_state);
typedef int32_t (*bypass_state_show_t)(struct rte_eth_dev *dev, uint32_t *state);
typedef int32_t (*bypass_event_set_t)(struct rte_eth_dev *dev, uint32_t state, uint32_t event);
typedef int32_t (*bypass_event_show_t)(struct rte_eth_dev *dev, uint32_t event_shift, uint32_t *event);
typedef int32_t (*bypass_wd_timeout_set_t)(struct rte_eth_dev *dev, uint32_t timeout);
typedef int32_t (*bypass_wd_timeout_show_t)(struct rte_eth_dev *dev, uint32_t *wd_timeout);
typedef int32_t (*bypass_ver_show_t)(struct rte_eth_dev *dev, uint32_t *ver);
typedef int32_t (*bypass_wd_reset_t)(struct rte_eth_dev *dev);
#endif

typedef int (*eth_add_syn_filter_t)(struct rte_eth_dev *dev,
			struct rte_syn_filter *filter, uint16_t rx_queue);
/**< @internal add syn filter rule on an Ethernet device */

typedef int (*eth_remove_syn_filter_t)(struct rte_eth_dev *dev);
/**< @internal remove syn filter rule on an Ethernet device */

typedef int (*eth_get_syn_filter_t)(struct rte_eth_dev *dev,
			struct rte_syn_filter *filter, uint16_t *rx_queue);
/**< @internal Get syn filter rule on an Ethernet device */

typedef int (*eth_add_ethertype_filter_t)(struct rte_eth_dev *dev,
			uint16_t index, struct rte_ethertype_filter *filter,
			uint16_t rx_queue);
/**< @internal Setup a new ethertype filter rule on an Ethernet device */

typedef int (*eth_remove_ethertype_filter_t)(struct rte_eth_dev *dev,
			uint16_t index);
/**< @internal Remove an ethertype filter rule on an Ethernet device */

typedef int (*eth_get_ethertype_filter_t)(struct rte_eth_dev *dev,
			uint16_t index, struct rte_ethertype_filter *filter,
			uint16_t *rx_queue);
/**< @internal Get an ethertype filter rule on an Ethernet device */

typedef int (*eth_add_2tuple_filter_t)(struct rte_eth_dev *dev,
			uint16_t index, struct rte_2tuple_filter *filter,
			uint16_t rx_queue);
/**< @internal Setup a new 2tuple filter rule on an Ethernet device */

typedef int (*eth_remove_2tuple_filter_t)(struct rte_eth_dev *dev,
			uint16_t index);
/**< @internal Remove a 2tuple filter rule on an Ethernet device */

typedef int (*eth_get_2tuple_filter_t)(struct rte_eth_dev *dev,
			uint16_t index, struct rte_2tuple_filter *filter,
			uint16_t *rx_queue);
/**< @internal Get a 2tuple filter rule on an Ethernet device */

typedef int (*eth_add_5tuple_filter_t)(struct rte_eth_dev *dev,
			uint16_t index, struct rte_5tuple_filter *filter,
			uint16_t rx_queue);
/**< @internal Setup a new 5tuple filter rule on an Ethernet device */

typedef int (*eth_remove_5tuple_filter_t)(struct rte_eth_dev *dev,
			uint16_t index);
/**< @internal Remove a 5tuple filter rule on an Ethernet device */

typedef int (*eth_get_5tuple_filter_t)(struct rte_eth_dev *dev,
			uint16_t index, struct rte_5tuple_filter *filter,
			uint16_t *rx_queue);
/**< @internal Get a 5tuple filter rule on an Ethernet device */

typedef int (*eth_add_flex_filter_t)(struct rte_eth_dev *dev,
			uint16_t index, struct rte_flex_filter *filter,
			uint16_t rx_queue);
/**< @internal Setup a new flex filter rule on an Ethernet device */

typedef int (*eth_remove_flex_filter_t)(struct rte_eth_dev *dev,
			uint16_t index);
/**< @internal Remove a flex filter rule on an Ethernet device */

typedef int (*eth_get_flex_filter_t)(struct rte_eth_dev *dev,
			uint16_t index, struct rte_flex_filter *filter,
			uint16_t *rx_queue);
/**< @internal Get a flex filter rule on an Ethernet device */



struct rte_eth_dev_sriov {
	uint8_t active;               /**< SRIOV is active with 16, 32 or 64 pools */
	uint8_t nb_q_per_pool;        /**< rx queue number per pool */
	uint16_t def_vmdq_idx;        /**< Default pool num used for PF */
	uint16_t def_pool_q_idx;      /**< Default pool queue start reg index */
};
#define RTE_ETH_DEV_SRIOV(dev)         ((dev)->data->sriov)

#define RTE_ETH_NAME_MAX_LEN (32)


/**
 * The eth device event type for interrupt, and maybe others in the future.
 */
enum rte_eth_event_type {
	RTE_ETH_EVENT_UNKNOWN,  /**< unknown event type */
	RTE_ETH_EVENT_INTR_LSC, /**< lsc interrupt event */
	RTE_ETH_EVENT_MAX       /**< max value of this enum */
};

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETHDEV_H_ */
