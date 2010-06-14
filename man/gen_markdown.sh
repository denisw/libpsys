#!/bin/sh

rm -r markdown
mkdir markdown

for page in `ls *.3 *.7`; do
    WIKILINK_CMD="s/\*\*psys\([a-z_]*\)\*\*(\([0-9]\))/\*\*\[\[Psys\1\]\]\*\*(\2)/g"
	python man2markdown.py $page | sed $WIKILINK_CMD > markdown/$page.txt
done
