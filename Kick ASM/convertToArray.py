# the compiler (kickass) compiles a .prg file
# this script converts that prg file into a binary array

# Read the prg file as a binary file and create the c-style byte array
# for the ESP32 include file

# my build and run script inside relaunch64 looks like this
# java -jar /home/bart/C64/kick/KickAss.jar RSOURCEFILE
# python /home/bart/C64/C64_Chat/convertToArray.py ROUTFILE "/home/bart/GitHub/Chat64/ESP32 Sketch"
# /usr/bin/x64 -silent OUTFILE

import sys
import os
import datetime

n = len(sys.argv)
inputfile= sys.argv[1]
outfolder= sys.argv[2]   # outfolder = "/home/bart/GitHub/Chat64/ESP32 Sketch"

asmfile=inputfile.replace(".prg",".asm")
# search for version and date in inputfile
with open(asmfile,'r') as f:
	for strline in f.readlines():
		if (strline.startswith("version:   ")):
			asmversion = strline.split('"')[1]
			 
		if (strline.startswith("version_date:   ")):
			asmdate = strline.split('"')[1]
			 
for root, dir, files in os.walk(outfolder):
      if "prgfile.h" in files:
         outputfile = (os.path.join(root, "prgfile.h"))      
         
print("converting " + inputfile +" to hex array as " + outputfile)
print("bin size is " + str(os.path.getsize(inputfile)))

strArray="// array size is " + str(os.path.getsize(inputfile)) + ". " + datetime.datetime.now().strftime("%Y-%m-%d %H:%M") + "\n"
strArray+="// version in ROM = "+ asmversion + ", date in ROM = " + asmdate + "\n"
strArray+="static const byte prgfile[] PROGMEM  = {\n"
c=0
with open(inputfile, "rb") as f:
	while (byte := f.read(1)):
		strArray=strArray+ "0x" + byte.hex() + ","
		c=c+1
		if (c > 15):
			c=0
			strArray = strArray + "\n"
strArray = strArray.rsplit(',', 1)[0]
strArray = strArray + "\n};"

		
with open(outputfile, "w") as file1:
    file1.writelines(strArray)

