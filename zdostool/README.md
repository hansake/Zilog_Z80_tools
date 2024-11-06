The diskette images I tested zdostool with were downloaded from
- https://github.com/sebhc/sebhc/tree/master/mcz#software-disk-images
- https://datamuseum.dk/wiki/Bits:Keyword/COMPANY/ZILOG

```
$ zdostool -h
Usage: zdostool [OPTIONS] [IMAGEFILES]...
Tool to list and export files from Zilog ZDOS diskette image FILES

  -a, --analyze         analyze the content on the imagefile and report errors
  -b, --backptr         analyze back pointers in sectors, not reliable yet
  -c, --createdir       create directory for each imagefile
  -d, --descriptor      print file descriptors
  -e, --export          export files from diskette image
  -f, --file [NAME]     name of the file if single file is listed or exported
  -h, --help            show help
  -v, --verbose         show details
``` 
Some checks of the image is still missing.
