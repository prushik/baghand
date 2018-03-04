// Baghand - zip to gz.tar converter. Converts a zip file to a tarball of gzipped files without decompressing anything.


#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define GZ_FLAGS_ENCRYPTED  0
#define GZ_FLAGS_RESTRICTED 0
#define GZ_FLAGS_EXTRA      0
#define GZ_FLAGS_HEADER_CRC 0
#define GZ_FLAGS_NAME_TRUNC 0

//wow, I didn't realize tarball headers were so huge
struct tar_posix_header
{                                 /* byte offset */
	char name[100];               /*   0 */
	char mode[8];                 /* 100 */
	char uid[8];                  /* 108 */
	char gid[8];                  /* 116 */
	char size[12];                /* 124 */
	char mtime[12];               /* 136 */
	char chksum[8];               /* 148 */
	char typeflag;                /* 156 */
	char linkname[100];           /* 157 */
	char magic[6];                /* 257 */
	char version[2];              /* 263 */
	char uname[32];               /* 265 */
	char gname[32];               /* 297 */
	char devmajor[8];             /* 329 */
	char devminor[8];             /* 337 */
	char prefix[155];             /* 345 */
								  /* 500 */
};

struct gz_header
{
	uint16_t magic;
	uint8_t  method;
	uint8_t  flags; //encrypted, restricted, extra_feild, header_crc, truncated_name
	uint32_t stamp;
	uint8_t  extra;
	uint8_t  os;
};

struct zip_directory
{
	uint32_t magic;
	uint16_t version;
	uint16_t min_version;
	uint16_t flags;
	uint16_t compression;
	uint16_t mtime;
	uint16_t mdate;
	uint32_t crc32;
	uint32_t zip_size;
	uint32_t unzip_size;
	uint16_t fname_len;
	uint16_t extra_len;
	uint16_t comment_len;
	uint16_t disk;
	uint16_t iattr;
	uint32_t eattr;
	uint16_t offset;
	unsigned char *fname;
	unsigned char *extra;
	unsigned char *comment;
};
/*
0 	4 	Central directory file header signature = 0x02014b50
4 	2 	Version made by
6 	2 	Version needed to extract (minimum)
8 	2 	General purpose bit flag
10 	2 	Compression method
12 	2 	File last modification time
14 	2 	File last modification date
16 	4 	CRC-32
20 	4 	Compressed size
24 	4 	Uncompressed size
28 	2 	File name length (n)
30 	2 	Extra field length (m)
32 	2 	File comment length (k)
34 	2 	Disk number where file starts
36 	2 	Internal file attributes
38 	4 	External file attributes
42 	4 	Relative offset of local file header. This is the number of bytes between the start of the first disk on which the file occurs, and the start of the local file header. This allows software reading the central directory to locate the position of the file inside the ZIP file.
46 	n 	File name
46+n 	m 	Extra field
46+n+m 	k 	File comment
* */
#define ZIP_EOCD_MAGIC 0x06054b50
struct zip_eocd
{
	uint32_t magic;
	uint16_t disk_num;
	uint16_t main_disk;
	uint16_t central_records;
	uint16_t total_central_records;
	uint32_t central_dir_size;
	uint32_t central_dir_offset;
	uint16_t comment_len;
	unsigned char *comment;
};

// this function checks the zip magic and the comment length to see if 
// the eocd structure has been located
int validate_eocd(struct zip_eocd *eocd, uint32_t offset)
{
	return (eocd->magic == ZIP_EOCD_MAGIC) && ((offset-23) == eocd->comment_len);
}

int main(int argc, char **argv)
{
	int fd[2];
	uint32_t offset = 0;

	struct stat stats[2];

	fd[0] = open(argv[1], O_RDONLY, 0);
	fd[1] = open(argv[2], O_WRONLY, 0);

	fstat(fd[0], &stats[0]);
	fstat(fd[1], &stats[1]);

	lseek(fd[0], -22, SEEK_END);
	struct zip_eocd zip_footer = {0};

	offset = 22;
	while (!validate_eocd(&zip_footer, offset))
	{
		read(fd[0], &zip_footer, 22);
		lseek(fd[0], -23, SEEK_CUR);
		offset += 1;
	}
	write(1, "It's a zip\n", 11);
	printf("Offset from end: %d\nComment: %d bytes\n", offset, zip_footer.comment_len);
}
