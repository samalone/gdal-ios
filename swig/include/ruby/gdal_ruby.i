/*
 * $Id$
 *
 * ruby specific code for gdal bindings.
 */

/*
 * $Log$
 * Revision 1.1  2005/09/02 16:19:23  kruland
 * Major reorganization to accomodate multiple language bindings.
 * Each language binding can define renames and supplemental code without
 * having to have a lot of conditionals in the main interface definition files.
 *
 */


%init %{
  /* gdal_ruby.i %init code */
  if ( GDALGetDriverCount() == 0 ) {
    GDALAllRegister();
  }
%}

%import typemaps_ruby.i

