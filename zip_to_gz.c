// Baghand - zip to gz.tar converter. Converts a zip file to a tarball of gzipped files without decompressing anything.


#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

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
	
}

int main(int argc, char **argv)
{
	int fd[2];

	struct stat stats[2];

	fd[0] = open(argv[1], O_RDONLY, 0);
	fd[1] = open(argv[2], O_WRONLY, 0);

	fstat(fd[0], &stats[0]);
	fstat(fd[1], &stats[1]);

	lseek(fd[0], -22, SEEK_END);
	struct zip_eocd zip_footer = {0};

	while (zip_footer.magic != ZIP_EOCD_MAGIC)
	{
		read(fd[0], &zip_footer, 4);
		lseek(fd[0], -5, SEEK_CUR);
	}
	write(1, "Its a zip\n", 10);
}
