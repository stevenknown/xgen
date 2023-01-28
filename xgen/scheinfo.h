/*@
Copyright (c) 2013-2021, Su Zhenyu steven.known@gmail.com

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Su Zhenyu nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

author: Su Zhenyu
@*/
#ifndef __SCHE_INFO_H__
#define __SCHE_INFO_H__

namespace xgen {

//Corresponding to the information of target machine operation.
#define ORSI_last_result_avail_cyc(oi) ((oi)->last_result_avail_cyc)
#define ORSI_first_result_avail_cyc(oi) ((oi)->first_result_avail_cyc)
#define ORSI_reg_result_avail_cyc(oi, i) ((oi)->reg_result_cyc_buf[(i)])
#define ORSI_mem_result_avail_cyc(oi, i) ((oi)->mem_result_cyc_buf[(i)])
#define ORSI_reg_result_cyc_buf(oi) ((oi)->reg_result_cyc_buf)
#define ORSI_mem_result_cyc_buf(oi) ((oi)->mem_result_cyc_buf)
typedef struct tagORScheInfo {
public:
    UINT * reg_result_cyc_buf; //record each of register result available cycle
    UINT * mem_result_cyc_buf; //record each of memory result available cycle
    UINT last_result_avail_cyc; //record last result available cycle
    UINT first_result_avail_cyc; //record first result available cycle

    void init()
    {
        reg_result_cyc_buf = nullptr;
        mem_result_cyc_buf = nullptr;
        last_result_avail_cyc = 0;
        first_result_avail_cyc = 0;
    }
} ORScheInfo;

} //namespace xgen
#endif
