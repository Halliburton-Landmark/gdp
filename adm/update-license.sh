#!/bin/sh

adm=`dirname $0`

update_license() {
	file=$1
	{
	    cp $file $file.BAK &&
	    awk -f $adm/update-license.awk $file > $file.$$ &&
	    cp $file.$$ $file &&
	    rm $file.$$
	} ||
	    echo WARNING: could not update license for $file 1>&2
}

if [ ! -r LICENSE ]
then
	echo "[ERROR] LICENSE file must exist"
	exit 1
fi


for f
do
	echo Updating $f
	update_license $f
done
