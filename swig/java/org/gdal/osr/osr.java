/* ----------------------------------------------------------------------------
 * This file was automatically generated by SWIG (http://www.swig.org).
 * Version 1.3.27
 *
 * Do not make changes to this file unless you know what you are doing--modify
 * the SWIG interface file instead.
 * ----------------------------------------------------------------------------- */

package org.gdal.osr;

public class osr implements osrConstants {
  public static int GetWellKnownGeogCSAsWKT(String name, String[] argout) {
    return osrJNI.GetWellKnownGeogCSAsWKT(name, argout);
  }

  public static String[] GetProjectionMethods() {
    return osrJNI.GetProjectionMethods();
  }

  public static String[] GetProjectionMethodParameterList(String method, String[] username) {
    return osrJNI.GetProjectionMethodParameterList(method, username);
  }

  public static void GetProjectionMethodParamInfo(String method, String param, String[] usrname, String[] type, SWIGTYPE_p_double defaultval) {
    osrJNI.GetProjectionMethodParamInfo(method, param, usrname, type, SWIGTYPE_p_double.getCPtr(defaultval));
  }

}
