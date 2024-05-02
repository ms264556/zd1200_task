#ifndef __AR531X_BSP_H__
#define __AR531X_BSP_H__

#if defined(V54_BSP)
#ifndef V54
#define V54
#endif


#if 1
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif

#endif



#define DATA_PART_SIZE  0x1000

#define RKS_BD_OFFSET   (DATA_PART_SIZE * 8)


#define RADIO_MAGIC     0x0000168C
#define RADIO_MAGIC_MASK    0x0000FFFF
#define IS_RADIO_DATA(v)        ( ((v) & RADIO_MAGIC_MASK) == RADIO_MAGIC )

struct ar531x_boarddata {
    u_int32_t magic;
#define BD_MAGIC        0x35333131
#ifndef AR531X_BD_MAGIC
#define AR531X_BD_MAGIC BD_MAGIC
#endif
    u_int16_t cksum;
    u_int16_t rev;
#define BD_REV  5
#if (BD_REV < 5) && defined(V54)
    char   boardName[32];
#define V54_BD_RANDOM_NUMBER_SIZE	16
    u_int8_t  randomNumber[V54_BD_RANDOM_NUMBER_SIZE];
    u_int32_t serialNumber;
    u_int16_t customerNumber;
    u_int16_t pad1[5];
#else
    char   boardName[64];
#endif
    u_int16_t major;
    u_int16_t minor;
    u_int32_t config;
#define BD_ENET0        0x00000001
#define BD_ENET1        0x00000002
#define BD_UART1        0x00000004
#define BD_UART0        0x00000008
#define BD_RSTFACTORY   0x00000010
#define BD_SYSLED       0x00000020
#define BD_EXTUARTCLK   0x00000040
#define BD_CPUFREQ      0x00000080
#define BD_SYSFREQ      0x00000100
#define BD_WLAN0        0x00000200
#define BD_MEMCAP       0x00000400
#define BD_DISWATCHDOG  0x00000800
#define BD_WLAN1        0x00001000
#define BD_ISCASPER     0x00002000
#define BD_WLAN0_2G_EN  0x00004000
#define BD_WLAN0_5G_EN  0x00008000
#define BD_WLAN1_2G_EN  0x00020000
#define BD_WLAN1_5G_EN  0x00040000
#define BD_LOCALBUS     0x00080000

    u_int16_t resetConfigGpio;
    u_int16_t sysLedGpio;

    u_int32_t cpuFreq;
    u_int32_t sysFreq;
    u_int32_t cntFreq;

    u_int8_t  wlan0Mac[6];
    u_int8_t  enet0Mac[6];
    u_int8_t  enet1Mac[6];

    u_int16_t pciId;
    u_int16_t memCap;

#define SIZEOF_BD_REV2  120


    u_int8_t  wlan1Mac[6];

#if (BD_REV < 5) && defined(V54)

    u_int8_t  sysLed3Gpio;
    u_int8_t  sysLed2Gpio;

    u_int8_t  boardType;
#define V54_BTYPE_PROTO		255
#define V54_BTYPE_ALL		0
#define V54_BTYPE_GD1		1
#define V54_BTYPE_GD2		2
#define V54_BTYPE_GD3		3
#define V54_BTYPE_GD4		4
#define V54_BTYPE_GD6_5		5
#define V54_BTYPE_GD6		6
#define V54_BTYPE_GD7   	7
#define V54_BTYPE_HUSKY   	8

#define V54_BTYPE_MAX		15

    u_int8_t  sysLed4Gpio;

    u_int32_t antCntl;
    u_int32_t v54Config;
#define BD_SYSLED2      0x00000001
#define BD_FIXED_OPM    0x00000002
#define BD_SYSLED3      0x00000004
#define BD_SYSLED4      0x00000008
#define BD_FIXED_CTRY   0x00000010
#define BD_ANTCNTL      0x00000020

    u_int8_t  reboot_threshold;

    u_int8_t  operationalMode;
#define BD_OPMODE_UNDEF	0
#define BD_OPMODE_AP	1
#define BD_OPMODE_STA	2
    u_int16_t appl_use;
    u_int16_t pad2[4];
#endif
};


#ifdef V54
typedef
struct rks_boarddata
{
#define RKS_BD_MAGIC    0x52434B53
    u_int32_t magic;

#define RKS_BD_REV  4
	u_int16_t cksum;
	u_int16_t rev;

#define RKS_BD_SN_SIZE  16
    u_int8_t serialNumber[RKS_BD_SN_SIZE];
#define RKS_BD_CN_SIZE 32
    u_int8_t customerID[RKS_BD_CN_SIZE];
#define RKS_BD_MODEL_SIZE 16
    u_int8_t model[RKS_BD_MODEL_SIZE];
#define V54_BD_RANDOM_NUMBER_SIZE	16
    u_int8_t randomNumber[V54_BD_RANDOM_NUMBER_SIZE];

	u_int8_t  enetxMac[4][6];




    u_int8_t  boardType;
#define V54_BTYPE_PROTO		255
#define V54_BTYPE_ALL		0
#define V54_BTYPE_GD1		1
#define V54_BTYPE_GD2		2
#define V54_BTYPE_GD3		3
#define V54_BTYPE_GD4		4
#define V54_BTYPE_GD6_5		5
#define V54_BTYPE_GD6_1		6
#define V54_BTYPE_GD7   	7
#define V54_BTYPE_HUSKY   	8
#define V54_BTYPE_RETRIEVER   	9
#define V54_BTYPE_GD8   	10
#define V54_BTYPE_BAHAMA   	11
#define V54_BTYPE_GD9   	12
#define V54_BTYPE_GD10   	13
#define V54_BTYPE_GD11   	14
#define V54_BTYPE_GD12   	15
#define V54_BTYPE_GD17   	16
#define V54_BTYPE_WALLE  	17
#define V54_BTYPE_BERMUDA   18
#define V54_BTYPE_GD22      19
#define V54_BTYPE_GD25      20
#define V54_BTYPE_GD26      21
#define V54_BTYPE_GD28      22
#define V54_BTYPE_GD34      23
#define V54_BTYPE_SPANIEL   24
#define V54_BTYPE_GD36      25
#define V54_BTYPE_GD35      26
#define V54_BTYPE_WALLE2    27
#define V54_BTYPE_GD43      28
#define V54_BTYPE_GD42      29
#define V54_BTYPE_GD50      30
#define V54_BTYPE_GD56      31
#define V54_BTYPE_GD60      32
#define V54_BTYPE_GD66      33
#define V54_BTYPE_MAX	    63

    u_int8_t  sysLed2Gpio;
    u_int8_t  sysLed3Gpio;
    u_int8_t  sysLed4Gpio;

    u_int32_t antCntl;
    u_int32_t v54Config;
#define BD_SYSLED2      0x00000001
#define BD_FIXED_OPM    0x00000002
#define BD_SYSLED3      0x00000004
#define BD_SYSLED4      0x00000008
#define BD_FIXED_CTRY   0x00000010
#define BD_ANTCNTL      0x00000020

#define BD_MACPOOL      0x00000080
#define BD_HEATER       0x00000100
#define BD_GE_PORT      0x00000200
#define BD_BROWNOUT12   0x00000400
#define BD_GPS          0x00000800
#define LED_AIMING_MODE 0x00001000
#define BD_USB          0x00002000

#define BD_FIPS_SKU     0x00004000
#define BD_FIPS_MODE    0x00008000


    u_int16_t MACcnt;
    u_int8_t  MACbase[6];
    u_int8_t  pad1[12];

#if ( RKS_BD_REV > 1 )
#define V54_RBD_BOOTREC 1

#define RKS_BD_REV_BOOTREC 2
#define RKS_BOOTREC_MAGIC 0x524B4254
    struct {
        u_int32_t magic;
        int32_t main_img;
    } bootrec;

#if ( RKS_BD_REV > 2 )
    u_int8_t    eth_port;
    u_int8_t    sym_imgs;

    u_int8_t   boardClass;
#define V54_BCLASS_PROTO   255
#define V54_BCLASS_ALL     0
#define V54_BCLASS_AP43    1
#define V54_BCLASS_AP51    2
#define V54_BCLASS_AP71    3
#define V54_BCLASS_P10XX   4
#define V54_BCLASS_QCA_ARM 5

#define V54_BCLASS_MAX     15

#else
    u_int8_t pad3[3];
#endif
    u_int8_t cpuClk;
#define RKS_BD_REV_SERIAL32 4
#if ( RKS_BD_REV >= RKS_BD_REV_SERIAL32 )
#define RKS_BD_SN_SIZE_32  32
    u_int8_t serialNumber32[RKS_BD_SN_SIZE_32];
    u_int8_t pad4[20];
#else
    u_int8_t pad4[52];
#endif
#else

#endif

} RKS_BOARD_DATA_T;


extern int v54_get_main_img_id(void);
extern int v54_set_main_img_id(int main_img);


#endif

extern char * v54BType(int);
extern int ar531xSetupFlash(void);
extern int ar531x_save_flash(struct ar531x_boarddata *bd, struct rks_boarddata *rbd);

extern int bd_get_lan_macaddr(int lanid, unsigned char *macaddr);




extern struct ar531x_boarddata *ar531x_boardConfig;
extern struct ar531x_boarddata *ar531x_boardData;
extern char *radioConfig;

#define RKS_SW2_PRESSED 0
#define RKS_SW2_HELD    1

typedef void (*FN_SW2IF)( int eventcode );
extern void rks_sw2_cb_reg( FN_SW2IF cb_rtn );
extern void rks_sw2_reset( void );



extern struct rks_boarddata *rks_boardConfig;
extern struct rks_boarddata *rks_boardData;
extern unsigned long get_bd_offset(void);
extern unsigned long get_rbd_offset(void);
extern int ar531x_get_board_config(void);
extern int bd_get_basemac(unsigned char *macaddr);
extern unsigned char rks_is_husky_board(void);
extern unsigned char rks_is_bahama_board(void);
extern unsigned char rks_is_gd9_board(void);
extern unsigned char rks_is_gd10_board(void);
extern unsigned char rks_is_gd11_board(void);
extern unsigned char rks_is_gd12_board(void);
extern unsigned char rks_is_gd17_board(void);
extern unsigned char rks_is_gd22_board(void);
extern unsigned char rks_is_gd25_board(void);
extern unsigned char rks_is_gd26_board(void);
extern unsigned char rks_is_spaniel_board(void);
extern unsigned char rks_is_spaniel_usb_board(void);
extern unsigned char rks_is_walle_board(void);
extern unsigned char rks_is_walle2_board(void);
extern unsigned char rks_is_bermuda_board(void);
extern unsigned char rks_is_7762s_board(void);
extern unsigned char rks_is_gd28_board(void);
extern unsigned char rks_is_gd34_board(void);
extern unsigned char rks_is_gd35_board(void);
extern unsigned char rks_is_gd36_board(void);
extern unsigned char rks_is_cm_board(void);
extern unsigned char rks_is_puli_board(void);
extern unsigned char rks_is_gd50_board(void);
extern unsigned char rks_is_gd56_board(void);
extern unsigned char rks_is_gd60_board(void);
extern unsigned char rks_is_r300_board(void);
extern unsigned char rks_is_gd43_board(void);
extern unsigned char rks_is_gd42_board(void);
extern unsigned char rks_is_r500_board(void);
extern unsigned char rks_is_r600_board(void);
extern unsigned char rks_is_t504_board(void);
extern unsigned char rks_is_p300_board(void);
extern unsigned char rks_is_h500_board(void);
extern unsigned char rks_is_r710_board(void);
extern unsigned char rks_is_r720_board(void);
extern unsigned char rks_is_t710_board(void);
extern unsigned char rks_is_t811cm_board(void);
extern unsigned char rks_is_r610_board(void);
extern unsigned char rks_is_t610_board(void);
extern unsigned char rks_is_h500_board(void);
extern unsigned char rks_is_c500_board(void);
extern unsigned char rks_is_r310_board(void);
extern unsigned char rks_is_c110_board(void);
extern unsigned char rks_is_r510_board(void);
extern unsigned char rks_is_h510_board(void);
extern unsigned char rks_is_h320_board(void);
extern unsigned char rks_is_t310_board(void);
extern unsigned char rks_is_e510_board(void);
extern unsigned char rks_is_r730_board(void);
extern unsigned char rks_is_m510_board(void);
extern unsigned char rks_is_t305_board(void);
extern unsigned char rks_is_r750_board(void);
extern unsigned char rks_is_t750_board(void);
extern unsigned char rks_is_r650_board(void);
extern unsigned char rks_is_r550_board(void);
extern unsigned char rks_is_r850_board(void);
extern unsigned char rks_is_t750se_board(void);
extern unsigned char rks_is_h550_board(void);
extern unsigned char rks_is_h350_board(void);
extern unsigned char rks_is_t350_board(void);
extern unsigned char rks_is_r350_board(void);
extern unsigned char rks_is_t350se_board(void);


extern unsigned char rks_is_fips_sku(void);

extern int i2c_rks_init (void);
extern void i2c_rks_exit (void);
extern int ts_config(int unit);
extern int ts_get_temp(int unit, unsigned char *data);
extern int accel_config(void);
extern int accel_get_xyz(unsigned char *data);
extern int pd_config(void);
extern int pd_read(unsigned char *data);
extern int tpm_get_clk(unsigned char *data);
extern int arm_ts_config(int unit);
extern int arm_ts_get_temp(int unit, unsigned char *data);
extern int set_i2c_adapter(void);
extern void gpio_exp_init(void);
int gpio_exp_read(int gpio);
void gpio_exp_write(int gpio, int val);


#endif

#endif
