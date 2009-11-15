#!/bin/bash
# Load test data in PostGIS
#
# NOTE 1: Assume RASTER_OVERVIEWS table created. If not, please execute the script
# create_raster_overviews_table.sh (in this same directory) first.
# 
# NOTE 2: gdal2wktraster script doesn't allow WKT Raster tables (raster_columns, 
# raster_overviews) and data tables in different schemas. If you want to store
# your data in a different schema from the schema where WKT Raster tables are, 
# you'll have to change the sql files generated by gdal2wktras by hand.
#
# NOTE 3: The execution of psql calls will ask you for the password of the user
# USER_NAME. If you want to avoid this, you'll have to modify the PostgreSQL
# configuration file (pg_hba.conf) to allow the user "trust" authentication
# method. See http://www.postgresql.org/docs/8.4/interactive/auth-pg-hba-conf.html

# CHANGE THIS TO MATCH YOUR ENVIROMENT
GDAL2WKTRASTER_PATH=/home/jorge/src/wktraster/scripts
IMG_TEST_FILES_PATH=/home/jorge/test_data/tiff_files
SQL_OUTPUT_FILES_PATH=/home/jorge/test_data/sql
DATABASE_NAME=gdal_wktraster_test
DATABASE_SCHEMA=public
HOST_NAME=localhost
USER_NAME=postgres

# We add -V option to create the RASTER_OVERVIEWS table. Only one time.

$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/utm.tif -t $DATABASE_SCHEMA.utm -l 1 -k 100x100 -o $SQL_OUTPUT_FILES_PATH/utm_level1.sql -s 26711 -I -M
$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/utm.tif -t $DATABASE_SCHEMA.utm -l 2 -k 100x100 -o $SQL_OUTPUT_FILES_PATH/utm_level2.sql -s 26711 -I -M
$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/utm.tif -t $DATABASE_SCHEMA.utm -l 4 -k 100x100 -o $SQL_OUTPUT_FILES_PATH/utm_level4.sql -s 26711 -I -M
$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/utm.tif -t $DATABASE_SCHEMA.utm -l 8 -k 100x100 -o $SQL_OUTPUT_FILES_PATH/utm_level8.sql -s 26711 -I -M
#$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/utm.tif -t utm -l 1 -k 100x100 -R -o $SQL_OUTPUT_FILES_PATH/utm_outdb_level1.sql -s 26711 -I -M

psql -h $HOST_NAME -U $USER_NAME -d $DATABASE_NAME -f $SQL_OUTPUT_FILES_PATH/utm_level1.sql
psql -h $HOST_NAME -U $USER_NAME -d $DATABASE_NAME -f $SQL_OUTPUT_FILES_PATH/utm_level2.sql
psql -h $HOST_NAME -U $USER_NAME -d $DATABASE_NAME -f $SQL_OUTPUT_FILES_PATH/utm_level4.sql
psql -h $HOST_NAME -U $USER_NAME -d $DATABASE_NAME -f $SQL_OUTPUT_FILES_PATH/utm_level8.sql

# Out-db support is not working in WKT Raster right now (August 17th 2009). To allow out-db rasters in AddRasterColumns function, you must change
# the $WKTRASTER_SRC/rt_pg/rtpostgis.sql code, comment lines 532 - 535 (verify out_db), and execute rtpostgis.sql again in the same database
#psql -h $HOST_NAME -U $USER_NAME -d $DATABASE_NAME -f $SQL_OUTPUT_FILES_PATH/utm_outdb_level1.sql


$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/small_world.tif -t $DATABASE_SCHEMA.small_world -l 1 -k 40x20 -o $SQL_OUTPUT_FILES_PATH/small_world_level1.sql -s 4326 -I -M
$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/small_world.tif -t $DATABASE_SCHEMA.small_world -l 2 -k 40x20 -o $SQL_OUTPUT_FILES_PATH/small_world_level2.sql -s 4326 -I -M
$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/small_world.tif -t $DATABASE_SCHEMA.small_world -l 4 -k 40x20 -o $SQL_OUTPUT_FILES_PATH/small_world_level4.sql -s 4326 -I -M
$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/small_world.tif -t $DATABASE_SCHEMA.small_world -l 8 -k 40x20 -o $SQL_OUTPUT_FILES_PATH/small_world_level8.sql -s 4326 -I -M


psql -h $HOST_NAME -U $USER_NAME -d $DATABASE_NAME -f $SQL_OUTPUT_FILES_PATH/small_world_level1.sql
psql -h $HOST_NAME -U $USER_NAME -d $DATABASE_NAME -f $SQL_OUTPUT_FILES_PATH/small_world_level2.sql
psql -h $HOST_NAME -U $USER_NAME -d $DATABASE_NAME -f $SQL_OUTPUT_FILES_PATH/small_world_level4.sql
psql -h $HOST_NAME -U $USER_NAME -d $DATABASE_NAME -f $SQL_OUTPUT_FILES_PATH/small_world_level8.sql
