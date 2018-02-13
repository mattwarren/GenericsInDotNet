// ==++==
// 
//   
//    Copyright (c) 2002 Microsoft Corporation.  All rights reserved.
//   
//    The use and distribution terms for this software are contained in the file
//    named license.txt, which can be found in the root of this distribution.
//    By using this software in any fashion, you are agreeing to be bound by the
//    terms of this license.
//   
//    This file contains modifications of the base SSCLI software to support generic
//    type definitions and generic methods,  THese modifications are for research
//    purposes.  They do not commit Microsoft to the future support of these or
//    any similar changes to the SSCLI or the .NET product.  -- 31st October, 2002.
//   
//    You must not remove this notice, or any other, from this software.
//   
// 
// ==--==
// ===========================================================================
// File: EnC.CPP
// 

// Handles EditAndContinue support in the EE
// ===========================================================================

#include "common.h"
#include "enc.h"
#include "utilcode.h"
#include "wsperf.h"
#include "dbginterface.h"
#include "ndirect.h"
#include "eeconfig.h"
#include "excep.h"

