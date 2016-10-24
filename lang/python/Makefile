install:
	cp -a gdp /usr/local/lib/python2.7/dist-packages/
	# fix library locations
	sed -i "s/\"..\", \"..\", \"..\", \"libs\"/\"\/\", \"usr\", \"lib\"/g" \
	    /usr/local/lib/python2.7/dist-packages/gdp/MISC.py
