// Baghand - zip to gz.tar converter. Converts a zip file to a tarball of gzipped files without decompressing anything.


#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>


//wow, I didn't realize tarball headers were so huge
struct tar_posix_header
{                                 /* byte offset */
	unsigned char name[100];               /*   0 */
	unsigned char mode[8];                 /* 100 */
	unsigned char uid[8];                  /* 108 */
	unsigned char gid[8];                  /* 116 */
	unsigned char size[12];                /* 124 */
	unsigned char mtime[12];               /* 136 */
	unsigned char chksum[8];               /* 148 */
	unsigned char typeflag;                /* 156 */
	unsigned char linkname[100];           /* 157 */
	unsigned char magic[6];                /* 257 */
	unsigned char version[2];              /* 263 */
	unsigned char uname[32];               /* 265 */
	unsigned char gname[32];               /* 297 */
	unsigned char devmajor[8];             /* 329 */
	unsigned char devminor[8];             /* 337 */
	unsigned char prefix[155];             /* 345 */
								  /* 500 */
};

#define GZ_MAGIC 0x8b1f
#define GZ_METHOD_DEFLATE 0x08

#define GZ_FLAGS_EXT        0x01 // ascii?
#define GZ_FLAGS_HEADER_CRC 0x02
#define GZ_FLAGS_EXTRA      0x04
#define GZ_FLAGS_NAME_TRUNC 0x08
#define GZ_FLAGS_ENCRYPTED  0x10
#define GZ_FLAGS_RESTRICTED 0xE0

struct gz_header
{
	uint16_t magic;
	uint8_t  method;
	uint8_t  flags; //encrypted, restricted, extra_feild, header_crc, truncated_name
	uint32_t stamp;
	uint8_t  extra;
	uint8_t  os;
};


#define ZIP_FILE_MAGIC 0x04034b50
#define ZIP_CD_MAGIC   0x02014b50
#define ZIP_EOCD_MAGIC 0x06054b50

struct zip_local_file
{
	uint32_t magic;
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
	unsigned char *fname;
	unsigned char *extra;
};

/*
Offset 	Bytes 	Description[24]
0 	4 	Local file header signature = 0x04034b50 (read as a little-endian number)
4 	2 	Version needed to extract (minimum)
6 	2 	General purpose bit flag
8 	2 	Compression method
10 	2 	File last modification time
12 	2 	File last modification date
14 	4 	CRC-32
18 	4 	Compressed size
22 	4 	Uncompressed size
26 	2 	File name length (n)
28 	2 	Extra field length (m)
30 	n 	File name
30+n 	m 	Extra field
*/

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
	int i;
	int fd[2];
	uint32_t offset = 0;
	unsigned char fname[512];

	struct stat stats[2];

	fd[0] = open(argv[1], O_RDONLY, 0);
	fd[1] = open(argv[2], O_WRONLY, 0);

	fstat(fd[0], &stats[0]);
	fstat(fd[1], &stats[1]);

	lseek(fd[0], -22, SEEK_END);
	struct zip_directory zip_dir = {0};
	struct zip_eocd zip_footer = {0};

	// Locate the End of Central Directory header (located at the end of the file)
	offset = 22;
	while (!validate_eocd(&zip_footer, offset))
	{
		read(fd[0], &zip_footer, 22);
		lseek(fd[0], -23, SEEK_CUR);
		offset += 1;
	}

	// Jump to the beginning of the Central Directories
	lseek(fd[0], zip_footer.central_dir_offset, SEEK_SET);

	// Iterate through all the Central Directories
	for (i = 0; i < zip_footer.total_central_records; i++)
	{
		read(fd[0], &zip_dir, 46);
		if (zip_dir.magic == ZIP_CD_MAGIC) // Just for sanity
		{
			read(fd[0], &fname, zip_dir.fname_len);
			fname[zip_dir.fname_len] = 0;
			write(1, fname, zip_dir.fname_len);
			write(1, "\n", 1);

			// jump to the file and extract it
			int pos = lseek(fd[0], 0, SEEK_CUR);
			lseek(fd[0], zip_dir.offset, SEEK_SET);

			struct zip_local_file file_header;
			read(fd[0], &file_header, 30);
			lseek(fd[0], zip_dir.fname_len + file_header.extra_len, SEEK_CUR);
			unsigned char *buffer;
			buffer = malloc(zip_dir.zip_size);
			read(fd[0], buffer, zip_dir.zip_size);
			int zfd = open(fname, O_WRONLY | O_CREAT, 0);

			struct gz_header gzh;
			gzh.magic = GZ_MAGIC;
			gzh.method = GZ_METHOD_DEFLATE;
			gzh.flags = 0;
			write(zfd, &gzh, sizeof(struct gz_header)-2);

			write(zfd, buffer, zip_dir.zip_size);
			close(zfd);

			free(buffer);

			lseek(fd[0], pos, SEEK_SET);
			// -------------------------------
		}
		lseek(fd[0], zip_dir.extra_len + zip_dir.comment_len, SEEK_CUR);
//		exit(1);
	}

	write(1, "It's a zip\n", 11);
}

