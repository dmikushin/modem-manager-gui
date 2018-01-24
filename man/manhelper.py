#!/usr/bin/env python3

import sys, os, tempfile, uuid, gzip

if len(sys.argv) != 3:
	print(sys.argv[0], '<source file>', '<destination file>')

srcfilename = os.path.abspath(sys.argv[1])
destfilename = os.path.abspath(sys.argv[2])

intfilename = os.path.join(tempfile.gettempdir(), str(uuid.uuid4()))
reffilename = os.path.join(os.path.dirname(sys.argv[0]), 'modem-manager-gui.1')

command = 'po4a-translate -f man -k 1 -m ' + reffilename + ' -p ' + srcfilename + ' -l ' + intfilename
wexit = os.system(command)

if (wexit == 0):
	with gzip.open(destfilename, 'wb') as destfile:
		with open(intfilename, 'r') as intfile:
			for line in intfile:
				destfile.write(line.encode('utf-8'))
	os.unlink(intfilename)

sys.exit(wexit)
