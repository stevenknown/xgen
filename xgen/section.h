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
#ifndef __SECTION_H__
#define __SECTION_H__

namespace xgen {

//Describing program section.e.g. code section, data section.
#define SECT_id(s)              (s)->id
#define SECT_var(s)             (s)->sect_var
#define SECT_size(s)            (s)->size
#define SECT_var_list(s)        (s)->var_list
#define SECT_var2vdesc_map(s)   (s)->var2vdesc_map
class Section {
public:
    INT id;
    xoc::VAR * sect_var;
    ULONGLONG size;
    VarElist var_list;
    TMap<xoc::VAR const*, VarDesc*> var2vdesc_map;

public:
    Section()
    {
        id = -1;
        sect_var = NULL;
        size = 0;
    }
    virtual ~Section() {}

    void clean()
    {
        //Clean xoc::VAR, VDDESC, etc func-unit related info
        //which should be initialized to new func unit.
        SECT_var_list(this).clean();
        SECT_size(this) = 0;
        SECT_var2vdesc_map(this).clean();
    }

    virtual void dump(CG const* cg);
};


class StackSection : public Section {
public:
    virtual ~StackSection() {}
    virtual void dump(CG const* cg);
};

} //namespace xgen
#endif
