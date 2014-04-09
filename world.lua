#!/usr/bin/lua

-- Example to convert latitude/longitude
-- to/from tile/subtile indicies/coords
-- for MGM map filenames
--
-- Based loosely on this description from JCamp...@gmail.com
-- https://groups.google.com/forum/m/?fromgroups#!topic/google-maps-api/oJkyualxzyY
--
-- For a good description of tiles and zoom levels
-- http://www.mapbox.com/developers/guide/
--
-- For a description of the Mercator Projection
-- http://en.m.wikipedia.org/wiki/Mercator_projection
--
-- To convert latitude/longitude/height to x/y/z
-- www.wollindina.com/HP-33S/XYZ_1.pdf

PI    = 3.141592654
log   = math.log
tan   = math.tan
cos   = math.cos
sin   = math.sin
floor = math.floor
atan  = math.atan
exp   = math.exp
sqrt  = math.sqrt

NUMSUBTILE      = 8
NUMSUBSAMPLEMGM = 256
NUMSUBSAMPLENED = 32

function feet2meters(feet)
	return feet*1609.344/5280.0
end

function coord2tile(zoom, lat, lon)
	local radlat   = lat*(PI/180)
	local radlon   = lon*(PI/180)

	local tile     = { }
	tile.mercx     = radlon
	tile.mercy     = log(tan(radlat) + 1/cos(radlat))
	tile.cartx     = tile.mercx + PI
	tile.carty     = PI - tile.mercy
	tile.worldu    = tile.cartx/(2*PI)
	tile.worldv    = tile.carty/(2*PI)
	tile.x         = tile.worldu*(2^zoom)/NUMSUBTILE
	tile.y         = tile.worldv*(2^zoom)/NUMSUBTILE
	tile.ix        = floor(tile.x)
	tile.iy        = floor(tile.y)
	tile.subtilex  = NUMSUBTILE*(tile.x - tile.ix)
	tile.subtiley  = NUMSUBTILE*(tile.y - tile.iy)
	tile.subtileix = floor(tile.subtilex)
	tile.subtileiy = floor(tile.subtiley)
	tile.subtileu  = tile.subtilex - tile.subtileix
	tile.subtilev  = tile.subtiley - tile.subtileiy

	return tile
end

function tile2coord(zoom, tilex, tiley)
	local coord  = { }
	coord.worldu = NUMSUBTILE*tilex/(2^zoom)
	coord.worldv = NUMSUBTILE*tiley/(2^zoom)
	coord.cartx  = 2*PI*coord.worldu
	coord.carty  = 2*PI*coord.worldv
	coord.mercx  = coord.cartx - PI
	coord.mercy  = PI - coord.carty

	local radlon = coord.mercx
	local radlat = 2*atan(exp(coord.mercy)) - PI/2

	coord.lat = radlat/(PI/180)
	coord.lon = radlon/(PI/180)

	return coord
end

function coord2xyz(lat, lon, height)
	-- WSG84/NAD83 coordinate system
	-- meters
	local a      = 6378137.0
	local e2     = 0.006694381
	local radlat = lat*(PI/180)
	local radlon = lon*(PI/180)
	local s2     = sin(radlat)*sin(radlat)
	local v      = a/sqrt(1.0 - e2*s2)

	local xyz = { }
	xyz.x     = (v + height)*cos(radlat)*cos(radlon)
	xyz.y     = (v + height)*cos(radlat)*sin(radlon)
	xyz.z     = (v*(1 - e2) + height)*sin(radlat)

	return xyz
end

home = { lat=40.061295, lon=-105.214552, height=feet2meters(5280) }

print("-- home --")
print("lat = " .. home.lat)
print("lon = " .. home.lon)

print("\n-- home coord2tile --")
tile = coord2tile(14, home.lat, home.lon)
print("lat       = " .. home.lat)
print("lon       = " .. home.lon)
print("worldu    = " .. tile.worldu)
print("worldv    = " .. tile.worldv)
print("x         = " .. tile.x)
print("y         = " .. tile.y)
print("ix        = " .. tile.ix)
print("iy        = " .. tile.iy)
print("subtileix = " .. tile.subtileix)
print("subtileiy = " .. tile.subtileiy)
print("subtileu  = " .. tile.subtileu)
print("subtilev  = " .. tile.subtilev)

print("\n-- home coord2time at zoom --")
for i=0,15 do
	tile = coord2tile(i, home.lat, home.lon)
	print(i .. ": ix =" .. tile.ix        .. ", iy =" .. tile.iy)
	print(i .. ": six=" .. tile.subtileix .. ", siy=" .. tile.subtileiy)
end

print("\n-- home tile2coord --")
coord = tile2coord(14, tile.x, tile.y)
print("x      = " .. tile.x)
print("y      = " .. tile.y)
print("worldu = " .. coord.worldu)
print("worldv = " .. coord.worldv)
print("lat    = " .. coord.lat)
print("lon    = " .. coord.lon)

print("\n-- home coord2xyz --")
xyz = coord2xyz(home.lat, home.lon, home.height)
print("lat    = " .. home.lat)
print("lon    = " .. home.lon)
print("height = " .. home.height)
print("x      = " .. xyz.x)
print("y      = " .. xyz.y)
print("z      = " .. xyz.z)

print("\n-- home coord2meter --")
xyz     = coord2xyz(home.lat, home.lon, home.height)
xyz_lat = coord2xyz(home.lat + 1.0, home.lon, home.height)
xyz_lon = coord2xyz(home.lat, home.lon + 1.0, home.height)
dlat    = sqrt((xyz_lat.x - xyz.x)^2 + (xyz_lat.y - xyz.y)^2 + (xyz_lat.z - xyz.z)^2)
dlon    = sqrt((xyz_lon.x - xyz.x)^2 + (xyz_lon.y - xyz.y)^2 + (xyz_lon.z - xyz.z)^2)
print("meters-per-lat = " .. dlat)
print("meters-per-lon = " .. dlon)

print("\n-- mgm sample-per-degree at home for NED13 --")
print("10812x10812 for n41w106")

print("\n-- mgm sample-per-degree at home --")
smgm = NUMSUBTILE*NUMSUBSAMPLEMGM
for i=1,15 do
	tile0 = coord2tile(i, 40, -106)
	tile1 = coord2tile(i, 41, -105)
	print(i .. ": dx=" .. smgm*(tile1.x - tile0.x) .. ", dy=" .. smgm*(tile0.y - tile1.y))
end

print("\n-- mgm sample-per-degree at alaska --")
smgm = NUMSUBTILE*NUMSUBSAMPLEMGM
for i=1,15 do
	tile0 = coord2tile(i, 71, -157)
	tile1 = coord2tile(i, 72, -156)
	print(i .. ": dx=" .. smgm*(tile1.x - tile0.x) .. ", dy=" .. smgm*(tile0.y - tile1.y))
end

print("\n-- mgm sample-per-degree at equator --")
for i=1,15 do
	tile0 = coord2tile(i, -0.5, -0.5)
	tile1 = coord2tile(i, 0.5, 0.5)
	print(i .. ": dx=" .. smgm*(tile1.x - tile0.x) .. ", dy=" .. smgm*(tile0.y - tile1.y))
end

print("\n-- ned sample-per-degree at home --")
sned = NUMSUBTILE*NUMSUBSAMPLENED
for i=1,15 do
	tile0 = coord2tile(i, 40, -106)
	tile1 = coord2tile(i, 41, -105)
	print(i .. ": dx=" .. sned*(tile1.x - tile0.x) .. ", dy=" .. sned*(tile0.y - tile1.y))
end

print("\n-- ned sample-per-degree at alaska --")
sned = NUMSUBTILE*NUMSUBSAMPLENED
for i=1,15 do
	tile0 = coord2tile(i, 71, -157)
	tile1 = coord2tile(i, 72, -156)
	print(i .. ": dx=" .. sned*(tile1.x - tile0.x) .. ", dy=" .. sned*(tile0.y - tile1.y))
end

print("\n-- ned sample-per-degree at equator --")
for i=1,15 do
	tile0 = coord2tile(i, -0.5, -0.5)
	tile1 = coord2tile(i, 0.5, 0.5)
	print(i .. ": dx=" .. sned*(tile1.x - tile0.x) .. ", dy=" .. sned*(tile0.y - tile1.y))
end
