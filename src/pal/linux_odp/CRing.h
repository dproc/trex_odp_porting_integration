#ifndef C_RING_H
#define C_RING_H
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


#include <assert.h>
#include <stdint.h>
#include <string>
#include <odp.h>
#include <odp/helper/ring.h>

#define RING_F_SP_ENQ 0x0001 /**< The default enqueue is "single-producer". */
#define RING_F_SC_DEQ 0x0002 /**< The default dequeue is "single-consumer". */

class CRingSp {
public:
    CRingSp(){
        m_ring = NULL;
    }

    bool Create(std::string name, 
                uint16_t cnt,
                int socket_id){
        m_ring=odph_ring_create((char *)name.c_str(), 
                               cnt,
			       RING_F_SP_ENQ | RING_F_SC_DEQ);
        assert(m_ring);
        return(true);
    }

    void Delete(void){
        //no odph_ring_free like function
    }

    int Enqueue(void *obj){
	return odph_ring_sp_enqueue_bulk(m_ring, &obj, 1);
    }

    int Dequeue(void * & obj){
	return odph_ring_mc_dequeue_bulk(m_ring, (void**)&obj, 1);
    }

    bool isFull(void){
	return (odph_ring_full(m_ring)?true:false);
    }

    bool isEmpty(void){
	return (odph_ring_empty(m_ring)?true:false);
    }

private:
    odph_ring_t* m_ring;
};

template <class T>
class CTRingSp : public CRingSp {
public:
    int Enqueue(T *obj){
        return (CRingSp::Enqueue((void*)obj));
    }

    int Dequeue(T * & obj){
        return (CRingSp::Dequeue(*((void **)&obj)));
    }
};

#endif

