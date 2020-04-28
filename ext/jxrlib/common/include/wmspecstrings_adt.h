//*@@@+++@@@@******************************************************************
//
// Copyright © Microsoft Corp.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// • Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// • Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//*@@@---@@@@******************************************************************

#pragma once
/*************************************************************************
* DEFINITIONS OF NEW TYPES
*************************************************************************/
#if !defined(__midl)
#define __DOLLAR_compname_props \
        __type_has_adt_prop(compname,nullterminated) \
        __type_has_adt_prop(compname,valid_schars) \
        __type_has_adt_prop(compname,correct_len) \
        __nullterminated
#if defined(UNICODE) || defined(_UNICODE)
#define  __DOLLAR_TCHAR unsigned short
#else
#define  __DOLLAR_TCHAR char
#endif
typedef __DOLLAR_compname_props char* ValidCompNameA;
typedef __DOLLAR_compname_props unsigned short* ValidCompNameW;
typedef __DOLLAR_compname_props const unsigned short* ConstValidCompNameW;
typedef __DOLLAR_compname_props  __DOLLAR_TCHAR* SAL_ValidCompNameT;
typedef __DOLLAR_compname_props const  __DOLLAR_TCHAR* SAL_ConstValidCompNameT;
#undef __DOLLAR_compname_props
#undef __DOLLAR_TCHAR
#endif

/*************************************************************************
* DEFINITIONS OF INLINE FUNCTIONS FOR CASTING TO THE NEW TYPES : USER
*************************************************************************/
#if (_MSC_VER >= 1000) && !defined(__midl) && defined(_PREFAST_)
#ifdef  __cplusplus
extern "C" {
#endif
void __inline __nothrow __SAL_ValidCompNameA(__out_has_type_adt_props(ValidCompNameA) const void *expr) { expr;}
void __inline __nothrow __SAL_ValidCompNameW(__out_has_type_adt_props(ValidCompNameW) const void *expr) { expr;}
#ifdef  __cplusplus
}
#endif
#define __assume_ValidCompNameA(expr) __SAL_ValidCompNameA(expr)
#define __assume_ValidCompNameW(expr) __SAL_ValidCompNameW(expr)
#else
#define __assume_ValidCompNameA(expr)
#define __assume_ValidCompNameW(expr)
#endif

