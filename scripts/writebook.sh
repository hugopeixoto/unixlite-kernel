#!/bin/sh
# run this script at the kernel source top directory
esc_()
{
        echo $1 | sed -e 's/_/\\_/g'
}

subdirof()
{
        find $1 -maxdepth 1 -mindepth 1 -type d -exec basename '{}' ';'
}

subregof()
{
        find $1 -maxdepth 1 -mindepth 1 -type f -exec basename '{}' ';'
}

sortmakefile()
{
        local head

        result=''
        for i ; {
        	if [ $i = "Makefile" ] ; then
        		head="Makefile"
        	else
        		result=$result" "$(echo $i)
        	fi
        }
        result=$head$result
}

writefile()
{
        echo '\verbatimtabinput[8]{'$1'}'
}

writesection()
{
        echo '\section{'$(esc_ $2)'}'
        writefile $1/$2 
}

writesubsection()
{
        echo '\subsection{'$(esc_ $3)'}'
        writefile $1/$2/$3
}

writesubdir() #(chapter,section,subsection)
{
        echo '\section{'$(esc_ $2)'}'
        sortmakefile $(subregof $1/$2)
        for i in $result; {
        	writesubsection $1 $2 $i
        }
        echo
}

writechapter()
{
        echo '\chapter{'$1'}'
        sortmakefile $(subregof $1)
        for i in $result; {
        	writesection $1 $i
        }	
        for i in $(subdirof $1); {
        	writesubdir $1 $i
        }
        echo
}

writemakefile()
{
        echo '\chapter{Makefile}'
        writefile Makefile
        echo '\chapter{Rules.make}'
        writefile Rules.make
}

main()
{
echo '
\documentclass[12pt,a4paper,twoside]{book}
\usepackage{verbatim}
\usepackage{moreverb}
\addtolength{\voffset}{-2cm}
\addtolength{\textheight}{3cm}
\addtolength{\hoffset}{-1cm}
\addtolength{\textwidth}{2cm}
\setlength{\evensidemargin}{10pt}
\begin{document}
\tableofcontents
\frontmatter
'
        writemakefile

        echo '\mainmatter'
        writechapter boot
        writechapter init
        writechapter lib
        writechapter asm
        writechapter mm 
        writechapter fs
        writechapter kern
        writechapter net
        writechapter dev

        echo '\end{document}'
}

main > unixlite.tex
latex unixlite 
latex unixlite 
dvipdf unixlite 
