// Baghand - zip to gz.tar converter. Converts a zip file to a tarball of gzipped files without decompressing anything.


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
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
#define ZIP_ALG_STORE 0
#define ZIP_ALG_DEFLATE 8
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
inline int validate_eocd(struct zip_eocd *eocd, uint32_t offset)
{
	return (eocd->magic == ZIP_EOCD_MAGIC) && ((offset-23) == eocd->comment_len);
}

void tar_write(unsigned char *fname, int zip_fd, int tar_fd, struct zip_directory *dir_entry)
{
	struct zip_local_file file_entry;
	unsigned char *buffer; // used for the compressed file data
	struct gz_header header = {0};
	struct gz_footer footer = {0};
	int gz_fd = open(fname, O_WRONLY | O_CREAT, 0);
	buffer = malloc(dir_entry->zip_size);

	header.magic = GZ_MAGIC;
	header.method = GZ_METHOD_DEFLATE;
	header.flags = 0;
	header.os = GZ_OS_LINUX;

	footer.crc = dir_entry->crc32;
	footer.isize = dir_entry->unzip_size;

	// jump to the file and extract it
	int pos = lseek(zip_fd, 0, SEEK_CUR);
	lseek(zip_fd, dir_entry->offset, SEEK_SET);

	read(zip_fd, &file_entry, 30);
	lseek(zip_fd, file_entry.fname_len + file_entry.extra_len, SEEK_CUR);
	read(zip_fd, buffer, dir_entry->zip_size);

	write(gz_fd, &header, sizeof(struct gz_header));

	write(gz_fd, buffer, dir_entry->zip_size);

	write(gz_fd, &footer, 8);

	close(gz_fd);
	free(buffer);

	lseek(zip_fd, pos, SEEK_SET);
}

void gz_create(unsigned char *fname, int zip_fd, struct zip_directory *dir_entry)
{
	struct zip_local_file file_entry;
	unsigned char *buffer; // used for the compressed file data
	struct gz_header header = {0};
	struct gz_footer footer = {0};
	int gz_fd = open(fname, O_WRONLY | O_CREAT, 0);
	buffer = malloc(dir_entry->zip_size);

	header.magic = GZ_MAGIC;
	header.method = GZ_METHOD_DEFLATE;
	header.flags = 0;
	header.os = GZ_OS_LINUX;

	footer.crc = dir_entry->crc32;
	footer.isize = dir_entry->unzip_size;

	// jump to the file and extract it
	int pos = lseek(zip_fd, 0, SEEK_CUR);
	lseek(zip_fd, dir_entry->offset, SEEK_SET);

	read(zip_fd, &file_entry, 30);
	lseek(zip_fd, file_entry.fname_len + file_entry.extra_len, SEEK_CUR);
	read(zip_fd, buffer, dir_entry->zip_size);

	write(gz_fd, &header, sizeof(struct gz_header));

	write(gz_fd, buffer, dir_entry->zip_size);

	write(gz_fd, &footer, 8);

	close(gz_fd);
	free(buffer);

	lseek(zip_fd, pos, SEEK_SET);
}

int zip_locate_eocd(int zip_fd, struct zip_eocd *zip_footer)
{
	uint32_t offset = 22;
	lseek(zip_fd, -(int)offset, SEEK_END);

	// Locate the End of Central Directory header (located at the end of the file)
	while (!validate_eocd(zip_footer, offset))
	{
		read(zip_fd, zip_footer, 22);
		if (!lseek(zip_fd, -23, SEEK_CUR))
			return -1;
		offset += 1;
	}

	return offset;
}

int main(int argc, char **argv)
{
	int i;
	int fd[2];
	uint32_t offset = 0;
	unsigned char fname[512];

//	struct stat stats[2];

	fd[0] = open(argv[1], O_RDONLY, 0);
	fd[1] = open(argv[2], O_WRONLY, 0);

//	fstat(fd[0], &stats[0]);
//	fstat(fd[1], &stats[1]);

	struct zip_directory zip_dir = {0};
	struct zip_eocd zip_footer = {0};

	// Locate the End of Central Directory header (located at the end of the file)
	offset = zip_locate_eocd(fd[0], &zip_footer);

	// Jump to the beginning of the Central Directories
	lseek(fd[0], zip_footer.central_dir_offset, SEEK_SET);

	// Iterate through all the Central Directories
	for (i = 0; i < zip_footer.total_central_records; i++)
	{
		read(fd[0], &zip_dir, 46);
		if (zip_dir.magic == ZIP_CD_MAGIC) // Just for sanity
		{
			read(fd[0], &fname, zip_dir.fname_len);
			fname[zip_dir.fname_len+0] = '.';
			fname[zip_dir.fname_len+1] = 'g';
			fname[zip_dir.fname_len+2] = 'z';
			fname[zip_dir.fname_len+3] = 0;
			write(1, fname, zip_dir.fname_len);
			write(1, ".gz\n", 4);

			gz_create(fname, fd[0], &zip_dir);

			// -------------------------------
		}
		lseek(fd[0], zip_dir.extra_len + zip_dir.comment_len, SEEK_CUR);
//		exit(1);
	}
}

