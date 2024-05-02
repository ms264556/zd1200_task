/*
 * Ruckus Wireless (c) 2006
 * Definitions for Board/Processor specific processing
 */
#ifndef H_V54_BSP_
#define H_V54_BSP_



inline void *V54_ACCESS_BUFFER( void *skbaddr, void *buf_data, unsigned int buf_len, void *sc_bdev )
{

	bus_unmap_single( sc_bdev, (unsigned int)skbaddr, buf_len, BUS_DMA_TODEVICE);
	return buf_data;

}

inline void V54_FLUSH_BUFFER( void **skbaddr, void *buf_data, unsigned int buf_len, void *sc_bdev )
{
    *skbaddr = (void *)bus_map_single( sc_bdev, buf_data, buf_len, BUS_DMA_TODEVICE);
}


const char * rks_get_boardtype(void);
unsigned int rks_get_antinfo(void);
unsigned char rks_is_gd6_board(void);

void rks_get_memory_info(unsigned long *totalmem, unsigned long *freemem);

unsigned int rks_get_boardrev(unsigned char *btype, unsigned short *major,
                              unsigned short *minor, unsigned char *model_len, char *model);


extern int rks_get_wlan_macaddr(int radio, int wlanid, char *macaddr);


extern int rks_assign_mac_subpool( int pool_size, u_int8_t **macaddr );
extern int rks_assign_wlan_macaddr( int radio, int wlanid, u_int8_t **macaddr);
extern int rks_release_wlan_macaddr(int radio, u_int8_t *macaddr);

#endif	/* H_V54_BSP_ */
