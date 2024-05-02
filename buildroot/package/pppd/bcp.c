/*
 * bcp.c - PPP Bridge Control Protocol.
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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "pppd.h"
#include "fsm.h"
#include "bcp.h"
#include "pathnames.h"

/* global vars */
bcp_options bcp_wantoptions[NUM_PPP];	/* Options that we want to request */
bcp_options bcp_gotoptions[NUM_PPP];	/* Options that peer ack'd */
bcp_options bcp_allowoptions[NUM_PPP]; /* Options we allow peer to request */
bcp_options bcp_hisoptions[NUM_PPP];	/* Options that we ack'd */

/* local vars */
static int bcp_is_up;			/* have called np_up() */

static bool bcp_maclocal_valid;
static u_char bcp_maclocal[ETH_ALEN];
static char bcp_maclocal_str[3*ETH_ALEN]; /* string form of "maclocal" arg */

/*
 * Callbacks for fsm code.  (CI = Configuration Information)
 */
static void bcp_resetci __P((fsm *));	/* Reset our CI */
static int  bcp_cilen __P((fsm *));	        /* Return length of our CI */
static void bcp_addci __P((fsm *, u_char *, int *)); /* Add our CI */
static int  bcp_ackci __P((fsm *, u_char *, int));	/* Peer ack'd our CI */
static int  bcp_nakci __P((fsm *, u_char *, int, int));	/* Peer nak'd our CI */
static int  bcp_rejci __P((fsm *, u_char *, int));	/* Peer rej'd our CI */
static int  bcp_reqci __P((fsm *, u_char *, int *, int)); /* Rcv CI */
static void bcp_up __P((fsm *));		/* We're UP */
static void bcp_down __P((fsm *));		/* We're DOWN */
static void bcp_start __P((fsm *));	/* Need lower layer */
static void bcp_finished __P((fsm *));	/* Don't need lower layer */

fsm bcp_fsm[NUM_PPP];		/* BCP fsm structure */

static fsm_callbacks bcp_callbacks = { /* BCP callback routines */
    bcp_resetci,		/* Reset our Configuration Information */
    bcp_cilen,			/* Length of our Configuration Information */
    bcp_addci,			/* Add our Configuration Information */
    bcp_ackci,			/* ACK our Configuration Information */
    bcp_nakci,			/* NAK our Configuration Information */
    bcp_rejci,			/* Reject our Configuration Information */
    bcp_reqci,			/* Request peer's Configuration Information */
    bcp_up,			/* Called when fsm reaches OPENED state */
    bcp_down,			/* Called when fsm leaves OPENED state */
    bcp_start,			/* Called when we want the lower layer up */
    bcp_finished,		/* Called when we want the lower layer down */
    NULL,			/* Called when Protocol-Reject received */
    NULL,			/* Retransmission is necessary */
    NULL,			/* Called to handle protocol-specific codes */
    "BCP"			/* String name of protocol */
};

/*
 * Command-line options.
 */
static int bcp_setmaclocal __P((char **));

static option_t bcp_option_list[] = {
    { "nobcp", o_bool, &bcp_protent.enabled_flag, "Disable BCP" },
    { "maclocal", o_special, (void *)bcp_setmaclocal, "set local MAC address",
      OPT_PRIO | OPT_A2STRVAL | OPT_STATIC, bcp_maclocal_str },
    { NULL }			/* terminating entry */
};
/*
 * Protocol entry points from main code.
 */
static void bcp_init __P((int));
static void bcp_open __P((int));
static void bcp_close __P((int, char *));
static void bcp_lowerup __P((int));
static void bcp_lowerdown __P((int));
static void bcp_input __P((int, u_char *, int));
static void bcp_protrej __P((int));
static int  bcp_printpkt __P((u_char *, int,
			       void (*) __P((void *, char *, ...)), void *));
static void bcp_check_options __P((void));

struct protent bcp_protent = {
    PPP_BCP,
    bcp_init,
    bcp_input,
    bcp_protrej,
    bcp_lowerup,
    bcp_lowerdown,
    bcp_open,
    bcp_close,
    bcp_printpkt,
    NULL,
    1,
    "BCP",
    "Bridging",
    bcp_option_list,
    bcp_check_options,
    NULL,
    NULL
};

static void bcp_script __P((fsm *, char *));	/* Run an up/down script */
static void bcp_script_done __P((void *));

/*
 * Lengths of configuration options.
 */
#define CILEN_VOID	2
#define CILEN_BRIDGELINEID	4
#define CILEN_MACSUPPORT 3
#define CILEN_TINYGRAM 3
#define CILEN_LANID 3
#define CILEN_MACADDR 8
#define CILEN_SPANTREE 3
#define CILEN_IEEE_802_TAGGED_FRAME 3
#define CILEN_MGMT_INLINE 2
#define CILEN_BPDU_INDICATOR 2

#define CODENAME(x)	((x) == CONFACK ? "ACK" : \
			 (x) == CONFNAK ? "NAK" : "REJ")

/*
 * This state variable is used to ensure that we don't
 * run an bcp-up/down script while one is already running.
 */
static enum script_state {
    s_down,
    s_up,
} bcp_script_state;
static pid_t bcp_script_pid;

/*
 * Convert a MAC address from a string to an array of bytes.
 */
static int
mac_aton(const char *cp, u_char *mac)
{
    unsigned int args[ETH_ALEN];
    int n;

    if (sscanf(cp, "%2x:%2x:%2x:%2x:%2x:%2x%n",
               &args[0], &args[1], &args[2],
               &args[3], &args[4], &args[5], &n) >= ETH_ALEN)
    {
        if (!cp[n]) /* expect to have reached the end of the string */
        {
            int i;
            for (i = 0; i < ETH_ALEN; ++i)
            {
                mac[i] = args[i];
            }
            return 1;
        }
    }

    return 0;
}

/*
 * Print a MAC address into a buffer and return the length.
 */
static int
slprintmac(char *buf, int buflen, const u_char *mac)
{
    return slprintf(buf, buflen, "%x:%x:%x:%x:%x:%x",
		    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/*
 * bcp_setmaclocal - set the MAC address to be used on the local interface.
 * Returns 0 on error, 1 on success.
 */
static int
bcp_setmaclocal(argv)
    char **argv;
{
    bcp_maclocal_valid = mac_aton(argv[0], bcp_maclocal);

    if (bcp_maclocal_valid && (bcp_maclocal[0] & BCP_MULTICAST))
	bcp_maclocal_valid = 0;

    if (!bcp_maclocal_valid)
    {
	option_error("invalid maclocal value '%s'", argv[0]);
	return 0;
    }

    slprintmac(bcp_maclocal_str, sizeof(bcp_maclocal_str), bcp_maclocal);
    return 1;
}

/*
 * bcp_init - Initialize BCP.
 */
static void
bcp_init(unit)
    int unit;
{
    fsm *f = &bcp_fsm[unit];
    bcp_options *wo = &bcp_wantoptions[unit];
    bcp_options *ao = &bcp_allowoptions[unit];

    int bcp_options_flag = 0xFF;
    char *bcp_options_env;
    bcp_options_env = getenv("L2TP_BCP_OPTS");
    if (bcp_options_env) {
        bcp_options_flag = atoi(bcp_options_env);
    }

    f->unit = unit;
    f->protocol = PPP_BCP;
    f->callbacks = &bcp_callbacks;
    fsm_init(&bcp_fsm[unit]);

    memset(wo, 0, sizeof(*wo));
    memset(ao, 0, sizeof(*ao));

    if (bcp_options_flag) { //////////////////////////////////////////////
    wo->neg_macsupport = 0;
    wo->macsupport[0] = MAC_IEEE_802_3;

    /* Try Management-Inline first, but prepare to fall back on
     * Spanning-Tree-Protocol if it fails. */
    wo->neg_mgmt_inline = 1;
    wo->neg_spantree = 0;
    wo->spantree = SPAN_IEEE_802_1D;

    wo->neg_tinygram = 0;
    wo->tinygram = 1;			/* Linux can receive Tinygrams. */

    wo->neg_ieee802tag = 1;
    wo->ieee802tag = 1;
    wo->neg_bpdu_indicator = 1;
    } ///////////////////////////////////////////////////////////////////

    ao->neg_macsupport = 1;
    ao->neg_tinygram = 1;
    ao->neg_macaddr = 1;

    ao->neg_mgmt_inline = 1;
    ao->neg_spantree = 1;

    ao->neg_ieee802tag = 1;
    ao->neg_bpdu_indicator = 1;
}


/*
 * bcp_open - BCP is allowed to come up.
 */
static void
bcp_open(unit)
    int unit;
{
    fsm_open(&bcp_fsm[unit]);
}


/*
 * bcp_close - Take BCP down.
 */
static void
bcp_close(unit, reason)
    int unit;
    char *reason;
{
    fsm_close(&bcp_fsm[unit], reason);
}


/*
 * bcp_lowerup - The lower layer is up.
 */
static void
bcp_lowerup(unit)
    int unit;
{
    fsm_lowerup(&bcp_fsm[unit]);
}


/*
 * bcp_lowerdown - The lower layer is down.
 */
static void
bcp_lowerdown(unit)
    int unit;
{
    fsm_lowerdown(&bcp_fsm[unit]);
}


/*
 * bcp_input - Input BCP packet.
 */
static void
bcp_input(unit, p, len)
    int unit;
    u_char *p;
    int len;
{
    fsm_input(&bcp_fsm[unit], p, len);
}


/*
 * bcp_protrej - A Protocol-Reject was received for BCP.
 *
 * Pretend the lower layer went down, so we shut up.
 */
static void
bcp_protrej(unit)
    int unit;
{
  fsm_lowerdown(&bcp_fsm[unit]);
}


/*
 * bcp_resetci - Reset our CI.
 * Called by fsm_sconfreq, Send Configure Request.
 */
static void
bcp_resetci(f)
    fsm *f;
{
    bcp_options *wo = &bcp_wantoptions[f->unit];
    bcp_options *go = &bcp_gotoptions[f->unit];

    /* Either announce our MAC address to the other side, or request one from
     * the other side.  (Another possibility is not to negotiate at all, but
     * that seems useless.)
     *
     * However, there are use cases where MAC-Address option is not needed:
     * - In case the BCP interface or VLAN subinterface will be bridged.
     * - PPP peer on remote side (e.g. Cisco 7301) rejects this option anyway.
     */
    wo->neg_macaddr = 0;
    if (bcp_maclocal_valid)
	memcpy(wo->macaddr, bcp_maclocal, sizeof(wo->macaddr));
    else
	memset(wo->macaddr, 0, sizeof(wo->macaddr));

    *go = *wo;
}


/*
 * bcp_cilen - Return length of our CI.
 * Called by fsm_sconfreq, Send Configure Request.
 */
static int
bcp_cilen(f)
    fsm *f;
{
    bcp_options *go = &bcp_gotoptions[f->unit];

    int macsupport_len = 0;
    int mac = 0;
    for (mac=0; mac<ETH_ALEN; mac++)
    {
      if (go->macsupport[mac])
      {
        macsupport_len += CILEN_MACSUPPORT;
      }
    }

#define LENCIBRIDGELINEID(neg)	((neg) ? CILEN_BRIDGELINEID : 0)
#define LENCITINYGRAM(neg)	((neg) ? CILEN_TINYGRAM : 0)
#define LENCILANID(neg)	((neg) ? CILEN_LANID : 0)
#define LENCIMACADDR(neg)	((neg) ? CILEN_MACADDR : 0)
#define LENCISPANTREE(neg)	((neg) ? CILEN_SPANTREE : 0)
#define LENCIIEEE802TAG(neg)	((neg) ? CILEN_IEEE_802_TAGGED_FRAME : 0)
#define LENCIMGMTINLINE(neg)	((neg) ? CILEN_MGMT_INLINE : 0)
#define LENCIBPDUINDICATOR(neg)	((neg) ? CILEN_BPDU_INDICATOR : 0)


    return (LENCIBRIDGELINEID(go->neg_bridgeid) +
            LENCIBRIDGELINEID(go->neg_lineid) +
            macsupport_len +
            LENCITINYGRAM(go->neg_tinygram) +
            LENCILANID(go->neg_lanid) +
            LENCIMACADDR(go->neg_macaddr) +
            LENCISPANTREE(go->neg_spantree) +
            LENCIIEEE802TAG(go->neg_ieee802tag) +
	    LENCIMGMTINLINE(go->neg_mgmt_inline) +
	    LENCIBPDUINDICATOR(go->neg_bpdu_indicator));
}

/*
 * bcp_addci - Add our desired CIs to a packet.
 * Called by fsm_sconfreq, Send Configure Request.
 */
static void
bcp_addci(f, ucp, lenp)
    fsm *f;
    u_char *ucp;
    int *lenp;
{
    bcp_options *go = &bcp_gotoptions[f->unit];
    int len = *lenp;

#define ADDCIBRIDGELINEID(opt, neg, lan_bridge_segno) \
    if (neg) { \
	if (len >= CILEN_BRIDGELINEID) { \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(CILEN_BRIDGELINEID, ucp); \
	    PUTSHORT(lan_bridge_segno, ucp); \
	    len -= CILEN_BRIDGELINEID; \
	} else \
	    neg = 0; \
    }

#define ADDCIMACSUPPORT(opt, neg, macsupport) \
    if (neg && macsupport) { \
	if (len >= CILEN_MACSUPPORT) { \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(CILEN_MACSUPPORT, ucp); \
	    PUTCHAR(macsupport, ucp); \
	    len -= CILEN_MACSUPPORT; \
	} else \
	    neg = 0; \
    }

#define ADDCITINYGRAM(opt, neg, tinygram) \
    if (neg) { \
	if (len >= CILEN_TINYGRAM) { \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(CILEN_TINYGRAM, ucp); \
	    PUTCHAR(tinygram, ucp); \
	    len -= CILEN_TINYGRAM; \
	} else \
	    neg = 0; \
    }

#define ADDCILANID(opt, neg, lanid) \
    if (neg) { \
	if (len >= CILEN_LANID) { \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(CILEN_LANID, ucp); \
	    PUTCHAR(lanid, ucp); \
	    len -= CILEN_LANID; \
	} else \
	    neg = 0; \
    }

#define ADDCIMACADDR(opt, neg, macaddr) \
    if (neg) { \
	if (len >= CILEN_MACADDR) { \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(CILEN_MACADDR, ucp); \
	    PUTCHAR(macaddr[0], ucp); \
	    PUTCHAR(macaddr[1], ucp); \
	    PUTCHAR(macaddr[2], ucp); \
	    PUTCHAR(macaddr[3], ucp); \
	    PUTCHAR(macaddr[4], ucp); \
	    PUTCHAR(macaddr[5], ucp); \
	    len -= CILEN_MACADDR; \
	} else \
	    neg = 0; \
    }

#define ADDCISPANTREE(opt, neg, spantree) \
    if (neg) { \
	if (len >= CILEN_SPANTREE) { \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(CILEN_SPANTREE, ucp); \
	    PUTCHAR(spantree, ucp); \
	    len -= CILEN_SPANTREE; \
	} else \
	    neg = 0; \
    }

#define ADDCIIEEE802TAG(opt, neg, ieee802tag) \
    if (neg) { \
	if (len >= CILEN_IEEE_802_TAGGED_FRAME) { \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(CILEN_IEEE_802_TAGGED_FRAME, ucp); \
	    PUTCHAR(ieee802tag, ucp); \
	    len -= CILEN_IEEE_802_TAGGED_FRAME; \
	} else \
	    neg = 0; \
    }

#define ADDCIMGMTINLINE(opt, neg) \
    if (neg) { \
	if (len >= CILEN_MGMT_INLINE) { \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(CILEN_MGMT_INLINE, ucp); \
	    len -= CILEN_MGMT_INLINE; \
	} else \
	    neg = 0; \
    }

#define ADDCIBPDUINDICATOR(opt, neg) \
    if (neg) { \
	if (len >= CILEN_BPDU_INDICATOR) { \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(CILEN_BPDU_INDICATOR, ucp); \
	    len -= CILEN_BPDU_INDICATOR; \
	} else \
	    neg = 0; \
    }

    ADDCIBRIDGELINEID(CI_BRIDGE_IDENTIFICATION, go->neg_bridgeid, go->lan_bridge_segno);

    ADDCIBRIDGELINEID(CI_LINE_IDENTIFICATION, go->neg_lineid, go->lan_bridge_segno);

    ADDCIMACSUPPORT(CI_MAC_SUPPORT, go->neg_macsupport, go->macsupport[0]);
    ADDCIMACSUPPORT(CI_MAC_SUPPORT, go->neg_macsupport, go->macsupport[1]);
    ADDCIMACSUPPORT(CI_MAC_SUPPORT, go->neg_macsupport, go->macsupport[2]);
    ADDCIMACSUPPORT(CI_MAC_SUPPORT, go->neg_macsupport, go->macsupport[3]);
    ADDCIMACSUPPORT(CI_MAC_SUPPORT, go->neg_macsupport, go->macsupport[4]);
    ADDCIMACSUPPORT(CI_MAC_SUPPORT, go->neg_macsupport, go->macsupport[5]);

    ADDCITINYGRAM(CI_TINYGRAM_COMPRESSION, go->neg_tinygram, go->tinygram);

    ADDCILANID(CI_LAN_IDENTIFICATION, go->neg_lanid, go->lanid);

    ADDCIMACADDR(CI_MAC_ADDRESS, go->neg_macaddr, go->macaddr);

    ADDCISPANTREE(CI_SPANNING_TREE_PROTOCOL, go->neg_spantree, go->spantree);

    ADDCIIEEE802TAG(CI_IEEE_802_TAGGED_FRAME, go->neg_ieee802tag, go->ieee802tag);

    ADDCIMGMTINLINE(CI_MANAGEMENT_INLINE, go->neg_mgmt_inline);

    ADDCIBPDUINDICATOR(CI_BPDU_INDICATOR, go->neg_bpdu_indicator);

    *lenp -= len;
}


/*
 * bcp_ackci - Ack our CIs.
 * Called by fsm_rconfack, Receive Configure ACK.
 *
 * Returns:
 *	0 - Ack was bad.
 *	1 - Ack was good.
 */
static int
bcp_ackci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    bcp_options *go = &bcp_gotoptions[f->unit];
    u_char cilen, citype, cichar;
    u_short cishort;

    /*
     * CIs must be in exactly the same order that we sent...
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define ACKCIBRIDGELINEID(opt, neg, lan_bridge_segno) \
    if (neg) { \
	if ((len -= CILEN_BRIDGELINEID) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_BRIDGELINEID || citype != opt) \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (lan_bridge_segno != cishort) \
	    goto bad; \
    }

#define ACKCIMACSUPPORT(opt, neg, macsupport) \
    if (neg && macsupport) { \
	if ((len -= CILEN_MACSUPPORT) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_MACSUPPORT || citype != opt) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (macsupport != cichar) \
	    goto bad; \
    }

#define ACKCITINYGRAM(opt, neg, tinygram) \
    if (neg) { \
	if ((len -= CILEN_TINYGRAM) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_TINYGRAM || citype != opt) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (tinygram != cichar) \
	    goto bad; \
    }

#define ACKCILANID(opt, neg, lanid) \
    if (neg) { \
	if ((len -= CILEN_LANID) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_LANID || citype != opt) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (lanid != cichar) \
	    goto bad; \
    }

#define ACKCIMACADDR(opt, neg, macaddr) \
    if (neg) { \
	if ((len -= CILEN_MACADDR) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_MACADDR || citype != opt) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (macaddr[0] != cichar) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (macaddr[1] != cichar) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (macaddr[2] != cichar) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (macaddr[3] != cichar) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (macaddr[4] != cichar) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (macaddr[5] != cichar) \
	    goto bad; \
    }

#define ACKCISPANTREE(opt, neg, spantree) \
    if (neg) { \
	if ((len -= CILEN_SPANTREE) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_SPANTREE || citype != opt) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (spantree != cichar) \
	    goto bad; \
    }

#define ACKCIIEEE802TAG(opt, neg, ieee802tag) \
    if (neg) { \
	if ((len -= CILEN_IEEE_802_TAGGED_FRAME) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_IEEE_802_TAGGED_FRAME || citype != opt) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (ieee802tag != cichar) \
	    goto bad; \
    }

#define ACKMGMTINLINE(opt, neg) \
    if (neg) { \
	if ((len -= CILEN_MGMT_INLINE) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_MGMT_INLINE || citype != opt) \
	    goto bad; \
    }

#define ACKBPDUINDICATOR(opt, neg) \
    if (neg) { \
	if ((len -= CILEN_BPDU_INDICATOR) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_BPDU_INDICATOR || citype != opt) \
	    goto bad; \
    }


    ACKCIBRIDGELINEID(CI_BRIDGE_IDENTIFICATION, go->neg_bridgeid, go->lan_bridge_segno);

    ACKCIBRIDGELINEID(CI_LINE_IDENTIFICATION, go->neg_lineid, go->lan_bridge_segno);

    ACKCIMACSUPPORT(CI_MAC_SUPPORT, go->neg_macsupport, go->macsupport[0]);
    ACKCIMACSUPPORT(CI_MAC_SUPPORT, go->neg_macsupport, go->macsupport[1]);
    ACKCIMACSUPPORT(CI_MAC_SUPPORT, go->neg_macsupport, go->macsupport[2]);
    ACKCIMACSUPPORT(CI_MAC_SUPPORT, go->neg_macsupport, go->macsupport[3]);
    ACKCIMACSUPPORT(CI_MAC_SUPPORT, go->neg_macsupport, go->macsupport[4]);
    ACKCIMACSUPPORT(CI_MAC_SUPPORT, go->neg_macsupport, go->macsupport[5]);

    ACKCITINYGRAM(CI_TINYGRAM_COMPRESSION, go->neg_tinygram, go->tinygram);

    ACKCILANID(CI_LAN_IDENTIFICATION, go->neg_lanid, go->lanid);

    ACKCIMACADDR(CI_MAC_ADDRESS, go->neg_macaddr, go->macaddr);

    ACKCISPANTREE(CI_SPANNING_TREE_PROTOCOL, go->neg_spantree, go->spantree);

    ACKCIIEEE802TAG(CI_IEEE_802_TAGGED_FRAME, go->neg_ieee802tag, go->ieee802tag);

    ACKMGMTINLINE(CI_MANAGEMENT_INLINE, go->neg_mgmt_inline);

    ACKBPDUINDICATOR(CI_BPDU_INDICATOR, go->neg_bpdu_indicator);

    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len != 0)
	goto bad;
    return (1);

bad:
    BCPDEBUG(("bcp_ackci: received bad Ack!"));
    return (0);
}

/*
 * bcp_nakci - Peer has sent a NAK for some of our CIs.
 * This should not modify any state if the Nak is bad
 * or if BCP is in the OPENED state.
 * Calback from fsm_rconfnakrej - Receive Configure-Nak or Configure-Reject.
 *
 * Returns:
 *	0 - Nak was bad.
 *	1 - Nak was good.
 */
static int
bcp_nakci(f, p, len, treat_as_reject)
    fsm *f;
    u_char *p;
    int len;
    int treat_as_reject;
{
    bcp_options *go = &bcp_gotoptions[f->unit];
    u_char cichar;
    u_char citype, cilen, *next;
    u_short cishort;
    bcp_options no;		/* options we've seen Naks for */
    bcp_options try;		/* options to request next time */
    u_char bridge_id;
    u_short lan_segno;
    u_char macaddr[ETH_ALEN];
    int mac;

    BZERO(&no, sizeof(no));
    try = *go;

    /*
     * Any Nak'd CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define NAKCIBRIDGELINEID(opt, neg, code) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_BRIDGELINEID) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	no.neg = 1; \
	code \
    }

#define NAKCIMACADDR(opt, neg, code) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_MACADDR) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	no.neg = 1; \
  GETCHAR(macaddr[0], p); \
  GETCHAR(macaddr[1], p); \
  GETCHAR(macaddr[2], p); \
  GETCHAR(macaddr[3], p); \
  GETCHAR(macaddr[4], p); \
  GETCHAR(macaddr[5], p); \
	code \
    }

#define NAKCISPANTREE(opt, neg, code) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_SPANTREE) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	no.neg = 1; \
	GETCHAR(cichar, p); \
	code \
    }

#define NAKCIIEEE802TAG(opt, neg, code) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_IEEE_802_TAGGED_FRAME) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	no.neg = 1; \
	GETCHAR(cichar, p); \
	code \
    }

#define NAKCIMGMTINLINE(opt, neg, code) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_MGMT_INLINE) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	no.neg = 1; \
	code \
    }

#define NAKCIBPDUINDICATOR(opt, neg, code) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_BPDU_INDICATOR) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	no.neg = 1; \
	code \
    }

    /*
     * Accept the peer's value of bridge_id provided that it
     * is greater than what we asked for.
     */
    NAKCIBRIDGELINEID(CI_BRIDGE_IDENTIFICATION, neg_bridgeid,
      bridge_id = cishort & 0xf;
      if (bridge_id > 0 &&
          bridge_id > (go->lan_bridge_segno & 0xf))
      {
        try.lan_bridge_segno = cishort;
      }
	    );

    /*
     * Accept the peer's value of lan_segno provided that it
     * is greater than what we asked for.
     */
    NAKCIBRIDGELINEID(CI_LINE_IDENTIFICATION, neg_lineid,
      lan_segno = cishort >> 4;
      if (lan_segno > 0 &&
          lan_segno > (go->lan_bridge_segno >> 4))
      {
        try.lan_bridge_segno = cishort;
      }
	    );

    /*
     * Peer is not supposed to send Mac-Support in a Configure-Nak.
     */

    /*
     * Peer is not supposed to send TinyGram-Compression in a Configure-Nak
     * if we already sent it in a Configure-Request already.
     */


    /*
     * Peer is not supposed to send LAN-Identification in a Configure-Nak.
     */

    /*
     * Accept the peer's value of macaddr provided that we requested
     * an address of 00-00-00-00-00-00 and the suggested address does
     * NOT have the multicast bit set.
     */
    NAKCIMACADDR(CI_MAC_ADDRESS, neg_macaddr,
      for (mac=0; mac<ETH_ALEN; mac++)
      {
        if (go->macaddr[mac] != 0)
          break;
      }
      if (mac == ETH_ALEN)
      {
        if (!(macaddr[0] & BCP_MULTICAST))
        {
          try.macaddr[0] = macaddr[0];
          try.macaddr[1] = macaddr[1];
          try.macaddr[2] = macaddr[2];
          try.macaddr[3] = macaddr[3];
          try.macaddr[4] = macaddr[4];
          try.macaddr[5] = macaddr[5];
        }
      }
	    );

    /*
     * Accept the peer's value of spantree provided it has a lower
     * protocol Id than the one we requested.
     */
    NAKCISPANTREE(CI_SPANNING_TREE_PROTOCOL, neg_spantree,
      if (cichar < go->spantree)
      {
        try.spantree = cichar;
      }
	    );

    NAKCIIEEE802TAG(CI_IEEE_802_TAGGED_FRAME, neg_ieee802tag,
      if (cichar < go->ieee802tag)
      {
        try.ieee802tag = cichar;
      }
	    );

    NAKCIMGMTINLINE(CI_MANAGEMENT_INLINE, neg_mgmt_inline,
      do {} while(0);
	    );

    NAKCIBPDUINDICATOR(CI_BPDU_INDICATOR, neg_bpdu_indicator,
      do {} while(0);
	    );

    /*
     * There may be remaining CIs, if the peer is requesting negotiation
     * on an option that we didn't include in our request packet.
     * If we see an option that we requested, or one we've already seen
     * in this packet, then this packet is bad.
     * If we wanted to respond by starting to negotiate on the requested
     * option(s), we could, but we don't, because if we are not negotiating
     * an option, it is because we were told not to.
     */
    while (len > CILEN_VOID) {
	GETCHAR(citype, p);
	GETCHAR(cilen, p);
	if( (len -= cilen) < 0 )
	    goto bad;
	next = p + cilen - 2;

	switch (citype) {
	case CI_BRIDGE_IDENTIFICATION:
	    if (go->neg_bridgeid || no.neg_bridgeid || cilen != CILEN_BRIDGELINEID)
		goto bad;
	    break;
	case CI_LINE_IDENTIFICATION:
	    if (go->neg_lineid || no.neg_lineid || cilen != CILEN_BRIDGELINEID)
		goto bad;
	    break;
	case CI_TINYGRAM_COMPRESSION:
	    if (go->neg_tinygram || cilen != CILEN_TINYGRAM)
		goto bad;
	    break;
	case CI_LAN_IDENTIFICATION:
	    if (go->neg_lanid || cilen != CILEN_LANID)
		goto bad;
	    break;
	case CI_MAC_ADDRESS:
	    if (go->neg_macaddr || cilen != CILEN_MACADDR)
		goto bad;
	    break;
	case CI_SPANNING_TREE_PROTOCOL:
	    if (go->neg_spantree || cilen != CILEN_SPANTREE)
		goto bad;
	    break;
	case CI_IEEE_802_TAGGED_FRAME:
	    if (go->neg_ieee802tag || cilen != CILEN_IEEE_802_TAGGED_FRAME)
		goto bad;
	    break;
	case CI_MANAGEMENT_INLINE:
	    if (go->neg_mgmt_inline || cilen != CILEN_MGMT_INLINE)
		goto bad;
	    break;
	case CI_BPDU_INDICATOR:
	    if (go->neg_bpdu_indicator || cilen != CILEN_BPDU_INDICATOR)
		goto bad;
	    break;
	}
	p = next;
    }

    /*
     * OK, the Nak is good.  Now we can update state.
     * If there are any remaining options, we ignore them.
     */
    if (f->state != OPENED)
	*go = try;

    return 1;

bad:
    BCPDEBUG(("bcp_nakci: received bad Nak!"));
    return 0;
}


/*
 * bcp_rejci - Reject some of our CIs.
 * Callback from fsm_rconfnakrej.
 */
static int
bcp_rejci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    bcp_options *go = &bcp_gotoptions[f->unit];
    u_char cilen, cichar;
    u_short cishort;
    bcp_options try;		/* options to request next time */

    try = *go;

    /*
     * Any Rejected CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define REJCIBRIDGELINEID(opt, neg, lan_bridge_segno) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_BRIDGELINEID) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	/* Check rejected value. */ \
	if (cishort != lan_bridge_segno) \
	    goto bad; \
	try.neg = 0; \
    }

#define REJCIMACSUPPORT(opt, neg, macsupport) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_MACSUPPORT) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETCHAR(cichar, p); \
	/* Check rejected value. */ \
	if (cichar != macsupport) \
	    goto bad; \
	try.neg = 0; \
    }

#define REJCITINYGRAM(opt, neg, tinygram) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_TINYGRAM) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETCHAR(cichar, p); \
	/* Check rejected value. */ \
	if (cichar != tinygram) \
	    goto bad; \
	try.neg = 0; \
    }

#define REJCILANID(opt, neg, lanid) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_LANID) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETCHAR(cichar, p); \
	/* Check rejected value. */ \
	if (cichar != lanid) \
	    goto bad; \
	try.neg = 0; \
    }

#define REJCIMACADDR(opt, neg, macaddr) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_MACADDR) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	/* Check rejected value. */ \
	GETCHAR(cichar, p); \
	if (macaddr[0] != cichar) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (macaddr[1] != cichar) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (macaddr[2] != cichar) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (macaddr[3] != cichar) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (macaddr[4] != cichar) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (macaddr[5] != cichar) \
	    goto bad; \
	try.neg = 0; \
    }

#define REJCISPANTREE(opt, neg, spantree) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_SPANTREE) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETCHAR(cichar, p); \
	/* Check rejected value. */ \
	if (cichar != spantree) \
	    goto bad; \
	try.neg = 0; \
    }

#define REJCIIEEE802TAG(opt, neg, ieee802tag) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_IEEE_802_TAGGED_FRAME) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETCHAR(cichar, p); \
	/* Check rejected value. */ \
	if (cichar != ieee802tag) \
	    goto bad; \
	try.neg = 0; \
    }

#define REJCIMGMTINLINE(opt, neg) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_MGMT_INLINE) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	try.neg = 0; \
    }

#define REJCIBPDUINDICATOR(opt, neg) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_BPDU_INDICATOR) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	try.neg = 0; \
    }

    REJCIBRIDGELINEID(CI_BRIDGE_IDENTIFICATION, neg_bridgeid, go->lan_bridge_segno);

    REJCIBRIDGELINEID(CI_LINE_IDENTIFICATION, neg_lineid, go->lan_bridge_segno);

    REJCIMACSUPPORT(CI_MAC_SUPPORT, neg_macsupport, go->macsupport[0]);
    REJCIMACSUPPORT(CI_MAC_SUPPORT, neg_macsupport, go->macsupport[1]);
    REJCIMACSUPPORT(CI_MAC_SUPPORT, neg_macsupport, go->macsupport[2]);
    REJCIMACSUPPORT(CI_MAC_SUPPORT, neg_macsupport, go->macsupport[3]);
    REJCIMACSUPPORT(CI_MAC_SUPPORT, neg_macsupport, go->macsupport[4]);
    REJCIMACSUPPORT(CI_MAC_SUPPORT, neg_macsupport, go->macsupport[5]);

    REJCITINYGRAM(CI_TINYGRAM_COMPRESSION, neg_tinygram, go->tinygram);

    REJCILANID(CI_LAN_IDENTIFICATION, neg_lanid, go->lanid);

    REJCIMACADDR(CI_MAC_ADDRESS, neg_macaddr, go->macaddr);

    REJCISPANTREE(CI_SPANNING_TREE_PROTOCOL, neg_spantree, go->spantree);

    REJCIIEEE802TAG(CI_IEEE_802_TAGGED_FRAME, neg_ieee802tag, go->ieee802tag);

    REJCIMGMTINLINE(CI_MANAGEMENT_INLINE, neg_mgmt_inline);

    REJCIBPDUINDICATOR(CI_MANAGEMENT_INLINE, neg_bpdu_indicator);

    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len != 0)
	goto bad;

    /* If Management-Inline was rejected, try to negotiate a
     * Spanning-Tree-Protocol instead. */
    if (go->neg_mgmt_inline && !try.neg_mgmt_inline)
	try.neg_spantree = 1;

    /*
     * Now we can update state.
     */
    if (f->state != OPENED)
	*go = try;
    return 1;

bad:
    BCPDEBUG(("bcp_rejci: received bad Reject!"));
    return 0;
}


/*
 * bcp_reqci - Check the peer's requested CIs and send appropriate response.
 * Callback from fsm_rconfreq, Receive Configure Request
 *
 * Returns: CONFACK, CONFNAK or CONFREJ and input packet modified
 * appropriately.  If reject_if_disagree is non-zero, doesn't return
 * CONFNAK; returns CONFREJ if it can't return CONFACK.
 */
static int
bcp_reqci(f, inp, len, reject_if_disagree)
    fsm *f;
    u_char *inp;		/* Requested CIs */
    int *len;			/* Length of requested CIs */
    int reject_if_disagree;
{
    bcp_options *ho = &bcp_hisoptions[f->unit];
    bcp_options *ao = &bcp_allowoptions[f->unit];
    bcp_options *go = &bcp_gotoptions[f->unit];
    u_char *cip, *next;		/* Pointer to current and next CIs */
    u_short cilen, citype;	/* Parsed len, type */
    u_short cishort;		/* Parsed short value */
    u_char cichar;		/* Parsed char value */
    u_char bridge_id;
    u_short lan_segno;
    int mac;
    u_char macaddr[ETH_ALEN];

    int rc = CONFACK;		/* Final packet return code */
    int orc;			/* Individual option return code */
    u_char *p;			/* Pointer to next char to parse */
    u_char *ucp = inp;		/* Pointer to current output char */
    int l = *len;		/* Length left */

    /*
     * Reset all his options.
     */
    BZERO(ho, sizeof(*ho));

    /*
     * Process all his options.
     */
    next = inp;
    while (l) {
	orc = CONFACK;			/* Assume success */
	cip = p = next;			/* Remember begining of CI */
	if (l < 2 ||			/* Not enough data for CI header or */
	    p[1] < 2 ||			/*  CI length too small or */
	    p[1] > l) {			/*  CI length too big? */
	    BCPDEBUG(("bcp_reqci: bad CI length!"));
	    orc = CONFREJ;		/* Reject bad CI */
	    cilen = l;			/* Reject till end of packet */
	    l = 0;			/* Don't loop again */
	    goto endswitch;
	}
	GETCHAR(citype, p);		/* Parse CI type */
	GETCHAR(cilen, p);		/* Parse CI length */
	l -= cilen;			/* Adjust remaining length */
	next += cilen;			/* Step to next CI */

	switch (citype) {		/* Check CI type */
  case CI_BRIDGE_IDENTIFICATION:
     if (!ao->neg_bridgeid ||		/* Allow option? */
         cilen != CILEN_BRIDGELINEID)   /* Check CI length */
     {
       orc = CONFREJ;		/* Reject CI */
       break;
     }
     GETSHORT(cishort, p); /* Parse lan_bridge_segno */

     /*
      * If we are also negotiating bridgeid, then
      * both sides must agree on the higher bridgeid.
      */
     bridge_id = cishort & 0xf;
     if ((bridge_id < (go->lan_bridge_segno & 0xf)) &&
         go->neg_bridgeid)
     {
       orc = CONFNAK;		/* Nak CI */
       DECPTR(sizeof(u_short), p);
       PUTSHORT(go->lan_bridge_segno, p);	/* Give him a hint */
       break;
     }
     ho->neg_bridgeid = 1;		/* Remember he negotiated this item */
     ho->lan_bridge_segno = cishort;		/* And remember value */
     break;

  case CI_LINE_IDENTIFICATION:
     if (!ao->neg_lineid ||		/* Allow option? */
         cilen != CILEN_BRIDGELINEID)   /* Check CI length */
     {
       orc = CONFREJ;		/* Reject CI */
       break;
     }
     GETSHORT(cishort, p); /* Parse lan_bridge_segno */

     /*
      * If we are also negotiating lineid, then
      * both sides must agree on the higher lan_segno.
      */
     lan_segno = cishort >> 4;
     if ((lan_segno < (go->lan_bridge_segno >> 4)) &&
         go->neg_lineid)
     {
       orc = CONFNAK;		/* Nak CI */
       DECPTR(sizeof(u_short), p);
       PUTSHORT(go->lan_bridge_segno, p);	/* Give him a hint */
       break;
     }
     ho->neg_lineid = 1;		/* Remember he negotiated this item */
     ho->lan_bridge_segno = cishort;		/* And remember value */
     break;

   case CI_MAC_SUPPORT:
     if (!ao->neg_macsupport ||		/* Allow option? */
         cilen != CILEN_MACSUPPORT)   /* Check CI length */
     {
       orc = CONFREJ;		/* Reject CI */
       break;
     }
     GETCHAR(cichar, p); /* Parse macsupport value */

     /*
      * Ensure a valid value, else reject.
      */
     switch (cichar)
     {
     case MAC_IEEE_802_3:
     case MAC_IEEE_802_4:
     case MAC_IEEE_802_5_NON:
     case MAC_FDDI_NON:
     case MAC_IEEE_802_5:
     case MAC_FDDI:
       ho->neg_macsupport = 1;
       for (mac=0; mac<ETH_ALEN; mac++)
       {
         if (ho->macsupport[mac] == 0)
           ho->macsupport[mac] = cichar;
       }
       break;
     default:
       orc = CONFREJ;		/* Reject CI */
       break;
     }
     break;

   case CI_TINYGRAM_COMPRESSION:
     if (!ao->neg_tinygram ||		/* Allow option? */
         cilen != CILEN_TINYGRAM)   /* Check CI length */
     {
       orc = CONFREJ;		/* Reject CI */
       break;
     }
     GETCHAR(cichar, p); /* Parse tinygram value */

     /*
      * Ensure a valid value, else reject.
      */
     if (cichar != 1 && cichar !=2)
     {
       orc = CONFREJ;  /* Reject CI */
       break;
     }
     ho->neg_tinygram = 1;		/* Remember he negotiated this item */
     ho->tinygram = cichar;		/* And remember value */
     break;

   case CI_LAN_IDENTIFICATION:
     if (!ao->neg_lanid ||		/* Allow option? */
         cilen != CILEN_LANID)   /* Check CI length */
     {
       orc = CONFREJ;		/* Reject CI */
       break;
     }
     GETCHAR(cichar, p); /* Parse lanid value */

     /*
      * Ensure a valid value, else reject.
      */
     if (cichar != 1 && cichar !=2)
     {
       orc = CONFREJ;  /* Reject CI */
       break;
     }
     ho->neg_lanid = 1;		/* Remember he negotiated this item */
     ho->lanid = cichar;		/* And remember value */
     break;


   case CI_MAC_ADDRESS:
     if (!ao->neg_macaddr ||		/* Allow option? */
         cilen != CILEN_MACADDR)   /* Check CI length */
     {
       orc = CONFREJ;		/* Reject CI */
       break;
     }
     GETCHAR(macaddr[0], p); /* Parse macaddr value */
     GETCHAR(macaddr[1], p);
     GETCHAR(macaddr[2], p);
     GETCHAR(macaddr[3], p);
     GETCHAR(macaddr[4], p);
     GETCHAR(macaddr[5], p);

    /*
     * Reject macaddr if it has the multicast bit set or
     * if it is all zeroes.  (All zeroes is a request for
     * us to assign a MAC address, which we could do, but...)
     */
     for (mac=0; mac<ETH_ALEN; mac++)
     {
       if (go->macaddr[mac] != 0)
         break;
     }
     if ((mac == ETH_ALEN) || (macaddr[0] & BCP_MULTICAST))
     {
       orc = CONFREJ;		/* Reject CI */
       break;
     }

     ho->neg_macaddr = 1;		/* Remember he negotiated this item */
     ho->macaddr[0] = macaddr[0];		/* And remember value */
     ho->macaddr[1] = macaddr[1];
     ho->macaddr[2] = macaddr[2];
     ho->macaddr[3] = macaddr[3];
     ho->macaddr[4] = macaddr[4];
     ho->macaddr[5] = macaddr[5];
     break;

   case CI_SPANNING_TREE_PROTOCOL:
     if (!ao->neg_spantree ||		/* Allow option? */
         cilen != CILEN_SPANTREE)   /* Check CI length */
     {
       orc = CONFREJ;		/* Reject CI */
       break;
     }
     GETCHAR(cichar, p); /* Parse spantree value */

     /*
      * Ensure a valid value, else reject.
      */
     if (cichar > MAX_SPANTREE)
     {
       orc = CONFREJ;  /* Reject CI */
       break;
     }

     if ((cichar > go->spantree) && go->neg_spantree)
     {
       orc = CONFNAK;		/* Nak CI */
       DECPTR(sizeof(u_short), p);
       PUTSHORT(go->spantree, p);	/* Give him a hint */
       break;
     }

     ho->neg_spantree = 1;		/* Remember he negotiated this item */
     ho->spantree = cichar;		/* And remember value */
     break;

   case CI_IEEE_802_TAGGED_FRAME:
     if (!ao->neg_ieee802tag ||		/* Allow option? */
         cilen != CILEN_IEEE_802_TAGGED_FRAME)	/* Check CI length */
     {
       orc = CONFREJ;		/* Reject CI */
       break;
     }
     GETCHAR(cichar, p); /* Parse IEEE 802 tagged frame vale */

     /*
      * Ensure a valid value, else reject.
      */
     if (cichar != 1 && cichar !=2)
     {
       orc = CONFREJ;  /* Reject CI */
       break;
     }
     ho->neg_ieee802tag = 1;		/* Remember he negotiated this item */
     ho->ieee802tag = cichar;		/* And remember value */
     break;

   case CI_MANAGEMENT_INLINE:
     if (!ao->neg_mgmt_inline ||	/* Allow option? */
         cilen != CILEN_MGMT_INLINE)	/* Check CI length */
     {
       orc = CONFREJ;		/* Reject CI */
       break;
     }

     ho->neg_mgmt_inline = 1;		/* Remember he negotiated this item */
     break;

   case CI_BPDU_INDICATOR:
     if (!ao->neg_bpdu_indicator ||	/* Allow option? */
         cilen != CILEN_BPDU_INDICATOR)	/* Check CI length */
     {
       orc = CONFREJ;		/* Reject CI */
       break;
     }

     ho->neg_bpdu_indicator = 1;		/* Remember he negotiated this item */
     break;

	default:
	    orc = CONFREJ;
	    break;
	}
endswitch:
	if (orc == CONFACK &&		/* Good CI */
	    rc != CONFACK)		/*  but prior CI wasnt? */
	    continue;			/* Don't send this one */

	if (orc == CONFNAK) {		/* Nak this CI? */
	    if (reject_if_disagree)	/* Getting fed up with sending NAKs? */
		orc = CONFREJ;		/* Get tough if so */
	    else {
		if (rc == CONFREJ)	/* Rejecting prior CI? */
		    continue;		/* Don't send this one */
		if (rc == CONFACK) {	/* Ack'd all prior CIs? */
		    rc = CONFNAK;	/* Not anymore... */
		    ucp = inp;		/* Backup */
		}
	    }
	}

	if (orc == CONFREJ &&		/* Reject this CI */
	    rc != CONFREJ) {		/*  but no prior ones? */
	    rc = CONFREJ;
	    ucp = inp;			/* Backup */
	}

	/* Need to move CI? */
	if (ucp != cip)
	    BCOPY(cip, ucp, cilen);	/* Move it */

	/* Update output pointer */
	INCPTR(cilen, ucp);
    }

    *len = ucp - inp;			/* Compute output length */
    BCPDEBUG(("bcp: returning Configure-%s", CODENAME(rc)));
    return (rc);			/* Return final code */
}


/*
 * bcp_up - BCP has come UP.
 *
 * Configure the IP network interface appropriately and bring it up.
 */
static void
bcp_up(f)
    fsm *f;
{
    bcp_options *go = &bcp_gotoptions[f->unit];
    bcp_options *ho = &bcp_hisoptions[f->unit];
    bcp_options *wo = &bcp_wantoptions[f->unit];
    u_char *maclocal;
    char buf[32];

    BCPDEBUG(("bcp: up"));

    /* Choose which MAC address (if any) to assign to the interface. */
    if (go->neg_macaddr)
	maclocal = go->macaddr;
    else if (wo->neg_macaddr) {
	int i;

	/* If the wanted MAC address is all zeroes, it means we wanted the peer
	 * to assign us one.  Since we didn't get one, there's a problem. */
	for (i = 0; i < ETH_ALEN; ++i) {
	    if (wo->macaddr[i] != 0)
		break;
	}

	if (i == ETH_ALEN) {
	    error("Could not determine local MAC address");
	    bcp_close(f->unit, "Could not determine local MAC address");
	    return;
	}

	maclocal = wo->macaddr;
    } else
	maclocal = NULL;

    /* If Management-Inline is not supported, tell the kernel to encapsulate
     * bridge PDUs in the old RFC 1638 format. */
    if (!go->neg_mgmt_inline)
    {
        sifnpmode(ifunit, PPP_BPDU_IEEE, NPMODE_PASS);
    }

    /* After this the "bcp%d" device will exist. */
    sifnpmode(ifunit, PPP_BRIDGE, NPMODE_PASS);

    if (maclocal) {
        char eth_ifname[16];
        slprintf(eth_ifname, sizeof(eth_ifname), "bcp%d", ifunit);
	if (set_if_hwaddr(maclocal, eth_ifname) < 0) {
	    if (debug)
		warn("Failed to set hardware address");
	    bcp_close(f->unit, "Interface configuration failed");
	    return;
        }
    }

    /*
     * Set up /etc/ppp/eth-up environment.
     */

    /* MACLOCAL contains the MAC address of the BCP interface, if it has one.
     * Otherwise, MACLOCAL is defined as an empty string. */
    if (maclocal)
	slprintmac(buf, sizeof(buf), maclocal);
    else
	buf[0] = '\0';
    script_setenv("MACLOCAL", buf, 0);

    /* MACREMOTE contains the MAC address of the peer's BCP interface, if the
     * peer published it during negotiation.  Otherwise, MACREMOTE is defined
     * as an empty string. */
    if (ho->neg_macaddr)
	slprintmac(buf, sizeof(buf), ho->macaddr);
    else
	buf[0] = '\0';
    script_setenv("MACREMOTE", buf, 0);

    np_up(f->unit, PPP_BRIDGE);
    bcp_is_up = 1;

    /*
     * Execute the eth-up script, like this:
     *	/etc/ppp/eth-up interface tty speed local-IP remote-IP
     */
    if (bcp_script_state == s_down && bcp_script_pid == 0) {
	bcp_script_state = s_up;
	bcp_script(f, _PATH_ETHUP);
    }
}


/*
 * bcp_down - BCP has gone DOWN.
 *
 */
static void
bcp_down(f)
    fsm *f;
{
    /* Execute the eth-down script */
    if (bcp_script_state == s_up && bcp_script_pid == 0) {
	bcp_script_state = s_down;
	bcp_script(f, _PATH_ETHDOWN);
    }

    /* After this the "bcp%d" device will not exist. */
    sifnpmode(ifunit, PPP_BRIDGE, NPMODE_DROP);

    sifnpmode(ifunit, PPP_BPDU_IEEE, NPMODE_DROP);

    BCPDEBUG(("bcp: down"));
    if (bcp_is_up)
    {
      bcp_is_up = 0;
      np_down(f->unit, PPP_BRIDGE);
    }
}


/*
 * bcp_start - called when we want the lower layer up.
 */
static void
bcp_start(f)
    fsm *f;
{
    np_start(f->unit, PPP_BRIDGE);
}


/*
 * bcp_finished - possibly shut down the lower layers.
 */
static void
bcp_finished(f)
    fsm *f;
{
    np_finished(f->unit, PPP_BRIDGE);
}


/*
 * bcp_script_done - called when the ip-up or ip-down script
 * has finished.
 */
static void
bcp_script_done(vp_f)
    void *vp_f;
{
    fsm *f = vp_f;

    bcp_script_pid = 0;
    switch (bcp_script_state) {
    case s_up:
	if (f->state != OPENED) {
	    bcp_script_state = s_down;
	    bcp_script(f, _PATH_ETHDOWN);
	}
	break;
    case s_down:
	if (f->state == OPENED) {
	    bcp_script_state = s_up;
	    bcp_script(f, _PATH_ETHUP);
	}
	break;
    }
}


/*
 * bcp_script - Execute a script with arguments
 * bcp-interface-name phys-interface-name
 */
static void
bcp_script(f, script)
    fsm *f;
    char *script;
{
    char eth_ifname[16];
    char *argv[8];

    slprintf(eth_ifname, sizeof(eth_ifname), "bcp%d", ifunit);

    argv[0] = script;
    argv[1] = eth_ifname;	/* bridge device (e.g. bcp<N>) */
    argv[2] = devnam;		/* physical device (e.g. hdlc<N>) */
    argv[3] = NULL;
    argv[4] = NULL;
    argv[5] = NULL;
    argv[6] = NULL;
    argv[7] = NULL;
    bcp_script_pid = run_program(script, argv, 0, bcp_script_done, f, 0);
}

/*
 * bcp_printpkt - print the contents of an BCP packet.
 */
static char *bcp_codenames[] = {
    "ConfReq", "ConfAck", "ConfNak", "ConfRej",
    "TermReq", "TermAck", "CodeRej"
};

static int
bcp_printpkt(p, plen, printer, arg)
    u_char *p;
    int plen;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
    int code, id, len, olen;
    u_char *pstart, *optend;
    u_short cishort;

    if (plen < HEADERLEN)
	return 0;
    pstart = p;
    GETCHAR(code, p);
    GETCHAR(id, p);
    GETSHORT(len, p);
    if (len < HEADERLEN || len > plen)
	return 0;

    if (code >= 1 && code <= sizeof(bcp_codenames) / sizeof(char *))
	printer(arg, " %s", bcp_codenames[code-1]);
    else
	printer(arg, " code=0x%x", code);
    printer(arg, " id=0x%x", id);
    len -= HEADERLEN;
    switch (code) {
    case CONFREQ:
    case CONFACK:
    case CONFNAK:
    case CONFREJ:
	/* print option list */
	while (len >= 2) {
	    GETCHAR(code, p);
	    GETCHAR(olen, p);
	    p -= 2;
	    if (olen < 2 || olen > len) {
		break;
	    }
	    printer(arg, " <");
	    len -= olen;
	    optend = p + olen;
	    switch (code) {
            case CI_BRIDGE_IDENTIFICATION:
                if (olen == CILEN_BRIDGELINEID) {
                    p += 2;
                    GETSHORT(cishort, p);
                    printer(arg, "Bridge-ID LAN=0x%03x Bridge=0x%X",
                            (cishort & 0xFFF0) >> 4,
                            cishort & 0x000F);
                }
                break;

            case CI_LINE_IDENTIFICATION:
                if (olen == CILEN_BRIDGELINEID) {
                    p += 2;
                    GETSHORT(cishort, p);
                    printer(arg, "Line-ID LAN=0x%03x Bridge=0x%X",
                            (cishort & 0xFFF0) >> 4,
                            cishort & 0x000F);
                }
                break;

            case CI_MAC_SUPPORT:
                if (olen == CILEN_MACSUPPORT) {
                    p += 2;
                    printer(arg, "MAC-Support");
                }
                break;

            case CI_TINYGRAM_COMPRESSION:
                if (olen == CILEN_TINYGRAM) {
                    p += 2;
                    printer(arg, "Tinygram-Compression");
                }
                break;

            case CI_LAN_IDENTIFICATION:
                if (olen == CILEN_LANID) {
                    p += 2;
                    printer(arg, "LAN-ID (obsolete)");
                }
                break;

            case CI_MAC_ADDRESS:
                if (olen == CILEN_MACADDR) {
                    p += 2;
                    printer(arg, "MAC-Address");
                }
                break;

            case CI_SPANNING_TREE_PROTOCOL:
                if (olen >= CILEN_SPANTREE) {
                    p += 2;
                    printer(arg, "Spanning-Tree-Protocol (old format)");
                }
                break;

            case CI_IEEE_802_TAGGED_FRAME:
                if (olen == CILEN_IEEE_802_TAGGED_FRAME) {
                    p += 2;
                    printer(arg, "IEEE-802-Tagged-Frame");
                }
                break;

            case CI_MANAGEMENT_INLINE:
                if (olen == CILEN_VOID) {
                    p += 2;
                    printer(arg, "Management-Inline");
                }
                break;

            case CI_BPDU_INDICATOR:
                if (olen == CILEN_VOID) {
                    p += 2;
                    printer(arg, "Bridge-Control-Packet-Indicator");
                }
                break;

            default:
		break;
	    }
	    while (p < optend) {
		GETCHAR(code, p);
		printer(arg, " %.2x", code);
	    }
	    printer(arg, ">");
	}
	break;

    case TERMACK:
    case TERMREQ:
	if (len > 0 && *p >= ' ' && *p < 0x7f) {
	    printer(arg, " ");
	    print_string((char *)p, len, printer, arg);
	    p += len;
	    len = 0;
	}
	break;
    }

    /* print the rest of the bytes in the packet */
    for (; len > 0; --len) {
	GETCHAR(code, p);
	printer(arg, " %.2x", code);
    }

    return p - pstart;
}

/*
 * bcp_check_options - check that any IP-related options are OK,
 * and assign appropriate defaults.
 */
static void
bcp_check_options()
{
}


