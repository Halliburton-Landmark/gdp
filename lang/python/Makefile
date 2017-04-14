install:
	cp -a gdp /usr/local/lib/python2.7/dist-packages/
	@echo "Fix library locations"
	# Under macOS Sierra, sed -i does not edit a file in place
	sed 's@"..", "..", "..", "libs"@"/", "usr", "lib"@g' \
		/usr/local/lib/python2.7/dist-packages/gdp/MISC.py > MISC.py.tmp
	mv MISC.py.tmp /usr/local/lib/python2.7/dist-packages/gdp/MISC.py
