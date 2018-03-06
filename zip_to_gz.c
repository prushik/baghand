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
#define GZ_OS_LINUX 0x03


#define GZ_FLAGS_TEXT       0x01 // ascii?
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
}__attribute__((packed));

struct gz_footer
{
	uint32_t crc;
	uint32_t isize;
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
}__attribute__((packed));

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
	uint32_t crc32; // padding is the problem here...
	uint32_t zip_size;
	uint32_t unzip_size;
	uint16_t fname_len;
	uint16_t extra_len;
	uint16_t comment_len;
	uint16_t disk;
	uint16_t iattr;
	uint32_t eattr; // data alignment is the problem here...
	uint32_t offset;
	unsigned char *fname;
	unsigned char *extra;
	unsigned char *comment;
}__attribute__((packed));
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

void gz_create(unsigned char * fname, unsigned char *buffer, int len)
{
	int zfd = open(fname, O_WRONLY | O_CREAT, 0);

	struct gz_header gzh;
	gzh.magic = GZ_MAGIC;
	gzh.method = GZ_METHOD_DEFLATE;
	gzh.flags = 0;
	write(zfd, &gzh, sizeof(struct gz_header)-2);

//	write(zfd, buffer, zip_dir.zip_size);
	close(zfd);

	free(buffer);
}

static uint32_t crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t crc32(uint32_t crc, const void *buf, size_t size)
{
	const uint8_t *p;

	p = buf;
	crc = ~crc;

	while (size--)
		crc = crc32_tab[(crc ^ *p++) & 0xff] ^ (crc >> 8);

	return ~crc;
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
			gzh.os = GZ_OS_LINUX;
			write(zfd, &gzh, sizeof(struct gz_header)-2);

			write(zfd, buffer, zip_dir.zip_size);

			struct gz_footer footer = {0};
			footer.crc = crc32(0, buffer, zip_dir.zip_size);
			footer.isize = zip_dir.unzip_size;
			write(zfd, &footer, 8);
			close(zfd);

			free(buffer);

			lseek(fd[0], pos, SEEK_SET);
			// -------------------------------
		}
		lseek(fd[0], zip_dir.extra_len + zip_dir.comment_len, SEEK_CUR);
//		exit(1);
	}
}

