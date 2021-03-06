/*
 * Copyright (c) 2019 Nutanix Inc. All rights reserved.
 *
 * Authors: Thanos Makatos <thanos@nutanix.com>
 *          Swapnil Ingle <swapnil.ingle@nutanix.com>
 *          Felipe Franciosi <felipe@nutanix.com>
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of Nutanix nor the names of its contributors may be
 *        used to endorse or promote products derived from this software without
 *        specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 *  DAMAGE.
 *
 */

#ifndef LIB_VFIO_USER_PCI_CAPS_PM_H
#define LIB_VFIO_USER_PCI_CAPS_PM_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Power Management Capabilities Register
 */
struct pc {
    unsigned int vs:3;
    unsigned int pmec:1;
    unsigned int res:1;
    unsigned int dsi:1;
    unsigned int auxc:3;
    unsigned int d1s:1;
    unsigned int d2s:1;
    unsigned int psup:5;
} __attribute__((packed));
_Static_assert(sizeof(struct pc) == 0x2, "bad PC size");

/*
 * Power Management Status and Control Register
 */
struct pmcs {
    union {
        uint16_t raw;
        struct {
            unsigned short ps:2;
            unsigned short res1:1;
            unsigned short nsfrst:1;
            unsigned short res2:4;
            unsigned short pmee:1;
            unsigned short dse:4;
            unsigned short dsc:2;
            unsigned short pmes:1;
        };
    };
} __attribute__((packed));
_Static_assert(sizeof(struct pmcs) == 0x2, "bad PMCS size");

struct pmcap {
    struct cap_hdr hdr;
    struct pc pc;
    struct pmcs pmcs;
    uint8_t pmcsr_bse;
    uint8_t data;
} __attribute__((packed));
_Static_assert(sizeof(struct pmcap) == PCI_PM_SIZEOF, "bad pmcap size");
_Static_assert(offsetof(struct pmcap, hdr) == 0, "bad offset");

#ifdef __cplusplus
}
#endif

#endif /* VFU_CAP_PM_H */

/* ex: set tabstop=4 shiftwidth=4 softtabstop=4 expandtab: */
