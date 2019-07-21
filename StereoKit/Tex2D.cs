﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace StereoKit
{
    public class Tex2D
    {
        #region Imports
        [DllImport(Util.DllName)]
        static extern IntPtr tex2d_create();
        [DllImport(Util.DllName, CharSet = CharSet.Ansi)]
        static extern IntPtr tex2d_create_file(string file);
        [DllImport(Util.DllName)]
        static extern void tex2d_destroy(IntPtr tex);
        #endregion

        internal IntPtr _texInst;
        public Tex2D()
        {
            _texInst = tex2d_create();
        }
        public Tex2D(string file)
        {
            _texInst = tex2d_create_file(file);
        }
        ~Tex2D()
        {
            if (_texInst != IntPtr.Zero)
                tex2d_destroy(_texInst);
        }
    }
}
