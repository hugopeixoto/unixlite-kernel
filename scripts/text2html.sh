#!/bin/sh
function text2html()
{
	echo '<html><head></head><body>'
	echo '<pre>'
	cat -n $1 | sed -e 's/</\&lt;/' -e 's/>/\&gt;/'
	echo '</pre>'
	echo '</body></html>'
}

FILES=$(find $1 -type f)
for f in $FILES; do
	text2html $f >__tmp__
	rm -f $f
	mv __tmp__ $f
done
