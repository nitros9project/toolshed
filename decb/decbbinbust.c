/********************************************************************
 * binbust.c - Remove DECB binary ambles.
 ********************************************************************/

#include <util.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <cocopath.h>

static char const *const helpMessage[] = {
    "Syntax: binbust <input> <output>\n",
    "Usage:  Strip DECB binary pre-, inner-, and post- ambles.\n",
    NULL
};

static uint16_t be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

int decbbinbust(int argc, char *argv[])
{
    coco_path_id in = NULL, out = NULL;
    error_code ec = 0;
    uint8_t hdr[5];
    int i;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-?") == 0)
        {
            show_help(helpMessage);
            return 0;
        }
        if (argv[i][0] == '-')
        {
            fprintf(stderr, "%s: unknown option '%s'\n", argv[0], argv[i]);
            return EOS_IA;
        }
    }

    if (argc != 3)
    {
        show_help(helpMessage);
        return 0;
    }

    ec = _coco_open(&in, argv[1], FAM_READ);
    if (ec != 0)
    {
        fprintf(stderr, "%s: error %d opening '%s'\n",
                argv[0], ec, argv[1]);
        return ec;
    }

    coco_file_stat stat = { 0 };

    ec = _coco_create(&out, argv[2], FAM_WRITE, &stat);
    if (ec != 0)
    {
        fprintf(stderr, "%s: error %d creating '%s'\n",
                argv[0], ec, argv[2]);
        _coco_close(in);
        return ec;
    }

    for (;;)
    {
        uint16_t length;
        uint16_t address;
        u_int size = sizeof(hdr);

        ec = _coco_read(in, hdr, &size);
        if (ec != 0 || size != sizeof(hdr))
        {
            fprintf(stderr, "Unexpected EOF\n");
            break;
        }

        length  = be16(&hdr[1]);
        address = be16(&hdr[3]);

        switch (hdr[0])
        {
        case 0x00:
        {
            uint8_t *buffer;

            (void)address;

            if (length == 0)
                break;

            buffer = malloc(length);
            if (buffer == NULL)
            {
                fprintf(stderr, "Out of memory\n");
                ec = EOS_MF;
                goto done;
            }

            size = length;
            ec = _coco_read(in, buffer, &size);
            if (ec != 0 || size != length)
            {
                fprintf(stderr, "Unexpected EOF in data block\n");
                free(buffer);
                goto done;
            }

            size = length;
            ec = _coco_write(out, buffer, &size);
            free(buffer);

            if (ec != 0 || size != length)
            {
                fprintf(stderr, "Write error\n");
                goto done;
            }

            break;
        }

        case 0xff:
            if (length != 0)
            {
                fprintf(stderr, "Malformed end record\n");
            }
            goto done;

        default:
            fprintf(stderr,
                    "Unknown block type $%02X\n",
                    hdr[0]);
            ec = EOS_IC;
            goto done;
        }
    }

done:
    if (in != NULL)
        _coco_close(in);

    if (out != NULL)
        _coco_close(out);

    return ec;
}
