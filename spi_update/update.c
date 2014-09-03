#include "spieeprom.h"
#include "nxp4330_boot.h"
#include "update.h"

#define SECBOOT_OFFSET 0
#define SECBOOT_SIZE 16 * 1024

#define UBOOT_OFFSET 64 * 1024
#define UBOOT_SIZE (512-64) * 1024
#define TEXT_BASE 0x40c00000
#define CONFIG_EEPROM_ADDRESS_STEP     3

#define SECBOOT	1
#define UBOOT	2
#define ERASE_BLOCK_SIZE	32*1024

#define POLY 0x04C11DB7L

#define SPIROM_DEVICE "/dev/spidev0.0"

unsigned int get_fcs(unsigned int fcs, unsigned char data)
{
    register int i;
    fcs ^= (unsigned int)data;

    for(i=0; i<8; i++)
    {
        if(fcs & 0x01) fcs ^= POLY; fcs >>= 1;
    }
    return fcs;
}

// type 1: second boot,
//      2: uboot
// path  : update file path
int update_eeprom( int type, char *path )
{
    int ret,i,opt;
    int size = 0;
    uint8_t *read_buf;
    uint8_t *buf, *wbuf;
    unsigned int *p;
    int CRC = 0;
    int w_size;
    int fd;
    int ffd;

    fd = open(SPIROM_DEVICE, O_RDWR);
    if (fd < 0) {
        printf("%s: can't open %s\n", __func__, SPIROM_DEVICE);
        return -EINVAL;
    }

    ffd = open(path, O_RDWR);
    if (ffd < 0) {
         printf("%s: can't open %s\n", __func__, path);
         return -EINVAL;
    }

    if(type == SECBOOT) {
        buf = (uint8_t *)malloc(SECBOOT_SIZE);
        p = (unsigned int *)buf;
        read_buf = (uint8_t *)malloc(SECBOOT_SIZE);

        ret = read(ffd,buf,SECBOOT_SIZE);
        if(ret < 0) {
            printf("READ fail secondboot %s \n",path);
            goto end;
        }
        for (i = 0; (SECBOOT_SIZE-16) > i; i++ )
            CRC = get_fcs(CRC, buf[i]);

        p[(SECBOOT_SIZE-16)/4] = CRC;

        eeprom_write_enable(fd);
        eeprom_blk_erase(fd, BLOCK_32K_ERASE, 0);	//Block Erase 64KB
        eeprom_write_enable(fd);
        size = SECBOOT_SIZE;

        w_size = 0;
        do {
            eeprom_write(fd, w_size, buf+w_size, 256);	//Write Data
            w_size += 256;
        } while (w_size < size);
        eeprom_read(fd, 0, read_buf, 16384);

    } else if(type == UBOOT) {
        int offset=0;
        struct boot_dev_head head;
        struct boot_dev_head *bh = &head;
        struct boot_dev_eeprom *bd = (struct boot_dev_eeprom *)&bh->bdi;
        int len = sizeof(head);
        int load = TEXT_BASE;
        w_size = 0;

        buf = (uint8_t *)malloc(UBOOT_SIZE);
        ret = read(ffd, buf, UBOOT_SIZE);
        if (ret < 0) {
            printf("READ fail uboot %s \n",path);
            goto end;
        }
        size = ret;
        CRC =0;
        for (i = 0; size > i; i++ )
            CRC = get_fcs(CRC, buf[i]);
        memset(bh, 0xff, len);

        bh->load_addr = load;
        bh->jump_addr = bh->load_addr;
        bh->load_size = (int)size;
        bh->signature = SIGNATURE_ID;
        bd->addr_step = CONFIG_EEPROM_ADDRESS_STEP;
        bd->crc32 = CRC;

        wbuf = (uint8_t *)malloc(size+len);
        memcpy(wbuf,bh,len);
        memcpy(wbuf+len,buf,size);

        offset = UBOOT_OFFSET  + w_size;
        size = size + len;
        do {
            eeprom_write_enable(fd);
            eeprom_blk_erase(fd ,BLOCK_32K_ERASE ,offset);	//Block Erase 32KB

            eeprom_write_enable(fd);
            eeprom_write(fd,offset,wbuf+w_size,ERASE_BLOCK_SIZE);	//Write Data
            w_size += ERASE_BLOCK_SIZE;
            offset += ERASE_BLOCK_SIZE;
            size -= ERASE_BLOCK_SIZE;
            if(size < 0)
                size = 0;
        } while (size >0);

    }

end:
    if(fd)
        close(fd);
    if(ffd)
        close(ffd);
    if(buf)
        free(buf);
    if(wbuf)
        free(wbuf);
    return 0;
}

int update_eeprom_from_mem( int type, char *mem, size_t img_size )
{
    int ret,i,opt;
    int size = 0;
    uint8_t *read_buf = NULL;
    uint8_t *buf = NULL, *wbuf = NULL;
    unsigned int *p = NULL;
    int CRC = 0;
    int w_size;
    int fd;

    printf("%s entered(type %d, mem %p, img_size %d)\n", __func__, type, mem, img_size);

    fd = open(SPIROM_DEVICE, O_RDWR);
    if (fd < 0) {
        printf("%s: can't open %s\n", __func__, SPIROM_DEVICE);
        return -EINVAL;
    }

    buf = mem;
    if(type == SECBOOT) {
        p = (unsigned int *)buf;
        read_buf = (uint8_t *)malloc(SECBOOT_SIZE);
        for (i = 0; (SECBOOT_SIZE-16) > i; i++ )
            CRC = get_fcs(CRC, buf[i]);

        p[(SECBOOT_SIZE-16)/4] = CRC;

        eeprom_write_enable(fd);
        eeprom_blk_erase(fd, BLOCK_32K_ERASE, 0);	//Block Erase 64KB
        eeprom_write_enable(fd);
        size = SECBOOT_SIZE;

        w_size = 0;
        do {
            eeprom_write(fd, w_size, buf+w_size, 256);	//Write Data
            w_size += 256;
        } while (w_size < size);
        eeprom_read(fd, 0, read_buf, 16384);

    } else if(type == UBOOT) {
        int offset = 0;
        struct boot_dev_head head;
        struct boot_dev_head *bh = &head;
        struct boot_dev_eeprom *bd = (struct boot_dev_eeprom *)&bh->bdi;
        int len = sizeof(head);
        int load = TEXT_BASE;
        w_size = 0;

        size = img_size;
        CRC =0;
        for (i = 0; size > i; i++ )
            CRC = get_fcs(CRC, buf[i]);
        memset(bh, 0xff, len);

        bh->load_addr = load;
        bh->jump_addr = bh->load_addr;
        bh->load_size = (int)size;
        bh->signature = SIGNATURE_ID;
        bd->addr_step = CONFIG_EEPROM_ADDRESS_STEP;
        bd->crc32 = CRC;

        wbuf = (uint8_t *)malloc(size+len);
        memcpy(wbuf, bh, len);
        memcpy(wbuf+len, buf, size);

        offset = UBOOT_OFFSET  + w_size;
        size = size + len;
        do {
            eeprom_write_enable(fd);
            eeprom_blk_erase(fd ,BLOCK_32K_ERASE ,offset);	//Block Erase 32KB

            eeprom_write_enable(fd);
            eeprom_write(fd,offset,wbuf+w_size,ERASE_BLOCK_SIZE);	//Write Data
            w_size += ERASE_BLOCK_SIZE;
            offset += ERASE_BLOCK_SIZE;
            size -= ERASE_BLOCK_SIZE;
            if(size < 0)
                size = 0;
        } while (size > 0);
    }

end:
    if(fd)
        close(fd);
    if(wbuf)
        free(wbuf);

    printf("%s exit\n", __func__);
    return 0;
}
