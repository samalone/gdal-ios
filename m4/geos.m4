dnl ***************************************************************************
dnl $Id$
dnl
dnl Project:  GDAL
dnl Purpose:  Test for GEOS library presence
dnl Author:   Andrey Kiselev, dron@ak4719.spb.edu
dnl	      Ideas borrowed from the old GDAL test and from the macro
dnl           supplied with GEOS package.
dnl
dnl ***************************************************************************
dnl Copyright (c) 2006, Andrey Kiselev
dnl
dnl Permission is hereby granted, free of charge, to any person obtaining a
dnl copy of this software and associated documentation files (the "Software"),
dnl to deal in the Software without restriction, including without limitation
dnl the rights to use, copy, modify, merge, publish, distribute, sublicense,
dnl and/or sell copies of the Software, and to permit persons to whom the
dnl Software is furnished to do so, subject to the following conditions:
dnl
dnl The above copyright notice and this permission notice shall be included
dnl in all copies or substantial portions of the Software.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
dnl OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
dnl FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
dnl THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
dnl LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
dnl FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
dnl DEALINGS IN THE SOFTWARE.
dnl ***************************************************************************

dnl
dnl GEOS_INIT (MINIMUM_VERSION)
dnl
dnl Test for GEOS: define HAVE_GEOS, GEOS_LIBS, GEOS_CFLAGS, GEOS_VERSION
dnl 
dnl Call as GEOS_INIT or GEOS_INIT(minimum version) in configure.in. Test
dnl HAVE_GEOS (yes|no) afterwards. If yes, all other vars above can be 
dnl used in program.
dnl
AC_DEFUN([GEOS_INIT],[
  AC_SUBST(GEOS_LIBS)
  AC_SUBST(GEOS_CFLAGS)
  AC_SUBST(HAVE_GEOS) 
  AC_SUBST(GEOS_VERSION)

  AC_ARG_WITH(geos,
    AS_HELP_STRING([--with-geos[=ARG]],
                   [Include GEOS support (ARG=yes, no or geos-config path)]),,)

  if test x"$with_geos" = x"no" ; then

    AC_MSG_RESULT([GEOS support disabled])
    GEOS_CONFIG=no

  elif test x"$with_geos" = x"yes" -o x"$with_geos" = x"" ; then

    AC_PATH_PROG(GEOS_CONFIG, geos-config, no)

  else

   if test "`basename xx/$with_geos`" = "geos-config" ; then
      AC_MSG_NOTICE([GEOS enabled with provided geos-config])
      GEOS_CONFIG="$with_geos"
    else
      AC_MSG_ERROR([--with-geos should have yes, no or a path to geos-config])
    fi

  fi

  if test x"$GEOS_CONFIG" != x"no" ; then

    min_geos_version=ifelse([$1], ,1.0.0,$1)

    AC_MSG_CHECKING(for GEOS version >= $min_geos_version)

    geos_major_version=`$GEOS_CONFIG --version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\1/'`
    geos_minor_version=`$GEOS_CONFIG --version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\2/'`
    geos_micro_version=`$GEOS_CONFIG --version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\3/'`

    req_major=`echo $min_geos_version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\1/'`
    req_minor=`echo $min_geos_version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\2/'`
    req_micro=`echo $min_geos_version | \
       sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\3/'`
      

    version_ok="no"
    if test $req_major -le $geos_major_version; then
       if test $req_minor -le $geos_minor_version; then
          if test $req_micro -le $geos_micro_version; then
             version_ok="yes"
          fi
       fi
    fi

    if test $version_ok = "no"; then
       HAVE_GEOS="no"
       AC_MSG_RESULT(no)
       AC_MSG_ERROR([geos-config reports version ${geos_major_version}.${geos_minor_version}.${geos_micro_version}, need at least $min_geos_version or configure --without-geos])
    else
      
      AC_MSG_RESULT(yes)
      HAVE_GEOS="yes"
      GEOS_LIBS="`${GEOS_CONFIG} --libs`"
      GEOS_CFLAGS="`${GEOS_CONFIG} --cflags`"
      GEOS_VERSION="`${GEOS_CONFIG} --version`"

      AC_MSG_CHECKING(for geos::Coordinate::getNull() in -lgeos)
      ax_save_LIBS="${LIBS}"
      LIBS=${GEOS_LIBS}
      ax_save_CFLAGS="${CFLAGS}"
      CFLAGS="${GEOS_CFLAGS}"
      AC_LANG_PUSH(C++)
      AC_TRY_COMPILE([#include "geos/geom.h"], [geos::Coordinate::getNull()], HAVE_GEOS=yes, HAVE_GEOS=no)
      AC_LANG_POP()
      CFLAGS="${ax_save_CFLAGS}"
      LIBS="${ax_save_LIBS}"
      AC_MSG_RESULT($HAVE_GEOS)

    fi

  fi
])

