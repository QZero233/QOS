/*
 * JOS file system format
 */

// We don't actually want to define off_t!
#define off_t xxx_off_t
#define bool xxx_bool
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#undef off_t
#undef bool

// Prevent inc/types.h, included from inc/fs.h,
// from attempting to redefine types defined in the host's inttypes.h.
#define JOS_INC_TYPES_H
// Typedef the types that inc/mmu.h needs.
typedef uint32_t physaddr_t;
typedef uint32_t off_t;
typedef int bool;

// #include <inc/mmu.h>
// #include <inc/fs.h>

// Bytes per file system block - same as page size
#define BLKSIZE		4096
#define BLKBITSIZE	(BLKSIZE * 8)

// Maximum size of a filename (a single path component), including null
// Must be a multiple of 4
#define MAXNAMELEN	128

// Maximum size of a complete pathname, including null
#define MAXPATHLEN	1024

// Number of block pointers in a File descriptor
#define NDIRECT		10
// Number of direct block pointers in an indirect block
#define NINDIRECT	(BLKSIZE / 4)

#define MAXFILESIZE	((NDIRECT + NINDIRECT) * BLKSIZE)

#define ROUNDUP(n, v) ((n) - 1 + (v) - ((n) - 1) % (v))
#define MAX_DIR_ENTS 128

// An inode block contains exactly BLKFILES 'struct File's
#define BLKFILES	(BLKSIZE / sizeof(struct File))

// File types
#define FTYPE_REG	0	// Regular file
#define FTYPE_DIR	1	// Directory


// File system super-block (both in-memory and on-disk)

#define FS_MAGIC	0x4A0530AE	// related vaguely to 'J\0S!'

struct File {
	char f_name[MAXNAMELEN];	// filename
	off_t f_size;			// file size in bytes
	uint32_t f_type;		// file type

	// Block pointers.
	// A block is allocated iff its value is != 0.
	uint32_t f_direct[NDIRECT];	// direct blocks
	uint32_t f_indirect;		// indirect block

	// Pad out to 256 bytes; must do arithmetic in case we're compiling
	// fsformat on a 64-bit machine.
	uint8_t f_pad[256 - MAXNAMELEN - 8 - 4*NDIRECT - 4];
} __attribute__((packed));	// required only on some 64-bit machines

struct Super {
	uint32_t s_magic;		// Magic number: FS_MAGIC
	uint32_t s_nblocks;		// Total number of blocks on disk
	struct File s_root;		// Root directory node
};

struct Dir
{
	struct File *f;
	struct File *ents;
	int n;
};

uint32_t nblocks;
int diskfd;
char *diskmap, *diskpos;
struct Super *super;
uint32_t *bitmap;

void
panic(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	abort();
}

void
readn(int f, void *out, size_t n)
{
	size_t p = 0;
	while (p < n) {
		ssize_t m = read(f, out + p, n - p);
		if (m < 0)
			panic("read: %s", strerror(errno));
		if (m == 0)
			panic("read: Unexpected EOF");
		p += m;
	}
}

uint32_t
blockof(void *pos)
{
	return ((char*)pos - diskmap) / BLKSIZE;
}

void *
alloc(uint32_t bytes)
{
	void *start = diskpos;
	diskpos += ROUNDUP(bytes, BLKSIZE);
	if (blockof(diskpos) >= nblocks)
		panic("out of disk blocks");
	return start;
}

void
opendisk(const char *name)
{
	int r, nbitblocks;

	// NOTE: Here, on Windows, also needs O_BINARY !!!!
	if ((diskfd = open(name, O_RDWR | O_CREAT | O_BINARY, 0666)) < 0)
		panic("open %s: %s", name, strerror(errno));

	if ((r = ftruncate(diskfd, 0)) < 0
	    || (r = ftruncate(diskfd, nblocks * BLKSIZE)) < 0)
		panic("truncate %s: %s", name, strerror(errno));

	// For windows, there is no native mmap
	// So we store everything in memory
	// And write to disk when finish
	diskmap=malloc(nblocks * BLKSIZE);
	diskpos = diskmap;

	alloc(BLKSIZE);
	super = alloc(BLKSIZE);
	super->s_magic = FS_MAGIC;
	super->s_nblocks = nblocks;
	super->s_root.f_type = FTYPE_DIR;
	strcpy(super->s_root.f_name, "/");

	nbitblocks = (nblocks + BLKBITSIZE - 1) / BLKBITSIZE;
	bitmap = alloc(nbitblocks * BLKSIZE);
	memset(bitmap, 0xFF, nbitblocks * BLKSIZE);
}

void
finishdisk(void)
{
	int r, i;

	for (i = 0; i < blockof(diskpos); ++i)
		bitmap[i/32] &= ~(1<<(i%32));

	// Here we need to write everything in memory back to disk
	int total_written=0;
	int target=nblocks * BLKSIZE;
	int len;

	while(total_written < target) {
		len = write(diskfd,diskmap+total_written,target-total_written);
		if(len < 0) {
			panic("Error when writting back");
		}
		total_written += len;
	}

	close(diskfd);
}

void
finishfile(struct File *f, uint32_t start, uint32_t len)
{
	int i;
	f->f_size = len;
	len = ROUNDUP(len, BLKSIZE);
	for (i = 0; i < len / BLKSIZE && i < NDIRECT; ++i)
		f->f_direct[i] = start + i;
	if (i == NDIRECT) {
		uint32_t *ind = alloc(BLKSIZE);
		f->f_indirect = blockof(ind);
		for (; i < len / BLKSIZE; ++i)
			ind[i - NDIRECT] = start + i;
	}
}

void
startdir(struct File *f, struct Dir *dout)
{
	dout->f = f;
	dout->ents = malloc(MAX_DIR_ENTS * sizeof *dout->ents);
	dout->n = 0;
}

struct File *
diradd(struct Dir *d, uint32_t type, const char *name)
{
	struct File *out = &d->ents[d->n++];
	if (d->n > MAX_DIR_ENTS)
		panic("too many directory entries");
	strcpy(out->f_name, name);
	out->f_type = type;
	return out;
}

void
finishdir(struct Dir *d)
{
	int size = d->n * sizeof(struct File);
	struct File *start = alloc(size);
	memmove(start, d->ents, size);
	finishfile(d->f, blockof(start), ROUNDUP(size, BLKSIZE));
	free(d->ents);
	d->ents = NULL;
}

void
writefile(struct Dir *dir, const char *name)
{
	int r, fd;
	struct File *f;
	struct stat st;
	const char *last;
	char *start;

	// Here must open it in binary mode
	if ((fd = open(name, O_RDONLY | O_BINARY)) < 0)
		panic("open %s: %s", name, strerror(errno));
	if ((r = fstat(fd, &st)) < 0)
		panic("stat %s: %s", name, strerror(errno));
	if (!S_ISREG(st.st_mode))
		panic("%s is not a regular file", name);
	if (st.st_size >= MAXFILESIZE)
		panic("%s too large", name);

	last = strrchr(name, '/');
	if (last)
		last++;
	else
		last = name;

	f = diradd(dir, FTYPE_REG, last);
	start = alloc(st.st_size);
	printf("Try to read %s %ld\n",name,st.st_size);
	readn(fd, start, st.st_size);
	finishfile(f, blockof(start), st.st_size);
	close(fd);
}

void
usage(void)
{
	fprintf(stderr, "Usage: fsformat fs.img NBLOCKS files...\n");
	exit(2);
}

int
main(int argc, char **argv)
{
	int i;
	char *s;
	struct Dir root;

	assert(BLKSIZE % sizeof(struct File) == 0);

	if (argc < 3)
		usage();

	nblocks = strtol(argv[2], &s, 0);
	if (*s || s == argv[2] || nblocks < 2 || nblocks > 1024)
		usage();

	opendisk(argv[1]);

	startdir(&super->s_root, &root);
	for (i = 3; i < argc; i++)
		writefile(&root, argv[i]);
	finishdir(&root);

	finishdisk();
	return 0;
}

