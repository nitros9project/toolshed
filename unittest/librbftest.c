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
				_os9_lsn_fseek(p, p->pl_fd_lsn);
				size = fwrite(&fd, 1, sizeof(fd), p->fd);
				ASSERT_EQUALS(sizeof(fd), size);

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
	RUN(test_os9_allocation_bitmap_boundaries);
	RUN(test_os9_short_t0_segment_io);
	RUN(test_os9_68k_short_t0);
	RUN(test_os9_full_t0_positions_unchanged);
	RUN(test_os9_format_short_t0_layout);
	RUN(test_os9_bitmap_crosses_short_t0);
	RUN(test_os9_format_rejects_bad_t0s);

	return TEST_REPORT();
}
