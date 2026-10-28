/*
 * ALWAYS BE TESTING!!!
 *
 * ALWAYS BE WRITING MORE TESTS!!!
 *
 * THIS WORK IS **NEVER** DONE!!!
 */

#include "tinytest.h"
#include <toolshed.h>

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

int main()
{
	remove("test.dsk");
	remove("test_alloc.dsk");

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
	RUN(test_os9_file_allocation);
	
	return TEST_REPORT();
}
