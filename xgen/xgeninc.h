/*@
Copyright (c) 2013-2014, Su Zhenyu steven.known@gmail.com

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
#ifndef __XGEN_INC_H__
#define __XGEN_INC_H__

//Common included files
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../opt/cominc.h"
#include "../opt/comopt.h"
#include "xgenoption.h"
#include "equortype.h"
#include "scheinfo.h"
#include "targ_interface.h"
#include "reg.h"
#include "regfile.h"
#include "regfilegroup.h"
#include "sr.h"
#include "srdesc.h"
#include "or.h"
#include "issue_package.h"
#include "or_bb.h"
#include "or_util.h"
#include "or_cfg.h"
#include "or_ddg.h"
#include "or_sim.h"
#include "or_lis.h"
#include "or_lra.h"
#include "or_gra.h"
#include "ramgr.h"
#include "asminfo.h"
#include "section.h"
#include "emit_asm.h"
#include "builtin.h"
#include "intrinsic_mgr.h"
#include "cg_mgr.h"
#include "ir2or.h"
#include "argdesc.h"
#include "cg.h"
#include "parallel_part_mgr.h"
#include "instruction_partition.h"
#include "cl_region_mgr.h"
#include "cl_dbg.h"

using namespace xgen;

#include "targ_hook.h"
#endif
