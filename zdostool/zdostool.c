/* zdostool.c - Tool to list and extract files from Zilog ZDOS diskette images. 
 *
 * The MCZ ZDOS-II image format is described in:
 * https://github.com/sebhc/sebhc/tree/master/mcz#software-disk-images
 * https://github.com/sebhc/sebhc/blob/master/mcz/docs/03-0072-01A_Z80_RIO_Operating_System_Users_Manual_Sep78.pdf
 *
 * You are free to use, modify, and redistribute
 * this source code. No warranties are given.
 * Hastily Cobbled Together 2024 by Hans-Ake Lund
 *
 */
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <error.h>
#include <libgen.h>

/* Options */
char *file_name = NULL;
int analyze_flag = 0;
int backptr_flag = 0;
int createdir_flag = 0;
int descriptor_flag = 0;
int export_flag = 0;
int ignore_flag = 0;
int verbose_flag = 0;

char *program_name;

/* Variables */
unsigned char sect_buf[136];
unsigned char des_buf[136];
unsigned char file_sec_buf[136];
unsigned char file_rec_buf[4096];
char zdos_filename[33];
char image_filename[256];
char disk_directory[256 + 4];
char image_path_filename[256 + 4 + 33];
FILE *imagefd = NULL;
FILE *exportfd = NULL;
long filesize;
int endtrack;


/* Print image file name and ZDOS file name if error detected
 * and the analyze flag is on.
 */
void prt_imgf_zdosf(void)
    {
    if (analyze_flag)
        printf("Image file: %s, ZDOS file: %s\n  -> ", image_filename, zdos_filename);
    }

/* Read sector from diskette image
 * Buffer sbuf must be at least 136 bytes
 * Input parameters sector and track start from 0
 */
int read_sector(unsigned char *sbuf, int sector, int track)
    {
    size_t insize;

    if ((sector < 0) || (32 <= sector))
        {
        prt_imgf_zdosf();
        printf("Sector out of range: %d\n", sector);
        return (0);
        }
    if ((track < 0) || (endtrack <= track))
        {
        prt_imgf_zdosf();
        printf("Track out of range: %d\n", sector);
        return (0);
        }
    if (0 != fseek(imagefd, ((sector * 136) + (track * 4352)), SEEK_SET))
        {
        fclose(imagefd);
        exit(EXIT_FAILURE);
        }
    insize = fread(sbuf, sizeof(sect_buf), 1, imagefd);
    if (!(sbuf[0] & 0x80) && !ignore_flag)
        {
        prt_imgf_zdosf();
        printf("Invalid sector on disk: %d, no start bit, expected to read: %d\n", sbuf[0], sector);
        return (0);
        }
    if (((sbuf[0] & 0x7f) != sector) && !ignore_flag)
        {
        prt_imgf_zdosf();
        printf("Invalid sector number on disk image: %d, expected to read: %d\n", sbuf[0] & 0x7f, sector);
        return (0);
        }
    if ((sbuf[1] != track) && !ignore_flag)
        {
        prt_imgf_zdosf();
        printf("Invalid track number on disk image: %d, expected to read: %d\n", sbuf[1], track);
        return (0);
        }
    return (insize);
    }

/* Print sector from sector buffer read from diskette image.
 * Debug routine for briefly inspecting and checking data in a sector.
 * Sector: (1 byte sector number) (1 byte track number) (128 bytes data) (2 byte back ptr) (2 byte fwd ptr) (2 byte crc) 
 * Sector number always has the high bit set. In total each sector is 136 bytes.
 */
void print_sector(unsigned char *sbuf)
    {
    int dump_index;

    printf("sect,track: %02d,%02d ", (sbuf[0] & 0x7f), sbuf[1]);
    for (dump_index = 2; dump_index < 10; dump_index++)
        {
        printf(" %02x", sbuf[dump_index]);
        }
    printf("  ");
    for (dump_index = 2; dump_index < 10; dump_index++)
        {
        if ((0x20 <= sbuf[dump_index]) && (sbuf[dump_index] < 0x7f))
            printf("%c", sbuf[dump_index]);
        else
            printf(".");
        }
    printf(" back: %02d,%02d fwd: %02d,%02d\n", sbuf[130], sbuf[131], sbuf[132], sbuf[133]);
    }

/* Go through file
 */
void file_walk(char *fname, unsigned char *dbuf)
    {
    int first_rec_sector;
    int first_rec_track;
    int last_rec_sector;
    int last_rec_track;
    int previous_rec_sector;
    int previous_rec_track;
    int curr_rec_sector;
    int curr_rec_track;
    int sectors_per_record;
    int sec_in_rec_cnt;
    int rec_cnt;
    int record_count;
    int record_length;
    int last_record_length;
    int sector_cntr = 0;
    unsigned char *rec_ptr;

    if (strncmp(fname, "DIRECTORY", 9) == 0)
        return;
    record_count = dbuf[2 + 13] + 256 * dbuf[2 + 14];
    record_length = dbuf[2 + 15] + 256 * dbuf[2 + 16];
    last_record_length = dbuf[2 + 22] + 256 * dbuf[2 + 23];
    sectors_per_record = record_length / 128;
    first_rec_sector = dbuf[2 + 8];
    first_rec_track = dbuf[2 + 9];
    last_rec_sector = dbuf[2 + 10];
    last_rec_track = dbuf[2 + 11];
    if (verbose_flag)
        {
        printf("Go through file: %s\n", fname);
        printf("  Record count: %d\n", record_count);
        printf("  Record length: %d, sectors per record: %d\n", record_length, sectors_per_record);
        printf("  Last record length: %d\n", last_record_length);
        printf("  First record: %d,%d\n", first_rec_sector, first_rec_track);
        printf("  Last record: %d,%d\n", last_rec_sector, last_rec_track);
        }
    curr_rec_sector = first_rec_sector;
    curr_rec_track = first_rec_track;

    if (export_flag)
        {
        if (createdir_flag)
            snprintf(image_path_filename, sizeof(image_path_filename), "%s/%s", disk_directory, fname);
        else
            snprintf(image_path_filename, sizeof(image_path_filename), "%s", fname);
        if ((exportfd = fopen(image_path_filename, "w")) == NULL)
            exit(EXIT_FAILURE);
        }
    previous_rec_sector = -1;
    previous_rec_track = -1;

    for (rec_cnt = 0; rec_cnt < record_count; rec_cnt++)
        {
        rec_ptr = file_rec_buf;
        for (sec_in_rec_cnt = 0; sec_in_rec_cnt < sectors_per_record; sec_in_rec_cnt++)
            {
            if (read_sector(file_sec_buf, curr_rec_sector + sec_in_rec_cnt, curr_rec_track) == 0)
                return;
            sector_cntr += 1;
            if (verbose_flag)
                print_sector(file_sec_buf);
            memcpy(rec_ptr, &file_sec_buf[2], 128);
            rec_ptr += 128;
            }
         if (export_flag)
            {
            if ((rec_cnt + 1) == record_count)
                fwrite(file_rec_buf, last_record_length, 1, exportfd);
            else
                fwrite(file_rec_buf, record_length, 1, exportfd);
            }
        /* Check that backward pointers are correct, not very reliable yet */
        if (backptr_flag && 0 < previous_rec_sector && 0 < previous_rec_track)
            {
            if ((previous_rec_sector != file_sec_buf[130]) || (previous_rec_track != file_sec_buf[131]))
                {
                prt_imgf_zdosf();
                printf("Invalid backward pointer: %02d,%02d in sector: %02d,%02d\n",
                    file_sec_buf[130], file_sec_buf[131],
                    file_sec_buf[132], file_sec_buf[133]);
                }
            }
        /* Save current sector and track number */
        previous_rec_sector = curr_rec_sector;
        previous_rec_track = curr_rec_track;
        /* Get sector and track number for next sector to read */
        curr_rec_sector = file_sec_buf[132];
        curr_rec_track = file_sec_buf[133];
        if ((curr_rec_sector == 0xff) && (curr_rec_track == 0xff))
            break;
        }
    /* Check if last read sector,track is same as last sector,track in header */
    if (analyze_flag)
        {
        if ((previous_rec_sector != last_rec_sector) || (previous_rec_track != last_rec_track))
            {
            prt_imgf_zdosf();
            printf("Last read sector and track: %02d,%02d in file is not same as in header: %02d,%02d\n",
                    previous_rec_sector, previous_rec_track,
                    last_rec_sector, last_rec_track);
            }        
        }
    if (verbose_flag)
        {
        printf("Sectors in file: %d, ", sector_cntr);
        printf("records in file: %d, ", sector_cntr / sectors_per_record);
        printf("record count in file header: %d\n", record_count);
        }
    if (exportfd)
        fclose(exportfd);
    }

/* Go through directory entries
 */
void directory_walk()
    {
    int get_entries = 1;
    int sector = 5;
    int track = 22;
    int dirent_len;
    int des_sector;
    int des_track;
    int ptr_idx;
    int segdes_cnt;
    int segdes_idx;
    unsigned char *dir_ptr;

    while (get_entries)
        {
        /* Read directory sector */
        if (read_sector(sect_buf, sector, track) == 0)
            return;
        if (sect_buf[2] == 0xff) /* end of directory sectors */
            {
            get_entries = 0;
            break;
            }
        /* Go through entries in directory sector */
        for (dir_ptr = &sect_buf[2]; (0x7f & *dir_ptr) && (*dir_ptr != 0xff);)
            {
            dirent_len = 0x7f & *dir_ptr++;
            memset(zdos_filename, 0, sizeof(zdos_filename));
            for (ptr_idx = 0; ptr_idx < dirent_len; ptr_idx++, dir_ptr++)
                zdos_filename[ptr_idx] = *dir_ptr;
            zdos_filename[ptr_idx] = 0;
            des_sector = *dir_ptr++;
            des_track = *dir_ptr++;
            if ((file_name && (strncmp(zdos_filename, file_name, 32) == 0) || (file_name == NULL)))
                {
                if ((strncmp(zdos_filename, "DIRECTORY", 9) != 0) || descriptor_flag)
                    {
                    if (!analyze_flag)
                        printf("%s\n", zdos_filename);
                    }
                if (read_sector(des_buf, des_sector, des_track) == 0)
                    return;
                /* Check that the header seems correct */
                if ((des_buf[2 + 0] | des_buf[2 + 1] | des_buf[2 + 2] | des_buf[2 + 3] | des_buf[2 + 4] | des_buf[2 + 5]) != 0)
                    {
                    prt_imgf_zdosf();
                    printf("Invalid file header\n");
                    if (!descriptor_flag)
                        continue;
                    }
                if (descriptor_flag)
                    {
                    printf("  Reserved: 0x%02x 0x%02x 0x%02x 0x%02x\n", des_buf[2 + 0], des_buf[2 + 1], des_buf[2 + 2], des_buf[2 + 3]);
                    printf("  File ID: 0x%02x 0x%02x\n", des_buf[2 + 4], des_buf[2 + 5]);
                    printf("  Directory sector: %d,%d\n", des_buf[2 + 6], des_buf[2 + 7]);
                    printf("  First record: %d,%d\n", des_buf[2 + 8], des_buf[2 + 9]);
                    printf("  Last record: %d,%d\n", des_buf[2 + 10], des_buf[2 + 11]);
                    printf("  File type and subtype: 0x%02x, ", des_buf[2 + 12]);
                    if (des_buf[2 + 12] & 0x80)
                        printf("Procedure");
                    if (des_buf[2 + 12] & 0x40)
                        printf("Directory");
                    if (des_buf[2 + 12] & 0x20)
                        printf("ASCII text");
                    if (des_buf[2 + 12] & 0x10)
                        printf("Data");
                    printf(", subtype: %d\n", des_buf[2 + 12] & 0x0f);
                    printf("  Record count: %d\n", des_buf[2 + 13] + 256 * des_buf[2 + 14]);
                    printf("  Record length: %d\n", des_buf[2 + 15] + 256 * des_buf[2 + 16]);
                    printf("  Block length: %d\n", des_buf[2 + 17] + 256 * des_buf[2 + 18]);
                    printf("  File properties: 0x%02x\n", des_buf[2 + 19]);
                    if (des_buf[2 + 12] & 0x80)
                        printf("  Procedure start address: 0x%04x\n", des_buf[2 + 20] + 256 * des_buf[2 + 21]);
                    printf("  Bytes in last record: %d\n", des_buf[2 + 22] + 256 * des_buf[2 + 23]);
                    printf("  Date of creation: %.6s\n", &des_buf[2 + 24]);
                    printf("  Date of last modification: %.6s\n", &des_buf[2 + 32]);
                    if (des_buf[2 + 12] & 0x80)
                        {
                        printf("  Segment descriptors\n");
                        for (segdes_cnt = 0; segdes_cnt <= 16; segdes_cnt++)
                            {
                            segdes_idx = 2 + 40 + (4 * segdes_cnt);
                            if ((des_buf[segdes_idx] + 256 * des_buf[segdes_idx + 1]) == 0)
                                break;
                            printf("    Segment %d: start address: 0x%04x, length: 0x%04x\n",
                                segdes_cnt,
                                des_buf[segdes_idx] + 256 * des_buf[segdes_idx + 1],
                                des_buf[segdes_idx + 2] + 256 * des_buf[segdes_idx + 3]);
                            }
                        printf("  Lowest segment starting address: 0x%04x\n", des_buf[2 + 122] + 256 * des_buf[2 + 123]);
                        printf("  Highest segment ending address: 0x%04x\n", des_buf[2 + 124] + 256 * des_buf[2 + 125]);
                        printf("  Stack size: 0x%04x\n", des_buf[2 + 126] + 256 * des_buf[2 + 127]);
                        }
                    }
                file_walk(zdos_filename, des_buf);
                }
            }
        sector = sect_buf[132];
        track = sect_buf[133];
        }
    }

/* Version information
 */
void version(int status)
    {
    printf("Compiled: %s\n", COMPILE_TIME);
    printf("Git branch: %s\n", GIT_BRANCH);
    printf("Git hash: %s\n", GIT_HASH);
    exit(status);
    }

/* Usage information
 */
void usage(int status)
    {

    if (status != EXIT_SUCCESS)
        fprintf(stderr, "Try `%s --help' for more information.\n", program_name);
    else
        {
        printf("Usage: %s [OPTIONS] [IMAGEFILES]...\n", program_name);
        fputs("\
Tool to list and export RIO files from Zilog ZDOS diskette image files\n\
\n\
  -a, --analyze         analyze the content of the image file and report errors\n\
  -b, --backptr         analyze back pointers in sectors and report errors\n\
  -c, --createdir       create a directory for each imagefile\n\
  -d, --descriptor      print file descriptor information\n\
  -e, --export          export files from diskette image\n\
  -f, --file [NAME]     name of the file if single file is listed or exported\n\
  -h, --help            show help\n\
  -i, --ignore          ignore if sector or track numbers do not match what\n\
                        is read from the diskette image\n\
  -V, --version         show version\n\
  -v, --verbose         show details\n", stdout);
        }
    exit(status);
    }

/* Analyze Zilog ZDOS diskette image file
 */
int main(int argc, char **argv)
    {
    int optchr;
    struct stat st = {0};

    static struct option long_options[] =
        {
            {"analyze",    no_argument,       NULL, 'a'},
            {"backptr",    no_argument,       NULL, 'b'},
            {"createdir",  no_argument,       NULL, 'c'},
            {"descriptor", no_argument,       NULL, 'd'},
            {"export",     no_argument,       NULL, 'e'},
            {"file",       required_argument, NULL, 'f'},
            {"help",       no_argument,       NULL, 'h'},
            {"ignore",     no_argument,       NULL, 'i'},
            {"version",    no_argument,       NULL, 'V'},
            {"verbose",    no_argument,       NULL, 'v'},
            {NULL, 0, NULL, 0}
        };
        /* getopt_long stores the option index here. */
    int option_index = 0;

    program_name = basename(argv[0]);
    while ((optchr = getopt_long(argc, argv, "abcdef:hiVv", long_options, NULL)) != -1)
        {
        switch (optchr)
            {
        case 'a':
            analyze_flag = 1;
            break;
        case 'b':
            backptr_flag = 1;
            break;
        case 'c':
            createdir_flag = 1;
            break;
        case 'd':
            descriptor_flag = 1;
            break;
        case 'e':
            export_flag = 1;
            break;
        case 'f':
            file_name = optarg;
            break;
        case 'h':
            usage(EXIT_SUCCESS);
            break;
        case 'i':
            ignore_flag = 1;
            break;
        case 'V':
            version(EXIT_SUCCESS);
            break;
        case 'v':
            verbose_flag = 1;
            break;
        default:
            usage(EXIT_FAILURE);
            }
        }

    if (optind >= argc)
        {
        fprintf(stderr, "At least one [IMAGEFILE] should be given\n");
        exit(EXIT_FAILURE);
        }

    for (; optind < argc; optind++)
        {
        snprintf(image_filename, sizeof(image_filename), "%s", argv[optind]);
        if (createdir_flag && export_flag)
            {
            snprintf(disk_directory, sizeof(disk_directory), "%s.dir", basename(image_filename));
            struct stat st = {0};
            if (stat(disk_directory, &st) == -1)
                {
                if  (mkdir(disk_directory, 0755) == -1)
                    exit(EXIT_FAILURE);
                }
            }
        if ((imagefd = fopen(image_filename, "r")) == NULL)
            {
            fprintf(stderr, "Can't open file: %s\n", image_filename);
            exit(EXIT_FAILURE);
            }
        if (0 != fseek(imagefd, 0, SEEK_END))
            {
            fclose(imagefd);
            exit(EXIT_FAILURE);
            }
        filesize = ftell(imagefd);
        if (export_flag)
            printf("Exporting files from: %s\n", image_filename);
        if (createdir_flag && export_flag)
            printf("into directory: %s\n", disk_directory);
        if (verbose_flag)
            printf("File size: %ld\n", filesize);
        if (filesize == 339456) /* file from https://datamuseum.dk/wiki/Bits:Keyword/COMPANY/ZILOG */
            endtrack = 78;
        else if (filesize == 335104) /* file from https://github.com/sebhc/sebhc/tree/master/mcz#software-disk-images */
            endtrack = 77;
        else
            {
            fclose(imagefd);
            fprintf(stderr, "Invalid file size\n");
            exit(EXIT_FAILURE);
            }
        directory_walk();
        fclose(imagefd);
        }
    exit(EXIT_SUCCESS);
    }

