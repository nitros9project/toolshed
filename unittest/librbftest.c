/*
 * ALWAYS BE TESTING!!!
 *
 * ALWAYS BE WRITING MORE TESTS!!!
 *
 * THIS WORK IS **NEVER** DONE!!!
 */

#include "tinytest.h"
#include <toolshed.h>
#include "imagedamage.h"


#define OS9_DMK "test_os9.dmk"
#define OS9_MAME_DMK "../../../unittest/testdata/os9 mame.dmk"
#define DMK_SPT 18

static void copy_file(const char *from, const char *to)
{
	char buffer[4096];
	size_t got;
	FILE *in = fopen(from, "rb");
	FILE *out = fopen(to, "wb");

	ASSERT("the fixture exists", in != NULL);
	while ((got = fread(buffer, 1, sizeof(buffer), in)) > 0)
	{
		fwrite(buffer, 1, got, out);
	}
	fclose(in);
	fclose(out);
}

static void lsn_address(int lsn, int *cylinder, int *sector)
{
	*cylinder = lsn / DMK_SPT;
	*sector = lsn % DMK_SPT + 1;
}

/* Write a new file on the image, and report where its first data sector is. */
static void os9_write_file(const char *pathlist, const u_char *data, u_int size, int *data_lsn)
{
	os9_path_id p;
	fd_stats fd;
	error_code ec;
	u_int count = size;

	ec = _os9_create(&p, (char *) pathlist, FAM_READ | FAM_WRITE, FAP_READ | FAP_WRITE);
	ASSERT_EQUALS(0, ec);
	ec = _os9_write(p, (void *) data, &count);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(size, count);
	ec = _os9_gs_fd(p, sizeof(fd), &fd);
	ASSERT_EQUALS(0, ec);
	*data_lsn = int3(fd.fd_seg[0].lsn);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);
}

void test_os9_dmk_read_write()
{
	u_char data[3000], back[3000];
	os9_path_id p;
	error_code ec;
	u_int size = sizeof(back);
	diskimage_format format;
	_path_type type;
	int data_lsn, i;

	for (i = 0; i < (int) sizeof(data); i++)
	{
		data[i] = (u_char) (i * 5);
	}
	copy_file(OS9_MAME_DMK, OS9_DMK);

	ec = _coco_identify_image(OS9_DMK ",", &type);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(OS9, type);

	os9_write_file(OS9_DMK ",FILE", data, sizeof(data), &data_lsn);

	ec = _diskimage_identify(OS9_DMK, &format);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(DISKIMAGE_DMK, format);

	ec = _os9_open(&p, OS9_DMK ",FILE", FAM_READ);
	ASSERT_EQUALS(0, ec);
	ec = _os9_read(p, back, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(data), size);
	ASSERT_EQUALS(0, memcmp(data, back, sizeof(data)));
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);

	ec = _os9_makdir(OS9_DMK ",DIR");
	ASSERT_EQUALS(0, ec);
	ec = _os9_delete(OS9_DMK ",FILE");
	ASSERT_EQUALS(0, ec);
	ec = _os9_open(&p, OS9_DMK ",FILE", FAM_READ);
	ASSERT_EQUALS(EOS_PNNF, ec);
	remove(OS9_DMK);
}

void test_os9_only_needed_sectors_are_errors()
{
	u_char first[1024], second[1024], back[1024];
	os9_path_id p;
	error_code ec;
	u_int size = sizeof(back);
	int first_lsn, second_lsn, cylinder, sector, i;

	for (i = 0; i < (int) sizeof(first); i++)
	{
		first[i] = (u_char) i;
		second[i] = (u_char) (i * 9);
	}
	copy_file(OS9_MAME_DMK, OS9_DMK);
	os9_write_file(OS9_DMK ",FIRST", first, sizeof(first), &first_lsn);
	os9_write_file(OS9_DMK ",SECOND", second, sizeof(second), &second_lsn);
	ASSERT("the second file comes later on the disk", second_lsn > first_lsn);

	/* A sector of one file fails that file, and moves no other sector. */
	lsn_address(first_lsn, &cylinder, &sector);
	image_remove_sector(OS9_DMK, cylinder, 0, sector);

	ec = _os9_open(&p, OS9_DMK ",SECOND", FAM_READ);
	ASSERT_EQUALS(0, ec);
	ec = _os9_read(p, back, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(0, memcmp(second, back, sizeof(second)));
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);

	ec = _os9_open(&p, OS9_DMK ",FIRST", FAM_READ);
	ASSERT_EQUALS(0, ec);
	size = sizeof(back);
	ec = _os9_read(p, back, &size);
	ASSERT_EQUALS(EOS_SECT, ec);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);

	/* Without the bitmap, files still read, but none can be allocated. */
	image_remove_sector(OS9_DMK, 0, 0, 2);

	ec = _os9_open(&p, OS9_DMK ",SECOND", FAM_READ);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(EOS_SECT, p->bitmap_ec);
	size = sizeof(back);
	ec = _os9_read(p, back, &size);
	ASSERT_EQUALS(0, ec);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);

	ec = _os9_create(&p, OS9_DMK ",NEW", FAM_READ | FAM_WRITE, FAP_READ | FAP_WRITE);
	ASSERT_EQUALS(EOS_SECT, ec);
	ec = _os9_delete(OS9_DMK ",SECOND");
	ASSERT_EQUALS(EOS_SECT, ec);

	/* Without LSN0 there is nothing to go on. */
	image_remove_sector(OS9_DMK, 0, 0, 1);
	ec = _os9_open(&p, OS9_DMK ",SECOND", FAM_READ);
	ASSERT_EQUALS(EOS_SECT, ec);
	remove(OS9_DMK);
}

void test_os9_bad_crc_is_read()
{
	u_char data[100], back[100];
	os9_path_id p;
	error_code ec;
	u_int size = sizeof(back);
	int data_lsn, cylinder, sector, i;

	for (i = 0; i < (int) sizeof(data); i++)
	{
		data[i] = (u_char) i;
	}
	copy_file(OS9_MAME_DMK, OS9_DMK);
	os9_write_file(OS9_DMK ",FILE", data, sizeof(data), &data_lsn);

	lsn_address(data_lsn, &cylinder, &sector);
	image_damage_sector(OS9_DMK, cylinder, 0, sector);

	ec = _os9_open(&p, OS9_DMK ",FILE", FAM_READ);
	ASSERT_EQUALS(0, ec);
	ec = _os9_read(p, back, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(data), size);
	ASSERT_EQUALS((u_char) (data[0] ^ 0xFF), back[0]);
	ASSERT_EQUALS(0, memcmp(data + 1, back + 1, sizeof(data) - 1));
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);
	remove(OS9_DMK);
}

void test_os9_write_protect()
{
	u_char data[16] = { 0 }, back[16];
	os9_path_id p;
	error_code ec;
	u_int size = sizeof(back);
	int data_lsn;

	copy_file(OS9_MAME_DMK, OS9_DMK);
	os9_write_file(OS9_DMK ",FILE", data, sizeof(data), &data_lsn);
	ec = _diskimage_set_write_protect(OS9_DMK, 1);
	ASSERT_EQUALS(0, ec);

	/* Reading is still fine, and a path that could write closes cleanly. */
	ec = _os9_open(&p, OS9_DMK ",FILE", FAM_READ | FAM_WRITE);
	ASSERT_EQUALS(0, ec);
	ec = _os9_read(p, back, &size);
	ASSERT_EQUALS(0, ec);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);

	ec = _os9_create(&p, OS9_DMK ",NEW", FAM_READ | FAM_WRITE, FAP_READ | FAP_WRITE);
	ASSERT_EQUALS(EOS_WP, ec);
	ec = _os9_delete(OS9_DMK ",FILE");
	ASSERT_EQUALS(EOS_WP, ec);

	ec = _diskimage_set_write_protect(OS9_DMK, 0);
	ASSERT_EQUALS(0, ec);
	ec = _os9_delete(OS9_DMK ",FILE");
	ASSERT_EQUALS(0, ec);
	remove(OS9_DMK);
}

void test_os9_format()
{
	error_code ec;

	// test format of a non-existent disk image
	unsigned int totalSectors, totalBytes;
	int clusterSize = 0;

	ec = _os9_format("test.dsk", 0, 35, 18, 18, 1, 256, &clusterSize, "Test Disk", 8, 8,
			 1, 1, 0, 0, 3, &totalSectors, &totalBytes);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(1, clusterSize);

	// test format of a non-existent disk image with a disk name that is way too long
	ec = _os9_format("test.dsk", 0, 35, 18, 18, 1, 256, &clusterSize,
			 "Test Disk with filename that is way too long for the field",
			 8, 8, 1, 1, 0, 0, 3, &totalSectors, &totalBytes);
	ASSERT_EQUALS(0, ec);


	// Test format of a hard drive with 65000 sectors.
	// Getting the parameters for _os9_format:
	//   $ gdb os9/os9
	//   (gdb) b _os9_format
	//   (gdb) run format test.dsk -l65000
	// Breakpoint 1, _os9_format (pathname=0x7fffffffe0ad "test.dsk", os968k=0,
	// tracks=125, sectorsPerTrack=4, sectorsTrack0=4, heads=130, sectorSize=256,
	// clusterSize=0x7fffffffdae8, diskName=0x5555555761df "CoCo Disk",
	// sectorAllocationSize=8, tpi=0, density=0, formatEntire=0, isDragon=0, isHDD=1,
	// totalSectors=0x7fffffffdaec, totalBytes=0x7fffffffdaf0) at ../../../librbf/librbfformat.c:25
	//   (gdb) cont
	// Continuing.
	// Format Summary
	// --------------
	// Geometry Data:
	//       Cylinders: 125
	//           Heads: 130
	//   Sectors/track: 4
	//     Sector size: 256
	//
	// Logical Data:
	//   Total sectors: 65000
	//   Size in bytes: 16640000
	//    Cluster size: 1

    // See test_os9_command_format for a test of the command line
    // that generates this scenario.

	ec = _os9_format("test.dsk", /*68k=*/0,
			/*tracks=*/125, /*sPT=*/4, /*sT0=*/4, /*heads=*/130, /*sSize=*/256, &clusterSize,
			 "HD-Test",
			 /*sas=*/8, /*tpi=*/0, /*density=*/0, /*fE=*/1, /*isD=*/0, /*isHDD=*/1, /*interleave-*/3,
			 &totalSectors, &totalBytes);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(65000, totalSectors);
	ASSERT_EQUALS(65000 * 256, totalBytes);

	// TODO: test format with oddball parameters to make sure it can survive
}

void test_os9_create()
{
	os9_path_id p;
	error_code ec;

	// test create of a non-existing file on a non-existing disk image
	ec = _os9_create(&p, "test_disk_doesnt_exist.dsk,file_doesnt_exist",
			 FAM_READ, FAP_READ);
	ASSERT_EQUALS(EOS_PNNF, ec);

	// test create of a non-existing file on an existing disk image
	ec = _os9_create(&p, "test.dsk,test.txt", FAM_READ, FAP_READ);
	ASSERT_EQUALS(0, ec);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);

	// test create of an existing file on an existing disk image
	ec = _os9_create(&p, "test.dsk,test.txt", FAM_READ, FAP_READ);
	ASSERT_EQUALS(0, ec);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);

	// test create of an existing file on an existing disk image with FAM_NOCREATE
	ec = _os9_create(&p, "test.dsk,test.txt", FAM_READ | FAM_NOCREATE,
			 FAP_READ);
	ASSERT_EQUALS(EOS_FAE, ec);

	// test create of an extra long (illegal) filename on an existing disk image
	ec = _os9_create(&p,
			 "test.dsk,file_doesnt_exist_and_is_much_longer_than_os9_limit_of_29_characters",
			 FAM_READ, FAP_READ);
	ASSERT_EQUALS(EOS_BPNAM, ec);

	// test create of an extra long (illegal) filename with a subfolder on an existing disk image
	// the root file is a directory when it isn't
	ec = _os9_create(&p,
			 "test.dsk,file_doesnt_exist_and_is_much_longer_than_rbf_limit_of_29_characters/and_this_is_an_even_longer_name_than_the_29_character_limit_in9_rbf_because_it_has_more_characters",
			 FAM_READ, FAP_READ);
	ASSERT_EQUALS(EOS_PNNF, ec);
}

void test_os9_read()
{
	os9_path_id p;
	error_code ec;

	// test create of a non-existing file on an existing disk image
	ec = _os9_create(&p, "test.dsk,test2.txt", FAM_READ, FAP_READ);
	ASSERT_EQUALS(0, ec);

	// test read of an empty file
	char buff[32];
	u_int size = 32;
	ec = _os9_read(p, buff, &size);
	ASSERT_EQUALS(EOS_EOF, ec);

	// test close of a validly opened file
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);
}

void test_os9_raw_read_end_of_disk()
{
	os9_path_id p;
	error_code ec;
	u_int disksize, size, pos;
	char expected[16], buffer[32], untouched[32];

	ec = _os9_open(&p, "test.dsk,@", FAM_READ);
	ASSERT_EQUALS(0, ec);
	ec = _os9_gs_size(p, &disksize);
	ASSERT_EQUALS(0, ec);

	/* Capture the final bytes with a read that ends exactly at EOF. */
	ec = _os9_seek(p, disksize - sizeof(expected), SEEK_SET);
	ASSERT_EQUALS(0, ec);
	size = sizeof(expected);
	ec = _os9_read(p, expected, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(expected), size);

	/* A read spanning EOF must report only the bytes remaining. */
	ec = _os9_seek(p, disksize - sizeof(expected), SEEK_SET);
	ASSERT_EQUALS(0, ec);
	memset(buffer, 0x5a, sizeof(buffer));
	memset(untouched, 0x5a, sizeof(untouched));
	size = sizeof(buffer);
	ec = _os9_read(p, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(expected), size);
	ASSERT_MEM_EQUALS(expected, buffer, sizeof(expected));
	ASSERT_MEM_EQUALS(untouched, buffer + sizeof(expected),
			  sizeof(buffer) - sizeof(expected));
	ec = _os9_gs_pos(p, &pos);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(disksize, pos);

	/* EOF returns no bytes and leaves the buffer and position unchanged. */
	memcpy(buffer, untouched, sizeof(buffer));
	size = sizeof(buffer);
	ec = _os9_read(p, buffer, &size);
	ASSERT_EQUALS(EOS_EOF, ec);
	ASSERT_EQUALS(0, size);
	ASSERT_MEM_EQUALS(untouched, buffer, sizeof(buffer));
	ec = _os9_gs_pos(p, &pos);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(disksize, pos);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);
}

void test_os9_raw_write_position()
{
	os9_path_id p;
	error_code ec;
	u_int disksize, size, pos, start;
	char original[64], replacement[16], buffer[16];

	ec = _os9_open(&p, "test.dsk,@", FAM_READ | FAM_WRITE);
	ASSERT_EQUALS(0, ec);
	ec = _os9_gs_size(p, &disksize);
	ASSERT_EQUALS(0, ec);
	start = disksize - sizeof(original);

	/* Use unused space at the end of the test image. */
	memset(original, 'A', sizeof(original));
	memset(replacement, 'B', sizeof(replacement));
	ec = _os9_seek(p, start, SEEK_SET);
	ASSERT_EQUALS(0, ec);
	size = sizeof(original);
	ec = _os9_write(p, original, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(original), size);
	ec = _os9_gs_pos(p, &pos);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(disksize, pos);

	ec = _os9_seek(p, start, SEEK_SET);
	ASSERT_EQUALS(0, ec);
	size = sizeof(replacement);
	ec = _os9_write(p, replacement, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(replacement), size);
	ec = _os9_gs_pos(p, &pos);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(start + sizeof(replacement), pos);

	/* A following read must start after the bytes just written. */
	size = sizeof(buffer);
	ec = _os9_read(p, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(buffer), size);
	ASSERT_MEM_EQUALS(original + sizeof(replacement), buffer, size);
	ec = _os9_gs_pos(p, &pos);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(start + sizeof(replacement) + sizeof(buffer), pos);

	/* Confirm the replacement was written at the requested offset. */
	ec = _os9_seek(p, start, SEEK_SET);
	ASSERT_EQUALS(0, ec);
	size = sizeof(buffer);
	ec = _os9_read(p, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(buffer), size);
	ASSERT_MEM_EQUALS(replacement, buffer, size);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);
}

void test_os9_readln_across_segments()
{
	os9_path_id p;
	error_code ec;
	fd_stats fd;
	u_int size, pos;
	char sector[256], buffer[32];
	char tail[] = "BC\rnext\r";

	/* Close after one sector so unused allocation is released. */
	ec = _os9_create(&p, "test.dsk,lines.txt", FAM_READ | FAM_WRITE,
			 FAP_READ | FAP_WRITE);
	ASSERT_EQUALS(0, ec);
	memset(sector, 'A', sizeof(sector));
	size = sizeof(sector);
	ec = _os9_write(p, sector, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(sector), size);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);

	/* Occupy the following space to force a second data segment. */
	ec = _os9_create(&p, "test.dsk,line_blocker", FAM_READ | FAM_WRITE,
			 FAP_READ | FAP_WRITE);
	ASSERT_EQUALS(0, ec);
	size = sizeof(sector);
	ec = _os9_write(p, sector, &size);
	ASSERT_EQUALS(0, ec);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);

	ec = _os9_open(&p, "test.dsk,lines.txt", FAM_READ | FAM_WRITE);
	ASSERT_EQUALS(0, ec);
	ec = _os9_seek(p, sizeof(sector), SEEK_SET);
	ASSERT_EQUALS(0, ec);
	size = strlen(tail);
	ec = _os9_write(p, tail, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(strlen(tail), size);
	ec = _os9_gs_fd(p, sizeof(fd), &fd);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(1, int2(fd.fd_seg[0].num));
	ASSERT_NEQUALS(0, int3(fd.fd_seg[1].lsn));

	/* Six bytes in the first segment, then three through the CR. */
	ec = _os9_seek(p, 250, SEEK_SET);
	ASSERT_EQUALS(0, ec);
	size = sizeof(buffer);
	ec = _os9_readln(p, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(9, size);
	ASSERT_MEM_EQUALS("AAAAAABC\r", buffer, size);
	ec = _os9_gs_pos(p, &pos);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(259, pos);

	size = sizeof(buffer);
	ec = _os9_readln(p, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(5, size);
	ASSERT_MEM_EQUALS("next\r", buffer, size);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);
	ec = _os9_delete("test.dsk,lines.txt");
	ASSERT_EQUALS(0, ec);
	ec = _os9_delete("test.dsk,line_blocker");
	ASSERT_EQUALS(0, ec);
}

void test_os9_write()
{
	os9_path_id p;
	error_code ec;

	// test create of a non-exsting file on an existing disk image
	ec = _os9_create(&p, "test.dsk,test3.txt", FAM_READ, FAP_READ);
	ASSERT_EQUALS(0, ec);

	// test write when file only open for read
	char *buff = "this is a string\nand this is another string\n";
	u_int size = strlen(buff);
	ec = _os9_write(p, buff, &size);
	ASSERT_EQUALS(EOS_BMODE, ec);

	// test close of file
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);

	// test create of a non-existing file on an existing disk image
	ec = _os9_create(&p, "test.dsk,test4.txt", FAM_READ | FAM_WRITE,
			 FAP_READ | FAP_WRITE);
	ASSERT_EQUALS(0, ec);

	// test write when file open for read and write
	buff = "this is a string";
	size = strlen(buff);
	u_int capture_size = size;
	ec = _os9_write(p, buff, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(size, capture_size);

	// test close of a validly opened file
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);
}

void test_os9_open_and_read()
{
	os9_path_id p;
	error_code ec;

	// test open of a non-existing disk image
	ec = _os9_open(&p, "test_disk_doesnt_exist.dsk,file_doesnt_exist",
		       FAM_READ);
	ASSERT_EQUALS(EOS_PNNF, ec);

	// test open of an existing file on an existing disk image
	ec = _os9_open(&p, "test.dsk,test4.txt", FAM_READ);
	ASSERT_EQUALS(0, ec);

	// test read of an existing file on an existing disk image
	char buf[256];
	u_int size = 128;

	ec = _os9_read(p, buf, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(16, size);

	// test read after all data has been read
	ec = _os9_read(p, buf, &size);
	ASSERT_EQUALS(EOS_EOF, ec);

	// test close of a validly opened file
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);
}

void test_os9_delete()
{
	error_code ec;

	// test deletion of a non-existing disk image
	ec = _os9_delete("test_disk_doesnt_exist.dsk,file_doesnt_exist");
	ASSERT_EQUALS(EOS_PNNF, ec);

	// test deletion of a non-exsting file on an existing disk image
	ec = _os9_delete("test.dsk,test4.txt");
	ASSERT_EQUALS(0, ec);
}

void test_os9_delete_write_only_file()
{
	os9_path_id p;
	error_code ec;
	fd_stats fd;
	int fd_lsn, i, j;
	u_int size;
	char buffer[512];

	ec = _os9_create(&p, "test.dsk,write_only.txt", FAM_WRITE, FAP_WRITE);
	ASSERT_EQUALS(0, ec);
	memset(buffer, 'W', sizeof(buffer));
	size = sizeof(buffer);
	ec = _os9_write(p, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(buffer), size);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);

	/* Capture the allocated sectors after close has trimmed allocation. */
	ec = _os9_open(&p, "test.dsk,write_only.txt", FAM_WRITE);
	ASSERT_EQUALS(0, ec);
	fd_lsn = p->pl_fd_lsn;
	ec = _os9_gs_fd(p, sizeof(fd), &fd);
	ASSERT_EQUALS(0, ec);
	ASSERT_NEQUALS(0, int3(fd.fd_seg[0].lsn));
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);

	/* Deletion must not require read access to the target file. */
	ec = _os9_delete("test.dsk,write_only.txt");
	ASSERT_EQUALS(0, ec);
	ec = _os9_open(&p, "test.dsk,write_only.txt", FAM_WRITE);
	ASSERT_EQUALS(EOS_PNNF, ec);

	/* Both the descriptor and all data clusters must be released. */
	ec = _os9_open(&p, "test.dsk,@", FAM_READ);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(0, _os9_ckbit(p->bitmap, fd_lsn / p->spc));
	for (i = 0; i < NUM_SEGS && int3(fd.fd_seg[i].lsn) != 0; i++)
	{
		for (j = 0; j < int2(fd.fd_seg[i].num); j += p->spc)
		{
			ASSERT_EQUALS(0, _os9_ckbit(p->bitmap,
				(int3(fd.fd_seg[i].lsn) + j) / p->spc));
		}
	}
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);
}

void test_os9_makdir()
{
	error_code ec;

	// test create of a non-existing directory in an existing disk image
	ec = _os9_makdir("test.dsk,dir_doesnt_exist");
	ASSERT_EQUALS(0, ec);

	// test create of an existing directory in an existing disk image
	ec = _os9_makdir("test.dsk,dir_doesnt_exist");
	ASSERT_EQUALS(EOS_FAE, ec);

	// test create of a non-existing subdirectory in a existing directory in an existing disk image
	ec = _os9_makdir("test.dsk,dir_doesnt_exist/sub_dir_doesnt_exist");
	ASSERT_EQUALS(ec, 0);
}

void test_os9_delete_directory()
{
	error_code ec;

	// test deletion of an existing directory in an existing disk image
	ec = _os9_delete_directory("test.dsk,dir_doesnt_exist");
	ASSERT_EQUALS(ec, 0);
}

void test_os9_rename()
{
	error_code ec;

	// test rename of a non-existing file in an existing disk image
	ec = _os9_rename("test.dsk,file_doesnt_exist",
			 "another_file_doesnt_exist");
	ASSERT_EQUALS(EOS_PNNF, ec);

	// test rename of an existing file in an existing disk image
	ec = _os9_rename("test.dsk,test3.txt", "test_renamed.txt");
	ASSERT_EQUALS(0, ec);
}

void test_os9_gs_calls()
{
	error_code ec;
	os9_path_id p;
	int perms = FAP_READ | FAP_WRITE;

	// test create of a non-existing file in an existing disk image
	ec = _os9_create(&p, "test.dsk,newfile.txt", FAM_READ | FAM_WRITE,
			 perms);
	ASSERT_EQUALS(0, ec);

	int gsperms;
	ec = _os9_gs_attr(p, &gsperms);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(perms, gsperms);

	// test close of a validly opened file
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);
}

void test_os9_ss_calls()
{
	// TODO: Write tests
}

/* A create that fails after taking a cluster for the file descriptor gives
 * that cluster back.  The undo has to clear the cluster's bit, not the bit
 * numbered by the descriptor's LSN: with two sectors per cluster those differ.
 */
void test_os9_failed_create_frees_its_cluster()
{
	static const char image[] = "test_create_full.dsk";
	u_char before[256], after[256];
	char pathlist[64];
	fd_stats fd;
	os9_path_id p;
	unsigned int totalSectors, totalBytes;
	int clusterSize = 2;
	int s, capacity = 0, files, i, free_cluster = -1;
	u_int map_bytes;
	FILE *f;
	size_t got;

	ASSERT_EC(0, _os9_format((char *) image, 0, 35, 18, 18, 1, 256, &clusterSize, "Full Dir", 8, 8,
				 1, 1, 0, 0, 3, &totalSectors, &totalBytes));
	ASSERT_EQUALS(2, clusterSize);

	/* Fill the root directory up to the room its segments already hold. */
	snprintf(pathlist, sizeof(pathlist), "%s,.", image);
	ASSERT_EC(0, _os9_gs_fd_pathlist(pathlist, sizeof(fd), &fd));
	for (s = 0; s < NUM_SEGS && int3(fd.fd_seg[s].lsn) != 0; s++)
	{
		capacity += int2(fd.fd_seg[s].num) * 256;
	}
	files = capacity / (int) sizeof(os9_dir_entry) - 2;	/* less . and .. */
	for (i = 0; i < files; i++)
	{
		snprintf(pathlist, sizeof(pathlist), "%s,F%d", image, i);
		ASSERT_EC(0, _os9_create(&p, pathlist, FAM_READ | FAM_WRITE, FAP_READ | FAP_WRITE));
		ASSERT_EC(0, _os9_close(p));
	}
	snprintf(pathlist, sizeof(pathlist), "%s,.", image);
	ASSERT_EC(0, _os9_gs_fd_pathlist(pathlist, sizeof(fd), &fd));
	ASSERT_EQUALS(capacity, (int) int4(fd.fd_siz));

	/* Leave one free cluster: enough for a file descriptor, but not for the
	 * directory to grow.
	 */
	f = fopen(image, "r+b");
	ASSERT("the image opens", f != NULL);
	ASSERT_EQUALS(0, fseek(f, 0x04, SEEK_SET));
	map_bytes = (u_int) fgetc(f) << 8;
	map_bytes |= (u_int) fgetc(f);
	ASSERT("the map fits a sector", map_bytes > 0 && map_bytes <= sizeof(before));
	ASSERT_EQUALS(0, fseek(f, 256, SEEK_SET));
	got = fread(before, 1, map_bytes, f);
	ASSERT_EQUALS(map_bytes, got);
	for (i = 0; i < (int) map_bytes * 8 && free_cluster < 0; i++)
	{
		if ((before[i / 8] & (0x80 >> (i % 8))) == 0)
		{
			free_cluster = i;
		}
	}
	ASSERT("a cluster is free", free_cluster >= 0);
	memset(before, 0xFF, map_bytes);
	before[free_cluster / 8] &= ~(0x80 >> (free_cluster % 8));
	ASSERT_EQUALS(0, fseek(f, 256, SEEK_SET));
	got = fwrite(before, 1, map_bytes, f);
	ASSERT_EQUALS(map_bytes, got);
	fclose(f);

	/* The descriptor takes the last cluster; the directory can't grow. */
	snprintf(pathlist, sizeof(pathlist), "%s,ONEMORE", image);
	ASSERT_EC(EOS_DF, _os9_create(&p, pathlist, FAM_READ | FAM_WRITE, FAP_READ | FAP_WRITE));

	/* Nothing was added, so the map is as it was. */
	f = fopen(image, "rb");
	ASSERT("the image opens", f != NULL);
	ASSERT_EQUALS(0, fseek(f, 256, SEEK_SET));
	got = fread(after, 1, map_bytes, f);
	ASSERT_EQUALS(map_bytes, got);
	fclose(f);
	ASSERT_MEM_EQUALS(before, after, map_bytes);

	remove(image);
}

/* Read or write one 256-byte sector of a raw image directly. */
static void raw_sector(const char *image, int lsn, u_char *buffer, int write)
{
	FILE *f = fopen(image, "r+b");
	size_t got;

	ASSERT("the image opens", f != NULL);
	ASSERT_EQUALS(0, fseek(f, (long) lsn * 256, SEEK_SET));
	got = write ? fwrite(buffer, 1, 256, f) : fread(buffer, 1, 256, f);
	fclose(f);
	ASSERT_EQUALS(256, (int) got);
}

static long raw_size(const char *image)
{
	FILE *f = fopen(image, "rb");
	long size;

	if (f == NULL)
	{
		return -1;
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fclose(f);
	return size;
}

/* Mark every cluster in the allocation map as used, and clear the rest of
 * the map's sector, past dd_map.  Those bytes are not part of the map, and
 * some formatters leave them zero.  Returns the map as written.
 */
static void fill_map_zero_pad(const char *image, u_char *map, u_int *map_bytes)
{
	u_char lsn0[256];

	raw_sector(image, 0, lsn0, 0);
	*map_bytes = int2(((lsn0_sect *) lsn0)->dd_map);
	ASSERT("the map fits a sector", *map_bytes > 0 && *map_bytes <= 256);
	memset(map, 0, 256);
	memset(map, 0xFF, *map_bytes);
	raw_sector(image, 1, map, 1);
}

/* A segment list ends at its first empty entry.  What follows is not part of
 * the file, even when the descriptor claims more bytes than the list holds.
 */
void test_os9_read_stops_at_segment_list_end()
{
	static const char image[] = "test_seg_end.dsk";
	char pathlist[64];
	u_char data[256], other[256], fd_buf[256], back[512];
	fd_stats *fd = (fd_stats *) fd_buf;
	os9_path_id p;
	unsigned int totalSectors, totalBytes;
	int clusterSize = 0, data_lsn, other_lsn, fd_lsn, pass;
	u_int size;

	memset(data, 0x11, sizeof(data));
	memset(other, 0xAA, sizeof(other));
	ASSERT_EC(0, _os9_format((char *) image, 0, 35, 18, 18, 1, 256, &clusterSize, "Seg End", 8, 8,
				 1, 1, 0, 0, 3, &totalSectors, &totalBytes));
	snprintf(pathlist, sizeof(pathlist), "%s,OTHER", image);
	os9_write_file(pathlist, other, sizeof(other), &other_lsn);
	snprintf(pathlist, sizeof(pathlist), "%s,FILE", image);
	os9_write_file(pathlist, data, sizeof(data), &data_lsn);

	ASSERT_EC(0, _os9_open(&p, pathlist, FAM_READ));
	fd_lsn = p->pl_fd_lsn;
	ASSERT_EC(0, _os9_close(p));

	/* One sector, then the end of the list, then a stray entry pointing at
	 * another file's data.  The size claims two sectors.
	 */
	raw_sector(image, fd_lsn, fd_buf, 0);
	memset(fd->fd_seg, 0, sizeof(fd->fd_seg));
	_int3(data_lsn, fd->fd_seg[0].lsn);
	_int2(1, fd->fd_seg[0].num);
	_int3(other_lsn, fd->fd_seg[2].lsn);
	_int2(1, fd->fd_seg[2].num);
	_int4(512, fd->fd_siz);
	raw_sector(image, fd_lsn, fd_buf, 1);

	for (pass = 0; pass < 2; pass++)
	{
		error_code (*reader)(os9_path_id, void *, u_int *) = pass == 0 ? _os9_read : _os9_readln;
		error_code ec;

		ASSERT_EC(0, _os9_open(&p, pathlist, FAM_READ));

		/* All the list holds, and no more than it holds. */
		memset(back, 0, sizeof(back));
		size = sizeof(back);
		ASSERT_EC(0, reader(p, back, &size));
		ASSERT_EQUALS(256, (int) size);
		ASSERT_MEM_EQUALS(data, back, 256);

		/* Nothing past the end of the list. */
		memset(back, 0, sizeof(back));
		size = 256;
		ec = reader(p, back, &size);
		ASSERT_NEQUALS(0, ec);
		ASSERT_EQUALS(0, (int) size);
		ASSERT("nothing from the other file", back[0] != 0xAA);

		ASSERT_EC(0, _os9_close(p));
	}

	remove(image);
}

/* A cluster past the end of the disk is never handed out, whatever the bytes
 * after the allocation map hold.
 */
void test_os9_no_allocation_past_last_cluster()
{
	static const char image[] = "test_map_end.dsk";
	char pathlist[64];
	u_char map[256], back[256], buffer[1000], fd_buf[256];
	fd_stats *fd = (fd_stats *) fd_buf;
	os9_path_id p;
	unsigned int totalSectors, totalBytes;
	int clusterSize = 2, fd_lsn, last_cluster;
	u_int map_bytes, size;
	long image_size;

	/* 576 sectors make 288 clusters, exactly 36 map bytes, so the bit after
	 * the last cluster is in the zeroed bytes past the map.
	 */
	ASSERT_EC(0, _os9_format((char *) image, 0, 32, 18, 18, 1, 256, &clusterSize, "Map End", 8, 8,
				 1, 1, 0, 0, 3, &totalSectors, &totalBytes));
	ASSERT_EQUALS(2, clusterSize);
	snprintf(pathlist, sizeof(pathlist), "%s,EMPTY", image);
	ASSERT_EC(0, _os9_create(&p, pathlist, FAM_READ | FAM_WRITE, FAP_READ | FAP_WRITE));
	ASSERT_EC(0, _os9_close(p));

	fill_map_zero_pad(image, map, &map_bytes);
	image_size = raw_size(image);

	/* No cluster for a new file's descriptor. */
	snprintf(pathlist, sizeof(pathlist), "%s,NEW", image);
	ASSERT_EC(EOS_DF, _os9_create(&p, pathlist, FAM_READ | FAM_WRITE, FAP_READ | FAP_WRITE));

	/* No segment for an existing file to grow into. */
	memset(buffer, 0x5A, sizeof(buffer));
	snprintf(pathlist, sizeof(pathlist), "%s,EMPTY", image);
	ASSERT_EC(0, _os9_open(&p, pathlist, FAM_READ | FAM_WRITE));
	size = sizeof(buffer);
	ASSERT_EC(EOS_DF, _os9_write(p, buffer, &size));
	fd_lsn = p->pl_fd_lsn;
	ASSERT_EC(0, _os9_close(p));

	/* Nor a cluster after the disk's last one, for a file that ends there. */
	raw_sector(image, fd_lsn, fd_buf, 0);
	memset(fd->fd_seg, 0, sizeof(fd->fd_seg));
	last_cluster = totalSectors / 2 - 1;
	_int3(last_cluster * 2, fd->fd_seg[0].lsn);
	_int2(2, fd->fd_seg[0].num);
	_int4(1, fd->fd_siz);	/* in use, so closing keeps the cluster */
	raw_sector(image, fd_lsn, fd_buf, 1);
	ASSERT_EC(0, _os9_open(&p, pathlist, FAM_READ | FAM_WRITE));
	size = 2 * 256 + 1;
	ASSERT_EC(EOS_DF, _os9_write(p, buffer, &size));
	ASSERT_EC(0, _os9_close(p));

	/* The image did not grow, and the map is as it was. */
	ASSERT_EQUALS(image_size, raw_size(image));
	raw_sector(image, 1, back, 0);
	ASSERT_MEM_EQUALS(map, back, map_bytes);

	remove(image);
}

void test_os9_file_allocation()
{
	os9_path_id p;
	error_code ec;
	int i;

	/* Create disk */
	unsigned int totalSectors, totalBytes;
	int clusterSize = 0;
	ec = _os9_format("test_alloc.dsk", 0, 80, 18, 18, 2, 256, &clusterSize, "Test Allo", 8, 8,
			 1, 1, 0, 0, 3, &totalSectors, &totalBytes);

	/* Record free space */
	char dname[64];
	u_int month, day, year;
	u_int bps, total_sectors, bytes_free, free_sectors,
		largest_free_block, sectors_per_cluster;
	u_int largest_count, sector_count;
	ec = TSRBFFree("test_alloc.dsk", dname, &month, &day, &year, &bps,
			   &total_sectors, &bytes_free, &free_sectors,
			   &largest_free_block, &sectors_per_cluster,
			   &largest_count, &sector_count);
	ASSERT_EQUALS(0, ec);

	/* Create 128 384 bytes files */
	char filename[128];
	u_int size;
	u_int bytes_per_file = 256*120+1;
	u_int end_file = 20;

	for(i=1; i<end_file; i++)
	{
		char *buffer = malloc(bytes_per_file);
		ASSERT_EQUALS(1, buffer != NULL);
		memset(buffer, i, bytes_per_file);
		sprintf(filename, "test_alloc.dsk,test%d.txt", i);
		ec = _os9_create(&p, filename, FAM_READ | FAM_WRITE, FAP_READ | FAP_WRITE);
		ASSERT_EQUALS(0, ec);
		size = bytes_per_file;
		ec = _os9_write(p, buffer, &size);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(bytes_per_file, size);
		ec = _os9_close(p);
		ASSERT_EQUALS(0, ec);
		free(buffer);
	}

	/* make sure bytes_free does not equal new_bytes_free */
	u_int new_bytes_free;
	ec = TSRBFFree("test_alloc.dsk", dname, &month, &day, &year, &bps,
		   &total_sectors, &new_bytes_free, &free_sectors,
		   &largest_free_block, &sectors_per_cluster,
		   &largest_count, &sector_count);
	ASSERT_EQUALS(0, ec);
	ASSERT_NEQUALS(new_bytes_free, bytes_free);

	/* Open all file */
	for(i=1; i<end_file; i++)
	{
		/* read back data and check bytes */
		char *buffer = malloc(bytes_per_file);
		ASSERT_EQUALS(1, buffer != NULL);
		memset(buffer, i, bytes_per_file);
		char *buffer2 = malloc(bytes_per_file);
		ASSERT_NEQUALS(buffer2, 0);
		sprintf(filename, "test_alloc.dsk,test%d.txt", i);
		ec = _os9_open(&p, filename, FAM_READ);
		ASSERT_EQUALS(ec, 0);
		size = bytes_per_file;
		ec = _os9_read(p, buffer2, &size);
		ASSERT_EQUALS(ec, 0);
		ASSERT_EQUALS(size, bytes_per_file);
		ASSERT_EQUALS(memcmp(buffer, buffer2, bytes_per_file), 0);
		ASSERT_EQUALS(ec, 0);
		free(buffer);
		free(buffer2);

		/* test for allocation bitmaps spanning two sectors */
		/* OS-9/6809 can not delete files if this is true */
		fd_stats fd;
		ASSERT_EQUALS(read_lsn(p, p->pl_fd_lsn, &fd),256);
		for(int j=0; j<NUM_SEGS; j++)
		{
			if(int3(fd.fd_seg[j].lsn)==0) break;
			ASSERT_EQUALS((int3(fd.fd_seg[j].lsn)+int2(fd.fd_seg[j].num)-1)/2048, int3(fd.fd_seg[j].lsn)/2048);
		}

		ec = _os9_close(p);
	}

	/* Delete all files */
	for(i=1; i<end_file; i++)
	{
		sprintf(filename, "test_alloc.dsk,test%d.txt", i);
		ec = _os9_delete(filename);
		ASSERT_EQUALS(0, ec);
	}

	/* compare free bytes again */
	ec = TSRBFFree("test_alloc.dsk", dname, &month, &day, &year, &bps,
		   &total_sectors, &new_bytes_free, &free_sectors,
		   &largest_free_block, &sectors_per_cluster,
		   &largest_count, &sector_count);
	ASSERT_EQUALS(ec, 0);
	ASSERT_EQUALS(bytes_free, new_bytes_free);
}

/* Exercise both SAS allocation and contiguous extension at bitmap boundaries.
 * A 256-byte bitmap sector describes 2048 clusters, not 2048 disk sectors.
 */
void test_os9_allocation_bitmap_boundaries()
{
	int spc, boundary, sas;
	error_code ec;

	for (spc = 1; spc <= 2; spc *= 2)
	{
		for (boundary = 2048; boundary <= 4096; boundary += 2048)
		{
			for (sas = 1; sas <= 8; sas += 7)
			{
				os9_path_id p;
				fd_stats fd;
				int clusterSize = spc;
				unsigned int totalSectors, totalBytes;
				u_int size;
				char buffer[8 * 2 * 256], actual[sizeof(buffer)];

				ec = _os9_format("test_boundary.dsk", 0,
					256, 16, 16, 4, 256, &clusterSize, "Boundary",
					sas * spc, 0, 0, 1, 0, 1, 3,
					&totalSectors, &totalBytes);
				ASSERT_EQUALS(0, ec);
				ec = _os9_create(&p, "test_boundary.dsk,file",
					FAM_READ | FAM_WRITE, FAP_READ | FAP_WRITE);
				ASSERT_EQUALS(0, ec);
				ASSERT_EQUALS(spc, p->spc);
				ASSERT_EQUALS(sas * spc, int1(p->lsn0->dd_opt.m6809.pd_sas));

				/* Discard the descriptor cluster's spare sectors so both
				 * cluster sizes start with an empty segment list. */
				ASSERT_EQUALS(256, read_lsn(p, p->pl_fd_lsn, &fd));
				memset(fd.fd_seg, 0, sizeof(fd.fd_seg));
				ASSERT_EQUALS(0, _os9_lsn_put(p, p->pl_fd_lsn, 0, &fd, sizeof(fd)));

				/* Leave only four free clusters before the boundary and
				 * sixteen after it. The file descriptor already exists.
				 */
				memset(p->bitmap, 0xff, p->bitmap_bytes);
				_os9_delbit(p->bitmap, boundary - 4, 20);
				size = 8 * spc * 256;
				memset(buffer, 0x5a, size);
				ec = _os9_write(p, buffer, &size);
				ASSERT_EQUALS(0, ec);
				ASSERT_EQUALS(8 * spc * 256, size);
				ASSERT_EQUALS(256, read_lsn(p, p->pl_fd_lsn, &fd));

				if (sas == 1)
				{
					/* Extension must split adjacent space at the boundary. */
					ASSERT_EQUALS((boundary - 4) * spc, int3(fd.fd_seg[0].lsn));
					ASSERT_EQUALS(4 * spc, int2(fd.fd_seg[0].num));
					ASSERT_EQUALS(boundary * spc, int3(fd.fd_seg[1].lsn));
					ASSERT_EQUALS(4 * spc, int2(fd.fd_seg[1].num));
					ASSERT_EQUALS(0, int3(fd.fd_seg[2].lsn));
				}
				else
				{
					/* An eight-cluster SAS must skip the short free run. */
					ASSERT_EQUALS(boundary * spc, int3(fd.fd_seg[0].lsn));
					ASSERT_EQUALS(8 * spc, int2(fd.fd_seg[0].num));
					ASSERT_EQUALS(0, int3(fd.fd_seg[1].lsn));
					ec = _os9_ckbit(p->bitmap, boundary - 4);
					ASSERT_EQUALS(0, ec);
				}

				for (int j = 0; j < NUM_SEGS && int3(fd.fd_seg[j].lsn); j++)
				{
					int start = int3(fd.fd_seg[j].lsn);
					int end = start + int2(fd.fd_seg[j].num) - 1;
					ASSERT_EQUALS(start / spc / 2048, end / spc / 2048);
				}

				ec = _os9_close(p);
				ASSERT_EQUALS(0, ec);
				/* Verify the persisted segment list also reads correctly. */
				ec = _os9_open(&p, "test_boundary.dsk,file", FAM_READ);
				ASSERT_EQUALS(0, ec);
				ec = _os9_read(p, actual, &size);
				ASSERT_EQUALS(0, ec);
				ASSERT_EQUALS(8 * spc * 256, size);
				ASSERT_MEM_EQUALS(buffer, actual, size);
				ec = _os9_close(p);
				ASSERT_EQUALS(0, ec);
				ec = _os9_delete("test_boundary.dsk,file");
				ASSERT_EQUALS(0, ec);
				ec = remove("test_boundary.dsk");
				ASSERT_EQUALS(0, ec);
			}
		}
	}
}

/* Return the image file offset of an LSN on a short track 0 disk, worked out
 * here rather than through _os9_lsn_fseek so that the tests check the layout.
 */
static long short_t0_offset(int lsn, int spt, int t0s, int bps)
{
	return (long) (lsn < t0s ? lsn : lsn + spt - t0s) * bps;
}

/* Check every sector of a file against the expected data at its position in
 * the image file. The file must be one segment. Returns 0 on success.
 */
static int short_t0_file_mismatch(const char *image, int lsn, const char *expected, int size,
				  int spt, int t0s, int bps)
{
	FILE *f = fopen(image, "rb");
	char sector[512];
	int pos, result = 0;

	if (f == NULL)
	{
		return -1;
	}

	for (pos = 0; pos < size && result == 0; pos += bps)
	{
		int count = size - pos < bps ? size - pos : bps;

		if (fseek(f, short_t0_offset(lsn + pos / bps, spt, t0s, bps), SEEK_SET) != 0
		    || fread(sector, 1, count, f) != (size_t) count
		    || memcmp(sector, expected + pos, count) != 0)
		{
			result = pos + 1;
		}
	}

	fclose(f);

	return result;
}

/* Write and read a file whose segment crosses T0S on a short track 0 disk. */
static void short_t0_segment_io(int os968k, int bps)
{
	const char *image = "test_short_t0.dsk";
	const int spt = 18, t0s = 14;
	os9_path_id p;
	error_code ec;
	fd_stats fd;
	int clusterSize = 1, lsn, i;
	unsigned int totalSectors, totalBytes;
	u_int size, chunk, pos;
	char data[6000], actual[sizeof(data)], patch[300];
	char line[] = "crossing\r";

	for (i = 0; i < (int) sizeof(data); i++)
	{
		data[i] = (char) (i * 7 + i / 256);
	}

	ec = _os9_format((char *) image, os968k, 8, spt, t0s, 2, bps, &clusterSize, "Short T0",
			 8, 0, 0, 1, 0, 0, 3, &totalSectors, &totalBytes);
	ASSERT_EQUALS(0, ec);

	/* A single write across the boundary. */
	ec = _os9_create(&p, "test_short_t0.dsk,file", FAM_READ | FAM_WRITE,
			 FAP_READ | FAP_WRITE);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(t0s, p->t0s);
	ASSERT_EQUALS(bps, p->bps);
	size = sizeof(data);
	ec = _os9_write(p, data, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(data), size);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);

	/* The test only means something if the segment starts below T0S and
	 * extends past it. */
	ec = _os9_open(&p, "test_short_t0.dsk,file", FAM_READ | FAM_WRITE);
	ASSERT_EQUALS(0, ec);
	ec = _os9_gs_fd(p, sizeof(fd), &fd);
	ASSERT_EQUALS(0, ec);
	lsn = int3(fd.fd_seg[0].lsn);
	ASSERT("segment starts below T0S", lsn < t0s);
	ASSERT("segment reaches past T0S", lsn + int2(fd.fd_seg[0].num) > t0s);
	ASSERT("file data reaches past T0S", lsn + (int) sizeof(data) / bps > t0s);
	ASSERT_EQUALS(0, int3(fd.fd_seg[1].lsn));
	ASSERT_EQUALS(0, short_t0_file_mismatch(image, lsn, data, sizeof(data), spt, t0s, bps));

	/* A single read across the boundary. */
	memset(actual, 0, sizeof(actual));
	ec = _os9_seek(p, 0, SEEK_SET);
	ASSERT_EQUALS(0, ec);
	size = sizeof(actual);
	ec = _os9_read(p, actual, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(data), size);
	ASSERT_EQUALS(sizeof(data), p->filepos);
	ASSERT_MEM_EQUALS(data, actual, sizeof(data));

	/* An unaligned overwrite that starts and ends mid-sector either side of
	 * the boundary. */
	pos = (t0s - lsn) * bps - 100;
	memset(patch, 0xa5, sizeof(patch));
	memcpy(data + pos, patch, sizeof(patch));
	ec = _os9_seek(p, pos, SEEK_SET);
	ASSERT_EQUALS(0, ec);
	size = sizeof(patch);
	ec = _os9_write(p, patch, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(pos + sizeof(patch), p->filepos);
	ASSERT_EQUALS(0, short_t0_file_mismatch(image, lsn, data, sizeof(data), spt, t0s, bps));

	/* An unaligned read of the same bytes. */
	ec = _os9_seek(p, pos, SEEK_SET);
	ASSERT_EQUALS(0, ec);
	size = sizeof(patch);
	memset(actual, 0, sizeof(patch));
	ec = _os9_read(p, actual, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(patch), size);
	ASSERT_MEM_EQUALS(patch, actual, sizeof(patch));

	/* A position wholly past T0S, reached through the start of the segment. */
	pos = (t0s - lsn + 2) * bps + 10;
	ec = _os9_seek(p, pos, SEEK_SET);
	ASSERT_EQUALS(0, ec);
	size = 20;
	ec = _os9_read(p, actual, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_MEM_EQUALS(data + pos, actual, 20);

	/* A line that spans the boundary ends at its terminator, not the buffer. */
	pos = (t0s - lsn) * bps - 4;
	memcpy(data + pos, line, strlen(line));
	ec = _os9_seek(p, pos, SEEK_SET);
	ASSERT_EQUALS(0, ec);
	size = strlen(line);
	ec = _os9_write(p, line, &size);
	ASSERT_EQUALS(0, ec);
	ec = _os9_seek(p, pos, SEEK_SET);
	ASSERT_EQUALS(0, ec);
	size = sizeof(actual);
	memset(actual, 0, sizeof(actual));
	ec = _os9_readln(p, actual, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(strlen(line), size);
	ASSERT_EQUALS(pos + strlen(line), p->filepos);
	ASSERT_MEM_EQUALS(line, actual, strlen(line));

	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(0, short_t0_file_mismatch(image, lsn, data, sizeof(data), spt, t0s, bps));

	/* Small writes, each starting from the segment start, land the same way. */
	ec = _os9_delete("test_short_t0.dsk,file");
	ASSERT_EQUALS(0, ec);
	ec = _os9_create(&p, "test_short_t0.dsk,file", FAM_READ | FAM_WRITE,
			 FAP_READ | FAP_WRITE);
	ASSERT_EQUALS(0, ec);
	for (pos = 0; pos < sizeof(data); pos += chunk)
	{
		chunk = sizeof(data) - pos < 100 ? sizeof(data) - pos : 100;
		size = chunk;
		ec = _os9_write(p, data + pos, &size);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(chunk, size);
	}
	ec = _os9_gs_fd(p, sizeof(fd), &fd);
	ASSERT_EQUALS(0, ec);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(lsn, int3(fd.fd_seg[0].lsn));
	ASSERT_EQUALS(0, int3(fd.fd_seg[1].lsn));
	ASSERT_EQUALS(0, short_t0_file_mismatch(image, lsn, data, sizeof(data), spt, t0s, bps));

	ec = remove(image);
	ASSERT_EQUALS(0, ec);
}

void test_os9_short_t0_segment_io()
{
	short_t0_segment_io(0, 256);
}

/* OS-9/68K keeps pd_t0s one byte further into DD.OPT, and a misread value is
 * nothing like the real one. Cover it directly and through file positions.
 */
void test_os9_68k_short_t0()
{
	os9_path_id p;
	error_code ec;
	int clusterSize = 1;
	unsigned int totalSectors, totalBytes;

	ec = _os9_format("test_osk_t0.dsk", 1, 8, 18, 14, 2, 256, &clusterSize, "OSK T0",
			 8, 0, 0, 1, 0, 0, 3, &totalSectors, &totalBytes);
	ASSERT_EQUALS(0, ec);
	ec = _os9_open(&p, "test_osk_t0.dsk,.", FAM_DIR | FAM_READ);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(14, p->t0s);
	ASSERT_EQUALS(18, p->spt);
	ASSERT_EQUALS(14, int2(p->lsn0->dd_opt.m68k.pd_t0s));
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);
	ec = remove("test_osk_t0.dsk");
	ASSERT_EQUALS(0, ec);

	short_t0_segment_io(1, 256);
	short_t0_segment_io(1, 512);
}

/* A full-track-0 disk has no missing sectors, so offsets stay linear. */
void test_os9_full_t0_positions_unchanged()
{
	os9_path_id p;
	error_code ec;
	fd_stats fd;
	int clusterSize = 1, i;
	unsigned int totalSectors, totalBytes;
	u_int size;
	char data[6000];

	for (i = 0; i < (int) sizeof(data); i++)
	{
		data[i] = (char) (i * 3 + i / 256);
	}

	ec = _os9_format("test_full_t0.dsk", 0, 8, 18, 18, 2, 256, &clusterSize, "Full T0",
			 8, 0, 0, 1, 0, 0, 3, &totalSectors, &totalBytes);
	ASSERT_EQUALS(0, ec);
	ec = _os9_create(&p, "test_full_t0.dsk,file", FAM_READ | FAM_WRITE,
			 FAP_READ | FAP_WRITE);
	ASSERT_EQUALS(0, ec);
	size = sizeof(data);
	ec = _os9_write(p, data, &size);
	ASSERT_EQUALS(0, ec);
	ec = _os9_gs_fd(p, sizeof(fd), &fd);
	ASSERT_EQUALS(0, ec);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(0, int3(fd.fd_seg[1].lsn));
	ASSERT_EQUALS(0, short_t0_file_mismatch("test_full_t0.dsk", int3(fd.fd_seg[0].lsn), data,
						sizeof(data), 18, 18, 256));
	ec = remove("test_full_t0.dsk");
	ASSERT_EQUALS(0, ec);
}

/* Read one sector of an image at its expected file position. */
static int read_short_t0_sector(FILE *f, int lsn, int spt, int t0s, int bps, void *sector)
{
	return fseek(f, short_t0_offset(lsn, spt, t0s, bps), SEEK_SET) == 0
		&& fread(sector, 1, bps, f) == (size_t) bps;
}

/* Check where format puts each structure, then that the disk is usable. */
static void short_t0_format_layout(int os968k, int t0s, int formatEntire, int bps)
{
	const char *image = "test_format_t0.dsk";
	const int tracks = 8, spt = 18, heads = 2;
	os9_path_id p;
	error_code ec;
	FILE *f;
	lsn0_sect s0;
	fd_stats fd;
	os9_dir_entry entry;
	int clusterSize = 1, dir_lsn, slot;
	unsigned int totalSectors, totalBytes;
	u_int size;
	char sector[512], data[3000], actual[sizeof(data)];

	ec = _os9_format((char *) image, os968k, tracks, spt, t0s, heads, bps, &clusterSize, "Format T0",
			 8, 0, 0, formatEntire, 0, 0, 3, &totalSectors, &totalBytes);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(tracks * spt * heads - spt + t0s, totalSectors);
	ASSERT_EQUALS(totalSectors * bps, totalBytes);

	f = fopen(image, "rb");
	ASSERT("open formatted image", f != NULL);
	/* ASSERT_EQUALS evaluates its arguments twice, so read outside of it. */
	size = fread(&s0, 1, sizeof(s0), f);
	ASSERT_EQUALS(sizeof(s0), size);
	ASSERT_EQUALS(totalSectors, int3(s0.dd_tot));
	dir_lsn = int3(s0.dd_dir);

	/* The first bitmap bit covers LSN0, which is always allocated. */
	ASSERT("read bitmap", read_short_t0_sector(f, 1, spt, t0s, bps, sector));
	ASSERT("bitmap in place", (sector[0] & 0x80) != 0);

	ASSERT("read root FD", read_short_t0_sector(f, dir_lsn, spt, t0s, bps, sector));
	memcpy(&fd, sector, sizeof(fd));
	ASSERT("root FD in place", (fd.fd_att & FAP_DIR) != 0);
	ASSERT_EQUALS(sizeof(os9_dir_entry) * 2, int4(fd.fd_siz));
	ASSERT_EQUALS(dir_lsn + 1, int3(fd.fd_seg[0].lsn));

	ASSERT("read root dir", read_short_t0_sector(f, dir_lsn + 1, spt, t0s, bps, sector));
	memcpy(&entry, sector, sizeof(entry));
	ASSERT_EQUALS('.', entry.name[0]);
	ASSERT_EQUALS(dir_lsn, int3(entry.lsn));

	/* Format's structures reaching T0S must leave the unused slots as fill. */
	if (formatEntire || dir_lsn + (int) int2(fd.fd_seg[0].num) >= t0s)
	{
		for (slot = t0s; slot < spt; slot++)
		{
			int i;

			ASSERT_EQUALS(0, fseek(f, (long) slot * bps, SEEK_SET));
			size = fread(sector, 1, bps, f);
			ASSERT_EQUALS(bps, size);
			for (i = 0; i < bps; i++)
			{
				ASSERT_EQUALS(0xE5, (unsigned char) sector[i]);
			}
		}
	}

	if (formatEntire)
	{
		ASSERT_EQUALS(0, fseek(f, 0, SEEK_END));
		ASSERT_EQUALS((long) tracks * spt * heads * bps, ftell(f));
	}
	fclose(f);

	memset(data, 0x5a, sizeof(data));
	ec = _os9_create(&p, "test_format_t0.dsk,file", FAM_READ | FAM_WRITE,
			 FAP_READ | FAP_WRITE);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(t0s, p->t0s);
	size = sizeof(data);
	ec = _os9_write(p, data, &size);
	ASSERT_EQUALS(0, ec);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);
	ec = _os9_open(&p, "test_format_t0.dsk,file", FAM_READ);
	ASSERT_EQUALS(0, ec);
	size = sizeof(actual);
	ec = _os9_read(p, actual, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(data), size);
	ASSERT_MEM_EQUALS(data, actual, sizeof(data));
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);

	ec = remove(image);
	ASSERT_EQUALS(0, ec);
}

void test_os9_format_short_t0_layout()
{
	static const int t0s_values[] = {1, 2, 4, 14, 17};
	int i, entire, osk;

	for (i = 0; i < (int) (sizeof(t0s_values) / sizeof(t0s_values[0])); i++)
	{
		for (entire = 0; entire <= 1; entire++)
		{
			for (osk = 0; osk <= 1; osk++)
			{
				short_t0_format_layout(osk, t0s_values[i], entire, 256);
			}
		}
		short_t0_format_layout(1, t0s_values[i], 1, 512);
	}
}

/* A bitmap of more than one sector crossing T0S must load and save in place. */
void test_os9_bitmap_crosses_short_t0()
{
	const char *image = "test_bitmap_t0.dsk";
	const int spt = 18, t0s = 2, bps = 256;
	const int test_cluster = 2048 + 100;	/* described by the second bitmap sector */
	os9_path_id p;
	error_code ec;
	FILE *f;
	int clusterSize = 1, bitmap_bytes;
	unsigned int totalSectors, totalBytes;
	char sector[256];

	ec = _os9_format((char *) image, 0, 80, spt, t0s, 2, bps, &clusterSize, "Bitmap T0",
			 8, 0, 0, 1, 0, 0, 3, &totalSectors, &totalBytes);
	ASSERT_EQUALS(0, ec);

	/* The bitmap's first sector is LSN 1 and its second is LSN 2, past T0S. */
	ec = _os9_open(&p, "test_bitmap_t0.dsk,.", FAM_DIR | FAM_READ | FAM_WRITE);
	ASSERT_EQUALS(0, ec);
	bitmap_bytes = p->bitmap_bytes;
	ASSERT("bitmap crosses T0S", bitmap_bytes > bps);
	ASSERT_EQUALS(0, _os9_ckbit(p->bitmap, test_cluster));
	_os9_allbit(p->bitmap, test_cluster, 1);
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);

	f = fopen(image, "rb");
	ASSERT("open bitmap image", f != NULL);
	ASSERT("read second bitmap sector", read_short_t0_sector(f, 2, spt, t0s, bps, sector));
	fclose(f);
	ASSERT("bit written past the gap", (sector[(test_cluster - 2048) / 8] & (0x80 >> (test_cluster % 8))) != 0);

	ec = _os9_open(&p, "test_bitmap_t0.dsk,.", FAM_DIR | FAM_READ);
	ASSERT_EQUALS(0, ec);
	ASSERT_NEQUALS(0, _os9_ckbit(p->bitmap, test_cluster));
	ASSERT_EQUALS(0, _os9_ckbit(p->bitmap, test_cluster + 1));
	ec = _os9_close(p);
	ASSERT_EQUALS(0, ec);

	ec = remove(image);
	ASSERT_EQUALS(0, ec);
}

/* T0S values with no valid layout must not touch an existing image. */
void test_os9_format_rejects_bad_t0s()
{
	const char *image = "test_bad_t0.dsk";
	static const int bad_values[] = {0, -1, 19};
	FILE *f;
	error_code ec;
	int clusterSize = 1, i;
	unsigned int totalSectors, totalBytes;

	f = fopen(image, "wb");
	ASSERT("create existing image", f != NULL);
	fputs("keep", f);
	fclose(f);

	for (i = 0; i < 3; i++)
	{
		ec = _os9_format((char *) image, 0, 8, 18, bad_values[i], 2, 256, &clusterSize, "Bad",
				 8, 0, 0, 1, 0, 0, 3, &totalSectors, &totalBytes);
		ASSERT_NEQUALS(0, ec);
	}

	f = fopen(image, "rb");
	ASSERT("reopen existing image", f != NULL);
	ASSERT_EQUALS(0, fseek(f, 0, SEEK_END));
	ASSERT_EQUALS(4, ftell(f));
	fclose(f);
	ec = remove(image);
	ASSERT_EQUALS(0, ec);
}

#define OS9_IMAGE "test_os9_format.img"

typedef struct
{
	diskimage_format format;
	int tracks;
	int heads;
	int sectors_per_track;
	int attributes;		/* JVC with sector attributes */
} os9_layout;

/* These can all lose a sector, and go through both sides where they have two. */
static const os9_layout damage_layouts[] = {
	{ DISKIMAGE_DMK, 35, 1, 18, 0 },
	{ DISKIMAGE_DMK, 35, 2, 18, 0 },
	{ DISKIMAGE_SDF, 35, 1, 18, 0 },
	{ DISKIMAGE_SDF, 35, 2, 18, 0 },
	{ DISKIMAGE_JVC, 16, 1, 16, 1 },
};

static error_code make_os9_image(const os9_layout *layout)
{
	unsigned int total_sectors, total_bytes;
	int cluster_size = 0;
	error_code ec;

	remove(OS9_IMAGE);
	ec = _os9_format_image(OS9_IMAGE, layout->attributes ? DISKIMAGE_RAW : layout->format, 0, layout->tracks,
			       layout->sectors_per_track, layout->sectors_per_track, layout->heads, 256,
			       &cluster_size, "Test", 8, 48, 1, 1, 0, 0, 3, &total_sectors, &total_bytes);
	if (ec == 0 && layout->attributes)
	{
		image_add_jvc_attributes(OS9_IMAGE, layout->sectors_per_track, layout->heads);
	}

	return ec;
}

static void layout_address(const os9_layout *layout, int lsn, int *cylinder, int *side, int *sector)
{
	int track = lsn / layout->sectors_per_track;

	*cylinder = track / layout->heads;
	*side = track % layout->heads;
	*sector = lsn % layout->sectors_per_track + 1;
}

static void check_os9_damage(const os9_layout *layout)
{
	static u_char first[6000], second[1024], back[6000];
	/* Past the first track, so on the second side of a disk that has one. */
	const int target_lsn = 20;
	diskimage_format format = DISKIMAGE_UNKNOWN;
	diskimage_id disk;
	os9_path_id p;
	u_int size = sizeof(back);
	u_int sector_size = 256;
	u_char direct[256];
	int first_lsn, second_lsn, cylinder, side, sector, i;

	for (i = 0; i < (int) sizeof(first); i++)
	{
		first[i] = (u_char) (i * 7);
	}
	for (i = 0; i < (int) sizeof(second); i++)
	{
		second[i] = (u_char) (i * 9);
	}

	ASSERT_EC(0, make_os9_image(layout));
	ASSERT_EC(0, _diskimage_identify(OS9_IMAGE, &format));
	ASSERT_EQUALS(layout->format, format);

	os9_write_file(OS9_IMAGE ",FIRST", first, sizeof(first), &first_lsn);
	os9_write_file(OS9_IMAGE ",SECOND", second, sizeof(second), &second_lsn);
	ASSERT("the file reaches the sector to remove", first_lsn <= target_lsn && target_lsn < first_lsn + 23);
	ASSERT("the second file comes after it", second_lsn > target_lsn);
	layout_address(layout, target_lsn, &cylinder, &side, &sector);
	if (layout->heads == 2)
	{
		ASSERT_EQUALS(1, side);
	}

	/* The LSN is where it should be on the disk. */
	ASSERT_EC(0, _diskimage_open(&disk, OS9_IMAGE, FAM_READ));
	ASSERT_EC(0, _diskimage_read_sector(disk, cylinder, side, sector, direct, &sector_size));
	ASSERT_EQUALS(0, memcmp(&first[(target_lsn - first_lsn) * 256], direct, 256));
	_diskimage_close(disk);

	/* A bad CRC gives the data as recorded. */
	image_damage_sector(OS9_IMAGE, cylinder, side, sector);
	ASSERT_EC(0, _os9_open(&p, OS9_IMAGE ",FIRST", FAM_READ));
	ASSERT_EC(0, _os9_read(p, back, &size));
	ASSERT_EQUALS(sizeof(first), size);
	ASSERT_EQUALS((u_char) (first[(target_lsn - first_lsn) * 256] ^ 0xFF), back[(target_lsn - first_lsn) * 256]);
	ASSERT_EC(0, _os9_close(p));

	/* A missing sector fails its file, and moves no other sector. */
	image_remove_sector(OS9_IMAGE, cylinder, side, sector);
	ASSERT_EC(0, _os9_open(&p, OS9_IMAGE ",SECOND", FAM_READ));
	size = sizeof(back);
	ASSERT_EC(0, _os9_read(p, back, &size));
	ASSERT_EQUALS(sizeof(second), size);
	ASSERT_EQUALS(0, memcmp(second, back, sizeof(second)));
	ASSERT_EC(0, _os9_close(p));

	ASSERT_EC(0, _os9_open(&p, OS9_IMAGE ",FIRST", FAM_READ));
	size = sizeof(back);
	ASSERT_EC(EOS_SECT, _os9_read(p, back, &size));
	ASSERT_EC(0, _os9_close(p));

	/* A raw path stops before it, and then says so. */
	ASSERT_EC(0, _os9_open(&p, OS9_IMAGE ",@", FAM_READ));
	ASSERT_EC(0, _os9_seek(p, (target_lsn - 1) * 256, SEEK_SET));
	size = sizeof(back);
	ASSERT_EC(0, _os9_read(p, back, &size));
	ASSERT_EQUALS(256, size);
	size = sizeof(back);
	ASSERT_EC(EOS_SECT, _os9_read(p, back, &size));
	ASSERT_EQUALS(0, size);
	ASSERT_EC(0, _os9_close(p));

	/* Without the bitmap, files still read, but none can be allocated or freed. */
	layout_address(layout, 1, &cylinder, &side, &sector);
	image_remove_sector(OS9_IMAGE, cylinder, side, sector);
	ASSERT_EC(0, _os9_open(&p, OS9_IMAGE ",SECOND", FAM_READ));
	ASSERT_EQUALS(EOS_SECT, p->bitmap_ec);
	size = sizeof(back);
	ASSERT_EC(0, _os9_read(p, back, &size));
	ASSERT_EC(0, _os9_close(p));
	ASSERT_EC(EOS_SECT, _os9_create(&p, OS9_IMAGE ",NEW", FAM_READ | FAM_WRITE, FAP_READ | FAP_WRITE));
	ASSERT_EC(EOS_SECT, _os9_delete(OS9_IMAGE ",SECOND"));

	/* Without LSN0 there is nothing to go on. */
	image_remove_sector(OS9_IMAGE, 0, 0, 1);
	ASSERT_EC(EOS_SECT, _os9_open(&p, OS9_IMAGE ",SECOND", FAM_READ));
	remove(OS9_IMAGE);
}

void test_os9_damage_all_formats()
{
	size_t i;

	for (i = 0; i < sizeof(damage_layouts) / sizeof(damage_layouts[0]); i++)
	{
		check_os9_damage(&damage_layouts[i]);
	}
}

/* Make a disk in a format, and use all that a disk does. */
static void check_os9_round_trip(const os9_layout *layout)
{
	static u_char data[3000], back[3000], whole[1024];
	os9_path_id p;
	u_int size = sizeof(back);
	int data_lsn, i;

	for (i = 0; i < (int) sizeof(data); i++)
	{
		data[i] = (u_char) (i * 5);
	}

	ASSERT_EC(0, make_os9_image(layout));
	os9_write_file(OS9_IMAGE ",FILE", data, sizeof(data), &data_lsn);
	ASSERT_EC(0, _os9_makdir(OS9_IMAGE ",DIR"));
	ASSERT_EC(0, _os9_rename(OS9_IMAGE ",FILE", "OTHER"));

	ASSERT_EC(0, _os9_open(&p, OS9_IMAGE ",OTHER", FAM_READ));
	ASSERT_EC(0, _os9_read(p, back, &size));
	ASSERT_EQUALS(sizeof(data), size);
	ASSERT_EQUALS(0, memcmp(data, back, sizeof(data)));

	/* Past the last track is not a sector. */
	ASSERT_EC(EOS_SECT, _os9_lsn_get(p, layout->tracks * layout->heads * layout->sectors_per_track, 0, whole, 256));
	ASSERT_EC(0, _os9_close(p));

	/* A raw path reads the disk by LSN, whatever the format. */
	ASSERT_EC(0, _os9_open(&p, OS9_IMAGE ",@", FAM_READ));
	ASSERT_EC(0, _os9_seek(p, data_lsn * 256, SEEK_SET));
	size = sizeof(whole);
	ASSERT_EC(0, _os9_read(p, whole, &size));
	ASSERT_EQUALS(sizeof(whole), size);
	ASSERT_EQUALS(0, memcmp(data, whole, sizeof(whole)));
	ASSERT_EC(0, _os9_seek(p, 0, SEEK_END));
	ASSERT_EC(EOS_EOF, _os9_gs_eof(p));
	ASSERT_EC(EOS_SE, _os9_seek(p, -100000000, SEEK_CUR));
	ASSERT_EC(0, _os9_close(p));

	/* And writes it too, keeping the rest of each sector. */
	ASSERT_EC(0, _os9_open(&p, OS9_IMAGE ",@", FAM_READ | FAM_WRITE));
	ASSERT_EC(0, _os9_seek(p, data_lsn * 256 + 250, SEEK_SET));
	size = 12;
	memset(whole, 0xA5, 12);
	ASSERT_EC(0, _os9_write(p, whole, &size));
	ASSERT_EQUALS(12, size);
	ASSERT_EC(0, _os9_close(p));

	ASSERT_EC(0, _os9_open(&p, OS9_IMAGE ",OTHER", FAM_READ));
	size = sizeof(back);
	ASSERT_EC(0, _os9_read(p, back, &size));
	ASSERT_EQUALS(data[249], back[249]);
	ASSERT_EQUALS(0xA5, back[250]);
	ASSERT_EQUALS(0xA5, back[261]);
	ASSERT_EQUALS(data[262], back[262]);
	ASSERT_EC(0, _os9_close(p));

	ASSERT_EC(0, _os9_delete(OS9_IMAGE ",OTHER"));
	ASSERT_EC(EOS_PNNF, _os9_open(&p, OS9_IMAGE ",OTHER", FAM_READ));
	remove(OS9_IMAGE);
}

void test_os9_round_trip_all_formats()
{
	static const os9_layout layouts[] = {
		{ DISKIMAGE_DMK, 35, 1, 18, 0 },
		{ DISKIMAGE_DMK, 40, 2, 18, 0 },
		{ DISKIMAGE_SDF, 35, 1, 18, 0 },
		{ DISKIMAGE_SDF, 40, 2, 18, 0 },
		{ DISKIMAGE_VDK, 35, 1, 18, 0 },
		{ DISKIMAGE_VDK, 40, 2, 18, 0 },
		{ DISKIMAGE_JVC, 40, 2, 10, 0 },
		{ DISKIMAGE_JVC, 16, 1, 16, 1 },
	};
	size_t i;

	for (i = 0; i < sizeof(layouts) / sizeof(layouts[0]); i++)
	{
		check_os9_round_trip(&layouts[i]);
	}
}

static void check_os9_write_protect(diskimage_format format)
{
	const os9_layout layout = { format, 35, 1, 18, 0 };
	u_char data[16] = { 0 }, back[16];
	os9_path_id p;
	u_int size = sizeof(back);
	int data_lsn;

	ASSERT_EC(0, make_os9_image(&layout));
	os9_write_file(OS9_IMAGE ",FILE", data, sizeof(data), &data_lsn);
	ASSERT_EC(0, _diskimage_set_write_protect(OS9_IMAGE, 1));

	/* Reading is still fine, and a path that could write closes cleanly. */
	ASSERT_EC(0, _os9_open(&p, OS9_IMAGE ",FILE", FAM_READ | FAM_WRITE));
	ASSERT_EC(0, _os9_read(p, back, &size));
	ASSERT_EC(0, _os9_close(p));

	ASSERT_EC(EOS_WP, _os9_create(&p, OS9_IMAGE ",NEW", FAM_READ | FAM_WRITE, FAP_READ | FAP_WRITE));
	ASSERT_EC(EOS_WP, _os9_delete(OS9_IMAGE ",FILE"));
	ASSERT_EC(EOS_WP, _os9_makdir(OS9_IMAGE ",DIR"));
	ASSERT_EC(EOS_WP, _os9_rename(OS9_IMAGE ",FILE", "OTHER"));

	/* A raw path is refused too. */
	ASSERT_EC(0, _os9_open(&p, OS9_IMAGE ",@", FAM_READ | FAM_WRITE));
	size = sizeof(back);
	ASSERT_EC(EOS_WP, _os9_write(p, back, &size));
	ASSERT_EQUALS(0, size);
	ASSERT_EC(0, _os9_close(p));

	ASSERT_EC(0, _diskimage_set_write_protect(OS9_IMAGE, 0));
	ASSERT_EC(0, _os9_delete(OS9_IMAGE ",FILE"));
	remove(OS9_IMAGE);
}

void test_os9_write_protect_all_formats()
{
	check_os9_write_protect(DISKIMAGE_DMK);
	check_os9_write_protect(DISKIMAGE_SDF);
	check_os9_write_protect(DISKIMAGE_VDK);
}

/* Patch LSN0 of an image, and see it fail rather than go wrong. */
void test_os9_bad_lsn0_on_tracks()
{
	const os9_layout normal = { DISKIMAGE_DMK, 35, 1, 18, 0 };
	unsigned int total_sectors, total_bytes;
	int cluster_size = 0;
	diskimage_id disk;
	lsn0_sect *lsn0;
	u_char sector[512];
	u_int size = sizeof(sector);
	os9_path_id p;

	/* No sectors per track must not divide by zero. */
	ASSERT_EC(0, make_os9_image(&normal));
	ASSERT_EC(0, _diskimage_open(&disk, OS9_IMAGE, FAM_READ | FAM_WRITE));
	size = sizeof(sector);
	ASSERT_EC(0, _diskimage_read_sector(disk, 0, 0, 1, sector, &size));
	lsn0 = (lsn0_sect *) sector;
	_int2(0, lsn0->dd_spt);
	_int2(0, lsn0->dd_opt.m6809.pd_t0s);
	ASSERT_EC(0, _diskimage_write_sector(disk, 0, 0, 1, sector, 256));
	_diskimage_close(disk);
	ASSERT_EC(EOS_SECT, _os9_open(&p, OS9_IMAGE ",", FAM_DIR | FAM_READ));
	remove(OS9_IMAGE);

	/* An LSN size that isn't the size of the disk's sectors */
	ASSERT_EC(0, _os9_format_image(OS9_IMAGE, DISKIMAGE_DMK, 0, 35, 9, 9, 1, 512, &cluster_size, "Test", 8, 48, 1, 1,
				       0, 0, 3, &total_sectors, &total_bytes));
	ASSERT_EC(0, _diskimage_open(&disk, OS9_IMAGE, FAM_READ | FAM_WRITE));
	size = sizeof(sector);
	ASSERT_EC(0, _diskimage_read_sector(disk, 0, 0, 1, sector, &size));
	ASSERT_EQUALS(512, size);
	lsn0 = (lsn0_sect *) sector;
	_int1(1, lsn0->dd_lsnsize);
	ASSERT_EC(0, _diskimage_write_sector(disk, 0, 0, 1, sector, 512));
	_diskimage_close(disk);
	ASSERT_EC(EOS_SECT, _os9_open(&p, OS9_IMAGE ",", FAM_DIR | FAM_READ));
	remove(OS9_IMAGE);
}

/*
 * "os9 files mame.dmk" was made by OS-9 Level Two VR 02.00.01 under MAME's coco3,
 * booted from OS9L2V2M.DSK with the clock set to 87/02/14 12:00:00.  Its format
 * command formatted the disk as FILES, a 6809 module written for the purpose
 * filled it through RBF, and DIRCMD is its CMDS/dir, copied after that.  The
 * clock loses time during disk access, so the dates are as OS-9 wrote them.
 *
 * Except for TEXT, files hold seed + (lo(i) ^ hi(i)) at offset i.  ZIGA and ZIGB
 * were written in turn, 2048 bytes at a time.  FULLSEGS and TEETH were written in
 * turn a sector at a time, with PD.SAS set to 1 by SS.Opt, until RBF refused
 * FULLSEGS a 48th segment with E$SLF.  TEETH was then deleted, and REFILL took
 * 20 of the holes that left.  RBF refused the names 9LIVES and _LEAD with E$BPNam.
 *
 * What this expects of the directories, free space and DIRCMD is what its dir e,
 * free and ident said.  Its dcheck said the file structure was intact.
 */
#define REAL_OS9 "../../../unittest/testdata/os9 files mame.dmk"
#define REAL_OS9_COPY "test_real_os9.dmk"
#define REAL_OS9_FREE 138
#define REAL_OS9_LARGEST 114

typedef struct
{
	const char *directory;
	const char *name;
	int lsn;
	u_int size;
	int attributes;
	int owner;
	int year, month, day, hour, minute;
} real_os9_entry;

static const real_os9_entry real_os9_entries[] = {
	{ "", "EMPTY", 0xA, 0x0, 0x1B, 0x0000, 87, 2, 14, 12, 11 },
	{ "", "ONEBYTE", 0xB, 0x1, 0x1B, 0x0000, 87, 2, 14, 12, 11 },
	{ "", "S255", 0xD, 0xFF, 0x1B, 0x0000, 87, 2, 14, 12, 11 },
	{ "", "S256", 0xF, 0x100, 0x1B, 0x0000, 87, 2, 14, 12, 11 },
	{ "", "S257", 0x11, 0x101, 0x1B, 0x0000, 87, 2, 14, 12, 11 },
	{ "", "LARGE", 0x14, 0x3A98, 0x1B, 0x0000, 87, 2, 14, 12, 11 },
	{ "", "ZIGA", 0x50, 0x2800, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "ZIGB", 0x51, 0x2800, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "SHRUNK", 0xA2, 0x3E8, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "SPARSE", 0xA7, 0xC1C, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "TEXT", 0xB5, 0x589, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "ATTRALL", 0xBC, 0x12C, 0x3F, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "ATTRNONE", 0xBF, 0x12C, 0x00, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "ATTREXEC", 0xC2, 0x12C, 0x2D, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "READONLY", 0xC5, 0x12C, 0x09, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "A", 0xC8, 0x14, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "ABCDEFGHIJKLMNOPQRSTUVWXYZ123", 0xCA, 0x15, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "MixedCase", 0xCC, 0x17, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "file.with.dots", 0xCE, 0x18, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "under_score", 0xD0, 0x19, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "DEEP", 0xD2, 0x80, 0xBF, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "EMPTYDIR", 0x122, 0x40, 0xBF, 0x0000, 87, 2, 14, 12, 13 },
	{ "", "DELS", 0x12B, 0xC0, 0xBF, 0x0000, 87, 2, 14, 12, 13 },
	{ "", "ALLGONE", 0x135, 0x80, 0xBF, 0x0000, 87, 2, 14, 12, 14 },
	{ "", "BIGDIR", 0x137, 0x900, 0xBF, 0x0000, 87, 2, 14, 12, 14 },
	{ "", "FULLSEGS", 0x1A0, 0x2F00, 0x1B, 0x0000, 87, 2, 14, 12, 17 },
	{ "", "REFILL", 0x1A1, 0x1400, 0x1B, 0x0000, 87, 2, 14, 12, 21 },
	{ "", "OWNED", 0x1CB, 0x64, 0x1B, 0x0102, 87, 2, 14, 12, 22 },
	{ "", "DIRCMD", 0x1CD, 0x36B, 0x2F, 0x0000, 87, 2, 13, 11, 11 },
	{ "DEEP", "FILE", 0xEF, 0x200, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "DEEP", "L2", 0xF2, 0x80, 0xBF, 0x0000, 87, 2, 14, 12, 12 },
	{ "DEEP/L2", "FILE", 0xFB, 0x200, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "DEEP/L2", "L3", 0xFE, 0x80, 0xBF, 0x0000, 87, 2, 14, 12, 13 },
	{ "DEEP/L2/L3", "FILE", 0x107, 0x200, 0x1B, 0x0000, 87, 2, 14, 12, 13 },
	{ "DEEP/L2/L3", "L4", 0x10A, 0x80, 0xBF, 0x0000, 87, 2, 14, 12, 13 },
	{ "DEEP/L2/L3/L4", "FILE", 0x113, 0x200, 0x1B, 0x0000, 87, 2, 14, 12, 13 },
	{ "DEEP/L2/L3/L4", "L5", 0x116, 0x60, 0xBF, 0x0000, 87, 2, 14, 12, 13 },
	{ "DEEP/L2/L3/L4/L5", "FILE", 0x11F, 0x200, 0x1B, 0x0000, 87, 2, 14, 12, 13 },
	{ "DELS", "REUSED", 0x134, 0x258, 0x1B, 0x0000, 87, 2, 14, 12, 13 },
	{ "DELS", "AFTERGONE", 0x138, 0x258, 0x1B, 0x0000, 87, 2, 14, 12, 13 },
	{ "DELS", "AFTERDEAD", 0x136, 0x258, 0x1B, 0x0000, 87, 2, 14, 12, 13 },
};

static const struct
{
	const char *pathlist;
	int seed;
	u_int size;
	int segments;
} real_os9_patterns[] = {
	{ "EMPTY", 1, 0, 0 },
	{ "ONEBYTE", 2, 1, 1 },
	{ "S255", 3, 255, 1 },
	{ "S256", 4, 256, 1 },
	{ "S257", 5, 257, 1 },
	{ "LARGE", 6, 15000, 1 },
	{ "ATTRALL", 7, 300, 1 },
	{ "ATTREXEC", 9, 300, 1 },
	{ "READONLY", 10, 300, 1 },
	{ "ZIGA", 11, 10240, 5 },
	{ "ZIGB", 12, 10240, 5 },
	{ "FULLSEGS", 13, 12032, 47 },
	{ "REFILL", 15, 5120, 20 },
	{ "A", 20, 20, 1 },
	{ "ABCDEFGHIJKLMNOPQRSTUVWXYZ123", 21, 21, 1 },
	{ "MixedCase", 23, 23, 1 },
	{ "file.with.dots", 24, 24, 1 },
	{ "under_score", 25, 25, 1 },
	{ "OWNED", 37, 100, 1 },
	{ "DEEP/FILE", 50, 512, 1 },
	{ "DEEP/L2/FILE", 51, 512, 1 },
	{ "DEEP/L2/L3/FILE", 52, 512, 1 },
	{ "DEEP/L2/L3/L4/FILE", 53, 512, 1 },
	{ "DEEP/L2/L3/L4/L5/FILE", 54, 512, 1 },
	{ "DELS/REUSED", 32, 600, 1 },
	{ "DELS/AFTERGONE", 31, 600, 1 },
	{ "DELS/AFTERDEAD", 34, 600, 1 },
};

static u_char real_os9_byte(int seed, u_int offset)
{
	return (u_char) (seed + ((offset & 255) ^ (offset >> 8)));
}

static void real_os9_pathlist(char *pathlist, size_t size, const char *image, const char *directory, const char *name)
{
	int length = snprintf(pathlist, size, "%s,%s%s%s", image, directory, directory[0] != '\0' ? "/" : "", name);

	ASSERT("the pathlist fits", length >= 0 && (size_t) length < size);
}

/* Read a whole file, as an empty one too. */
static error_code real_os9_read(const char *image, const char *name, u_char *buffer, u_int capacity, u_int *size)
{
	char pathlist[128];
	os9_path_id p;
	error_code ec;

	real_os9_pathlist(pathlist, sizeof(pathlist), image, "", name);
	ec = _os9_open(&p, pathlist, FAM_READ);
	if (ec != 0)
	{
		return ec;
	}
	*size = capacity;
	ec = _os9_read(p, buffer, size);
	if (ec == EOS_EOF)
	{
		*size = 0;
		ec = 0;
	}
	_os9_close(p);

	return ec;
}

/* The names in a directory, in slot order, with "" for a deleted entry. */
static error_code real_os9_slots(const char *image, const char *directory, char names[][32], int capacity, int *count)
{
	char pathlist[128];
	os9_path_id p;
	error_code ec = 0;

	real_os9_pathlist(pathlist, sizeof(pathlist), image, directory, "");
	if (directory[0] != '\0')
	{
		pathlist[strlen(pathlist) - 1] = '\0';
	}
	else
	{
		strcat(pathlist, ".");
	}
	ec = _os9_open(&p, pathlist, FAM_DIR | FAM_READ);
	if (ec != 0)
	{
		return ec;
	}
	*count = 0;
	while (_os9_gs_eof(p) == 0 && *count < capacity)
	{
		os9_dir_entry entry;

		ec = _os9_readdir(p, &entry);
		if (ec != 0)
		{
			break;
		}
		memset(names[*count], 0, 32);
		memcpy(names[*count], entry.name, 29);
		OS9StringToCString((u_char *) names[*count]);
		(*count)++;
	}
	_os9_close(p);

	return ec;
}

static void check_real_free(const char *image, u_int expected_spc, u_int expected_free, u_int expected_largest)
{
	char dname[64];
	u_int month, day, year, bps, total, bytes_free, free_sectors, largest, spc, largest_count, sector_count;

	ASSERT_EC(0, TSRBFFree((char *) image, dname, &month, &day, &year, &bps, &total, &bytes_free, &free_sectors,
			       &largest, &spc, &largest_count, &sector_count));
	ASSERT_EQUALS(630, total);
	ASSERT_EQUALS(expected_spc, spc);
	ASSERT_EQUALS(expected_free, free_sectors);
	if (expected_largest != 0)
	{
		ASSERT_EQUALS(expected_largest, largest);
	}
	ASSERT_EQUALS(expected_free * 256, bytes_free);
}

static void check_real_os9_free(const char *image, u_int expected_free, u_int expected_largest)
{
	check_real_free(image, 1, expected_free, expected_largest);
}

/* Every entry OS-9 listed, in its order, with no others. */
static void check_real_entries(const char *image, const char *const *directories, size_t directory_count,
			       const real_os9_entry *entries, size_t entry_count)
{
	static char names[80][32];
	size_t d, e = 0;

	for (d = 0; d < directory_count; d++)
	{
		int count = 0, slot;

		ASSERT_EC(0, real_os9_slots(image, directories[d], names, 80, &count));
		for (slot = 0; slot < count; slot++)
		{
			char pathlist[128];
			fd_stats fd;

			if (names[slot][0] == '\0' || strcmp(names[slot], ".") == 0 || strcmp(names[slot], "..") == 0)
			{
				continue;
			}
			ASSERT("no more entries than dir e showed", e < entry_count);
			ASSERT_STRING_EQUALS(entries[e].directory, directories[d]);
			ASSERT_STRING_EQUALS(entries[e].name, names[slot]);
			real_os9_pathlist(pathlist, sizeof(pathlist), image, directories[d], names[slot]);
			ASSERT_EC(0, _os9_gs_fd_pathlist(pathlist, sizeof(fd), &fd));
			ASSERT_EQUALS(entries[e].size, int4(fd.fd_siz));
			ASSERT_EQUALS(entries[e].attributes, fd.fd_att);
			ASSERT_EQUALS(entries[e].owner, int2(fd.fd_own));
			ASSERT_EQUALS(entries[e].year, fd.fd_dat[0]);
			ASSERT_EQUALS(entries[e].month, fd.fd_dat[1]);
			ASSERT_EQUALS(entries[e].day, fd.fd_dat[2]);
			ASSERT_EQUALS(entries[e].hour, fd.fd_dat[3]);
			ASSERT_EQUALS(entries[e].minute, fd.fd_dat[4]);
			e++;
		}
	}
	ASSERT_EQUALS(entry_count, e);
}

static void check_real_os9_entries(const char *image)
{
	static const char *const directories[] = {
		"", "DEEP", "DEEP/L2", "DEEP/L2/L3", "DEEP/L2/L3/L4", "DEEP/L2/L3/L4/L5", "DELS"
	};

	check_real_entries(image, directories, sizeof(directories) / sizeof(directories[0]),
			   real_os9_entries, sizeof(real_os9_entries) / sizeof(real_os9_entries[0]));
}

static void check_real_os9_contents(const char *image)
{
	static u_char data[20000];
	size_t f;
	u_int size, i;

	for (f = 0; f < sizeof(real_os9_patterns) / sizeof(real_os9_patterns[0]); f++)
	{
		char pathlist[128];
		fd_stats fd;
		int segments = 0;

		ASSERT_EC(0, real_os9_read(image, real_os9_patterns[f].pathlist, data, sizeof(data), &size));
		ASSERT_EQUALS(real_os9_patterns[f].size, size);
		for (i = 0; i < size; i++)
		{
			ASSERT_EQUALS(real_os9_byte(real_os9_patterns[f].seed, i), data[i]);
		}
		real_os9_pathlist(pathlist, sizeof(pathlist), image, "", real_os9_patterns[f].pathlist);
		ASSERT_EC(0, _os9_gs_fd_pathlist(pathlist, sizeof(fd), &fd));
		while (segments < NUM_SEGS && int3(fd.fd_seg[segments].lsn) != 0)
		{
			segments++;
		}
		ASSERT_EQUALS(real_os9_patterns[f].segments, segments);
	}

	/* RBF allocated every sector up to the seek, and left the gap alone. */
	ASSERT_EC(0, real_os9_read(image, "SPARSE", data, sizeof(data), &size));
	ASSERT_EQUALS(3100, size);
	for (i = 3000; i < size; i++)
	{
		ASSERT_EQUALS(real_os9_byte(17, i), data[i]);
	}

	/* Cut to 1000 bytes while open, and RBF then flushed the sector it was
	   holding for offset 3840 over the last one kept.  This is what it did. */
	ASSERT_EC(0, real_os9_read(image, "SHRUNK", data, sizeof(data), &size));
	ASSERT_EQUALS(1000, size);
	for (i = 0; i < 768; i++)
	{
		ASSERT_EQUALS(real_os9_byte(16, i), data[i]);
	}
	for (i = 0; i < 160; i++)
	{
		ASSERT_EQUALS(real_os9_byte(16, 3840 + i), data[768 + i]);
	}
}

/* Copy every sector into a new image of another format. */
static void copy_real_disk(const char *source, diskimage_format format, const char *pathname)
{
	diskimage_id from, to;
	u_char sector[256];
	int cylinder, s;

	remove(pathname);
	ASSERT_EC(0, _diskimage_create(pathname, format, 1, 35, 256, 18, 0, 0, 0xFF));
	ASSERT_EC(0, _diskimage_open(&from, source, FAM_READ));
	ASSERT_EC(0, _diskimage_open(&to, pathname, FAM_READ | FAM_WRITE));
	for (cylinder = 0; cylinder < 35; cylinder++)
	{
		for (s = 1; s <= 18; s++)
		{
			u_int size = sizeof(sector);

			ASSERT_EC(0, _diskimage_read_sector(from, cylinder, 0, s, sector, &size));
			ASSERT_EQUALS(256, size);
			ASSERT_EC(0, _diskimage_write_sector(to, cylinder, 0, s, sector, size));
		}
	}
	ASSERT_EC(0, _diskimage_close(from));
	ASSERT_EC(0, _diskimage_close(to));
}

void test_real_os9_is_recognized()
{
	u_int filesystems = 0;
	_path_type type;
	os9_path_id p;

	ASSERT_EC(0, _coco_detect_filesystems(REAL_OS9, &filesystems));
	ASSERT_EQUALS(COCO_FS_OS9, filesystems);
	ASSERT_EC(0, _coco_identify_image(REAL_OS9 ",", &type));
	ASSERT_EQUALS(OS9, type);

	ASSERT_EC(0, _os9_open(&p, REAL_OS9 ",@", FAM_READ));
	ASSERT_EQUALS(630, int3(p->lsn0->dd_tot));
	ASSERT_EQUALS(18, int1(p->lsn0->dd_tks));
	ASSERT_EQUALS(79, int2(p->lsn0->dd_map));
	ASSERT_EQUALS(1, int2(p->lsn0->dd_bit));
	ASSERT_EQUALS(2, int3(p->lsn0->dd_dir));
	ASSERT_EQUALS(87, p->lsn0->dd_dat[0]);
	ASSERT_EQUALS(2, p->lsn0->dd_dat[1]);
	ASSERT_EQUALS(14, p->lsn0->dd_dat[2]);
	ASSERT_MEM_EQUALS("FILE\xD3", p->lsn0->dd_nam, 5);
	ASSERT_EC(0, _os9_close(p));
}

void test_real_os9_directories_match_the_machine()
{
	static char names[80][32];
	char name[8];
	int count = 0, i;

	check_real_os9_entries(REAL_OS9);

	/* A slot freed and reused, then one freed and left. */
	ASSERT_EC(0, real_os9_slots(REAL_OS9, "DELS", names, 80, &count));
	ASSERT_EQUALS(6, count);
	ASSERT_STRING_EQUALS("..", names[0]);
	ASSERT_STRING_EQUALS(".", names[1]);
	ASSERT_STRING_EQUALS("REUSED", names[2]);
	ASSERT_STRING_EQUALS("AFTERGONE", names[3]);
	ASSERT_STRING_EQUALS("", names[4]);
	ASSERT_STRING_EQUALS("AFTERDEAD", names[5]);

	/* Only deleted entries, and none. */
	ASSERT_EC(0, real_os9_slots(REAL_OS9, "ALLGONE", names, 80, &count));
	ASSERT_EQUALS(4, count);
	ASSERT_STRING_EQUALS("", names[2]);
	ASSERT_STRING_EQUALS("", names[3]);
	ASSERT_EC(0, real_os9_slots(REAL_OS9, "EMPTYDIR", names, 80, &count));
	ASSERT_EQUALS(2, count);

	/* More entries than the first segment of a directory holds. */
	ASSERT_EC(0, real_os9_slots(REAL_OS9, "BIGDIR", names, 80, &count));
	ASSERT_EQUALS(72, count);
	for (i = 0; i < 70; i++)
	{
		snprintf(name, sizeof(name), "F%02d", i);
		ASSERT_STRING_EQUALS(name, names[i + 2]);
	}
}

void test_real_os9_free_matches_the_machine()
{
	check_real_os9_free(REAL_OS9, REAL_OS9_FREE, REAL_OS9_LARGEST);
}

void test_real_os9_files_match_their_patterns()
{
	check_real_os9_contents(REAL_OS9);
}

/* Reads in small pieces and seeks go across segments. */
void test_real_os9_reads_across_segments()
{
	static u_char data[13000];
	os9_path_id p;
	u_int total = 0, size, i;
	fd_stats fd;
	int s;
	error_code ec;

	ASSERT_EC(0, _os9_open(&p, REAL_OS9 ",FULLSEGS", FAM_READ));
	for (;;)
	{
		size = 37;
		ec = _os9_read(p, data + total, &size);
		if (ec != 0)
		{
			break;
		}
		total += size;
	}
	ASSERT_EQUALS(EOS_EOF, ec);
	ASSERT_EQUALS(12032, total);
	for (i = 0; i < total; i++)
	{
		ASSERT_EQUALS(real_os9_byte(13, i), data[i]);
	}

	/* Each segment is a sector, and two bytes straddle each pair of them. */
	ASSERT_EC(0, _os9_gs_fd(p, sizeof(fd), &fd));
	for (s = 1; s < 47; s++)
	{
		ASSERT_EQUALS(1, int2(fd.fd_seg[s].num));
		ASSERT_EC(0, _os9_seek(p, s * 256 - 1, SEEK_SET));
		size = 2;
		ASSERT_EC(0, _os9_read(p, data, &size));
		ASSERT_EQUALS(2, size);
		ASSERT_EQUALS(real_os9_byte(13, s * 256 - 1), data[0]);
		ASSERT_EQUALS(real_os9_byte(13, s * 256), data[1]);
	}
	ASSERT_EC(0, _os9_close(p));
	ASSERT_EQUALS(0, int3(fd.fd_seg[47].lsn));
}

void test_real_os9_reads_lines()
{
	static const u_int lengths[] = { 0, 1, 2, 80, 254, 255, 256, 257, 300 };
	char line[400];
	os9_path_id p;
	u_int size;
	size_t l;

	ASSERT_EC(0, _os9_open(&p, REAL_OS9 ",TEXT", FAM_READ));
	for (l = 0; l < sizeof(lengths) / sizeof(lengths[0]); l++)
	{
		u_int i;

		size = sizeof(line);
		ASSERT_EC(0, _os9_readln(p, line, &size));
		ASSERT_EQUALS(lengths[l] + 1, size);
		for (i = 0; i < lengths[l]; i++)
		{
			ASSERT_EQUALS('A' + (int) l, line[i]);
		}
		ASSERT_EQUALS('\r', line[lengths[l]]);
	}

	/* The last line has no CR. */
	size = sizeof(line);
	ASSERT_EC(0, _os9_readln(p, line, &size));
	ASSERT_EQUALS(3, size);
	ASSERT_MEM_EQUALS("END", line, 3);
	size = sizeof(line);
	ASSERT_EC(EOS_EOF, _os9_readln(p, line, &size));
	ASSERT_EC(0, _os9_close(p));
}

/* OS-9's name search ignores case: "list /d1/mixedcase" found it. */
void test_real_os9_names_ignore_case()
{
	os9_path_id p;

	ASSERT_EC(0, _os9_open(&p, REAL_OS9 ",mixedcase", FAM_READ));
	ASSERT_EC(0, _os9_close(p));
	ASSERT_EC(0, _os9_open(&p, REAL_OS9 ",MIXEDCASE", FAM_READ));
	ASSERT_EC(0, _os9_close(p));
	ASSERT_EC(0, _os9_open(&p, REAL_OS9 ",deep/l2/FILE", FAM_READ));
	ASSERT_EC(0, _os9_close(p));
}

void test_real_os9_permissions()
{
	char pathlist[] = REAL_OS9 ",ATTRNONE";
	os9_path_id p;
	fd_stats fd;

	/* The file descriptor of a file nobody can read can still be read. */
	ASSERT_EC(EOS_FNA, _os9_open(&p, REAL_OS9 ",ATTRNONE", FAM_READ));
	ASSERT_EC(0, _os9_gs_fd_pathlist(pathlist, sizeof(fd), &fd));
	ASSERT_EQUALS(0, fd.fd_att);
	ASSERT_EQUALS(300, int4(fd.fd_siz));

	ASSERT_EC(EOS_FNA, _os9_open(&p, REAL_OS9 ",READONLY", FAM_WRITE));
	ASSERT_EC(0, _os9_open(&p, REAL_OS9 ",READONLY", FAM_READ));
	ASSERT_EC(0, _os9_close(p));
}

/* Change a copy, and what was not changed stays as it was. */
void test_real_os9_delete()
{
	static char names[80][32];
	static u_char data[20000];
	os9_path_id p;
	u_int size, i;
	int count = 0;

	copy_file(REAL_OS9, REAL_OS9_COPY);

	/* OS-9 reported these two numbers after del /d1/ONEBYTE. */
	ASSERT_EC(0, _os9_delete(REAL_OS9_COPY ",ONEBYTE"));
	check_real_os9_free(REAL_OS9_COPY, 140, 114);

	/* Its 47 segments and its file descriptor go back. */
	ASSERT_EC(0, _os9_delete(REAL_OS9_COPY ",FULLSEGS"));
	check_real_os9_free(REAL_OS9_COPY, 188, 0);
	ASSERT_EC(EOS_PNNF, _os9_open(&p, REAL_OS9_COPY ",FULLSEGS", FAM_READ));
	ASSERT_EC(0, real_os9_read(REAL_OS9_COPY, "REFILL", data, sizeof(data), &size));
	ASSERT_EQUALS(5120, size);
	for (i = 0; i < size; i++)
	{
		ASSERT_EQUALS(real_os9_byte(15, i), data[i]);
	}

	/* A new file takes the first slot that is free. */
	ASSERT_EC(0, _os9_create(&p, REAL_OS9_COPY ",DELS/NEW", FAM_WRITE, FAP_READ | FAP_WRITE));
	ASSERT_EC(0, _os9_close(p));
	ASSERT_EC(0, real_os9_slots(REAL_OS9_COPY, "DELS", names, 80, &count));
	ASSERT_EQUALS(6, count);
	ASSERT_STRING_EQUALS("NEW", names[4]);
	ASSERT_STRING_EQUALS("AFTERDEAD", names[5]);
	remove(REAL_OS9_COPY);
}

void test_real_os9_rename_and_makdir()
{
	static char names[80][32];
	static u_char data[1000];
	u_int size;
	int count = 0;

	copy_file(REAL_OS9, REAL_OS9_COPY);
	ASSERT_EC(0, _os9_rename(REAL_OS9_COPY ",S256", "RENAMED"));
	ASSERT_EC(0, real_os9_read(REAL_OS9_COPY, "RENAMED", data, sizeof(data), &size));
	ASSERT_EQUALS(256, size);
	ASSERT_EQUALS(real_os9_byte(4, 255), data[255]);

	ASSERT_EC(0, _os9_makdir(REAL_OS9_COPY ",DEEP/L2/NEWDIR"));
	ASSERT_EC(0, real_os9_slots(REAL_OS9_COPY, "DEEP/L2/NEWDIR", names, 80, &count));
	ASSERT_EQUALS(2, count);
	remove(REAL_OS9_COPY);
}

/* A file with room to grow keeps what it had, and takes sectors from the holes. */
void test_real_os9_extend_and_truncate()
{
	static u_char more[1000], data[20000];
	os9_path_id p;
	u_int size, i;

	memset(more, 0x5A, sizeof(more));
	copy_file(REAL_OS9, REAL_OS9_COPY);
	ASSERT_EC(0, _os9_open(&p, REAL_OS9_COPY ",ZIGA", FAM_READ | FAM_WRITE));
	ASSERT_EC(0, _os9_seek(p, 10240, SEEK_SET));
	size = sizeof(more);
	ASSERT_EC(0, _os9_write(p, more, &size));
	ASSERT_EC(0, _os9_close(p));
	ASSERT_EC(0, real_os9_read(REAL_OS9_COPY, "ZIGA", data, sizeof(data), &size));
	ASSERT_EQUALS(11240, size);
	for (i = 0; i < 10240; i++)
	{
		ASSERT_EQUALS(real_os9_byte(11, i), data[i]);
	}
	ASSERT_MEM_EQUALS(more, data + 10240, sizeof(more));
	ASSERT_EC(0, real_os9_read(REAL_OS9_COPY, "ZIGB", data, sizeof(data), &size));
	for (i = 0; i < size; i++)
	{
		ASSERT_EQUALS(real_os9_byte(12, i), data[i]);
	}

	/* Cutting a fragmented file frees its last sectors. */
	ASSERT_EC(0, _os9_open(&p, REAL_OS9_COPY ",REFILL", FAM_READ | FAM_WRITE));
	ASSERT_EC(0, _os9_ss_size(p, 1000));
	ASSERT_EC(0, _os9_close(p));
	ASSERT_EC(0, real_os9_read(REAL_OS9_COPY, "REFILL", data, sizeof(data), &size));
	ASSERT_EQUALS(1000, size);
	for (i = 0; i < size; i++)
	{
		ASSERT_EQUALS(real_os9_byte(15, i), data[i]);
	}
	remove(REAL_OS9_COPY);
}

/* RBF stopped this file at 47 segments with E$SLF, so nothing may add a 48th. */
void test_real_os9_segment_list_is_full()
{
	static u_char sector[256], data[13000];
	u_int size, i;
	os9_path_id p;
	fd_stats fd;
	char pathlist[] = REAL_OS9_COPY ",FULLSEGS";

	copy_file(REAL_OS9, REAL_OS9_COPY);
	ASSERT_EC(0, _os9_open(&p, REAL_OS9_COPY ",FULLSEGS", FAM_READ | FAM_WRITE));
	ASSERT_EC(0, _os9_seek(p, 12032, SEEK_SET));
	size = sizeof(sector);
	ASSERT_EC(EOS_SF, _os9_write(p, sector, &size));
	ASSERT_EC(0, _os9_close(p));

	ASSERT_EC(0, _os9_gs_fd_pathlist(pathlist, sizeof(fd), &fd));
	ASSERT_EQUALS(0, int3(fd.fd_seg[47].lsn));
	ASSERT_EC(0, real_os9_read(REAL_OS9_COPY, "FULLSEGS", data, sizeof(data), &size));
	ASSERT_EQUALS(12032, size);
	for (i = 0; i < size; i++)
	{
		ASSERT_EQUALS(real_os9_byte(13, i), data[i]);
	}
	remove(REAL_OS9_COPY);
}

/* The same disk in the other formats reads the same. */
void test_real_os9_all_formats()
{
	static const struct
	{
		diskimage_format format;
		const char *pathname;
	} formats[] = {
		{ DISKIMAGE_RAW, "test_real_os9.dsk" },
		{ DISKIMAGE_DMK, "test_real_os9_copy.dmk" },
		{ DISKIMAGE_SDF, "test_real_os9.sdf" },
		{ DISKIMAGE_VDK, "test_real_os9.vdk" },
	};
	size_t f;

	for (f = 0; f < sizeof(formats) / sizeof(formats[0]); f++)
	{
		copy_real_disk(REAL_OS9, formats[f].format, formats[f].pathname);
		check_real_os9_entries(formats[f].pathname);
		check_real_os9_contents(formats[f].pathname);
		check_real_os9_free(formats[f].pathname, REAL_OS9_FREE, REAL_OS9_LARGEST);
		remove(formats[f].pathname);
	}
}

/*
 * "os9 clusters mame.dmk" has 2-sector clusters, and was made the same way by the
 * same OS-9, booted from a copy of OS9L2V2M.DSK.  That disk's format, edition 22,
 * ignores /2/ on its command line, so the copy had the byte at offset $17E of
 * CMDS/format changed from 1 to 2, its default cluster size, and its CRC redone.
 * Nothing else was changed, and RBF wrote all the files.  They were written by a
 * 6809 module written for the purpose, and hold the same pattern.
 *
 * ZIGA and ZIGB were written in turn, 2048 bytes at a time.  ODDA and ODDB were
 * written in turn, 300 bytes at a time, and COMB and TEETH 512 bytes at a time,
 * all with PD.SAS set to 1.  TEETH was then deleted, and REFILL took 4 of the
 * holes.  UNCUT is as written, and CUT* were written alike, closed, opened again
 * and cut with SS.Size.
 *
 * RBF starts a file's first segment in the second sector of the cluster that
 * holds its file descriptor, and ends every segment at the end of a cluster.  So
 * a first segment is an odd number of sectors, and even an empty file has one.
 *
 * What this expects is what its dir e and free said, and dcheck said the file
 * structure was intact.  What it expects of deleting files is what free said on
 * a copy after del of each of EMPTY, UNCUT, COMB, ODDA and CUT1100, in that order.
 */
#define REAL_CLUSTERS "../../../unittest/testdata/os9 clusters mame.dmk"
#define REAL_CLUSTERS_COPY "test_real_clusters.dmk"
#define REAL_CLUSTERS_FREE 226
#define REAL_CLUSTERS_LARGEST 214

static const real_os9_entry real_clusters_entries[] = {
	{ "", "EMPTY", 0xA, 0x0, 0x1B, 0x0000, 87, 2, 14, 12, 11 },
	{ "", "ONEBYTE", 0xC, 0x1, 0x1B, 0x0000, 87, 2, 14, 12, 11 },
	{ "", "S256", 0xE, 0x100, 0x1B, 0x0000, 87, 2, 14, 12, 11 },
	{ "", "S257", 0x10, 0x101, 0x1B, 0x0000, 87, 2, 14, 12, 11 },
	{ "", "S512", 0x14, 0x200, 0x1B, 0x0000, 87, 2, 14, 12, 11 },
	{ "", "S513", 0x18, 0x201, 0x1B, 0x0000, 87, 2, 14, 12, 11 },
	{ "", "LARGE", 0x1C, 0x1770, 0x1B, 0x0000, 87, 2, 14, 12, 11 },
	{ "", "UNCUT", 0x36, 0xBB8, 0x1B, 0x0000, 87, 2, 14, 12, 11 },
	{ "", "CUT1100", 0x44, 0x44C, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "CUT1024", 0x52, 0x400, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "CUT1", 0x60, 0x1, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "CUT0", 0x6E, 0x0, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "ZIGA", 0x4A, 0x1800, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "ZIGB", 0x4C, 0x1800, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "ODDA", 0x4E, 0x4B0, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "ODDB", 0x50, 0x4B0, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "SUB", 0x94, 0x80, 0xBF, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "DELS", 0x15C, 0xC0, 0xBF, 0x0000, 87, 2, 14, 12, 12 },
	{ "", "COMB", 0x160, 0x1800, 0x1B, 0x0000, 87, 2, 14, 12, 13 },
	{ "", "REFILL", 0x168, 0x800, 0x1B, 0x0000, 87, 2, 14, 12, 14 },
	{ "", "DIRCMD", 0x180, 0x36B, 0x2F, 0x0000, 87, 2, 13, 11, 11 },
	{ "SUB", "FILE", 0x96, 0x2BC, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "SUB", "DEEP", 0x9A, 0x60, 0xBF, 0x0000, 87, 2, 14, 12, 12 },
	{ "SUB/DEEP", "FILE", 0x158, 0x2BC, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "DELS", "REUSED", 0x15E, 0x258, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "DELS", "AFTERGONE", 0x162, 0x258, 0x1B, 0x0000, 87, 2, 14, 12, 12 },
	{ "DELS", "AFTERDEAD", 0x16A, 0x258, 0x1B, 0x0000, 87, 2, 14, 12, 13 },
};

static const struct
{
	const char *pathlist;
	int seed;
	u_int size;
	int segments;
	int sectors[16];
} real_clusters_files[] = {
	{ "EMPTY", 1, 0, 1, { 1 } },
	{ "ONEBYTE", 2, 1, 1, { 1 } },
	{ "S256", 3, 256, 1, { 1 } },
	{ "S257", 4, 257, 1, { 3 } },
	{ "S512", 5, 512, 1, { 3 } },
	{ "S513", 6, 513, 1, { 3 } },
	{ "LARGE", 7, 6000, 1, { 25 } },
	{ "UNCUT", 16, 3000, 1, { 13 } },
	{ "CUT1100", 16, 1100, 1, { 5 } },
	{ "CUT1024", 16, 1024, 1, { 5 } },
	{ "CUT1", 16, 1, 1, { 1 } },
	{ "CUT0", 16, 0, 1, { 1 } },
	{ "ZIGA", 11, 6144, 4, { 1, 8, 8, 8 } },
	{ "ZIGB", 12, 6144, 4, { 1, 8, 8, 8 } },
	{ "ODDA", 13, 1200, 3, { 1, 2, 2 } },
	{ "ODDB", 14, 1200, 3, { 1, 2, 2 } },
	{ "COMB", 15, 6144, 13, { 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 } },
	{ "REFILL", 18, 2048, 5, { 1, 2, 2, 2, 2 } },
	{ "SUB/FILE", 20, 700, 1, { 3 } },
	{ "SUB/DEEP/FILE", 21, 700, 1, { 3 } },
	{ "DELS/REUSED", 32, 600, 2, { 1, 2 } },
	{ "DELS/AFTERGONE", 31, 600, 1, { 3 } },
	{ "DELS/AFTERDEAD", 34, 600, 1, { 3 } },
};

void test_real_clusters_is_recognized()
{
	u_int filesystems = 0;
	os9_path_id p;

	ASSERT_EC(0, _coco_detect_filesystems(REAL_CLUSTERS, &filesystems));
	ASSERT_EQUALS(COCO_FS_OS9, filesystems);

	ASSERT_EC(0, _os9_open(&p, REAL_CLUSTERS ",@", FAM_READ));
	ASSERT_EQUALS(630, int3(p->lsn0->dd_tot));
	ASSERT_EQUALS(40, int2(p->lsn0->dd_map));
	ASSERT_EQUALS(2, int2(p->lsn0->dd_bit));
	ASSERT_EQUALS(2, int3(p->lsn0->dd_dir));
	ASSERT_EQUALS(2, p->spc);
	ASSERT_EQUALS(512, p->cs);
	ASSERT_MEM_EQUALS("CLUSTER\xB2", p->lsn0->dd_nam, 8);
	ASSERT_EC(0, _os9_close(p));
}

static void check_real_clusters_entries(const char *image)
{
	static const char *const directories[] = { "", "SUB", "SUB/DEEP", "DELS" };

	check_real_entries(image, directories, sizeof(directories) / sizeof(directories[0]),
			   real_clusters_entries, sizeof(real_clusters_entries) / sizeof(real_clusters_entries[0]));
}

void test_real_clusters_directories_match_the_machine()
{
	static char names[80][32];
	int count = 0;

	check_real_clusters_entries(REAL_CLUSTERS);

	ASSERT_EC(0, real_os9_slots(REAL_CLUSTERS, "DELS", names, 80, &count));
	ASSERT_EQUALS(6, count);
	ASSERT_STRING_EQUALS("REUSED", names[2]);
	ASSERT_STRING_EQUALS("AFTERGONE", names[3]);
	ASSERT_STRING_EQUALS("", names[4]);
	ASSERT_STRING_EQUALS("AFTERDEAD", names[5]);
}

void test_real_clusters_free_matches_the_machine()
{
	check_real_free(REAL_CLUSTERS, 2, REAL_CLUSTERS_FREE, REAL_CLUSTERS_LARGEST);
}

static void check_real_clusters_files(const char *image)
{
	static u_char data[20000];
	size_t f;

	for (f = 0; f < sizeof(real_clusters_files) / sizeof(real_clusters_files[0]); f++)
	{
		char pathlist[128];
		fd_stats fd;
		u_int size, i;
		int s;

		ASSERT_EC(0, real_os9_read(image, real_clusters_files[f].pathlist, data, sizeof(data), &size));
		ASSERT_EQUALS(real_clusters_files[f].size, size);
		for (i = 0; i < size; i++)
		{
			ASSERT_EQUALS(real_os9_byte(real_clusters_files[f].seed, i), data[i]);
		}

		real_os9_pathlist(pathlist, sizeof(pathlist), image, "", real_clusters_files[f].pathlist);
		ASSERT_EC(0, _os9_gs_fd_pathlist(pathlist, sizeof(fd), &fd));
		for (s = 0; s < real_clusters_files[f].segments; s++)
		{
			ASSERT_NEQUALS(0, int3(fd.fd_seg[s].lsn));
			ASSERT_EQUALS(real_clusters_files[f].sectors[s], int2(fd.fd_seg[s].num));
			/* Every segment ends at the end of a cluster. */
			ASSERT_EQUALS(0, (int3(fd.fd_seg[s].lsn) + int2(fd.fd_seg[s].num)) % 2);
		}
		ASSERT_EQUALS(0, int3(fd.fd_seg[s].lsn));
	}
}

void test_real_clusters_files_match_the_machine()
{
	check_real_clusters_files(REAL_CLUSTERS);
}

/* The two bytes at each end of a segment are where sector arithmetic goes wrong. */
void test_real_clusters_reads_across_segments()
{
	u_char data[2];
	os9_path_id p;
	fd_stats fd;
	u_int position = 0, size;
	int s;

	ASSERT_EC(0, _os9_open(&p, REAL_CLUSTERS ",COMB", FAM_READ));
	ASSERT_EC(0, _os9_gs_fd(p, sizeof(fd), &fd));
	for (s = 0; int3(fd.fd_seg[s + 1].lsn) != 0; s++)
	{
		position += int2(fd.fd_seg[s].num) * 256;
		ASSERT_EC(0, _os9_seek(p, position - 1, SEEK_SET));
		size = sizeof(data);
		ASSERT_EC(0, _os9_read(p, data, &size));
		ASSERT_EQUALS(2, size);
		ASSERT_EQUALS(real_os9_byte(15, position - 1), data[0]);
		ASSERT_EQUALS(real_os9_byte(15, position), data[1]);
	}
	ASSERT_EC(0, _os9_close(p));
}

/* Each step leaves what OS-9's free said after the same del. */
void test_real_clusters_delete_matches_the_machine()
{
	static const struct
	{
		const char *pathlist;
		u_int free;
	} steps[] = {
		{ REAL_CLUSTERS_COPY ",EMPTY", 228 },
		{ REAL_CLUSTERS_COPY ",UNCUT", 242 },
		{ REAL_CLUSTERS_COPY ",COMB", 268 },
		{ REAL_CLUSTERS_COPY ",ODDA", 274 },
		{ REAL_CLUSTERS_COPY ",CUT1100", 280 },
	};
	size_t i;

	copy_file(REAL_CLUSTERS, REAL_CLUSTERS_COPY);
	for (i = 0; i < sizeof(steps) / sizeof(steps[0]); i++)
	{
		ASSERT_EC(0, _os9_delete((char *) steps[i].pathlist));
		check_real_free(REAL_CLUSTERS_COPY, 2, steps[i].free, REAL_CLUSTERS_LARGEST);
	}
	remove(REAL_CLUSTERS_COPY);
}

/* UNCUT cut to each size has the segment and frees the sectors that RBF's did. */
void test_real_clusters_truncate_matches_the_machine()
{
	static const struct
	{
		int size;
		int sectors;
	} cuts[] = {
		{ 1100, 5 },
		{ 1024, 5 },
		{ 1, 1 },
		{ 0, 1 },
	};
	static u_char data[4000];
	size_t c;

	for (c = 0; c < sizeof(cuts) / sizeof(cuts[0]); c++)
	{
		char pathlist[] = REAL_CLUSTERS_COPY ",UNCUT";
		os9_path_id p;
		fd_stats fd;
		u_int size;
		int i;

		copy_file(REAL_CLUSTERS, REAL_CLUSTERS_COPY);
		ASSERT_EC(0, _os9_open(&p, pathlist, FAM_READ | FAM_WRITE));
		ASSERT_EC(0, _os9_ss_size(p, cuts[c].size));
		ASSERT_EC(0, _os9_close(p));

		ASSERT_EC(0, _os9_gs_fd_pathlist(pathlist, sizeof(fd), &fd));
		ASSERT_EQUALS(cuts[c].size, int4(fd.fd_siz));
		ASSERT_EQUALS(cuts[c].sectors, int2(fd.fd_seg[0].num));
		check_real_free(REAL_CLUSTERS_COPY, 2, REAL_CLUSTERS_FREE + 13 - cuts[c].sectors, REAL_CLUSTERS_LARGEST);

		ASSERT_EC(0, real_os9_read(REAL_CLUSTERS_COPY, "UNCUT", data, sizeof(data), &size));
		ASSERT_EQUALS(cuts[c].size, size);
		for (i = 0; i < cuts[c].size; i++)
		{
			ASSERT_EQUALS(real_os9_byte(16, i), data[i]);
		}
	}
	remove(REAL_CLUSTERS_COPY);
}

/* A segment ends at the end of a cluster, whatever sector it started on. */
void test_real_clusters_extend()
{
	static u_char more[2000], data[4000];
	char pathlist[] = REAL_CLUSTERS_COPY ",ONEBYTE";
	os9_path_id p;
	fd_stats fd;
	u_int size, i;
	int s, sectors = 0;

	memset(more, 0x5A, sizeof(more));
	copy_file(REAL_CLUSTERS, REAL_CLUSTERS_COPY);

	ASSERT_EC(0, _os9_open(&p, pathlist, FAM_READ | FAM_WRITE));
	ASSERT_EC(0, _os9_seek(p, 1, SEEK_SET));
	size = sizeof(more);
	ASSERT_EC(0, _os9_write(p, more, &size));
	ASSERT_EQUALS(sizeof(more), size);
	ASSERT_EC(0, _os9_close(p));

	ASSERT_EC(0, real_os9_read(REAL_CLUSTERS_COPY, "ONEBYTE", data, sizeof(data), &size));
	ASSERT_EQUALS(2001, size);
	ASSERT_EQUALS(real_os9_byte(2, 0), data[0]);
	ASSERT_MEM_EQUALS(more, data + 1, sizeof(more));

	ASSERT_EC(0, _os9_gs_fd_pathlist(pathlist, sizeof(fd), &fd));
	for (s = 0; s < NUM_SEGS && int3(fd.fd_seg[s].lsn) != 0; s++)
	{
		ASSERT_EQUALS(0, (int3(fd.fd_seg[s].lsn) + int2(fd.fd_seg[s].num)) % 2);
		sectors += int2(fd.fd_seg[s].num);
	}
	ASSERT("room for what it holds", sectors * 256 >= 2001);

	/* What the file took came from the free clusters, and nothing else moved. */
	check_real_free(REAL_CLUSTERS_COPY, 2, REAL_CLUSTERS_FREE - (sectors - 1), 0);
	ASSERT_EC(0, real_os9_read(REAL_CLUSTERS_COPY, "S256", data, sizeof(data), &size));
	ASSERT_EQUALS(256, size);
	for (i = 0; i < size; i++)
	{
		ASSERT_EQUALS(real_os9_byte(3, i), data[i]);
	}
	remove(REAL_CLUSTERS_COPY);
}

/* The same disk in the other formats reads the same. */
void test_real_clusters_all_formats()
{
	static const struct
	{
		diskimage_format format;
		const char *pathname;
	} formats[] = {
		{ DISKIMAGE_RAW, "test_real_clusters.dsk" },
		{ DISKIMAGE_SDF, "test_real_clusters.sdf" },
		{ DISKIMAGE_VDK, "test_real_clusters.vdk" },
	};
	size_t f;

	for (f = 0; f < sizeof(formats) / sizeof(formats[0]); f++)
	{
		copy_real_disk(REAL_CLUSTERS, formats[f].format, formats[f].pathname);
		check_real_clusters_entries(formats[f].pathname);
		check_real_clusters_files(formats[f].pathname);
		check_real_free(formats[f].pathname, 2, REAL_CLUSTERS_FREE, REAL_CLUSTERS_LARGEST);
		remove(formats[f].pathname);
	}
}

int main()
{
	remove("test.dsk");
	remove("test_alloc.dsk");

	RUN(test_os9_dmk_read_write);
	RUN(test_os9_only_needed_sectors_are_errors);
	RUN(test_os9_bad_crc_is_read);
	RUN(test_os9_write_protect);
	RUN(test_os9_format);
	RUN(test_os9_create);
	RUN(test_os9_read);
	RUN(test_os9_raw_read_end_of_disk);
	RUN(test_os9_raw_write_position);
	RUN(test_os9_readln_across_segments);
	RUN(test_os9_write);
	RUN(test_os9_open_and_read);
	RUN(test_os9_delete);
	RUN(test_os9_delete_write_only_file);
	RUN(test_os9_makdir);
	RUN(test_os9_delete_directory);
	RUN(test_os9_rename);
	RUN(test_os9_ss_calls);
	RUN(test_os9_gs_calls);
	RUN(test_os9_failed_create_frees_its_cluster);
	RUN(test_os9_read_stops_at_segment_list_end);
	RUN(test_os9_no_allocation_past_last_cluster);
	RUN(test_os9_file_allocation);
	RUN(test_os9_allocation_bitmap_boundaries);
	RUN(test_os9_short_t0_segment_io);
	RUN(test_os9_68k_short_t0);
	RUN(test_os9_full_t0_positions_unchanged);
	RUN(test_os9_format_short_t0_layout);
	RUN(test_os9_bitmap_crosses_short_t0);
	RUN(test_os9_format_rejects_bad_t0s);

	RUN(test_os9_damage_all_formats);
	RUN(test_os9_round_trip_all_formats);
	RUN(test_os9_write_protect_all_formats);
	RUN(test_os9_bad_lsn0_on_tracks);

	RUN(test_real_os9_is_recognized);
	RUN(test_real_os9_directories_match_the_machine);
	RUN(test_real_os9_free_matches_the_machine);
	RUN(test_real_os9_files_match_their_patterns);
	RUN(test_real_os9_reads_across_segments);
	RUN(test_real_os9_reads_lines);
	RUN(test_real_os9_names_ignore_case);
	RUN(test_real_os9_permissions);
	RUN(test_real_os9_delete);
	RUN(test_real_os9_rename_and_makdir);
	RUN(test_real_os9_extend_and_truncate);
	RUN(test_real_os9_segment_list_is_full);
	RUN(test_real_os9_all_formats);

	RUN(test_real_clusters_is_recognized);
	RUN(test_real_clusters_directories_match_the_machine);
	RUN(test_real_clusters_free_matches_the_machine);
	RUN(test_real_clusters_files_match_the_machine);
	RUN(test_real_clusters_reads_across_segments);
	RUN(test_real_clusters_delete_matches_the_machine);
	RUN(test_real_clusters_truncate_matches_the_machine);
	RUN(test_real_clusters_extend);
	RUN(test_real_clusters_all_formats);
	return TEST_REPORT();
}
