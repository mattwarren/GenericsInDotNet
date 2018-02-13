////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 1999-2000 Microsoft Corporation.  All Rights Reserved.
// Microsoft Confidential.
////////////////////////////////////////////////////////////////////////////////
//
// TyParInfo is an class that represents a formal type parameter to a class or method. 
//
// Author: akenn
// Date: Jun 01
//
namespace System.Reflection {
    
    using System;
    using System.Reflection.Cache;
    using System.Runtime.CompilerServices;
    
    public class TyParInfo
    {
        protected String NameImpl;                  // The name of the parameter
        protected Type[] BoundsImpl;	            // Type bounds on the parameter

        static private Type TyParInfoType = typeof(System.Reflection.TyParInfo);
        
        // Prevent users from creating empty parameters.
        protected TyParInfo() {}
        
        // This constructor is called by the runtime and also by users (for Reflection Emit)
        public TyParInfo(String name, Type[] bounds) 
        {
            NameImpl = name;
            BoundsImpl = bounds;
        }
    
        public String Name {
            get {return NameImpl;}
        }
        
        public Type[] Bounds {
            get {return BoundsImpl;}
        }
    }
}
