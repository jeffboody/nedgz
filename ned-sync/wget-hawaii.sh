function mapget()
{
	lat0=$1
	lat1=$2
	lon=$3
	for lat in $(eval "echo {${lat0}..${lat1}..1}")
	do
		if [ -e n${lat}w${lon}.zip ]; then
			# echo "skip n${lat}w${lon}.zip"
			continue
		fi
		wget http://tdds3.cr.usgs.gov/Ortho9/ned/ned_13/float/n${lat}w${lon}.zip
	done
}

# lat0 lat1 lon
mapget 22 23 161
mapget 22 23 160
mapget 22 22 159
mapget 21 22 158
mapget 20 22 157
mapget 19 21 156
mapget 20 20 155
