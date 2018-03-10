// Baghand - zip to gz.tar converter. Converts a zip file to a tarball of gzipped files without decompressing anything.


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>


//wow, I didn't realize tarball headers were so huge, or all in ascii...
struct tar_posix_header
{
	unsigned char name[100];
	unsigned char mode[8];
	unsigned char uid[8];
	unsigned char gid[8];
	unsigned char size[12];
	unsigned char mtime[12];
	unsigned char chksum[8];
	unsigned char typeflag;
	unsigned char linkname[100];
	unsigned char magic[6];
	unsigned char version[2];
	unsigned char uname[32];
	unsigned char gname[32];
	unsigned char devmajor[8];
	unsigned char devminor[8];
	unsigned char prefix[155];
	unsigned char pad[12];
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
	uint32_t crc32;
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

void octal(int n, unsigned char *buffer, int len)
{
	int i = 0;
	while (i<len)
	{
		buffer[len-1-i] = (n & 7) + '0';
		i++;
		n = n>>3;
	}
}

static unsigned char padding[512] = {0};

void tar_set_checksum(struct tar_posix_header *header)
{
	uint32_t sum = 0;
	int i;
	unsigned char *header_data = (unsigned char *)header;
	for (i=0; i<8; i++)
		header->chksum[i] = ' ';

	for (i=0;i<sizeof(struct tar_posix_header); i++)
		sum += header_data[i];

	octal(sum, header->chksum, 6);
	header->chksum[6] = '\0';
}

void tar_write(unsigned char *fname, int zip_fd, int tar_fd, struct zip_directory *dir_entry)
{
	struct zip_local_file file_entry;
	unsigned char *buffer; // used for the compressed file data
	struct tar_posix_header tar_header = {0};
	struct gz_header header = {0};
	struct gz_footer footer = {0};
	int i;
	buffer = malloc(dir_entry->zip_size);

	// tar headers
	if (dir_entry->fname_len < 98)
		for (i=0; fname[i] != 0; i++)
				tar_header.name[i] = fname[i];
	else
		for (i=0; fname[i] != 0; i++)
			if (i < 100)
				tar_header.prefix[i] = fname[i];
			else
				tar_header.name[i-100] = fname[i];

	for (i=0; i < 7; i++)
	{
		tar_header.mode[i] = '0';
		tar_header.gid[i] = '0';
		tar_header.uid[i] = '0';
		tar_header.mtime[i] = '0';
		tar_header.chksum[i] = '0';
	}
	tar_header.typeflag = '0';

	octal(dir_entry->zip_size + sizeof(struct gz_header) + 8, tar_header.size, 11);

	tar_header.magic[0] = 'u';
	tar_header.magic[1] = 's';
	tar_header.magic[2] = 't';
	tar_header.magic[3] = 'a';
	tar_header.magic[4] = 'r';
	tar_header.magic[5] = ' ';
	tar_header.magic[6] = ' ';
//	tar_header.version[0] = '0';
//	tar_header.version[1] = '0';

	tar_set_checksum(&tar_header);

	// gz headers
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

	write(tar_fd, &tar_header, sizeof(struct tar_posix_header));
	write(tar_fd, &header, sizeof(struct gz_header));
	write(tar_fd, buffer, dir_entry->zip_size);
	write(tar_fd, &footer, 8);
	write(tar_fd, padding, 512 - ((sizeof(struct tar_posix_header) + sizeof(struct gz_header) + dir_entry->zip_size + 8) % 512));

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
		if (lseek(zip_fd, -23, SEEK_CUR) == -1)
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

	if (argc < 3)
	{
		printf("You are not argumentative enough to use this program.\n");
		exit(1);
	}

	fd[0] = open(argv[1], O_RDONLY, 0);
	if (fd[0] == -1)
	{
		printf("A zip file is required.\n");
		exit(1);
	}

	fd[1] = open(argv[2], O_WRONLY | O_CREAT, 0);
	if (fd[1] == -1)
	{
		printf("Could not save tar file.\n");
		exit(1);
	}

	struct zip_directory zip_dir = {0};
	struct zip_eocd zip_footer = {0};

	// Locate the End of Central Directory header (located at the end of the file)
	offset = zip_locate_eocd(fd[0], &zip_footer);
	if (offset == -1)
	{
		printf("This does not appear to be a zip file.\n");
		exit(1);
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
			fname[zip_dir.fname_len+0] = '.';
			fname[zip_dir.fname_len+1] = 'g';
			fname[zip_dir.fname_len+2] = 'z';
			fname[zip_dir.fname_len+3] = 0;
			write(1, fname, zip_dir.fname_len);
			write(1, ".gz\n", 4);

			tar_write(fname, fd[0], fd[1], &zip_dir);

			// -------------------------------
		}
		lseek(fd[0], zip_dir.extra_len + zip_dir.comment_len, SEEK_CUR);
	}
	close(fd[0]);
	close(fd[1]);

	exit(0);
}

