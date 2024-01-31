/* Operating system specific defines to be used when targeting GCC for
   hosting on Windows CE, using GNU tools and the Windows32 API Library.
   Copyright (C) 2006
   Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

#undef EXTRA_OS_CPP_BUILTINS
#define EXTRA_OS_CPP_BUILTINS()					\
  do								\
    {								\
      builtin_define ("__CEGCC_VERSION__");			\
      builtin_define ("__COREDLL__");				\
      builtin_define ("__MINGW32__");				\
      builtin_define ("__MINGW32CE__");				\
      builtin_define ("_WIN32");				\
      builtin_define_std ("WIN32");				\
      builtin_define_std ("WINNT");				\
    }								\
  while (0)


#undef TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS()				\
  do								\
  {								\
      builtin_define ("_X86_=1");                               \
      \
      /* We currently define UNDER_CE to a non-value, as it seems \
         MSVC2005 does the same.  */ \
      builtin_define_std ("UNDER_CE");				\
      builtin_define ("_UNICODE");				\
      builtin_define_std ("UNICODE");				\
      \
      /* Unlike original CeGCC, we only redefine stdcall to cdecl here. \
         This is what VS2008 does in windef.h when compiling for WinCE. \
         We correctly define fastcall and thiscall. */ \
      builtin_define ("__stdcall=__attribute__((__cdecl__))");	\
      builtin_define ("__fastcall=__attribute__((__fastcall__))");	\
	    builtin_define ("__thiscall=__attribute__((__thiscall__))");	\
      builtin_define ("__cdecl=__attribute__((__cdecl__))");		\
      \
      if (!flag_iso)							\
        {								\
          builtin_define ("_stdcall=__attribute__((__cdecl__))");	\
          builtin_define ("_fastcall=__attribute__((__fastcall__))");	\
          builtin_define ("_thiscall=__attribute__((__thiscall__))");	\
          builtin_define ("_cdecl=__attribute__((__cdecl__))");	\
        }								\
      /* Even though linkonce works with static libs, this is needed 	\
          to compare typeinfo symbols across dll boundaries.  */	\
	builtin_define ("__GXX_MERGED_TYPEINFO_NAMES=0");		\
	builtin_define ("__GXX_TYPEINFO_EQUALITY_INLINE=0");		\
	EXTRA_OS_CPP_BUILTINS ();					\
  }									\
  while (0)


#undef CPP_SPEC
#define CPP_SPEC "%{posix:-D_POSIX_SOURCE} %{mthreads:-D_MT}"

#undef LIB_SPEC
#define LIB_SPEC "%{pg:-lgmon} -lcoredll"

/* Include in the mingw32 libraries with libgcc */
#undef LINK_SPEC
#define LINK_SPEC "%{shared|mdll: --shared} \
  %{static:-Bstatic} %{!static:-Bdynamic} \
  %{shared|mdll: -e DllMainCRTStartup}"

/* Include in the mingw32 libraries with libgcc */
#undef LIBGCC_SPEC
#define LIBGCC_SPEC \
  "%{mthreads:-lmingwthrd} -lmingw32 -lgcc -lceoldname -lmingwex -lcoredll"

#undef STARTFILE_SPEC
#define STARTFILE_SPEC "%{shared|mdll:dllcrt3%O%s} \
  %{!shared:%{!mdll:crt3%O%s}} %{pg:gcrt3%O%s}"

/* Override startfile prefix defaults.  */
#ifndef STANDARD_STARTFILE_PREFIX_1
#define STANDARD_STARTFILE_PREFIX_1 "/mingw/lib/"
#endif
#ifndef STANDARD_STARTFILE_PREFIX_2
#define STANDARD_STARTFILE_PREFIX_2 ""
#endif

/* Output STRING, a string representing a filename, to FILE.
   We canonicalize it to be in Unix format (backslashes are replaced
   forward slashes.  */
#undef OUTPUT_QUOTED_STRING
#define OUTPUT_QUOTED_STRING(FILE, STRING)               \
do {						         \
  const char *_string = (const char *) (STRING);	 \
  char c;					         \
						         \
  putc ('\"', (FILE));				         \
						         \
  while ((c = *_string++) != 0)			         \
    {						         \
      if (c == '\\')				         \
	c = '/';				         \
						         \
      if (ISPRINT (c))                                   \
        {                                                \
          if (c == '\"')			         \
	    putc ('\\', (FILE));		         \
          putc (c, (FILE));			         \
        }                                                \
      else                                               \
        fprintf ((FILE), "\\%03o", (unsigned char) c);	 \
    }						         \
						         \
  putc ('\"', (FILE));					 \
} while (0)
