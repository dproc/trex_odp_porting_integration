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
#ifndef PAL_UTL_H
#define PAL_UTL_H

#include <stdint.h>
#include <odp.h>

#if ODP_BYTE_ORDER == ODP_LITTLE_ENDIAN
#define PAL_WORDSWAP(x) ((uint16_t)odp_cpu_to_be_16(x))
#else
#define PAL_WORDSWAP(x) ((uint16_t)odp_cpu_to_le_16(x))
#endif

#define PAL_NTOHL(x)     ((uint32_t)odp_be_to_cpu_32(x))
#define PAL_NTOHS(x)     ((uint16_t)odp_be_to_cpu_16(x))
 
#define PAL_HTONS(x)     (PAL_NTOHS(x))
#define PAL_HTONL(x)     (PAL_NTOHL(x))

#define pal_ntohl64(x) ((uint64_t)odp_be_to_cpu_64(x))

#define PAL_NTOHLL(x)     ((uint64_t)odp_be_to_cpu_64(x))


#endif


