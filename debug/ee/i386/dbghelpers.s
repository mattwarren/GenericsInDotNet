// ==++==
// 
//  
//   Copyright (c) 2002 Microsoft Corporation.  All rights reserved.
//  
//   The use and distribution terms for this software are contained in the file
//   named license.txt, which can be found in the root of this distribution.
//   By using this software in any fashion, you are agreeing to be bound by the
//   terms of this license.
//  
//   You must not remove this notice, or any other, from this software.
//  
// 
// ==--== 
//
//  *** NOTE:  If you make changes to this file, propagate the changes to
//             dbghelpers.asm in this directory                            

	.intel_syntax
	.arch i586

// a handy macro for declaring a function
#define ASMFUNC(n)                \
        .global n               ; \
        .func n                 ; \
n:                              ;

//
// This is the method that we hijack a thread running managed code. It calls
// FuncEvalHijackWorker, which actually performs the func eval, then jumps to 
// the patch address so we can complete the cleanup.
//
// Note: the parameter is passed in eax - see Debugger::FuncEvalSetup for
//       details
//
ASMFUNC(FuncEvalHijack)
        push %eax       // the ptr to the DebuggerEval
        call FuncEvalHijackWorker
        jmp  %eax       // return is the patch addresss to jmp to
.endfunc

	.end








