function mapget()
{
	lat0=$1
	lat1=$2
	lon=$3

	# add delay to allow ctrl-c since mq-sync captures SIGINT
	echo ./mq-sync 15 ${lat1} ${lon} ${lat0} ${lon}
	echo starting in 3
	sleep 1
	echo starting in 2
	sleep 1
	echo starting in 1
	sleep 1

	# zoom 15 is highest resolution that seems to be available
	# across the entire lower 48 states
	./mq-sync 15 ${lat1} ${lon} ${lat0} ${lon}
}

# sync US (lower 48 states)
# lat0 lat1 lon
mapget 40 49 -125
mapget 38 49 -124
mapget 37 49 -123
mapget 36 49 -122
mapget 34 49 -121
mapget 34 49 -120
mapget 33 49 -119
mapget 33 49 -118
mapget 33 49 -117
mapget 33 49 -116
mapget 33 49 -115
mapget 32 49 -114
mapget 32 49 -113
mapget 32 49 -112
mapget 32 49 -111
mapget 32 49 -110
mapget 32 49 -109
mapget 32 49 -108
mapget 32 49 -107
mapget 31 49 -106
mapget 30 49 -105
mapget 29 49 -104
mapget 30 49 -103
mapget 30 49 -102
mapget 29 49 -101
mapget 27 49 -100
mapget 27 49 -99
mapget 26 49 -98
mapget 28 49 -97
mapget 29 50 -96
mapget 30 50 -95
mapget 30 49 -94
mapget 30 49 -93
mapget 30 49 -92
mapget 30 49 -91
mapget 29 49 -90
mapget 30 49 -89
mapget 31 48 -88
mapget 31 47 -87
mapget 30 47 -86
mapget 30 47 -85
mapget 30 47 -84
mapget 27 45 -83
mapget 25 25 -83
mapget 25 42 -82
mapget 32 43 -81
mapget 25 30 -81
mapget 33 44 -80
mapget 34 44 -79
mapget 34 44 -78
mapget 35 45 -77
mapget 36 45 -76
mapget 39 46 -75
mapget 41 46 -74
mapget 41 46 -73
mapget 42 46 -72
mapget 42 47 -71
mapget 44 48 -70
mapget 42 42 -70
mapget 44 48 -69
mapget 45 48 -68
mapget 45 45 -67
