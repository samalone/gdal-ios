/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Standalone shared library that can be LD_PRELOAD'ed as an overload of
 *           libc to enable VSI Virtual FILE API to be used with binaries using
 *           regular libc for I/O.
 * Author:   Even Rouault <even dot rouault at mines dash paris.org>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines dash paris dot org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

// WARNING: Linux glibc ONLY
// Might work with some adaptations (mainly around 64bit symbols) on other Unix systems

// Compile:
// g++ -Wall -fPIC port/vsipreload.cpp -shared -o vsipreload.so -Iport -L. -L.libs -lgdal

// Run:
// LD_PRELOAD=vsipreload.so ....
// e.g: 
// LD_PRELOAD=vsipreload.so gdalinfo /vsicurl/http://download.osgeo.org/gdal/data/ecw/spif83.ecw
// LD_PRELOAD=vsipreload.so gdalinfo 'HDF4_EOS:EOS_GRID:"/vsicurl/http://download.osgeo.org/gdal/data/hdf4/MOD09Q1G_EVI.A2006233.h07v03.005.2008338190308.hdf":MODIS_NACP_EVI:MODIS_EVI'
// LD_PRELOAD=vsipreload.so ogrinfo /vsicurl/http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/testavc -ro
// even non GDAL binaries :
// LD_PRELOAD=vsipreload.so h5dump -d /x /vsicurl/http://download.osgeo.org/gdal/data/netcdf/utm-big-chunks.nc
// LD_PRELOAD=vsipreload.so sqlite3 /vsicurl/http://download.osgeo.org/gdal/data/sqlite3/polygon.db "select * from polygon limit 10"

#define _GNU_SOURCE 1
#define _LARGEFILE64_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <dlfcn.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <set>
#include <map>
#include <string>
#include "cpl_vsi.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

static int DEBUG_VSIPRELOAD = 0;
static int DEBUG_VSIPRELOAD_ONLY_VSIL = 1;
#define DEBUG_OUTPUT_READ 0

#define DECLARE_SYMBOL(x, retType, args) \
    typedef retType (*fn ## x ## Type)args;\
    static fn ## x ## Type pfn ## x = NULL

DECLARE_SYMBOL(fopen, FILE*, (const char *path, const char *mode));
DECLARE_SYMBOL(fopen64, FILE*, (const char *path, const char *mode));
DECLARE_SYMBOL(fread, size_t, (void *ptr, size_t size, size_t nmemb, FILE *stream));
DECLARE_SYMBOL(fwrite, size_t, (const void *ptr, size_t size, size_t nmemb, FILE *stream));
DECLARE_SYMBOL(fclose, int, (FILE *stream));
DECLARE_SYMBOL(__xstat, int, (int ver, const char *path, struct stat *buf));
DECLARE_SYMBOL(__xstat64, int, (int ver, const char *path, struct stat64 *buf));
DECLARE_SYMBOL(fseeko64, int, (FILE *stream, off64_t off, int whence));
DECLARE_SYMBOL(fseek, int, (FILE *stream, off_t off, int whence));
DECLARE_SYMBOL(ftello64, off64_t, (FILE *stream));
DECLARE_SYMBOL(ftell, off_t, (FILE *stream));
DECLARE_SYMBOL(feof, int, (FILE *stream));
DECLARE_SYMBOL(fflush, int, (FILE *stream));
DECLARE_SYMBOL(fgetpos, int, (FILE *stream, fpos_t *pos));
DECLARE_SYMBOL(fsetpos, int, (FILE *stream, fpos_t *pos));
DECLARE_SYMBOL(fileno, int, (FILE *stream));
DECLARE_SYMBOL(ferror, int, (FILE *stream));

DECLARE_SYMBOL(fdopen, FILE*, (int fd, const char *mode));
DECLARE_SYMBOL(freopen, FILE*, (const char *path, const char *mode, FILE *stream));

DECLARE_SYMBOL(open, int, (const char *path, int flags, mode_t mode));
DECLARE_SYMBOL(open64, int, (const char *path, int flags, mode_t mode));
//DECLARE_SYMBOL(creat, int, (const char *path, mode_t mode));
DECLARE_SYMBOL(close, int, (int fd));
DECLARE_SYMBOL(read, ssize_t, (int fd, void *buf, size_t count));
DECLARE_SYMBOL(write, ssize_t, (int fd, const void *buf, size_t count));
DECLARE_SYMBOL(fsync, int, (int fd));
DECLARE_SYMBOL(fdatasync, int, (int fd));
DECLARE_SYMBOL(__fxstat, int, (int ver, int fd, struct stat *__stat_buf));
DECLARE_SYMBOL(__fxstat64, int, (int ver, int fd, struct stat64 *__stat_buf));
DECLARE_SYMBOL(lseek, off_t, (int fd, off_t off, int whence));
DECLARE_SYMBOL(lseek64, off64_t , (int fd, off64_t off, int whence));

DECLARE_SYMBOL(truncate, int, (const char *path, off_t length));
DECLARE_SYMBOL(ftruncate, int, (int fd, off_t length));

DECLARE_SYMBOL(opendir, DIR* , (const char *name));

static void* hMutex = NULL;

std::set<VSILFILE*> oSetFiles;
std::map<int, VSILFILE*> oMapfdToVSI;
std::map<VSILFILE*, int> oMapVSITofd;
std::map<VSILFILE*, std::string> oMapVSIToString;

/************************************************************************/
/*                             myinit()                                 */
/************************************************************************/

#define LOAD_SYMBOL(x) \
    pfn ## x = (fn ## x ## Type) dlsym(RTLD_NEXT, #x); \
    assert(pfn ## x)

static void myinit(void)
{
    CPLMutexHolderD(&hMutex);

    if( pfnfopen64 != NULL ) return;
    DEBUG_VSIPRELOAD = getenv("DEBUG_VSIPRELOAD") != NULL;
    LOAD_SYMBOL(fopen);
    LOAD_SYMBOL(fopen64);
    LOAD_SYMBOL(fread);
    LOAD_SYMBOL(fwrite);
    LOAD_SYMBOL(fclose);
    LOAD_SYMBOL(fseeko64);
    LOAD_SYMBOL(fseek);
    LOAD_SYMBOL(__xstat);
    LOAD_SYMBOL(__xstat64);
    LOAD_SYMBOL(ftello64);
    LOAD_SYMBOL(ftell);
    LOAD_SYMBOL(feof);
    LOAD_SYMBOL(fflush);
    LOAD_SYMBOL(fgetpos);
    LOAD_SYMBOL(fsetpos);
    LOAD_SYMBOL(fileno);
    LOAD_SYMBOL(ferror);

    LOAD_SYMBOL(fdopen);
    LOAD_SYMBOL(freopen);

    LOAD_SYMBOL(open);
    LOAD_SYMBOL(open64);
    //LOAD_SYMBOL(creat);
    LOAD_SYMBOL(close);
    LOAD_SYMBOL(read);
    LOAD_SYMBOL(write);
    LOAD_SYMBOL(fsync);
    LOAD_SYMBOL(fdatasync);
    LOAD_SYMBOL(__fxstat);
    LOAD_SYMBOL(__fxstat64);
    LOAD_SYMBOL(lseek);
    LOAD_SYMBOL(lseek64);

    LOAD_SYMBOL(truncate);
    LOAD_SYMBOL(ftruncate);

    LOAD_SYMBOL(opendir);
}

/************************************************************************/
/*                          getVSILFILE()                               */
/************************************************************************/

static VSILFILE* getVSILFILE(FILE* stream)
{
    VSILFILE* ret;
    CPLMutexHolderD(&hMutex);
    std::set<VSILFILE*>::iterator oIter = oSetFiles.find((VSILFILE*)stream);
    if( oIter != oSetFiles.end() )
        ret = *oIter;
    else
        ret = NULL;
    return ret;
}

/************************************************************************/
/*                          getVSILFILE()                               */
/************************************************************************/

static VSILFILE* getVSILFILE(int fd)
{
    VSILFILE* ret;
    CPLMutexHolderD(&hMutex);
    std::map<int, VSILFILE*>::iterator oIter = oMapfdToVSI.find(fd);
    if( oIter != oMapfdToVSI.end() )
        ret = oIter->second;
    else
        ret = NULL;
    return ret;
}

/************************************************************************/
/*                        VSIFSeekLHelper()                             */
/************************************************************************/

static int VSIFSeekLHelper(VSILFILE* fpVSIL, off64_t off, int whence)
{
    if( off < 0 && whence == SEEK_CUR )
    {
        return VSIFSeekL(fpVSIL, VSIFTellL(fpVSIL) + off, SEEK_SET);
    }
    else if( off < 0 && whence == SEEK_END )
    {
        VSIFSeekL(fpVSIL, 0, SEEK_END);
        return VSIFSeekL(fpVSIL, VSIFTellL(fpVSIL) + off, SEEK_SET);
    }
    else
        return VSIFSeekL(fpVSIL, off, whence);
}

/************************************************************************/
/*                          VSIFopenHelper()                            */
/************************************************************************/

static VSILFILE* VSIFfopenHelper(const char *path, const char *mode)
{
    VSILFILE* fpVSIL = VSIFOpenL(path, mode);
    if( fpVSIL != NULL )
    {
        CPLMutexHolderD(&hMutex);
        oSetFiles.insert(fpVSIL);
        oMapVSIToString[fpVSIL] = path;
    }
    return fpVSIL;
}

/************************************************************************/
/*                         getfdFromVSILFILE()                          */
/************************************************************************/

static int getfdFromVSILFILE(VSILFILE* fpVSIL)
{
    CPLMutexHolderD(&hMutex);

    int fd;
    std::map<VSILFILE*, int>::iterator oIter = oMapVSITofd.find(fpVSIL);
    if( oIter != oMapVSITofd.end() )
        fd = oIter->second;
    else
    {
        fd = open("/dev/zero", O_RDONLY);
        assert(fd >= 0);
        oMapVSITofd[fpVSIL] = fd;
        oMapfdToVSI[fd] = fpVSIL;
    }
    return fd;
}

/************************************************************************/
/*                          VSIFopenHelper()                            */
/************************************************************************/

static int VSIFopenHelper(const char *path, int flags)
{
    const char* pszMode = "rb";
    if ((flags & 3) == O_RDONLY) 
        pszMode = "rb";
    else if ((flags & 3) == O_WRONLY) 
    {
        if( flags & O_APPEND )
            pszMode = "ab";
        else
            pszMode = "wb";
    }
    else
    {
        if( flags & O_APPEND )
            pszMode = "ab+";
        else
            pszMode = "rb+";
    }
    VSILFILE* fpVSIL = VSIFfopenHelper(path, pszMode );
    int fd;
    if( fpVSIL != NULL )
    {
        if( flags & O_TRUNC )
        {
            VSIFTruncateL(fpVSIL, 0);
            VSIFSeekL(fpVSIL, 0, SEEK_SET);
        }
        fd = getfdFromVSILFILE(fpVSIL);
    }
    else
        fd = -1;
    return fd;
}

/************************************************************************/
/*                    GET_DEBUG_VSIPRELOAD_COND()                             */
/************************************************************************/

static int GET_DEBUG_VSIPRELOAD_COND(const char* path)
{
    return (DEBUG_VSIPRELOAD && (!DEBUG_VSIPRELOAD_ONLY_VSIL || strncmp(path, "/vsi", 4) == 0) );
}

static int GET_DEBUG_VSIPRELOAD_COND(VSILFILE* fpVSIL)
{
    return (DEBUG_VSIPRELOAD && (!DEBUG_VSIPRELOAD_ONLY_VSIL || fpVSIL != NULL));
}

/************************************************************************/
/*                         copyBuf64ToBuf()                             */
/************************************************************************/

static void copyBuf64ToBuf(struct stat64 *buf64, struct stat *buf)
{
    buf->st_dev = buf64->st_dev;
    buf->st_ino = buf64->st_ino;
    buf->st_mode = buf64->st_mode;
    buf->st_nlink = buf64->st_nlink;
    buf->st_uid = buf64->st_uid;
    buf->st_gid = buf64->st_gid;
    buf->st_rdev = buf64->st_rdev;
    buf->st_size = buf64->st_size;
    buf->st_blksize = buf64->st_blksize;
    buf->st_blocks = buf64->st_blocks;
    buf->st_atime = buf64->st_atime;
    buf->st_mtime = buf64->st_mtime;
    buf->st_ctime = buf64->st_ctime;
}

/************************************************************************/
/*                             fopen()                                  */
/************************************************************************/

FILE *fopen(const char *path, const char *mode)
{
    myinit();
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(path);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fopen(%s, %s)\n", path, mode);
    FILE* ret;
    if( strncmp(path, "/vsi", 4) == 0 )
        ret = (FILE*) VSIFfopenHelper(path, mode);
    else
        ret = pfnfopen(path, mode);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fopen() = %p\n", ret);
    return ret;
}

/************************************************************************/
/*                            fopen64()                                 */
/************************************************************************/

FILE *fopen64(const char *path, const char *mode)
{
    myinit();
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(path);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fopen64(%s, %s)\n", path, mode);
    FILE* ret;
    if( strncmp(path, "/vsi", 4) == 0 )
        ret = (FILE*) VSIFfopenHelper(path, mode);
    else
        ret = pfnfopen64(path, mode);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fopen64() = %p\n", ret);
    return ret;
}

/************************************************************************/
/*                            fread()                                   */
/************************************************************************/

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(stream);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fread(stream=%p,size=%d,nmemb=%d)\n",
        stream, (int)size, (int)nmemb);
    size_t ret;
    if( fpVSIL )
        ret = VSIFReadL(ptr, size, nmemb, fpVSIL);
    else
        ret = pfnfread(ptr, size, nmemb, stream);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fread(stream=%p,size=%d,nmemb=%d) -> %d\n",
        stream, (int)size, (int)nmemb, (int)ret);
    return ret;
}

/************************************************************************/
/*                            fwrite()                                  */
/************************************************************************/

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(stream);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fwrite(stream=%p,size=%d,nmemb=%d)\n",
        stream, (int)size, (int)nmemb);
    size_t ret;
    if( fpVSIL != NULL )
        ret = VSIFWriteL(ptr, size, nmemb, fpVSIL);
    else
        ret = pfnfwrite(ptr, size, nmemb, stream);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fwrite(stream=%p,size=%d,nmemb=%d) -> %d\n",
        stream, (int)size, (int)nmemb, (int)ret);
    return ret;
}

/************************************************************************/
/*                            fclose()                                  */
/************************************************************************/

int fclose(FILE *stream)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(stream);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fclose(stream=%p)\n", stream);
    if( fpVSIL != NULL )
    {
        CPLMutexHolderD(&hMutex);

        int ret = VSIFCloseL(fpVSIL);
        oMapVSIToString.erase(fpVSIL);
        oSetFiles.erase(fpVSIL);

        std::map<VSILFILE*, int>::iterator oIter = oMapVSITofd.find(fpVSIL);
        if( oIter != oMapVSITofd.end() )
        {
            int fd = oIter->second;
            pfnclose(fd);
            oMapVSITofd.erase(oIter);
            oMapfdToVSI.erase(fd);
        }

        return ret;
    }
    else
        return pfnfclose(stream);
}

/************************************************************************/
/*                            __xstat()                                 */
/************************************************************************/

int __xstat(int ver, const char *path, struct stat *buf)
{
    myinit();
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(path);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "__xstat(%s)\n", path);
    if( strncmp(path, "/vsi", 4) == 0 )
    {
        VSIStatBufL sStatBufL;
        int ret = VSIStatL(path, &sStatBufL);
        if( ret == 0 )
        {
            if (DEBUG_VSIPRELOAD_COND) fprintf(stderr,
                "__xstat(%s) ret = 0, mode = %d, size=%d\n",
                path, sStatBufL.st_mode, (int)sStatBufL.st_size);
            copyBuf64ToBuf(&sStatBufL, buf);
        }
        return ret;
    }
    else
    {
        int ret = pfn__xstat(ver, path, buf);
        if( ret == 0 )
        {
            if (DEBUG_VSIPRELOAD_COND) fprintf(stderr,
                "__xstat ret = 0, mode = %d\n", buf->st_mode);
        }
        return ret;
    }
}

/************************************************************************/
/*                           __xstat64()                                */
/************************************************************************/

int __xstat64(int ver, const char *path, struct stat64 *buf)
{
    myinit();
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(path);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "__xstat64(%s)\n", path);
    if( strncmp(path, "/vsi", 4) == 0 )
    {
        int ret = VSIStatL(path, buf);
        if( ret == 0 )
        {
            if (DEBUG_VSIPRELOAD_COND) fprintf(stderr,
                "__xstat64(%s) ret = 0, mode = %d, size = %d\n",
                path, buf->st_mode, (int)buf->st_size);
        }
        return ret;
    }
    else
        return pfn__xstat64(ver, path, buf);
}

/************************************************************************/
/*                           fseeko64()                                 */
/************************************************************************/

int fseeko64 (FILE *stream, off64_t off, int whence)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(stream);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fseeko64(stream=%p, off=%d, whence=%d)\n",
        stream, (int)off, whence);
    if( fpVSIL != NULL )
        return VSIFSeekLHelper(fpVSIL, off, whence);
    else
        return pfnfseeko64(stream, off, whence);
}

/************************************************************************/
/*                           fseeko()                                 */
/************************************************************************/

int fseeko (FILE *stream, off_t off, int whence)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(stream);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fseeko(stream=%p, off=%d, whence=%d)\n",
        stream, (int)off, whence);
    if( fpVSIL != NULL )
        return VSIFSeekLHelper(fpVSIL, off, whence);
    else
        return pfnfseeko64(stream, off, whence);
}

/************************************************************************/
/*                            fseek()                                   */
/************************************************************************/

int fseek (FILE *stream, off_t off, int whence)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(stream);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fseek(stream=%p, off=%d, whence=%d)\n",
        stream, (int)off, whence);
    if( fpVSIL != NULL )
        return VSIFSeekLHelper(fpVSIL, off, whence);
    else
        return pfnfseek(stream, off, whence);
}

/************************************************************************/
/*                           ftello64()                                 */
/************************************************************************/

off64_t ftello64(FILE *stream)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(stream);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "ftello64(stream=%p)\n", stream);
    if( fpVSIL != NULL )
        return VSIFTellL(fpVSIL);
    else
        return pfnftello64(stream);
}

/************************************************************************/
/*                            ftello()                                  */
/************************************************************************/

off64_t ftello(FILE *stream)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(stream);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "ftello(stream=%p)\n", stream);
    if( fpVSIL != NULL )
        return VSIFTellL(fpVSIL);
    else
        return pfnftello64(stream);
}

/************************************************************************/
/*                            ftell()                                   */
/************************************************************************/

off_t ftell(FILE *stream)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(stream);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "ftell(stream=%p)\n", stream);
    if( fpVSIL != NULL )
        return VSIFTellL(fpVSIL);
    else
        return pfnftell(stream);
}

/************************************************************************/
/*                             feof()                                   */
/************************************************************************/

int feof(FILE *stream)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(stream);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "feof(stream=%p)\n", stream);
    if( fpVSIL != NULL )
        return VSIFEofL(fpVSIL);
    else
        return pfnfeof(stream);
}

/************************************************************************/
/*                            rewind()                                  */
/************************************************************************/

void rewind(FILE *stream)
{
    fseek(stream, 0, SEEK_SET);
}

/************************************************************************/
/*                            fflush()                                  */
/************************************************************************/

int fflush(FILE *stream)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(stream);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fflush(stream=%p)\n", stream);
    if( fpVSIL != NULL )
        return 0;
    else
        return pfnfflush(stream);
}

/************************************************************************/
/*                            fgetpos()                                 */
/************************************************************************/

int fgetpos(FILE *stream, fpos_t *pos)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(stream);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fgetpos(stream=%p)\n", stream);
    if( fpVSIL != NULL )
    {
        fprintf(stderr, "fgetpos() unimplemented for VSILFILE\n");
        return -1; // FIXME
    }
    else
        return pfnfgetpos(stream, pos);
}

/************************************************************************/
/*                            fsetpos()                                 */
/************************************************************************/

int fsetpos(FILE *stream, fpos_t *pos)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(stream);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fsetpos(stream=%p)\n", stream);
    if( fpVSIL != NULL )
    {
        fprintf(stderr, "fsetpos() unimplemented for VSILFILE\n");
        return -1; // FIXME
    }
    else
        return pfnfsetpos(stream, pos);
}

/************************************************************************/
/*                             fileno()                                 */
/************************************************************************/

int fileno(FILE *stream)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(stream);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fileno(stream=%p)\n", stream);
    int fd;
    if( fpVSIL != NULL )
        fd = getfdFromVSILFILE(fpVSIL);
    else
        fd = pfnfileno(stream);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fileno(stream=%p) = %d\n", stream, fd);
    return fd;
}

/************************************************************************/
/*                             ferror()                                 */
/************************************************************************/

int ferror(FILE *stream)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(stream);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "ferror(stream=%p)\n", stream);
    if( fpVSIL != NULL )
    {
        fprintf(stderr, "ferror() unimplemented for VSILFILE\n");
        return 0; // FIXME ?
    }
    else
        return pfnferror(stream);
}

/************************************************************************/
/*                             fdopen()                                 */
/************************************************************************/

FILE * fdopen(int fd, const char *mode)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(fd);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fdopen(fd=%d)\n", fd);
    if( fpVSIL != NULL )
    {
        fprintf(stderr, "fdopen() unimplemented for VSILFILE\n");
        return NULL; // FIXME ?
    }
    else
        return pfnfdopen(fd, mode);
}

/************************************************************************/
/*                             freopen()                                */
/************************************************************************/

FILE *freopen(const char *path, const char *mode, FILE *stream)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(stream);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "freopen(path=%s,mode=%s,stream=%p)\n", path, mode, stream);
    if( fpVSIL != NULL )
    {
        fprintf(stderr, "freopen() unimplemented for VSILFILE\n");
        return NULL; // FIXME ?
    }
    else
        return pfnfreopen(path, mode, stream);
}

/************************************************************************/
/*                              open()                                  */
/************************************************************************/

int open(const char *path, int flags, ...)
{
    myinit();
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(path);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "open(%s)\n", path);

    va_list args;
    va_start(args, flags);
    mode_t mode = va_arg(args, mode_t);
    int fd;
    if( strncmp(path, "/vsi", 4) == 0 )
        fd = VSIFopenHelper(path, flags);
    else
        fd = pfnopen(path, flags, mode);
    va_end(args);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "open(%s) = %d\n", path, fd);
    return fd;
}

/************************************************************************/
/*                             open64()                                 */
/************************************************************************/

int open64(const char *path, int flags, ...)
{
    myinit();
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(path);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "open64(%s)\n", path);

    va_list args;
    va_start(args, flags);
    mode_t mode = va_arg(args, mode_t);
    int fd;
    if( strncmp(path, "/vsi", 4) == 0 )
        fd = VSIFopenHelper(path, flags);
    else
        fd = pfnopen64(path, flags, mode);
    va_end(args);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "open64(%s) = %d\n", path, fd);
    return fd;
}

/************************************************************************/
/*                             creat()                                  */
/************************************************************************/

int creat(const char *path, mode_t mode)
{
    return open64(path, O_CREAT|O_WRONLY|O_TRUNC, mode);
}

/************************************************************************/
/*                             close()                                  */
/************************************************************************/

int close(int fd)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(fd);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "close(fd=%d)\n", fd);
    if( fpVSIL != NULL )
    {
        VSIFCloseL(fpVSIL);
        CPLMutexHolderD(&hMutex);
        oSetFiles.erase(fpVSIL);
        pfnclose(oMapVSITofd[fpVSIL]);
        oMapVSITofd.erase(fpVSIL);
        oMapfdToVSI.erase(fd);
        oMapVSIToString.erase(fpVSIL);
        return 0;
    }
    else
        return pfnclose(fd);
}

/************************************************************************/
/*                              read()                                  */
/************************************************************************/

ssize_t read(int fd, void *buf, size_t count)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(fd);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "read(fd=%d, count=%d)\n", fd, (int)count);
    ssize_t ret;
    if( fpVSIL != NULL )
        ret = VSIFReadL(buf, 1, count, fpVSIL);
    else
        ret = pfnread(fd, buf, count);
    if (DEBUG_VSIPRELOAD_COND && DEBUG_OUTPUT_READ && ret < 40)
    {
        fprintf(stderr, "read() : ");
        for(int i=0;i<ret;i++)
        {
            if( ((unsigned char*)buf)[i] >= 'A' && ((unsigned char*)buf)[i] <= 'Z' )
                fprintf(stderr, "%c ", ((unsigned char*)buf)[i]);
            else
                fprintf(stderr, "\\%02X ", ((unsigned char*)buf)[i]);
        }
        fprintf(stderr, "\n");
    }
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "read() -> %d\n", (int)ret);
    return ret;
}

/************************************************************************/
/*                              write()                                 */
/************************************************************************/

ssize_t write(int fd, const void *buf, size_t count)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(fd);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "write(fd=%d, count=%d)\n", fd, (int)count);
    if( fpVSIL != NULL )
        return VSIFWriteL(buf, 1, count, fpVSIL);
    else
        return pfnwrite(fd, buf, count);
}

/************************************************************************/
/*                              fsync()                                 */
/************************************************************************/

int fsync(int fd)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(fd);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fsync(fd=%d)\n", fd);
    if( fpVSIL != NULL )
        return 0;
    else
        return pfnfsync(fd);
}

/************************************************************************/
/*                           fdatasync()                                */
/************************************************************************/

int fdatasync(int fd)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(fd);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "fdatasync(fd=%d)\n", fd);
    if( fpVSIL != NULL )
        return 0;
    else
        return pfnfdatasync(fd);
}

/************************************************************************/
/*                            __fxstat()                                */
/************************************************************************/

int __fxstat (int ver, int fd, struct stat *buf)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(fd);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "__fxstat(fd=%d)\n", fd);
    if( fpVSIL != NULL )
    {
        VSIStatBufL sStatBufL;
        std::string name;
        {
            CPLMutexHolderD(&hMutex);
            name = oMapVSIToString[fpVSIL];
        }
        int ret = VSIStatL(name.c_str(), &sStatBufL);
        if( ret == 0 )
        {
            if (DEBUG_VSIPRELOAD_COND) fprintf(stderr,
                "__fxstat ret = 0, mode = %d, size = %d\n",
                sStatBufL.st_mode, (int)sStatBufL.st_size);
            copyBuf64ToBuf(&sStatBufL, buf);
        }
        return ret;
    }
    else
        return pfn__fxstat(ver, fd, buf);
}

/************************************************************************/
/*                           __fxstat64()                               */
/************************************************************************/

int __fxstat64 (int ver, int fd, struct stat64 *buf)
{
    myinit();
    VSILFILE* fpVSIL = getVSILFILE(fd);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "__fxstat64(fd=%d)\n", fd);
    if( fpVSIL != NULL )
    {
        std::string name;
        {
            CPLMutexHolderD(&hMutex);
            name = oMapVSIToString[fpVSIL];
        }
        int ret = VSIStatL(name.c_str(), buf);
        if( ret == 0 )
        {
            if (DEBUG_VSIPRELOAD_COND) fprintf(stderr,
                "__fxstat64 ret = 0, mode = %d, size = %d\n",
                buf->st_mode, (int)buf->st_size);
        }
        return ret;
    }
    else
        return pfn__fxstat64(ver, fd, buf);
}

/************************************************************************/
/*                              lseek()                                 */
/************************************************************************/

off_t lseek(int fd, off_t off, int whence)
{
    myinit();
    off_t ret;
    VSILFILE* fpVSIL = getVSILFILE(fd);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr,
        "lseek(fd=%d, off=%d, whence=%d)\n", fd, (int)off, whence);
    if( fpVSIL != NULL )
    {
        VSIFSeekLHelper(fpVSIL, off, whence);
        ret = VSIFTellL(fpVSIL);
    }
    else
        ret = pfnlseek(fd, off, whence);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "lseek() -> ret = %d\n", (int)ret);
    return ret;
}

/************************************************************************/
/*                             lseek64()                                */
/************************************************************************/

off64_t lseek64(int fd, off64_t off, int whence)
{
    myinit();
    off_t ret;
    VSILFILE* fpVSIL = getVSILFILE(fd);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr,
        "lseek64(fd=%d, off=%d, whence=%d)\n", fd, (int)off, whence);
    if( fpVSIL != NULL )
    {
        VSIFSeekLHelper(fpVSIL, off, whence);
        ret = VSIFTellL(fpVSIL);
    }
    else
        ret = pfnlseek64(fd, off, whence);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr,
        "lseek64() -> ret = %d\n", (int)ret);
    return ret;
}

/************************************************************************/
/*                            truncate()                                */
/************************************************************************/

int truncate(const char *path, off_t length)
{
    myinit();
    int ret;
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(path);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "truncate(%s)\n", path);

    if( strncmp(path, "/vsi", 4) == 0 )
    {
        VSILFILE* fpVSIL = VSIFOpenL(path, "wb+");
        if( fpVSIL )
        {
            ret = VSIFTruncateL(fpVSIL, length);
            VSIFCloseL(fpVSIL);
        }
        else
            ret = -1;
    }
    else
        ret = pfntruncate(path, length);
    return ret;
}

/************************************************************************/
/*                           ftruncate()                                */
/************************************************************************/

int ftruncate(int fd, off_t length)
{
    myinit();
    int ret;
    VSILFILE* fpVSIL = getVSILFILE(fd);
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(fpVSIL);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "ftruncate(fd=%d)\n", fd);
    if( fpVSIL != NULL )
    {
        ret = VSIFTruncateL(fpVSIL, length);
    }
    else
        ret = pfnftruncate(fd, length);
    return ret;
}

/************************************************************************/
/*                             opendir()                                */
/************************************************************************/

DIR *opendir(const char *name)
{
    myinit();
    int DEBUG_VSIPRELOAD_COND = GET_DEBUG_VSIPRELOAD_COND(name);
    if (DEBUG_VSIPRELOAD_COND) fprintf(stderr, "opendir(%s)\n", name);

    if( strncmp(name, "/vsi", 4) == 0 )
    {
        fprintf(stderr, "opendir() unimplemented for VSILFILE\n");
        return NULL; // FIXME
    }
    else
        return pfnopendir(name);
}
