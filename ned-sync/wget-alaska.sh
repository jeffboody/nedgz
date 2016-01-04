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

function mapgete()
{
	lat0=$1
	lat1=$2
	lon=$3
	for lat in $(eval "echo {${lat0}..${lat1}..1}")
	do
		if [ -e n${lat}w${lon}.zip ]; then
			# echo "skip n${lat}e${lon}.zip"
			continue
		fi
		wget http://tdds3.cr.usgs.gov/Ortho9/ned/ned_13/float/n${lat}e${lon}.zip
	done
}

# lat0 lat1 lon
# east
mapgete 53 54 171
mapgete 53 53 172
mapgete 53 53 173
mapgete 53 53 174
mapgete 52 53 176
mapgete 52 53 177
mapgete 52 53 178
mapgete 52 52 179

#west
mapget 52 52 179
mapget 52 52 178
mapget 52 53 177
mapget 52 53 176
mapget 53 53 175
mapget 53 53 174
mapget 61 61 174
mapget 53 53 173
mapget 61 61 173
mapget 53 53 172
mapget 64 64 172
mapget 53 53 171
mapget 58 58 171
mapget 64 64 171
mapget 53 54 170
mapget 57 57 170
mapget 63 64 170
mapget 66 66 170
mapget 53 54 169
mapget 64 66 169
mapget 54 54 168
mapget 60 61 168
mapget 66 66 168
mapget 54 55 167
mapget 60 63 167
mapget 65 67 167
mapget 69 69 167
mapget 55 55 166
mapget 60 63 166
mapget 65 69 166
mapget 55 55 165
mapget 60 69 165
mapget 55 56 164
mapget 60 70 164
mapget 55 56 163
mapget 59 71 163
mapget 55 57 162
mapget 59 71 162
mapget 55 57 161
mapget 59 71 161
mapget 55 57 160
mapget 59 71 160
mapget 56 71 159
mapget 57 72 158
mapget 56 72 157
mapget 56 56 156
mapget 58 72 156
mapget 57 72 155
mapget 57 71 154
mapget 58 71 153
mapget 59 71 152
mapget 60 71 151
mapget 60 71 150
mapget 60 71 149
mapget 60 71 148
mapget 61 71 147
mapget 61 71 146
mapget 60 71 145
mapget 60 71 144
mapget 61 71 143
mapget 60 70 142
mapget 60 61 141
mapget 60 61 140
mapget 59 60 139
mapget 59 60 138
mapget 58 60 137
mapget 57 60 136
mapget 56 60 135
mapget 54 59 134
mapget 53 58 133
mapget 52 57 132
mapget 53 53 131
mapget 55 57 131
mapget 56 57 130
