function mapget()
{
	lat0=$1
	lat1=$2
	lon=$3
	for lat in $(eval "echo {${lat0}..${lat1}..1}")
	do
		#if [ -e n${lat}w${lon}.zip ]; then
		if [ -e n${lat}w0${lon}.zip ]; then
			# echo "skip n${lat}w${lon}.zip"
			# echo "skip n${lat}w0${lon}.zip"
			continue
		fi
		#wget http://tdds3.cr.usgs.gov/Ortho9/ned/ned_13/float/n${lat}w${lon}.zip
		wget http://tdds3.cr.usgs.gov/Ortho9/ned/ned_13/float/n${lat}w0${lon}.zip
	done
}

# lat0 lat1 lon
mapget 40 49 125
mapget 38 49 124
mapget 37 49 123
mapget 36 49 122
mapget 34 49 121
mapget 34 49 120
mapget 33 49 119
mapget 33 49 118
mapget 33 49 117
mapget 33 49 116
mapget 33 49 115
mapget 32 49 114
mapget 32 49 113
mapget 32 49 112
mapget 32 49 111
mapget 32 49 110
mapget 32 49 109
mapget 32 49 108
mapget 32 49 107
mapget 31 49 106
mapget 30 49 105
mapget 29 49 104
mapget 30 49 103
mapget 30 49 102
mapget 29 49 101
mapget 27 49 100
mapget 27 49 99
mapget 26 49 98
mapget 28 49 97
mapget 29 50 96
mapget 30 50 95
mapget 30 49 94
mapget 30 49 93
mapget 30 49 92
mapget 30 49 91
mapget 29 49 90
mapget 30 49 89
mapget 31 48 88
mapget 31 47 87
mapget 30 47 86
mapget 30 47 85
mapget 30 47 84
mapget 27 45 83
mapget 25 25 83
mapget 25 42 82
mapget 32 43 81
mapget 25 30 81
mapget 33 44 80
mapget 34 44 79
mapget 34 44 78
mapget 35 45 77
mapget 36 45 76
mapget 39 46 75
mapget 41 46 74
mapget 41 46 73
mapget 42 46 72
mapget 42 47 71
mapget 44 48 70
mapget 42 42 70
mapget 44 48 69
mapget 45 48 68
mapget 45 45 67
