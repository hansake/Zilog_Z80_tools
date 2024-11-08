The diskette images I tested zdostool with were downloaded from
- https://github.com/sebhc/sebhc/tree/master/mcz#software-disk-images
- https://datamuseum.dk/wiki/Bits:Keyword/COMPANY/ZILOG

```
$ zdostool -h
Usage: zdostool [OPTIONS] [IMAGEFILES]...
Tool to list and export RIO files from Zilog ZDOS diskette image files

  -a, --analyze         analyze the content of the image file and report errors
  -b, --backptr         analyze back pointers in sectors and report errors
  -c, --createdir       create a directory for each imagefile
  -d, --descriptor      print file descriptor information
  -e, --export          export files from diskette image
  -f, --file [NAME]     name of the file if single file is listed or exported
  -h, --help            show help
  -i, --ignore          ignore if sector or track numbers do not match what
                        is read from the diskette image
  -v, --verbose         show details
``` 
Some checks of the image are still missing.
