#ifndef _AR7100_SPI_H
#define _AR7100_SPI_H

/*
 * Defined in ar7100.h
 *
#define AR7100_SPI_FS           0x1f000000
#define AR7100_SPI_CLOCK        0x1f000004
#define AR7100_SPI_WRITE        0x1f000008
#define AR7100_SPI_READ         0x1f000000
#define AR7100_SPI_RD_STATUS    0x1f00000c
#define AR7100_SPI_CS_DIS       0x70000
*/

#define AR7100_SPI_CE_LOW       0x60000
#define AR7100_SPI_CE_HIGH      0x60100

#define AR7100_SPI_CMD_WREN         0x06
#if defined(V54_BSP)
#define AR7100_SPI_CMD_RDID         0x9f
#endif
#define AR7100_SPI_CMD_RD_STATUS    0x05
#define AR7100_SPI_CMD_FAST_READ    0x0b
#define AR7100_SPI_CMD_PAGE_PROG    0x02
#define AR7100_SPI_CMD_SECTOR_ERASE 0xd8

#define AR7100_SPI_CMD_WRITE_SR     0x01
#define AR7100_SPI_CMD_BRRD         0x16
#define AR7100_SPI_CMD_BRWR         0x17
#define AR7100_SPI_CMD_BRAC         0xb9

/* n25q256a13 id 0x20ba19 */
//For Micon Support
#define AR7100_MICRON_SPI_CMD_WREAR	0xc5
#define AR7100_MICRON_SPI_CMD_RDEAR	0xc8
#define AR7100_SPI_CMD_WREAR        0xc5
#define AR7100_SPI_CMD_RDEAR        0xc8
#define MICRON_VENDOR_ID	        0x20ba19

#define AR7100_SPI_SECTOR_SIZE      (1024*64)
#define AR7100_SPI_PAGE_SIZE        256

#define AR7100_FLASH_MAX_BANKS  1


#if !defined(V54_BSP)
#define display(_x)     ar7100_reg_wr_nf(0x18040008, (_x))
#endif

/*
 * primitives
 */

#define ar7100_be_msb(_val, __i) (((_val) & (1 << (7 - __i))) >> (7 - __i))

#define ar7100_spi_bit_banger(_byte)  do {        \
    int _i;                                      \
    for(_i = 0; _i < 8; _i++) {                    \
        ar7100_reg_wr_nf(AR7100_SPI_WRITE,      \
                        AR7100_SPI_CE_LOW | ar7100_be_msb(_byte, _i));  \
        ar7100_reg_wr_nf(AR7100_SPI_WRITE,      \
                        AR7100_SPI_CE_HIGH | ar7100_be_msb(_byte, _i)); \
    }       \
}while(0);

#define ar7100_spi_go() do {        \
    ar7100_reg_wr_nf(AR7100_SPI_WRITE, AR7100_SPI_CE_LOW); \
    ar7100_reg_wr_nf(AR7100_SPI_WRITE, AR7100_SPI_CS_DIS); \
}while(0);


#define ar7100_spi_send_addr(_addr) do {                    \
    ar7100_spi_bit_banger(((addr & 0xff0000) >> 16));                 \
    ar7100_spi_bit_banger(((addr & 0x00ff00) >> 8));                 \
    ar7100_spi_bit_banger(addr & 0x0000ff);                 \
}while(0);

#define ar7100_spi_delay_8()    ar7100_spi_bit_banger(0)
#define ar7100_spi_done()       ar7100_reg_wr(AR7100_SPI_FS, 0)

#endif /*_AR7100_SPI_H*/
