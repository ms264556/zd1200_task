/*
 * bcp.h - Bridge Control Protocol definitions.
 *
 * Copyright (c) 2001-2004 Applied Innovation Inc.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms.  The name of Applied Innovation Inc.
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Copyright (c) 2007-     Ruckus Wireless Inc.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms.  The name of Ruckus Wireless Inc.
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * Options.
 * FIXME: RFC 3518 requires bit B in data packets be processed.
 * That should be done in kernel space.
 */
#define CI_BRIDGE_IDENTIFICATION	1
#define CI_LINE_IDENTIFICATION		2
#define CI_MAC_SUPPORT			3
#define CI_TINYGRAM_COMPRESSION		4
#define CI_LAN_IDENTIFICATION		5	/* obsolete (since RFC 2878) */
#define CI_MAC_ADDRESS			6
#define CI_SPANNING_TREE_PROTOCOL	7	/* old format (RFC 1638) */
#define CI_IEEE_802_TAGGED_FRAME	8
#define CI_MANAGEMENT_INLINE		9	/* new format (RFC 2878) */
#define CI_BPDU_INDICATOR		10	/* new option (RFC 3518) */

/* values for MAC Support */
#define MAC_IEEE_802_3			1
#define MAC_IEEE_802_4			2
#define MAC_IEEE_802_5_NON		3
#define MAC_FDDI_NON			4
#define MAC_IEEE_802_5			11
#define MAC_FDDI			12

/* Multicast bit */
#define BCP_MULTICAST			1

/* values for Spanning Tree protocol */
#define SPAN_NONE			0
#define SPAN_IEEE_802_1D		1
#define SPAN_IEEE_802_1G		2
#define SPAN_IBM			3
#define SPAN_DEC			4
#define MAX_SPANTREE			SPAN_DEC

#define ETH_ALEN			6	/* ethernet address length */

typedef struct bcp_options {
    u_int16_t	lan_bridge_segno;
    u_char	macsupport[6];	/* MAC support */
    u_char	tinygram;	/* Tinygram Compression 1=enable, 2=disable */
    u_char	lanid;		/* Lan ID 1=enable, 2=disable*/
    u_char	macaddr[ETH_ALEN];	/* MAC Address */
    u_char	spantree;	/* Spanning Tree Protocol */
    u_char	ieee802tag;	/* IEEE 802.1q tag */
    bool	neg_bridgeid;
    bool	neg_lineid;
    bool	neg_macsupport;
    bool	neg_tinygram;
    bool	neg_lanid;
    bool	neg_macaddr;
    bool	neg_spantree;
    bool	neg_ieee802tag;
    bool	neg_mgmt_inline;
    bool	neg_bpdu_indicator;
} bcp_options;

extern fsm bcp_fsm[];
extern bcp_options bcp_wantoptions[];
extern bcp_options bcp_gotoptions[];
extern bcp_options bcp_allowoptions[];
extern bcp_options bcp_hisoptions[];

extern struct protent bcp_protent;
