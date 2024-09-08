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
#ifndef __ARM_H__
#define __ARM_H__

#include "../arm/arm_targinfo.h"
#include "../arm/arm_sr.h"
#include "../arm/arm_or.h"
#include "../arm/arm_ramgr.h"
#include "../arm/arm_lra.h"
#include "../arm/arm_sim.h"
#include "../arm/arm_lis.h"
#include "../arm/arm_passmgr.h"
#include "../arm/arm_region.h"
#include "../arm/arm_region_mgr.h"
#include "../arm/armir2or.h"
#include "../arm/armasmprinter.h"
#include "../arm/arm_cg.h"
#include "../arm/arm_cgmgr.h"
#include "../arm/arm_var.h"
#include "../arm/arm_dumgr.h"
#include "../arm/arm_scalar_opt.h"
#include "../arm/arm_simp.h"
#include "../arm/arm_refine.h"
#include "../arm/arm_ddg.h"

#if defined REF_TARGMACH_INFO || defined FOR_IP
  #include "../arm/arm_irmgr.h"
  #include "../arm/arm_ir_dump.h"
  #include "../arm/arm_linear_scan.h"
#endif

#if defined FOR_IP
  #include "../arm/arm_derivative.h"
#endif

#endif
