    /*
    * Copyright c                  Realtek Semiconductor Corporation, 2007
    * All rights reserved.
    *
    * Program : RTL8363 switch low-level API
    * Abstract :
    * Author : Robin Zheng-bei Xing
    *  $Id: rtl8363_asicdrv.c,v 1.53 2008-05-07 11:41:41 robin_xing Exp $
    */

/*	@doc RTL8363_ASICDRV_API

	@module Rtl8363_AsicDrv.c - RTL8213M/RTL8203M/RTL8363H/RTL8303H/RTL8363S Switch asic driver API documentation	|
	This document explains API interface of the asic.
	@normal robin_xing <date>

	Copyright <cp>2008 Realtek<tm> Semiconductor Cooperation, All Rights Reserved.

	@head3 List of Symbols |
	Here is a list of all functions and variables in this module.

	@index | RTL8363_ASICDRIVER_API
*/


#if defined(V54_BSP)
#include "linux/types.h"
#include "rtl_types.h"
#include "rtl8363_asicdrv.h"
#include "rtl8363Phy.h"
#else
#include "rtl_types.h"
/*RTK_X86_ASICDRV is used to compile cle in X86 */
#ifndef RTK_X86_ASICDRV
#include "mdcmdio.h"
#endif
#include "rtl8363_asicdrv.h"
#endif



void mac2index(uint8 *macAddr, uint32 *pIndex)
{
    uint8 idx;
    uint8 i;

    idx = (macAddr[0] ^  macAddr[1]^macAddr[2]^macAddr[3]^macAddr[4]^macAddr[5]);
    *pIndex = 0 ;
    for ( i = 0; i < 8; i ++)
    {
        *pIndex |= (idx & 0x1) ? (1 << (7 - i)) :0;
        idx >>= 1;
        if (!idx)
            break;
    }

}

void ip2index(uint32 sip, uint32 dip, uint32 *pIndex)
{
    uint8 idx;
    uint8 ip[4];

    ip[0] = (uint8)((dip & 0x00FF0000) >> 16);
    ip[1] = (uint8)((dip & 0xFF00) >> 8 );
    ip[2] = (uint8)(dip & 0xFF);
    idx = ip[0] ^ ip[1] ^ ip[2];
    ip[0] = (uint8)((sip & 0xFF000000) >> 24);
    ip[1] = (uint8)((sip & 0x00FF0000) >> 16);
    ip[2] = (uint8)((sip & 0xFF00) >> 8);
    ip[3] = (uint8)(sip & 0xFF);
    idx ^= ip[0]^ip[1]^ip[2]^ip[3];
    *pIndex = idx;

}

/*
@func int8 | rtl8363_setAsicReg | Set Asic registers
@parm uint8 | phyad | PHY address (0~2).
@parm uint8 | regad |  Register address(0~31)
@parm uint8 | pagtype | Register Page Type
@parm uint8 | npage | Register Page Number
@parm uint16 | value | Write value to register
@rvalue SUCCESS | Success.
@rvalue FAILED | Failure.
@comm
There are four page type which are MACPAG, UTPPAG, SERLPAG, SERHPAG,
and each page type has several pages. This API use phy + register + page type + page numbr
register coding method.
*/

int8 rtl8363_setAsicReg(uint8 phyad, uint8 regad, uint8 pagtype,  uint8 npage, uint16 value)
{
    uint32 regval;

    #if (RTL8363_INF == RTL8363_INF_ISA)
    uint32 isa_page = 0; /*ISA interface Page number*/
    uint32 isa_regL  = 0; /*ISA interface 16bit-Low register address*/
    uint8 i = 0;
    uint8 type = 0;
    #elif (RTL8363_INF == RTL8363_INF_8051)
    uint16 isa_page = 0; /* Page number*/
    uint8 isa_regL  = 0; /* register address*/
    uint8 i = 0;
    uint8 type = 0;
    #endif


    if ( (phyad > 2) || (regad > 31) || (pagtype > SERHPAG) || (npage > 15) )
        return FAILED;
    regval = 0;

    #if (RTL8363_INF == RTL8363_INF_SMI)
    {

	       /*regad is 31*/
		if (regad == 31)
		{

#ifndef RTK_X86_ASICDRV
                    smiWrite(phyad, regad, value & 0xFFFF);
#else
                    w_phy(8, phyad, regad, value & 0xFFFF);
#endif
			return SUCCESS;
		}

            /*Page Select*/
            if (phyad == 0 || phyad == 1)
            {
               regval = (pagtype << 4) |npage;
#ifndef  RTK_X86_ASICDRV
               smiWrite(phyad, 31, regval);
#else
               w_phy(8, phyad, 31, regval);
#endif
            }
            else if ((phyad == 2) && (pagtype == MACPAG) && (npage < 8) )
            {

#ifndef  RTK_X86_ASICDRV
                smiRead(phyad, 31, &regval);
#else
                regval = r_phy(8, phyad, 31);
#endif
                regval &= 0xFFF8;
                regval |= npage;

#ifndef  RTK_X86_ASICDRV
                smiWrite(phyad, 31, regval);
#else
                w_phy(8, phyad, 31, regval);
#endif
            }
            else
            {
                return FAILED;
            }

            /*Write value to the register*/
#ifndef  RTK_X86_ASICDRV
            smiWrite(phyad, regad, value & 0xFFFF);
#else
            w_phy(8, phyad, regad, value & 0xFFFF);
#endif
    }


    #elif ((RTL8363_INF == RTL8363_INF_ISA) || (RTL8363_INF == RTL8363_INF_8051))
    {

            /*first map SMI page &reg. to ISA page reg.*/
            if ((!pagtype ) &&(regad <= 11))
            {
                isa_page = phyad * 2;
                isa_regL = 4 + regad * 2;
            }
            else if ((!pagtype) &&(regad >= 12) && (regad <= 15))
            {
                isa_page = phyad *2 + 1;
                isa_regL = 4 + (regad - 12) *2;
            }
            else if (regad == 31)
            {
                isa_page = 6;
                isa_regL = 4 + phyad*2;
            }
            else if ((!pagtype) && (regad == 16))
            {
                isa_page = 7;
                isa_regL = 4 + phyad*2;
            }
            else if ((!pagtype) && (regad >=17) && (regad <=30))
            {
                isa_page = 8 + phyad * 16 + npage;
                isa_regL = 4 + (regad -17) *2;
            }
	     else if (pagtype && (phyad != 2))
	     {
	          isa_page = 41;
	          isa_regL = 28;
		   if (pagtype == UTPPAG)
			type = 0;
		   else if (pagtype == SERLPAG)
			type = 1;
		   else if (pagtype == SERHPAG)
			type = 2;
		   else
			return FAILED;
	     }
            else
            {
                return FAILED;
            }

        /*second write value through ISA interface*/
        #if ((RTL8363_BUSWI_ISA == RTL8363_32BITS_ISA) && (RTL8363_INF != RTL8363_INF_8051))
        {
	     if ((pagtype) && (isa_page == 41) && (isa_regL == 28))
	     {

		  WRITE_ISA32(0, isa_page);
		  regval = READ_ISA32(isa_regL);
		  while ((regval >>31) & 0x1)
		  {
		     regval = READ_ISA32(isa_regL);
		     i++;
		     if (i>=100)
		       return FAILED;
		  }

		  regval = (1<<31) |((phyad & 0x01)<<30) |((type & 0x03)<<28) |((npage & 0xf)<<24)|((regad & 0x1f)<<19) |(1<<18) |value;
		  WRITE_ISA32(isa_regL, regval);

		  regval = READ_ISA32(isa_regL);
		  while ((regval >>31) & 0x1)
		  {
		     regval = READ_ISA32(isa_regL);
		     i++;
		     if (i>=100)
		       return FAILED;
		  }
	     }
	     else
	     {

            WRITE_ISA32(0, isa_page); /*Select ISA page*/
            regval = READ_ISA32(isa_regL & (~0x3)); /*32bit alignment address*/
            if (isa_regL % 4)
            {
                regval &= 0xFFFF;
                regval |= (value & 0xFFFF) << 16;
            }
            else
            {
                regval &= 0xFFFF0000;
                regval |= (value & 0xFFFF);
            }
            WRITE_ISA32(isa_regL & (~0x3), regval);
        }
         }
        #elif ((RTL8363_BUSWI_ISA == RTL8363_16BITS_ISA) && (RTL8363_INF != RTL8363_INF_8051))
        {
		uint16 regval16;
		if ((pagtype) && (isa_page == 41) && (isa_regL == 28))
            {
                WRITE_ISA16(0, isa_page);
		  regval16 = READ_ISA16(isa_regL+2);
		  while ((regval16 >>15) & 0x1)
		  {
		     regval16 = READ_ISA16(isa_regL+2);
		     i++;
		     if (i>=100)
		       return FAILED;
		  }
		  WRITE_ISA16(isa_regL, value);
		  regval16 = (1<<15) |((phyad & 0x01)<<14) |((type & 0x03)<<12) |((npage & 0xf)<<8)|((regad & 0x1f)<<3) |(1<<2);
		  WRITE_ISA16(30, regval16);

		  regval16 = READ_ISA16(isa_regL+2);
		  while ((regval16 >>15) & 0x1)
		  {
		     regval16 = READ_ISA16(isa_regL+2);
		     i++;
		     if (i>=100)
		       return FAILED;
		  }
            }
	     else
	    {
            WRITE_ISA16(0, isa_page); /*Select ISA page*/
            WRITE_ISA16(isa_regL, (uint16)(value & 0xFFFF));
        }
        }
        #elif ((RTL8363_BUSWI_ISA == RTL8363_8BITS_ISA) && (RTL8363_INF != RTL8363_INF_8051))
        {
	     uint8 regval8;
	     if ((pagtype) && (isa_page == 41)&&(isa_regL == 28))
	     {
	         WRITE_ISA8(0, isa_page);
	         regval8 = READ_ISA8(31);
		  while ((regval8 >> 7) & 0x1)
		  {
		      regval8 = READ_ISA8(31);
		      i++;
		      if (i>=100)
		          return FAILED;
		  }
		  WRITE_ISA8(28, (uint8)(value & 0x00FF));
		  WRITE_ISA8(29, (uint8)((value & 0xFF00)>>8));
		  regval8 = ((regad & 0x1f)<<3) |(1<<2);
		  WRITE_ISA8(30, regval8);
		  regval8 = (1<<7) |((phyad & 0x01)<<6) |((type & 0x03)<<4) |(npage & 0xf);
		  WRITE_ISA8(31, regval8);
		   regval8 = READ_ISA8(31);
		  while ((regval8 >> 7) & 0x1)
		  {
		      regval8 = READ_ISA8(31);
		      i++;
		      if (i>=100)
		          return FAILED;
		  }
	     }
	     else
	     {
            WRITE_ISA8(0, isa_page); /*Select ISA page*/
            WRITE_ISA8(isa_regL, (uint8)(value & 0xFF));
            WRITE_ISA8(isa_regL + 1, (uint8)((value & 0xFF00) >> 8));
        }
        }
        #endif
        #if (RTL8363_INF == RTL8363_INF_8051)
        {
            uint16 regValue;
            if (!pagtype)
            {
               setReg16(isa_page, isa_regL, value);
               return SUCCESS;
            }else if (pagtype == UTPPAG)
            {
               regValue = 0x8004|((phyad&1)<<14)|((npage&0xF)<<8)|((regad&0x1F)<<3);
            }else if (pagtype == SERLPAG)
            {
               regValue = 0x9004|((phyad&1)<<14)|((npage&0xF)<<8)|((regad&0x1F)<<3);
            }
            else if (pagtype == SERHPAG)
            {
               regValue = 0xA004|((phyad&1)<<14)|((npage&0xF)<<8)|((regad&0x1F)<<3);
            }
            setReg16(41, 28, value);
            setReg(41, 30, regValue);
            setReg(41, 31, regValue>>8);
            for(i = 0; i < 200; i++)
            {
               isa_regL = getReg(41, 31);
               if(!(isa_regL&0x80))
                  return SUCCESS;
            }
            return FAILED;
        }
	#endif
    }
    #endif

    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicReg | Set Asic registers
@parm uint8 | phyad | PHY address (0~2).
@parm uint8 | regad |  Register address(0~31)
@parm uint8 | pagtype | Register Page Type
@parm uint8 | npage | Register Page Number
@parm uint16* | pvalue |  Register value to register
@rvalue SUCCESS | Success.
@rvalue FAILED | Failure.
@comm
There are four page type which are MACPAG, UTPPAG, SERLPAG, SERHPAG,
and each page type has several pages. This API use phy + register + page type + page numbr
register coding method.
*/

int8 rtl8363_getAsicReg(uint8 phyad, uint8 regad, uint8 pagtype,  uint8 npage, uint16 *pvalue)
{
    uint32 regval;

    #if (RTL8363_INF == RTL8363_INF_ISA)
    uint32 isa_page = 0; /*ISA interface Page number*/
    uint32 isa_regL  = 0; /*ISA interface 16bit-Low register address*/
    uint8 i = 0;
    uint8 type = 0;
    #elif (RTL8363_INF == RTL8363_INF_8051)
    uint16 isa_page = 0; /* Page number*/
    uint8 isa_regL  = 0; /* register address*/
    uint8 i = 0;
    uint8 type = 0;
    #endif

    regval = 0;
    if ( (phyad > 2) || (regad > 31) || (pagtype > SERHPAG) ||
        (npage > 15) || (pvalue == NULL))
        return FAILED;

    #if (RTL8363_INF == RTL8363_INF_SMI)
    {

        /*regad is 31*/
        if (regad == 31)
        {

#ifndef RTK_X86_ASICDRV
             smiRead(phyad, regad, &regval);
#else
             regval = r_phy(8,phyad, regad);
#endif
             *pvalue = (uint16)regval;
		return SUCCESS;
	 }

	  /*Page Select*/
        if (phyad == 0 || phyad == 1)
        {
            regval = (pagtype << 4) |npage;

#ifndef RTK_X86_ASICDRV
            smiWrite(phyad, 31, regval);
#else
            w_phy(8, phyad, 31, regval);
#endif

        }
        else if ((phyad == 2) && (pagtype == MACPAG) && (npage < 8) )
        {

#ifndef RTK_X86_ASICDRV
            smiRead(phyad, 31, &regval);
#else
            regval = r_phy(8, phyad, 31);
#endif
            regval &= 0xFFF8;
            regval |= npage;

#ifndef RTK_X86_ASICDRV
            smiWrite(phyad, 31, regval);
#else
            w_phy(8, phyad, 31, regval);
#endif

        }
        else
        {
            return FAILED;
        }

        /*Read regiter valuer*/
#ifndef RTK_X86_ASICDRV
        smiRead(phyad, regad, &regval);
#else
        regval = r_phy(8,phyad, regad);
#endif
        *pvalue = (uint16)regval;

    }

    #elif ((RTL8363_INF == RTL8363_INF_ISA) || (RTL8363_INF == RTL8363_INF_8051))
    {
            /*first map SMI page &reg. to ISA page reg.*/
            if ((!pagtype ) &&(regad <= 11))
            {
                isa_page = phyad * 2;
                isa_regL = 4 + regad * 2;
            }
            else if ((!pagtype ) && (regad >= 12) && (regad <= 15))
            {
                isa_page = phyad *2 + 1;
                isa_regL = 4 + (regad - 12) *2;
            }
            else if (regad == 31)
            {
                isa_page = 6;
                isa_regL = 4 + phyad*2;
            }
            else if ((!pagtype) && (regad == 16))
            {
                isa_page = 7;
                isa_regL = 4 + phyad*2;
            }
            else if ((!pagtype) && (regad >=17) && (regad <=30))
            {
                isa_page = 8 + phyad * 16 + npage;
                isa_regL = 4 + (regad -17) *2;
            }
	     else if (pagtype && (phyad != 2))
	     {
	          isa_page = 41;
	          isa_regL = 28;
		   if (pagtype == UTPPAG)
			type = 0;
		   else if (pagtype == SERLPAG)
			type = 1;
		   else if (pagtype == SERHPAG)
			type = 2;
		   else
			return FAILED;
	     }
            else
            {
                return FAILED;
            }

            /*secondly read register value*/
        #if ((RTL8363_BUSWI_ISA == RTL8363_32BITS_ISA) &&(RTL8363_INF != RTL8363_INF_8051))
            {
	        if ((pagtype) && (isa_page == 41) && (isa_regL == 28))
	        {

		  WRITE_ISA32(0, isa_page);
		  regval = READ_ISA32(isa_regL);
		  while ((regval >>31) & 0x1)
		  {
		     regval = READ_ISA32(isa_regL);
		     i++;
		     if (i>=100)
		       return FAILED;
		  }

		  regval = (1<<31) |((phyad & 0x01)<<30) |((type & 0x03)<<28) |((npage & 0xf)<<24)|((regad & 0x1f)<<19) |(regval & 0x0000FFFF);
		  WRITE_ISA32(isa_regL, regval);

		  regval = READ_ISA32(isa_regL);
		  while ((regval >>31) & 0x1)
		  {
		     regval = READ_ISA32(isa_regL);
		     i++;
		     if (i>=100)
		       return FAILED;
		  }
		  regval = READ_ISA32(isa_regL);
		  *pvalue = (uint16) (regval & 0x0000FFFF);
	        }

		else
		{
                WRITE_ISA32(0, isa_page); /*Select ISA page*/
                regval = READ_ISA32(isa_regL & (~0x3));
                if (isa_regL % 4)
                {
                    *pvalue = (regval & 0xFFFF0000) >> 16;
                }
                else
                {
                    *pvalue = regval & 0xFFFF;
                }
            }
            }
        #elif ((RTL8363_BUSWI_ISA == RTL8363_16BITS_ISA)&&(RTL8363_INF != RTL8363_INF_8051))
            {

	        uint16 regval16;

		 if ((pagtype) && (isa_page == 41) && (isa_regL == 28))
            {
                WRITE_ISA16(0, isa_page);
		  regval16 = READ_ISA16(isa_regL+2);
		  while ((regval16 >>15) & 0x1)
		  {
		     regval16 = READ_ISA16(isa_regL+2);
		     i++;
		     if (i>=100)
		       return FAILED;
		  }
		  regval16 = (1<<15) |((phyad & 0x01)<<14) |((type & 0x03)<<12) |((npage & 0xf)<<8)|((regad & 0x1f)<<3);
		  WRITE_ISA16(30, regval16);

		  regval16 = READ_ISA16(isa_regL+2);
		  while ((regval16 >>15) & 0x1)
		  {
		     regval16 = READ_ISA16(isa_regL+2);
		     i++;
		     if (i>=100)
		       return FAILED;
		  }
		  *pvalue = READ_ISA16(isa_regL);
            }

                WRITE_ISA16(0, isa_page); /*Select ISA page*/
                *pvalue = (uint16)READ_ISA16(isa_regL);
            }
        #elif ((RTL8363_BUSWI_ISA == RTL8363_8BITS_ISA) && (RTL8363_INF != RTL8363_INF_8051))
            {
		   uint8 regval8;
		   if ((pagtype) && (isa_page == 41)&&(isa_regL == 28))
	          {
	              WRITE_ISA8(0, isa_page);
	              regval8 = READ_ISA8(31);
		       while ((regval8 >> 7) & 0x1)
		       {
		           regval8 = READ_ISA8(31);
		           i++;
		           if (i>=100)
		               return FAILED;
		       }

		       regval8 = ((regad & 0x1f)<<3);
		       WRITE_ISA8(30, regval8);
		       regval8 = (1<<7) |((phyad & 0x01)<<6) |((type & 0x03)<<4) |(npage & 0xf);
		       WRITE_ISA8(31, regval8);
		       regval8 = READ_ISA8(31);
		       while ((regval8 >> 7) & 0x1)
		       {
		           regval8 = READ_ISA8(31);
		           i++;
		           if (i>=100)
		               return FAILED;
		       }
			*pvalue = (uint16)READ_ISA8(isa_regL) + (((uint16)(READ_ISA8(isa_regL + 1))) << 8 );
	          }

		   else
		   {
                WRITE_ISA8(0, isa_page); /*Select ISA page*/
                    *pvalue = (uint16)READ_ISA8(isa_regL) + (((uint16)(READ_ISA8(isa_regL + 1))) << 8 );

		   }
            }
        #endif
        #if (RTL8363_INF == RTL8363_INF_8051)
        {
            uint16 regValue;
            if (!pagtype)
            {
               *pvalue = getReg16(isa_page, isa_regL);
               return SUCCESS;
            }else if (pagtype == UTPPAG)
            {
               regValue = 0x8000|((phyad&1)<<14)|((npage&0xF)<<8)|((regad&0x1F)<<3);
            }else if (pagtype == SERLPAG)
            {
               regValue = 0x9000|((phyad&1)<<14)|((npage&0xF)<<8)|((regad&0x1F)<<3);
            }
            else if (pagtype == SERHPAG)
            {
               regValue = 0xA000|((phyad&1)<<14)|((npage&0xF)<<8)|((regad&0x1F)<<3);
            }
            setReg(41, 30, regValue);
            setReg(41, 31, regValue>>8);
            for(i = 0; i < 200; i++)
            {
               isa_regL = getReg(41, 31);
               if(!(isa_regL&0x80))
                  break;
            }
            if (i == 200)
               return FAILED;
            *pvalue = getReg16(41, 28);
            return SUCCESS;
        }
	#endif
    }
    #endif

    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicRegBit | Set Asic register bit value
@parm uint8 | phyad | PHY address (0~2).
@parm uint8 | regad |  Register address(0~31)
@parm uint8 | bitpos | Register bit position(0~15)
@parm uint8 | pagtype | Register Page Type
@parm uint8 | npage | Register Page Number
@parm uint16 | value | Write value to register
@rvalue SUCCESS | Success.
@rvalue FAILED | Failure.
@comm
There are four page type which are MACPAG, UTPPAG, SERLPAG, SERHPAG,
and each page type has several pages. This API use phy + register + page type + page numbr
register coding method.
*/
int8 rtl8363_setAsicRegBit(uint8 phyad, uint8 regad, uint8 bitpos, uint8 pagtype, uint8 npage,  uint16 value)
{
	uint16 rdata;

	if (rtl8363_getAsicReg(phyad, regad,  pagtype, npage, &rdata) != SUCCESS)
		return FAILED;
	if (value)
		rtl8363_setAsicReg(phyad, regad, pagtype, npage, rdata | (1 << bitpos));
	else
		rtl8363_setAsicReg(phyad, regad, pagtype, npage, rdata & (~(1 << bitpos)));
	return SUCCESS;
}

/*
@func int8 | rtl8363_getAsicRegBit | Set Asic register bit value
@parm uint8 | phyad | PHY address (0~2).
@parm uint8 | regad |  Register address(0~31)
@parm uint8 | pagtype | Register Page Type
@parm uint8 | npage | Register Page Number
@parm uint16* | pvalue |  Register value to register
@rvalue SUCCESS | Success.
@rvalue FAILED | Failure.
@comm
There are four page type which are MACPAG, UTPPAG, SERLPAG, SERHPAG,
and each page type has several pages. This API use phy + register + page type + page numbr
register coding method.
*/

int8 rtl8363_getAsicRegBit(uint8 phyad, uint8 regad, uint8 bitpos, uint8 pagtype, uint8 npage,  uint16 * pvalue)
{
	uint16 rdata;

	if (rtl8363_getAsicReg(phyad, regad,  pagtype, npage, &rdata) != SUCCESS)
		return FAILED;
	if (rdata & (1 << bitpos))
		*pvalue =1;
	else
		*pvalue =0;

	return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicEthernetPHY | Set ethernet PHY registers for desired ability.
@parm uint8 | phy | PHY number (0~1).
@parm phyCfg_t | cfg | phy configuration
@struct phyCfg_t | This structure describe PHY parameter
@field uint8 | AutoNegotiation | AutoNegotiation ability
@field uint8 | Speed | force speed, could be PHY_10M, PHY_100M, PHY_1000M
@field uint8 | Fullduplex | full or half duplex
@field uint8 | Cap_10Half | 10BASE-TX half duplex capable
@field uint8 | Cap_10Full  | 10BASE-TX full duplex capable
@field uint8 | Cap_100Half | 100BASE-TX half duplex capable
@field uint8 | Cap_100Full | 100BASE-TX full duplex capable
@field uint8 | FC | flow control capability
@field uint8 | AsyFC | asymmetric flow control capability
@rvalue SUCCESS | Success.
@rvalue FAILED | Failure.
@comm
	If Full_1000 bit is set to 1, the AutoNegotiation,Full_100 and Full_10 will be automatic set to 1. While both AutoNegotiation and Full_1000 are set to
	0, the PHY speed and duplex selection will be set as following 100F > 100H > 10F > 10H priority sequence.
*/
int8 rtl8363_setAsicEthernetPHY(uint8 phy, phyCfg_t cfg)
{
	uint16 phyData;

	uint16 phyEnMsk0;
	uint16 phyEnMsk4;
	uint16 phyEnMsk9;

	if(phy > 1)
		return FAILED;

	phyEnMsk0 = 0;
	phyEnMsk4 = 0;
	phyEnMsk9 = 0;


      /*PHY speed, [0.6 0.13]:00= 10Mpbs, 01= 100Mpbs,10= 1000Mpbs,11=Reserved */
      if (cfg.Speed == PHY_10M)
      {
	    phyEnMsk0 = phyEnMsk0 & (~(1<<6));
	    phyEnMsk0 = phyEnMsk0 & (~(1<<13));

      }
      else if (cfg.Speed == PHY_100M)
      {
	    phyEnMsk0 = phyEnMsk0 & (~(1<<6));
	    phyEnMsk0 = phyEnMsk0 | (1<<13);
      }
      else if (cfg.Speed == PHY_1000M)
      {
	    phyEnMsk0 = phyEnMsk0 | (1<<6);
	    phyEnMsk0 = phyEnMsk0 & (~(1<<13));
      }

      /*duplex*/
      if (cfg.Fullduplex)
      {
	    /*Full duplex mode in reg 0.8*/
	    phyEnMsk0 = phyEnMsk0 | (1<<8);
      }

      /*Advertisement Ability*/
	if(cfg.Cap_10Half)
	{
		/*10BASE-TX half duplex capable in reg 4.5*/
		phyEnMsk4 = phyEnMsk4 | (1<<5);

	}

	if(cfg.Cap_10Full)
	{
		/*10BASE-TX full duplex capable in reg 4.6*/
		phyEnMsk4 = phyEnMsk4 | (1<<6);
	}

	if(cfg.Cap_100Half)
	{
		/*100BASE-TX half duplex capable in reg 4.7*/
		phyEnMsk4 = phyEnMsk4 | (1<<7);

	}

	if(cfg.Cap_100Full)
	{
		/*100BASE-TX full duplex capable in reg 4.8*/
		phyEnMsk4 = phyEnMsk4 | (1<<8);
	}

	if(cfg.Cap_1000Full)
	{
		/*1000 BASE-T FULL duplex capable setting in reg 9.9*/
		phyEnMsk9 = phyEnMsk9 | (1<<9);
		/*100BASE-TX full duplex capable in reg 4.8*/
		phyEnMsk4 = phyEnMsk4 | (1<<8);
		/*10BASE-TX full duplex capable in reg 4.6*/
		phyEnMsk4 = phyEnMsk4 | (1<<6);

		/*Force Auto-Negotiation setting in reg 0.12*/
		cfg.AutoNegotiation = 1;


	}

	if(cfg.AutoNegotiation)
	{
		/*Auto-Negotiation setting in reg 0.12*/
		phyEnMsk0 = phyEnMsk0 | (1<<12);
	}

	if(cfg.AsyFC)
	{
		/*Asymetric flow control in reg 4.11*/
		phyEnMsk4 = phyEnMsk4 | (1<<11);
	}
	if(cfg.FC)
	{
		/*Flow control in reg 4.10*/
		phyEnMsk4 = phyEnMsk4 | (1<<10);
	}


	/*1000 BASE-T control register setting*/
	if(SUCCESS != rtl8363_getAsicReg(phy,PHY_1000_BASET_CONTROL_REG, UTPPAG, 0, &phyData))
		return FAILED;

	phyData = (phyData & (~0x0200)) | phyEnMsk9 ;

	if(SUCCESS != rtl8363_setAsicReg(phy, PHY_1000_BASET_CONTROL_REG, UTPPAG, 0, phyData))
		return FAILED;


	/*Auto-Negotiation control register setting*/
	if(SUCCESS != rtl8363_getAsicReg(phy,PHY_AN_ADVERTISEMENT_REG, UTPPAG, 0, &phyData))
		return FAILED;

	phyData = (phyData & (~0x0DE0)) | phyEnMsk4;

	if(SUCCESS != rtl8363_setAsicReg(phy, PHY_AN_ADVERTISEMENT_REG, UTPPAG, 0, phyData))
		return FAILED;


	/*Control register setting and restart auto*/
	if(SUCCESS != rtl8363_getAsicReg(phy,PHY_CONTROL_REG, UTPPAG, 0, &phyData))
		return FAILED;

	phyData = (phyData & (~0x3140)) | phyEnMsk0;
	/*If have auto-negotiation capable, then restart auto negotiation*/
	if(cfg.AutoNegotiation)
	{
		phyData = phyData | (1 << 9);
	}

	if(SUCCESS != rtl8363_setAsicReg(phy,PHY_CONTROL_REG, UTPPAG, 0, phyData))
		return FAILED;

	return SUCCESS;
}

/*
@func int8 | rtl8363_getAsicEthernetPHY | Get PHY ability through PHY registers.
@parm uint8 | phy | PHY number (0~1).
@parm phyCfg_t* | cfg | phy configuration
@struct phyCfg_t | This structure describe PHY parameter
@field uint8 | AutoNegotiation | AutoNegotiation ability
@field uint8 | Speed | force speed, could be PHY_10M, PHY_100M, PHY_1000M
@field uint8 | Fullduplex | full or half duplex
@field uint8 | Cap_10Half | 10BASE-TX half duplex capable
@field uint8 | Cap_10Full  | 10BASE-TX full duplex capable
@field uint8 | Cap_100Half | 100BASE-TX half duplex capable
@field uint8 | Cap_100Full | 100BASE-TX full duplex capable
@field uint8 | FC | flow control capability
@field uint8 | AsyFC | asymmetric flow control capability
@rvalue SUCCESS | Success.
@rvalue FAILED | Failure.
@comm
	Get the capablity of specified PHY.
*/
int8 rtl8363_getAsicEthernetPHY(uint8 phy, phyCfg_t* cfg)
{
	uint16 phyData0;
	uint16 phyData4;
	uint16 phyData9;
      uint16 phyData17;


	if(phy > 1)
		return FAILED;


	/*Control register setting and restart auto*/
	if(SUCCESS != rtl8363_getAsicReg(phy,PHY_CONTROL_REG, UTPPAG, 0, &phyData0))
		return FAILED;

	/*Auto-Negotiation control register setting*/
	if(SUCCESS != rtl8363_getAsicReg(phy,PHY_AN_ADVERTISEMENT_REG, UTPPAG, 0, &phyData4))
		return FAILED;

	/*1000 BASE-T control register setting*/
	if(SUCCESS != rtl8363_getAsicReg(phy,PHY_1000_BASET_CONTROL_REG, UTPPAG, 0, &phyData9))
		return FAILED;

      cfg->Cap_1000Full = (phyData9 & (1<<9)) ? 1 : 0;
      cfg->AsyFC  = (phyData4 & (1<<11)) ? 1:0;
      cfg->FC = (phyData4 & (1<<10)) ? 1:0;
      cfg->Cap_100Full = (phyData4 & (1<<8)) ? 1:0;
      cfg->Cap_100Half = (phyData4 & (1<<7)) ? 1:0;
      cfg->Cap_10Full = (phyData4 & (1<<6)) ? 1:0;
      cfg->Cap_10Half = (phyData4 & (1<<5)) ? 1:0;
      cfg->AutoNegotiation = (phyData0 & (1<<12)) ? 1:0;

      /*phy register 0, [bit6, bit13] not reflect real speed after n-way, only reflect speed in force mode,
      but reg17 [15:14] will reflect real speed after n-way*/
	if(SUCCESS != rtl8363_getAsicReg(phy,17, UTPPAG, 0, &phyData17))
            return FAILED;
       cfg->Fullduplex = (phyData17 & (1 << 13)) ? 1:0;
       cfg->Speed = (phyData17 & (0x3<<14)) >> 14;



      /*[S7,S8] could reflect terminal-side/network-side real speed*/
      //rtl8363_getAsicReg(phy?0:1, 23, MACPAG, 6, &phyData0);
      //cfg->Speed = (phyData0 & (0x3 << 3)) >> 3;
      //cfg->Fullduplex = (phyData0 & (0x1 << 2)) ? 1:0;


	return SUCCESS;
}

/*
@func int8 | rtl8363_getAsicPHYLinkStatus | Get ethernet PHY linking status
@parm uint8 | phy | PHY number (0~1).
@parm uint8* | linkStatus | PHY link status 1:link up 0:link down
@rvalue SUCCESS | Success.
@rvalue FAILED | Failure.
@comm
	Output link status of bit 2 in PHY register 1. API will return status is link up under both auto negotiation complete and link status are set to 1.
*/
int8 rtl8363_getAsicPHYLinkStatus(uint8 phy, uint8 *linkStatus)
{
	uint16 phyData;

	if(phy > 1)
		return FAILED;


	/*Get PHY status register, the second read will be stable*/
	if(SUCCESS != rtl8363_getAsicReg(phy,PHY_STATUS_REG, UTPPAG, 0, &phyData))
		return FAILED;
	if(SUCCESS != rtl8363_getAsicReg(phy,PHY_STATUS_REG, UTPPAG, 0, &phyData))
		return FAILED;
	/*check link status*/
	if(phyData & (1<<2))
	{
		*linkStatus = 1;
	}
	else
	{
		*linkStatus = 0;
	}

     #if 0
     /*Pn_Reg17.10 utp page 0  is always stable */
     rtl8363_getAsicRegBit(phy, 17, 10, UTPPAG, 0, &phyData);
     *linkStatus = phyData ? 1:0;
     #endif

	return SUCCESS;
}



/*
@func int8 | rtl8363_setAsicPortLearnAbility | Set port learing ability
@parm uint8 | port | port number (0~2).
@parm uint8 | enabled | TRUE or FALSE
@rvalue SUCCESS | Success.
@rvalue FAILED | Failure.
@comm
*/

int8 rtl8363_setAsicPortLearnAbility(uint8 port, uint8 enabled)
{
    if (port > 2)
        return FAILED;

    rtl8363_setAsicRegBit(1, 18, port, MACPAG, 0, enabled ? 0:1);

    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicPortLearnAbility | Get port learing ability
@parm uint8 | port | port number (0~2).
@parm uint8 | enabled | TRUE or FALSE
@rvalue SUCCESS | Success.
@rvalue FAILED | Failure.
@comm
*/

int8 rtl8363_getAsicPortLearnAbility(uint8 port, uint8 *pEnabled)
{
    uint16 bitVal;

    if (port > 2)
        return FAILED;
    rtl8363_getAsicRegBit(1, 18, port, MACPAG, 0, &bitVal);
    *pEnabled = bitVal ? FALSE:TRUE;

    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicSoftReset | Set Asic software reset
@parm uint8 | port | port number (0~2).
@rvalue SUCCESS | Success.
@rvalue FAILED | Failure.
@comm
*/

int8 rtl8363_setAsicSoftReset(void)
{

    rtl8363_setAsicRegBit(0, 16, 15, MACPAG, 0, 1);
    return SUCCESS;
}


/*
@func int8 | rtl8363_setAsicFWDMode | Set asic forwarding mode.
@parm FWDmode_t | fwd | Specify forwarding parameter
@struct FWDmode_t | This structure describe forwarding parameter
@field uint8 | mode | forwarding mode
@rvalue SUCCESS
@rvalue FAILED
@comm
asic has 4 forwarding mode:
 RTL8363_FWD_NOR - normal mode;
 RTL8363_FWD_MCT - modified cut through mode;
 RTL8363_FWD_SPT - smart pass through mode;
 RTL8363_FWD_PT   - pass through mode;
*/

int8 rtl8363_setAsicFWDMode(FWDmode_t fwd)
{
   uint16 regval;

   rtl8363_getAsicReg(1, 17, MACPAG, 0, &regval);
   regval &= ~(0x3 << 8);
   regval |= (fwd.mode & 0x3) << 8;
   rtl8363_setAsicReg(1, 17, MACPAG, 0, regval);

   return SUCCESS;
}

/*
@func int8 | rtl8363_getAsicFWDMode | Set asic forwarding mode.
@parm FWDmode_t* | pFfwd | Get forwarding parameter
@struct FWDmode_t | This structure describe forwarding parameter
@field uint8 | mode | forwarding mode
@rvalue SUCCESS
@rvalue FAILED
@comm
asic has 4 forwarding mode:
 RTL8363_FWD_NOR - normal mode;
 RTL8363_FWD_MCT - modified cut through mode;
 RTL8363_FWD_SPT - smart pass through mode;
 RTL8363_FWD_PT   - pass through mode;
*/

int8 rtl8363_getAsicFWDMode(FWDmode_t *pFwd)
{
   uint16 regval;

   rtl8363_getAsicReg(1, 17, MACPAG, 0, &regval);
   pFwd->mode = (regval & (0x3 << 8)) >> 8;

   return SUCCESS;
}



/*
@func int8 | rtl8363_setAsicEnableAcl | Enable ACL function
@parm uint8 | enabled | TRUE or FALSE
@rvalue SUCCESS
@rvalue FAILED
@comm
*/
int8 rtl8363_setAsicEnableAcl(uint8 enabled)
{
    /*disaclsrch*/
    rtl8363_setAsicRegBit(1, 22, 10, MACPAG, 0, enabled ? 0:1);
    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicEnableAcl | Get ACL function enable or disabled
@parm uint8 | enabled | TRUE or FALSE
@rvalue SUCCESS
@rvalue FAILED
@comm
*/
int8 rtl8363_getAsicEnableAcl(uint8* enabled)
{
    uint16 bitValue;

    rtl8363_getAsicRegBit(1, 22, 10, MACPAG, 0, &bitValue);
    *enabled = (bitValue ? FALSE : TRUE);
    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicAclEntry | Set Asic ACL table
@parm uint8 | entryadd | Acl entry address (0~15)
@parm uint8 | phyport | Acl physical port
@parm uint8 | action | Acl action: RTL8363_ACT_PERMIT, RTL8363_ACT_PERMIT, RTL8363_ACT_TRAPCPU, RTL8363_ACT_MIRROR
@parm uint8 | protocol | Acl protocol: RTL8363_ACL_ETHER(ether type), RTL8363_ACL_TCP(TCP), RTL8306_ACL_UDP(UDP), RTL8306_ACL_TCPUDP(TCP or UDP)
@parm uint8 | data | ether type value or TCP/UDP port
@parm uint8 | priority | Acl priority: RTL8363_PRIO0~RTL8363_PRIO3
@rvalue SUCCESS
@rvalue FAILED
@comm
phyport could be:<nl>
RTL8363_ACL_INVALID - entry invalid<nl>
RTL8363_ACL_PORT0   - port 0<nl>
RTL8363_ACL_PORT1   - port 1<nl>
RTL8363_ACL_PORT2   - port 2<nl>
RTL8363_ACL_PORT01 - port0 & port 1<nl>
RTL8363_ACL_PORT02 - port0 & port 2<nl>
RTL8363_ACL_PORT12 - port 1 & port 2<nl>
RTL8363_ACL_ANYPORT - anyport<nl>
*/

int8 rtl8363_setAsicAclEntry(uint8 entryadd, uint8 phyport, uint8 action, uint8 protocol, uint16 dataval, uint8 priority)
{
	uint16 regValue, value;
	uint32 pollcnt  ;
	uint16 bitValue;

	if ((entryadd >= RTL8363_ACL_ENTRYNUM) || (phyport > RTL8363_ACL_ANYPORT) ||
		(action > RTL8363_ACT_MIRROR) ||(protocol > RTL8363_ACL_TCPUDP)
		||(priority > RTL8363_PRIO3))
			return FAILED;

	/*set EtherType or TCP/UDP Ports, ACL entry access register 0*/

      rtl8363_setAsicReg(2, 22, MACPAG, 1, dataval);
	/*set ACL entry access register 1*/
      rtl8363_getAsicReg(2, 23, MACPAG, 1, &regValue);
      value = (1<< 14) | (entryadd << 10) | (priority << 8) | (action << 6) | (phyport << 3) | (protocol << 1);
      regValue = (regValue & 0x1) | value  ;
      rtl8363_setAsicReg(2, 23, MACPAG, 1, regValue);
	/*Polling whether the command is done*/
	for (pollcnt = 0; pollcnt < RTL8363_IDLE_TIMEOUT ; pollcnt++)
      {
        rtl8363_getAsicRegBit(2, 23,  14, MACPAG, 1, &bitValue);
        if (!bitValue)
	    break;
	}
	if (pollcnt == RTL8363_IDLE_TIMEOUT)
		return FAILED;

	return SUCCESS;
}


/*
@func int8 | rtl8306_setAsicAclEntry | Set Asic ACL table
@parm uint8 | entryadd | Acl entry address (0~15)
@parm uint8* | phyport | Acl physical port
@parm uint8* | action | Acl action: RTL8363_ACT_PERMIT, RTL8363_ACT_PERMIT, RTL8363_ACT_TRAPCPU, RTL8363_ACT_MIRROR
@parm uint8* | protocol | Acl protocol: RTL8363_ACL_ETHER(ether type), RTL8363_ACL_TCP(TCP), RTL8363_ACL_UDP(UDP), RTL8363_ACL_TCPUDP(TCP or UDP)
@parm uint8* | data | ether type value or TCP/UDP port
@parm uint8* | priority | Acl priority: RTL8363_PRIO0~RTL8363_PRIO3
@rvalue SUCCESS
@rvalue FAILED
@comm
phyport could be:<nl>
RTL8363_ACL_INVALID - entry invalid<nl>
RTL8363_ACL_PORT0   - port 0<nl>
RTL8363_ACL_PORT1   - port 1<nl>
RTL8363_ACL_PORT2   - port 2<nl>
RTL8363_ACL_PORT01 - port0 & port 1<nl>
RTL8363_ACL_PORT02 - port0 & port 2<nl>
RTL8363_ACL_PORT12 - port 1 & port 2<nl>
RTL8363_ACL_ANYPORT - anyport<nl>
*/

int8 rtl8363_getAsicAclEntry(uint8 entryadd, uint8 *phyport, uint8 *action, uint8 *protocol, uint16  *dataval, uint8 *priority)
{
	uint16 regValue;
	uint32 pollcnt  ;
	uint16 bitValue;

	if ((entryadd >= RTL8363_ACL_ENTRYNUM) || (phyport == NULL) || (action == NULL) ||
		(protocol == NULL) || (dataval == NULL) || (priority == NULL))
		return FAILED;

	/*trigger a command to read ACL entry*/
	regValue =  (0x3 << 14) | (entryadd << 10);
      rtl8363_setAsicReg(2, 23, MACPAG, 1, regValue);

	/*Polling whether the command is done*/
	for (pollcnt = 0; pollcnt < 100 ; pollcnt++) {
            rtl8363_getAsicRegBit(2, 23,  14, MACPAG, 1, &bitValue);
		if (!bitValue)
			break;
	}
	if (pollcnt > 50)
		return FAILED;

      rtl8363_getAsicReg(2, 22, MACPAG, 1, &regValue);
      *dataval = regValue;
      rtl8363_getAsicReg(2, 23, MACPAG, 1, &regValue);
      *priority = (regValue >> 8) & 0x3;
      *action = (regValue >> 6) & 0x3;
      *phyport = (regValue >> 3) & 0x7;
      *protocol = (regValue >> 1) & 0x3;

	return SUCCESS;
}


/*
@func int8 | rtl8363_setAsicVlanEnable | Configure switch to be VLAN switch.
@parm uint8 | enabled | Configure RTL8363 to be VLAN switch or not.
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_setAsicVlanEnable(uint8 enabled)
{

    rtl8363_setAsicRegBit(0, 26, 7, MACPAG, 0, enabled ? 0:1);

    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicVlanEnable | Get whether switch is VLAN switch.
@parm uint8 * | pEnabled | RTL8363 is VLAN switch or not.
@rvalue SUCCESS
@rvalue FAILED
@comm
*/
int8 rtl8363_getAsicVlanEnable(uint8* pEnabled)
{
    uint16 bitValue;

    rtl8363_getAsicRegBit(0, 26, 7, MACPAG, 0, &bitValue);
    *pEnabled = (bitValue ? FALSE:TRUE);

    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicVlanTagAware | Configure switch to be VLAN tag awared.
@parm uint8 | enabled | Configure RTL8363 VLAN tag awared
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_setAsicVlanTagAware(uint8 enabled)
{
    rtl8363_setAsicRegBit(0, 26, 6, MACPAG, 0, enabled ? 0:1);
    return SUCCESS;
}

/*
@func int8 | rtl8363_getAsicVlanTagAware | Get switch VLAN tag awared setting.
@parm uint8 * | enabled | RTL8363 is VLAN tag awared or not.
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_getAsicVlanTagAware(uint8 *pEnabled)
{
    uint16 bitval;
    rtl8363_getAsicRegBit(0, 26, 6, MACPAG, 0, &bitval);
    *pEnabled = bitval ? FALSE:TRUE;
    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicVlanIngressFilter | Configure switch to be VLAN ingress filtered or not.
@parm uint8 | enabled | Configure RTL8363 VLAN ingress filter
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_setAsicVlanIngressFilter(uint8 enabled)
{

    rtl8363_setAsicRegBit(0, 26, 5, MACPAG, 0, enabled ? 0:1);

    return SUCCESS;
}

/*
@func int8| rtl8363_getAsicVlanIngressFilter | Get switch VLAN ingress filter setting.
@parm uint8 * | enabled | RTL8363 is VLAN ingress filter or not.
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_getAsicVlanIngressFilter(uint8 *pEnabled)
{
    uint16 bitval;
    rtl8363_getAsicRegBit(0, 26, 5, MACPAG, 0, &bitval);
    *pEnabled = bitval ? FALSE:TRUE;

    return SUCCESS;
}

/*
@func int32 | rtl8363_setAsicVlanTaggedOnly | Configure switch to be VLAN tagged only or not.
@parm uint8 | enabled | Configure RTL8363 receive VLAN tagged only
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_setAsicVlanTaggedOnly(uint8 enabled)
{

    rtl8363_setAsicRegBit(0, 26, 4, MACPAG, 0, enabled ? 0:1);
    return SUCCESS;
}


/*
@func int32 | rtl8363_getAsicVlanTaggedOnly | Get switch to be VLAN tagged only or not.
@parm uint32* | enabled |  Whether RTL8306 receive VLAN tagged only
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_getAsicVlanTaggedOnly(uint8 *pEnabled)
{
    uint16 bitval;

    rtl8363_getAsicRegBit(0, 26, 4, MACPAG, 0, &bitval);
    *pEnabled = bitval ? FALSE:TRUE;
    return SUCCESS;

}

/*
@func int8 | rtl8363_setAsicVlan | Configure switch VLAN ID and corresponding member ports.
@parm uint8 | vlanIndex | Specify the position to configure VLAN
@parm uint8 | vid | Specify the VLAN ID
@parm uint8 | memberPortMask | Specify the member port list of corresponding VLAN ID
@rvalue SUCCESS
@rvalue FAILED
@comm
	Total have 16 entries to specify the VID and corresponding member port. This API will not
check whether configured VID is duplicated or not.
*/

int8 rtl8363_setAsicVlan(uint8 vlanIndex, uint16 vid, uint8 memberPortMask)
{
    uint16 regValue;

    if (vlanIndex > 15)
        return FAILED;

   regValue = vid & 0xFFF;
   regValue |= (memberPortMask & 0x7) << 13;
    if ((vlanIndex == 0) || (vlanIndex == 1))
    {
        rtl8363_setAsicReg(0, 29 + vlanIndex, MACPAG, 0, regValue);
    }
    else
    {
        rtl8363_setAsicReg(0, 17 + (vlanIndex -2), MACPAG, 1, regValue);
    }

    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicVlan | Get switch VLAN ID and corresponding member ports.
@parm uint8 | vlanIndex | Specify the position to configure VLAN
@parm uint8 * | vid | Specify the VLAN ID
@parm uint8 * | memberPortMask | Specify the member port list of corresponding VLAN ID
@rvalue SUCCESS
@rvalue FAILED
@comm
	Total have 16 entries to store the VID and corresponding member port. This API will not
check whether entry already configured or not.
*/

int8 rtl8363_getAsicVlan(uint8 vlanIndex, uint16 *pVid, uint8 *pMemberPortMask)
{
    uint16 regValue;

    if(pVid == NULL || pMemberPortMask == NULL)
		return FAILED;

    if ((vlanIndex == 0) || (vlanIndex == 1))
    {
        rtl8363_getAsicReg(0, 29 + vlanIndex, MACPAG, 0, &regValue);
    }
    else
    {
        rtl8363_getAsicReg(0, 17 + (vlanIndex -2), MACPAG, 1, &regValue);
    }
    *pVid = regValue & 0xFFF;
    *pMemberPortMask = (regValue >> 13) & 0x7;

    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicPortVlanIndex | Configure switch port un-tagged packet belonged VLAN index
@parm uint8 | port | Specify the port(port 0 ~ port 2) to configure VLAN index
@parm uint8 | vlanIndex | Specify the VLAN index
@rvalue SUCCESS
@rvalue FAILED
@comm
There are 16 valid index. This function does not check whether VLAN index do configured before port belong to it.
*/

int8 rtl8363_setAsicPortVlanIndex(uint8 port, uint8 vlanIndex)
{
    uint16 regval;

    rtl8363_getAsicReg(0, 28, MACPAG, 0, &regval);
    switch (port)
    {
        case RTL8363_PORT0:
            regval &= ~0xF;
            regval |= (vlanIndex & 0xF);
            break;
        case RTL8363_PORT1:
            regval &= ~0xF0;
            regval |= ((vlanIndex & 0xF) << 4);
            break;
        case RTL8363_PORT2:
            regval &= ~0xF00;
            regval |= ((vlanIndex & 0xF) << 8);
            break;
        default :
            return FAILED;

    }
    rtl8363_setAsicReg(0, 28, MACPAG, 0, regval);
    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicPortVlanIndex | Get configured switch port un-tagged packet belonged VLAN index
@parm uint8 | port | Specify the port(port0 ~ port 2) to configure VLAN index
@parm uint8 * | vlanIndex | Port belonged VLAN index
@comm
This function only get the configured register value and do not check whether VLAN do exist
*/

int8 rtl8363_getAsicPortVlanIndex(uint8 port, uint8 *pVlanIndex)
{
    uint16 regval;

    rtl8363_getAsicReg(0, 28, MACPAG, 0, &regval);
    switch (port)
    {
        case RTL8363_PORT0:
            *pVlanIndex = regval & 0xF;
            break;
        case RTL8363_PORT1:
            *pVlanIndex = (regval & 0xF0) >> 4;
            break;
        case RTL8363_PORT2:
            *pVlanIndex = (regval & 0xF00) >> 8;
            break;
        default :
            return FAILED;

    }
    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicLeakyVlan | Configure switch to forward unicast frames to other VLANs.
@parm uint8 | enabled | Configure RTL8363 to forward unicast frames to other VLANs.
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_setAsicLeakyVlan(uint8 enabled)
{
    rtl8363_setAsicRegBit(0, 26, 2, MACPAG, 0, enabled ? 0 :1);

    return SUCCESS;
}

/*
@func int32 | rtl8363_getAsicLeakyVlan | Get switch whether forward unicast frames to other VLANs.
@parm uint8 * | enabled | Whether RTL8363 forwards unicast frames to other VLANs.
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_getAsicLeakyVlan(uint8 *pEnabled)
{
    uint16 bitval;

    rtl8363_getAsicRegBit(0, 26, 2, MACPAG, 0, &bitval);
    *pEnabled = bitval ? FALSE:TRUE;
    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicArpVlan | Configure switch to forward arp frames to other VLANs.
@parm uint8 | enabled | Configure RTL8363 to forward arp frames to other VLANs.
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_setAsicArpVlan(uint8 enabled)
{
    rtl8363_setAsicRegBit(0, 26, 3, MACPAG, 0, enabled ? 0:1);
    return SUCCESS;
}

/*
@func int8 | rtl8363_getAsicArpVlan | Get switch whether forward arp frames to other VLANs.
@parm uint8 * | enabled | Whether RTL8363 forwards arp frames to other VLANs.
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_getAsicArpVlan(uint8 *pEnabled)
{
    uint16 bitval;

    rtl8363_getAsicRegBit(0, 26, 3, MACPAG, 0, &bitval);
    *pEnabled = bitval ? FALSE:TRUE;

    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicMulticastVlan | Configure switch whether forward inter-VLANS multicast packet to other VLANs.
@parm uint8  | enabled | Whether RTL8363 forwards inter-VLANS multicast packet to other VLANs.
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_setAsicMulticastVlan(uint8 enabled)
{
    rtl8363_setAsicRegBit(0, 26, 1, MACPAG, 0, enabled ? 0:1);
    return SUCCESS;
}

/*
@func int8 | rtl8363_getAsicMulticastVlan | Get switch whether forward inter-VLANS multicast packet to other VLANs.
@parm uint8 * | enabled | Whether RTL8363 forwards inter-VLANS multicast packet to other VLANs.
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_getAsicMulticastVlan(uint8 *pEnabled)
{
    uint16 bitval;

    rtl8363_getAsicRegBit(0, 26, 1, MACPAG, 0, &bitval);
    *pEnabled = bitval ? FALSE:TRUE;
    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicMirrorVlan | Configure switch whether enable the inter_VLAN mirror function .
@parm uint8  | enabled | Whether RTL8363 mirror function ignore the VLAN member set domain limitation
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_setAsicMirrorVlan(uint8 enabled)
{
    rtl8363_setAsicRegBit(0, 26, 0, MACPAG, 0, enabled ? 0:1);
    return SUCCESS;
}

/*
@func int8 | rtl8363_getAsicMirrorVlan | Get switch whether enable the inter_VLAN mirror function .
@parm uint8*  | enabled | Whether RTL8363 mirror function ignore the VLAN member set domain limitation
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_getAsicMirrorVlan(uint8 *pEnabled)
{
    uint16 bitval;

    rtl8363_getAsicRegBit(0, 26, 0, MACPAG, 0, &bitval);
    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicNullVidReplaceVlan | Configure switch whether replace a NULL VID with a port VID
@parm uint8 | port | Specify the port to replace NULL VID with port VID
@parm uint8  | enabled | Whether RTL8363 enables NULL VID replacement
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_setAsicNullVidReplaceVlan(uint8 port, uint8 enabled)
{
    uint16 regval;

    if (port > RTL8363_PORT2)
        return FAILED;

    rtl8363_getAsicReg(0, 27, MACPAG, 0, &regval);
    regval &=  ~(0x1 << (3 + port));
    regval |= enabled ? (1 << (3 + port)):0;
    rtl8363_setAsicReg(0, 27, MACPAG, 0, regval);

    return SUCCESS;
}

/*
@func int8 | rtl8363_getAsicNullVidReplaceVlan | Get switch whether replace a NULL VID with a port VID
@parm uint8 | port | Specify the port
@parm uint8*  | enabled | Whether RTL8363 enables NULL VID replacement
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_getAsicNullVidReplaceVlan(uint8 port, uint8 *pEnabled)
{
    uint16 regval;

    if (port > RTL8363_PORT2)
        return FAILED;

    rtl8363_getAsicReg(0, 27, MACPAG, 0, &regval);
    *pEnabled = (regval & (0x1 << (3 + port ))) ? TRUE:FALSE;

    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicNonPVIDDiscardVlan | Set the port discards packets whose VID does not match ingress port PVID if tagged packets are received
@parm uint8 | port | Specify the port
@parm uint8 | enabled | Whether enables Non PVID discard
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_setAsicNonPVIDDiscardVlan(uint8 port, uint8 enabled)
{

    if (port > RTL8363_PORT2)
        return FAILED;

    rtl8363_setAsicRegBit(0, 27, port, MACPAG, 0, enabled ? 1:0);
    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicNonPVIDDiscardVlan | Get the port whether discards packets whose VID does not match ingress port PVID if tagged packets are received
@parm uint8 | port | Specify the port
@parm uint8* | pEnabled | Whether enables Non PVID discard
@rvalue SUCCESS
@rvalue FAILED
@comm
*/
int8 rtl8363_getAsicNonPVIDDiscardVlan(uint8 port, uint8 *pEnabled)
{
    uint16 regval;

    if (port > RTL8363_PORT2)
        return FAILED;

    rtl8363_getAsicRegBit(0, 27, port, MACPAG, 0,&regval);
    *pEnabled = regval ? 1:0;

    return SUCCESS;
}


/*
@func int8 | rtl8363_setAsicVlanTagInsertRemove | Port Vlan tag insertion and removal.
@parm uint8 | port | Specify the port.
@parm uint8 | option | There are 4 Macros respresenting 4 options.
@rvalue SUCCESS
@rvalue FAILED
@comm
The 4 options are :<nl>
RTL8363_VLAN_IRTAG	The switch will remove VLAN tags and add new tags, new tag is ingress port's PVID. This is a replacement processing for tagged packets and an insertion for untagged packets.<nl>
RTL8363_VLAN_RTAG 	The switch will remove VLAN tags<nl>
RTL8363_VLAN_ITAG	The switch will add new VLAN tag (ingress port's PVID ) for untagged packet, but will not add tags to the packet already tagged. <nl>
RTL8363_VLAN_UNDOTAG 	Do not insert or remove  VLAN tag<nl>
Other issures should be aware: <nl>
For packet with CPU tag packet, either remove or don't touch CPU tag, it will not follow VLAN Tag insert/remove rule (VLAN Tag always don't touch);
For packet without CPU, it will follow VLAN Tag insert/remove rule.
When the TX port is CPU, and at the same time CPU function and inserting CPU tag are enable, the behavior is inserting CPU tag and VLAN Tag don't touch.
If packet with CPU tag, it don't care insert CPU tag option

*/

int8 rtl8363_setAsicVlanTagInsertRemove(uint8 port, uint8 option)
{
    uint16 regval;

    rtl8363_getAsicReg(0, 27, MACPAG, 0, &regval);
    regval &= ~(0x3 << (10 + port*2));
    regval |= (option & 0x3) << (10 + port*2);
    rtl8363_setAsicReg(0, 27, MACPAG, 0, regval);

    return SUCCESS;
}


/*
@func int8  | rtl8363_getAsicVlanTagInsertRemove | Get vlan tag insert and removal information.
@parm uint8 | port | Specify the port.
@parm uint8* | option | There are 4 Macros respresenting 4 options.
@rvalue SUCCESS
@rvalue FAILED
@comm
The 4 options are :<nl>
	RTL8363_VLAN_IRTAG	The switch will remove VLAN tags and add new tags<nl>
	RTL8363_VLAN_RTAG 	The switch will remove VLAN tags<nl>
	RTL8363_VLAN_ITAG		The switch will  add new VLANtag<nl>
	RTL8363_VLAN_UNDOTAG 	Do not insert or remove  VLAN tag<nl>
*/

int8 rtl8363_getAsicVlanTagInsertRemove(uint8 port, uint8 *pOption)
{
    uint16 regval;

    if (port > RTL8363_PORT2)
        return FAILED;
    rtl8363_getAsicReg(0, 27, MACPAG, 0, &regval);
    *pOption = (regval & (0x3 << (10 + port*2))) >> (10 + port*2);

    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicVlanTrapToCPU | Set vlan-taged frame that does not hit vlan table trap to cpu or not.
@parm uint8 | enabled | TRUE or FALSE.
@rvalue SUCCESS
@rvalue FAILED
@comm
If enabled = TRUE, the VLAN-tagged frame that does not hit the VLAN table will be insert cpu tag and
trapped to CPU; if enabled = FALSE, the packets will be dropped.
*/

int8 rtl8363_setAsicVlanTrapToCPU(uint8 enabled)
{

    rtl8363_setAsicRegBit(0, 27, 6, MACPAG, 0, enabled ? 0:1);
    return SUCCESS;
}

/*
@func int8 | rtl8363_getAsicVlanTrapToCPU | Get vlan-taged frame that does not hit vlan table trap to cpu or not.
@parm uint8* | enabled | Trap or not.
@comm
*/
int8 rtl8363_getAsicVlanTrapToCPU(uint8 *pEnabled)
{
    uint16 bitval;

    rtl8363_getAsicRegBit(0, 27, 6, MACPAG, 0, &bitval);
    *pEnabled = bitval ? FALSE:TRUE;
    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsic1pRemarkingVlan | Set vlan 802.1P remarking ability.
@parm uint8 | port | Specify the port.
@parm uint8 | enabled | TRUE or FALSE.
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_setAsic1pRemarkingVlan(uint8 port, uint8 enabled)
{

    switch(port)
    {
        case RTL8363_PORT0:
            rtl8363_setAsicRegBit(1, 27, 5, MACPAG, 4, enabled ?1:0);
            break;
        case RTL8363_PORT1:
            rtl8363_setAsicRegBit(1, 27, 13, MACPAG, 4, enabled ?1:0);
            break;
        case RTL8363_PORT2:
            rtl8363_setAsicRegBit(1, 28, 5, MACPAG, 4, enabled ?1:0);
            break;
        default:
            return FAILED;
    }
    return SUCCESS;
}

/*
@func int8 | rtl8363_getAsic1pRemarkingVlan | Get vlan 802.1P remarking ability.
@parm uint8 | port | Specify the port.
@parm uint8* | enabled | TRUE or FALSE.
@rvalue SUCCESS
@rvalue FAILED
@comm
*/


int8 rtl8363_getAsic1pRemarkingVlan(uint8 port, uint8 *pEnabled)
{
    uint16 bitval;

    switch(port)
    {
        case RTL8363_PORT0:
            rtl8363_getAsicRegBit(1, 27, 5, MACPAG, 4, &bitval);
            *pEnabled = bitval ? 1:0;
            break;
        case RTL8363_PORT1:
            rtl8363_getAsicRegBit(1, 27, 13, MACPAG, 4, &bitval);
            *pEnabled = bitval ? 1:0;
            break;
        case RTL8363_PORT2:
            rtl8363_getAsicRegBit(1, 28, 5, MACPAG, 4, &bitval);
            *pEnabled = bitval ? 1:0;
            break;
        default:
            return FAILED;
    }

    return SUCCESS;
}


/*
@func int8 | rtl8363_setAsic1pRemarkingPriority | Set vlan 802.1P remarking priority.
@parm uint8 | priority | Packet priority(0~4).
@parm uint8 | priority1p | 802.1P priority(0~7).
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_setAsic1pRemarkingPriority(uint8 pri, uint8 pri1p)
{
    uint16 regval;

    rtl8363_getAsicReg(1, 26, MACPAG, 4, &regval);
    switch (pri)
    {
        case RTL8363_PRIO0:
            regval &= ~0x7;
            regval |= pri1p & 0x7;
            break;
        case RTL8363_PRIO1:
            regval &= ~(0x7 << 3);
            regval |= (pri1p & 0x7) << 3;
            break;
        case RTL8363_PRIO2:
            regval &= ~(0x7 << 8);
            regval |= (pri1p & 0x7) << 8;
            break;
        case RTL8363_PRIO3:
            regval &= ~(0x7 << 11);
            regval |= (pri1p & 0x7) << 11;
            break;
        default:
            return FAILED;
    }
    rtl8363_setAsicReg(1, 26, MACPAG, 4, regval);
    return SUCCESS;
}

/*
@func int8 | rtl8363_getAsic1pRemarkingPriority | Get vlan 802.1P remarking priority.
@parm uint8 | priority | Packet priority(0~4).
@parm uint8* | priority1p | 802.1P priority(0~7).
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_getAsic1pRemarkingPriority(uint8 pri, uint8 *pPri1p)
{
    uint16 regval;

    rtl8363_getAsicReg(1, 26, MACPAG, 4, &regval);
    switch (pri)
    {
        case RTL8363_PRIO0:
            *pPri1p = regval & 0x7;
            break;
        case RTL8363_PRIO1:
            *pPri1p = (regval & (0x7 << 3)) >> 3;
            break;
        case RTL8363_PRIO2:
            *pPri1p = (regval & (0x7 << 8)) >> 8;
            break;
        case RTL8363_PRIO3:
            *pPri1p = (regval & (0x7 << 11)) >> 11;
            break;
        default:
            return FAILED;
    }

    return SUCCESS;
}


/*
@func int8 | rtl8363_setAsicLUT | set asic LUT entry
@para uint32 | entry | specify 4-way entry (0~3) of an index
@parm LUTPara_t * | pLut | Specify the properties of the entry cresponding to the MAC Address
@struct LUTPara_t | This structure describe the property parameters of the entry in the LUT
@field  uint8 | type | Specify the type of the entry, LUT_L2DYN, LUT_L2STA, LUT_HDSNP.
@field  uint8 | mac[6] |Specify the MAC Address
@field  uint8 | port | For dynamic entry, specify  the source port of this Source MAC address learned; for static or multiicast, specify the port mask.
@field  uint8 | isStatic | For unicast MAC only, TRUE(static entry), FALSE(dynamic entry)
@field  uint8 | isAuth |  Whether the mac address is authorized by IEEE 802.1x
@field  uint8 | age | Specify age time, has effect only on unicast MAC and isStatic is set as FALSE
@field  uint8 | enBlk | Enable block(the packets from source MAC address ID of this entry will be dropped), only has effect on multicast MAC or unicast MAC & isStatic is set as TRUE.
@field  uint32 | dip |For IP Multicast IGMPv3 Entry only, destination IP
@field  uint32 | sip |For IP Multicast IGMPv3 Entry only, soure IP
@field  uint16 |gmitv_p[3] |  group member interval
@rvalue SUCCESS
@rvalue FAILED
@comm
the entry is 4-way entry of an index, its value is from 0 to 3, the index is got from
mac address or ip address(IGMPv3 entry), index and 4-way enry make up of the exact
entry number of whole LUT
*/

int8 rtl8363_setAsicLUT(uint32 entry, LUTPara_t * pLut)
{
    uint8 i;
    uint8 idx, index = 0;
    uint8 ip[4];
    uint16 regval, bitValue;
    uint32 pollcnt ;
    uint8 cntprd; /*Group member interval granularity*/

    if (pLut == NULL)
        return FAILED;

    /*Enable lookup table access*/
    rtl8363_setAsicRegBit(0, 21, 0, MACPAG, 0, 1);
    switch (pLut->type)
    {
        case LUT_L2DYN:
        case LUT_L2STA:
			if(pLut->type == LUT_L2DYN)
			{
				pLut->isStatic = FALSE;
			}
			else
			{
				pLut->isStatic = TRUE;
			}

            idx = (pLut->mac[0]^ pLut->mac[1]^pLut->mac[2]^pLut->mac[3]^pLut->mac[4]^pLut->mac[5]);
            index = 0 ;
            for ( i = 0; i < 8; i ++)
            {
                index |= (idx & 0x1) ? (1 << (7 - i)) :0;
                idx >>= 1;
                if (!idx)
                    break;
            }
            /*Write Data[71:56]*/
            regval  = 0;
            if (!pLut->isStatic)
            {
                /*Dynamic entry*/
                regval |= ((pLut->port & 0x3) << 4) | ((pLut->age & 0x3) << 6) | (pLut->isAuth? (1<<11):0 );
            }
            else
            {
                /*Static entry*/
                regval |= ((pLut->port & 0x7) << 4) | (pLut->enblk ? (1 << 10): 0) |(pLut->isAuth ? (1<<11):0 ) |(1<<12);
            }
            rtl8363_setAsicReg(0, 25, MACPAG, 0, regval);
            /*Write Data[55:40]*/
            regval = pLut->mac[5];
            rtl8363_setAsicReg(0, 24, MACPAG, 0, regval);
            /*Write Data[39:24]*/
            regval = (pLut->mac[4] << 8) | pLut->mac[3];
            rtl8363_setAsicReg(0, 23, MACPAG, 0, regval);
            /*Write Data[23:8]*/
            regval = pLut->mac[2] << 8 | pLut->mac[1];  //modified 2008.10.30
            rtl8363_setAsicReg(0, 22, MACPAG, 0, regval);
            /*Write Data[7:0]*/
            rtl8363_getAsicReg(0, 21, MACPAG, 0, &regval);
            regval &= ~0xFF00;
            regval |= pLut->mac[0] << 8;
            rtl8363_setAsicReg(0, 21, MACPAG, 0, regval);
            /*Write entry index, entry, Command execution, Read/Write operation*/
            regval = 0x0 | (entry & 0x3) << 2 | (index << 8);
            rtl8363_setAsicReg(0, 20, MACPAG, 0, regval);
            rtl8363_setAsicRegBit(0, 20, 1, MACPAG, 0, 1);

            /*Waiting for write command done and prevent polling dead loop*/
            for (pollcnt = 0; pollcnt < RTL8363_IDLE_TIMEOUT; pollcnt ++)
            {
                rtl8363_getAsicRegBit(0, 20, 1, MACPAG, 0, &bitValue);
                if (!bitValue)
	            break;
            }
            if (pollcnt == RTL8363_IDLE_TIMEOUT)
                return FAILED;

            break;

        case LUT_IGMPV3:
            /*check DIP whether DIP[31:28] == 0b1110*/
            if ((pLut->dip & 0xE0000000) != 0xE0000000)
                return FAILED;
			/*enable ENSIPSR (SIP + DIP hash)*/
			rtl8363_setAsicRegBit(2, 17, 5, MACPAG, 0, 1);
            ip[0] = (uint8)((pLut->dip & 0x00FF0000) >> 16);
            ip[1] = (uint8)((pLut->dip & 0xFF00) >> 8 );
            ip[2] = (uint8)(pLut->dip & 0xFF);
            idx = ip[0] ^ ip[1] ^ ip[2];
            ip[0] = (uint8)((pLut->sip & 0xFF000000) >> 24);
            ip[1] = (uint8)((pLut->sip & 0x00FF0000) >> 16);
            ip[2] = (uint8)((pLut->sip & 0xFF00) >> 8);
            ip[3] = (uint8)(pLut->sip & 0xFF);
            idx ^= ip[0]^ip[1]^ip[2]^ip[3];
            /*Write Data[71:56]*/
            regval  = 0;
            regval = (0x1 << 13) | ((pLut->port & 0x7) << 4) | ((pLut->dip & 0x0F000000) >> 24);
            rtl8363_setAsicReg(0, 25, MACPAG, 0, regval);
            /*Write Data[55:40]*/
            regval = (pLut->dip & 0x00FFFF00) >> 8;
            rtl8363_setAsicReg(0, 24, MACPAG, 0, regval);
            /*Write Data[39:24]*/
            regval = ((pLut->dip & 0xFF) << 8) | (pLut->sip & 0xFF000000) >> 24;
            rtl8363_setAsicReg(0, 23, MACPAG, 0, regval);
            /*Write Data[23:8]*/
            regval = (pLut->sip & 0x00FFFF00) >> 8;
            rtl8363_setAsicReg(0, 22, MACPAG, 0, regval);
            /*Write Data[7:0]*/
            rtl8363_getAsicReg(0, 21, MACPAG, 0, &regval);
            regval &= 0x00FF;
            regval |= ((pLut->sip & 0xFF) << 8);
            rtl8363_setAsicReg(0, 21, MACPAG, 0, regval);
            /*Write entry index, entry, Command execution, Read/Write operation*/
            regval = 0x0 | (entry & 0x3) << 2 | (index << 8);
            rtl8363_setAsicReg(0, 20, MACPAG, 0, regval);
            rtl8363_setAsicRegBit(0, 20, 1, MACPAG, 0, 1);
            /*Waiting for write command done and prevent polling dead loop*/
            for (pollcnt = 0; pollcnt < RTL8363_IDLE_TIMEOUT; pollcnt ++)
            {
                rtl8363_getAsicRegBit(0, 20, 1, MACPAG, 0, &bitValue);
                if (!bitValue)
	            break;
            }
            if (pollcnt == RTL8363_IDLE_TIMEOUT)
                return FAILED;
            break;

        case LUT_HDSNP:
            /*check whether it is IP multicast 01-00-5e-xx-xx-xx(ipv4) or 33-33-xx-xx-xx-xx*/
            if ((pLut->mac[0] == 0x01) && ((pLut->mac[1] != 0x0)|| (pLut->mac[2] != 0x5e)))
                return FAILED;
            if ((pLut->mac[0] == 0x33) && (pLut->mac[1] != 0x33))
                return FAILED;
			/*disable ENSIPSR (SIP + DIP hash)*/
			rtl8363_setAsicRegBit(2, 17, 5, MACPAG, 0, 0);
            for (i = 0; i < 3; i++)
            {
                pLut->gmitv_p[i] &= 0x3FF;
                /*modify Group member interval granularity*/
                if (pLut->gmitv_p[i] > 508 )
                    rtl8363_setAsicRegBit(2, 17, 3, MACPAG, 0, 1);
            }
            rtl8363_getAsicRegBit(2, 17, 3, MACPAG, 0, &bitValue);
            cntprd = bitValue ? 8:4;
            pLut->gmitv_p[2] /= cntprd  ;
            pLut->gmitv_p[2] &= 0x7F;
            pLut->gmitv_p[1] /= cntprd  ;
            pLut->gmitv_p[1] &= 0x7F;
            pLut->gmitv_p[0] /= cntprd  ;
            pLut->gmitv_p[0] &= 0x7F;

			/*caculate index*/
            idx = (pLut->mac[0]^ pLut->mac[1]^pLut->mac[2]^pLut->mac[3]^pLut->mac[4]^pLut->mac[5]);
            index = 0 ;
            for ( i = 0; i < 8; i ++)
            {
                index |= (idx & 0x1) ? (1 << (7 - i)) :0;
                idx >>= 1;
                if (!idx)
                    break;
            }
            /*Write Data[71:56]*/
            regval = (0x1 << 13) |(pLut->gmitv_p[2] << 6) |  (pLut->gmitv_p[1] >> 1);
            rtl8363_setAsicReg(0, 25, MACPAG, 0, regval);
            /*Write Data[55:40]*/
            regval = ((pLut->gmitv_p[1] & 0x1) ? (0x1<< 15):0) | (pLut->gmitv_p[0] << 8);
            regval |= pLut->mac[5];
            rtl8363_setAsicReg(0, 24, MACPAG, 0, regval);
            /*Write Data[39:24]*/
            regval = (pLut->mac[4] << 8) | pLut->mac[3];
            rtl8363_setAsicReg(0, 23, MACPAG, 0, regval);
            /*Write Data[23:8]*/
            regval = pLut->mac[2] << 8 | pLut->mac[1];
            rtl8363_setAsicReg(0, 22, MACPAG, 0, regval);
            /*Write Data[7:0]*/
            rtl8363_getAsicReg(0, 21, MACPAG, 0, &regval);
            regval &= 0x00FF;
            regval |= (pLut->mac[0] << 8);
            rtl8363_setAsicReg(0, 21, MACPAG, 0, regval);
            /*Write entry index, entry, Command execution, Read/Write operation*/
            regval = 0x0 | (entry & 0x3) << 2 | (index << 8);
            rtl8363_setAsicReg(0, 20, MACPAG, 0, regval);
	     rtl8363_setAsicRegBit(0, 20, 1, MACPAG, 0, 1);

            /*Waiting for write command done and prevent polling dead loop*/
            for (pollcnt = 0; pollcnt < RTL8363_IDLE_TIMEOUT; pollcnt ++)
            {
                rtl8363_getAsicRegBit(0, 20, 1, MACPAG, 0, &bitValue);
                if (!bitValue)
	            break;
            }
            if (pollcnt == RTL8363_IDLE_TIMEOUT)
                return FAILED;

            break;
        default:
            return FAILED;

    }

    /*Disable lookup table access*/
    rtl8363_setAsicRegBit(0, 21, 0, MACPAG, 0, 0);

    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicLUT | set asic LUT entry
@para uint32 | entry | specify the entry number (0 ~1023)
@parm LUTPara_t * | pLut | Specify the properties of the entry cresponding to the MAC Address
@struct LUTPara_t | This structure describe the property parameters of the entry in the LUT
@field  uint8 | type | Specify the type of the entry, LUT_L2DYN, LUT_L2STA, LUT_HDSNP.
@field  uint8 | mac[6] |Specify the MAC Address
@field  uint8 | port | For dynamic entry, specify  the source port of this Source MAC address learned; for static or multiicast, specify the port mask.
@field  uint8 | isStatic | For unicast MAC only, TRUE(static entry), FALSE(dynamic entry)
@field  uint8 | isAuth |  Whether the mac address is authorized by IEEE 802.1x
@field  uint8 | age | Specify age time, has effect only on unicast MAC and isStatic is set as FALSE
@field  uint8 | enBlk | Enable block(the packets from source MAC address ID of this entry will be dropped), only has effect on multicast MAC or unicast MAC & isStatic is set as TRUE.
@field  uint32 | dip |For IP Multicast IGMPv3 Entry only, destination IP
@field  uint32 | sip |For IP Multicast IGMPv3 Entry only, soure IP
@field  uint16 |gmitv_p[3] |  group member interval
@rvalue SUCCESS
@rvalue FAILED
@comm
asic LUT has 1024 entry total, this API could get each entry content
*/

int8 rtl8363_getAsicLUT(uint32 entry, LUTPara_t * pLut)
{
    uint16 regval, bitval;
    uint32 pollcnt;
    uint8 cntprd; /*Group member interval granularity*/

    if ((entry > 1023) || (pLut == NULL))
        return FAILED;

    /*Enable lookup table access*/
    rtl8363_setAsicRegBit(0, 21, 0, MACPAG, 0, 1);

    /*Trigger a command to read lookup table*/
    regval = 0x1 | ((entry & 0x3) << 2) | ((entry >> 2) << 8);
    rtl8363_setAsicReg(0, 20, MACPAG, 0, regval);
    rtl8363_setAsicRegBit(0, 20, 1, MACPAG, 0, 1);

    /*Waiting until read operation is finished */
    for (pollcnt = 0; pollcnt < RTL8363_IDLE_TIMEOUT; pollcnt ++)
    {
        rtl8363_getAsicRegBit(0, 20, 1, MACPAG, 0, &bitval);
        if (!bitval)
            break;
    }
    if (pollcnt == RTL8363_IDLE_TIMEOUT)
        return FAILED;
    /*Access Data[71:56]*/
    rtl8363_getAsicReg(0, 25, MACPAG, 0, &regval);
    if (regval & LUT_IP_MULT)
    {
            /*read ENSIPSR (PHY 2 Reg 17.5, MAC page 0 to see it is IP multicast
			entry for IGMPv3 or for Hardware IGMP/MLD snooping)*/
            rtl8363_getAsicRegBit(2, 17, 5, MACPAG, 0, &bitval);
            if (bitval)
            {
                /*IP multicast entry for IGMPv3*/
                pLut->type = LUT_IGMPV3;
                pLut->port = (regval & LUT_MBR_MASK) >> LUT_MBR_OFFSET;
                pLut->dip = 0xE0000000 | ((regval & 0xF) << 24); /*only dip highest 8bits*/
                rtl8363_getAsicReg(0, 24, MACPAG, 0, &regval);
                pLut->dip |= (regval << 8);
                rtl8363_getAsicReg(0, 23, MACPAG, 0, &regval);
                pLut->dip |= ((regval & 0xFF00) >> 8);
                pLut->sip = (regval & 0xFF) << 24;
                rtl8363_getAsicReg(0, 22, MACPAG, 0, &regval);
                pLut->sip |= (regval << 8);
                rtl8363_getAsicReg(0, 21, MACPAG, 0, &regval);
                pLut->sip |= ((regval & 0xFF00) >> 8);
            }
            else
            {
                /*IP multicast entry for Hardware IGMP/MLD snooping*/
                pLut->type = LUT_HDSNP;
                rtl8363_getAsicRegBit(2, 17, 3, MACPAG, 0, &bitval);
                cntprd = bitval ? 8:4;
                pLut->gmitv_p[2] = ((regval & LUT_GMITV_P2_MASK) >> LUT_GMITV_P2_OFFSET) *cntprd ;
                pLut->gmitv_p[1] = (regval & 0x3F) << 1; /*only highest 6 bit*/
                rtl8363_getAsicReg(0, 24, MACPAG, 0, &regval);
                pLut->gmitv_p[1] |= (regval & 0x8000) >> 15;
                pLut->gmitv_p[1] *= cntprd;
                pLut->gmitv_p[0] = ((regval & LUT_GMITV_P0_MASK) >> LUT_GMITV_P0_OFFSET);
                pLut->gmitv_p[0] *=cntprd ;
                pLut->mac[5] = (regval & LUT_MAC5_MASK) >> LUT_MAC5_OFFSET;
                rtl8363_getAsicReg(0, 23, MACPAG, 0, &regval);
                pLut->mac[4] = (regval & LUT_MAC4_MASK) >> LUT_MAC4_OFFSET;
                pLut->mac[3] = (regval & LUT_MAC3_MASK) >> LUT_MAC3_OFFSET;
                rtl8363_getAsicReg(0, 22, MACPAG, 0, &regval);
                pLut->mac[2] = (regval & LUT_MAC2_MASK) >> LUT_MAC2_OFFSET;
                pLut->mac[1] = (regval & LUT_MAC1_MASK) >> LUT_MAC1_OFFSET;
                rtl8363_getAsicReg(0, 21, MACPAG, 0, &regval);
                pLut->mac[0] = (regval & LUT_MAC0_MASK) >> LUT_MAC0_OFFSET;
            }

    }
    else
    {
            pLut->type = (regval & LUT_STATIC) ? (LUT_L2STA):(LUT_L2DYN);
            pLut->isStatic = (regval & LUT_STATIC) ? TRUE:FALSE;
            pLut->isAuth = (regval & LUT_AUTH) ? TRUE:FALSE;
            if(pLut->type == LUT_L2STA)
            {
                /*static layer2 entry*/
                pLut->enblk  = (regval & LUT_BLK) ? TRUE:FALSE;
                pLut->port = (regval & LUT_MBR_MASK) >> LUT_MBR_OFFSET;
            }
            else
            {
               /*dynamic layer2 entry*/
               pLut->port = (regval & LUT_SPA_MASK) >> LUT_SPA_OFFSET;
	        pLut->age = (regval & LUT_AGE_MASK) >> LUT_AGE_OFFSET;
            }

            rtl8363_getAsicReg(0, 24, MACPAG, 0, &regval);
            pLut->mac[5] = (regval & LUT_MAC5_MASK) >> LUT_MAC5_OFFSET;
            rtl8363_getAsicReg(0, 23, MACPAG, 0, &regval);
            pLut->mac[4] = (regval & LUT_MAC4_MASK) >> LUT_MAC4_OFFSET;
            pLut->mac[3] = (regval & LUT_MAC3_MASK) >> LUT_MAC3_OFFSET;
            rtl8363_getAsicReg(0, 22, MACPAG, 0, &regval);
            pLut->mac[2] = (regval & LUT_MAC2_MASK) >> LUT_MAC2_OFFSET;
            pLut->mac[1] = (regval & LUT_MAC1_MASK) >> LUT_MAC1_OFFSET;
            rtl8363_getAsicReg(0, 21, MACPAG, 0, &regval);
            pLut->mac[0] = (regval & LUT_MAC0_MASK) >> LUT_MAC0_OFFSET;

    }

    /*Disable lookup table access*/
    rtl8363_setAsicRegBit(0, 21, 0, MACPAG, 0, 0);
    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicCPUPort | Specify Asic CPU port.
@parm uint8 | port | Specify the port.
@parm uint8 | enTag | CPU tag insert or not.
@rvalue SUCCESS
@rvalue FAILED
@comm
If the port is specified RTL8363_NOCPUPORT, it means that no port is assigned as cpu port
*/

int8 rtl8363_setAsicCPUPort(uint8 port, uint8 enTag)
{
    uint16 regval;

    if (port > RTL8363_NOCPUPORT)
		return FAILED;
    rtl8363_getAsicReg(2, 20, MACPAG, 0, &regval);
    regval &= ~RTL8363_CPUPORT_MASK;
    regval |= (port & 0x3) << RTL8363_CPUPORT_OFFSET;
    regval |= RTL8363_CPUTAGRM | RTL8363_CPUTAGAW;
    regval &= ~RTL8363_CPUTAGIST;
    regval |= (enTag ? RTL8363_CPUTAGIST: 0);
    rtl8363_setAsicReg(2, 20, MACPAG, 0, regval);

    /*Disable IEEE802.1x function of CPU Port*/
    if (port < RTL8363_NOCPUPORT )
   {
        rtl8363_getAsicReg(0, 19, MACPAG, 0, &regval);
        regval &= ~(1 << (8 + port));
        rtl8363_setAsicReg(0, 19, MACPAG, 0, regval);
    }

    return SUCCESS;
}

/*
@func int8 | rtl8363_getAsicCPUPort | Get Asic CPU port.
@parm uint8* | port |  Which port is cpu port.
@parm uint8* | enTag | CPU tag insert or not.
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_getAsicCPUPort(uint8 *pPort, uint8 *pEntag)
{
    uint16 regval;

    if ((pPort == NULL) || (pEntag == NULL))
        return FAILED;
    rtl8363_getAsicReg(2, 20, MACPAG, 0, &regval);
    *pPort = (regval & RTL8363_CPUPORT_MASK) >> RTL8363_CPUPORT_OFFSET;
    *pEntag = (regval & RTL8363_CPUTAGIST) ? TRUE: FALSE;

    return SUCCESS;
}


/*
@func int8 | rtl8363_setAsicHWSNPEnable | enable hardware snooping
@parm uint8 | type | Specify RTL8363_IGMPSNP(hardware IGMPv1/v2 snooping) or RTL8363_MLDSNP(hardware MLDv1 snooping )
@parm SNPCfg_t | snp
@struct SNPCfg_t | This structure hardware snooping parameter
@field uint8 | ensnp | enable/disable hardware snooping
@field uint8 | enfastlv | enable/disable fast leave for hardware snooping.
@field uint8 | adpm | The added port mask. Record the user assigned member port and router port for all the groups in snooping
@rvalue SUCCESS
@rvalue FAILED
@comm
if enable fast leave, a group address will delete the port from its member port immediately when receiving leave message.
from the port. control messages will also forward to adpm which is specified by user.
*/
int8 rtl8363_setAsicHWSNPEnable(uint8 type, SNPCfg_t snp)
{
    uint16 regval;

    if (type == RTL8363_IGMPSNP)
    {
        rtl8363_setAsicRegBit(2, 17, 7, MACPAG, 0, snp.ensnp ? 1:0);
        rtl8363_setAsicRegBit(2, 17, 15, MACPAG, 0, snp.enfastlv ? 1:0);
        rtl8363_getAsicReg(2, 17, MACPAG, 0, &regval);
        regval &= ~(0x7 << 12);
        regval |= (snp.adpm & 0x7) << 12;
        rtl8363_setAsicReg(2, 17, MACPAG, 0, regval);
    }
    else if (type == RTL8363_MLDSNP)
    {
        rtl8363_setAsicRegBit(2, 17, 6, MACPAG, 0, snp.ensnp ? 1:0);
        rtl8363_setAsicRegBit(2, 19, 7, MACPAG, 0, snp.enfastlv ? 1:0);
        rtl8363_getAsicReg(2, 19, MACPAG, 0, &regval);
        regval &= ~(0x7 << 4);
        regval |= (snp.adpm & 0x7) << 4;
        rtl8363_setAsicReg(2, 17, MACPAG, 0, regval);
    }
    /*if enable hardware snp, diable ENSIPSR*/
    if (snp.ensnp)
    {
        rtl8363_setAsicRegBit(2, 17, 5, MACPAG, 0, 0);
    }

    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicHWSNPEnable | get hardware snooping enable or disabled
@parm uint8 | type | Specify RTL8363_IGMPSNP(hardware IGMPv1/v2 snooping) or RTL8363_MLDSNP(hardware MLDv1 snooping )
@parm SNPCfg_t* | snp
@struct SNPCfg_t | This structure hardware snooping parameter
@field uint8 | ensnp | enable/disable hardware snooping
@field uint8 | enfastlv | enable/disable fast leave for hardware snooping.
@field uint8 | adpm | The added port mask. Record the user assigned member port and router port for all the groups in snooping
@rvalue SUCCESS
@rvalue FAILED
@comm
if enable fast leave, a group address will delete the port from its member port immediately when receiving leave message.
from the port. control messages will also forward to adpm which is specified by user.
*/

int8 rtl8363_getAsicHWSNPEnable(uint8 type, SNPCfg_t *pSnp)
{
    uint16 bitval;

    if (type == RTL8363_IGMPSNP)
    {
        rtl8363_getAsicRegBit(2, 17, 7, MACPAG, 0, &bitval);
        pSnp->ensnp = bitval ? TRUE:FALSE;
        rtl8363_getAsicRegBit(2, 17, 15, MACPAG, 0, &bitval);
        pSnp->enfastlv = bitval ? TRUE:FALSE;
        rtl8363_getAsicReg(2, 17, MACPAG, 0, &bitval);
        pSnp->adpm = (bitval & IGMP_ADPM_MASK) >> IGMP_ADPM_OFFSET;
    }
    else if (type == RTL8363_MLDSNP)
    {
       rtl8363_getAsicRegBit(2, 17, 6, MACPAG, 0, &bitval);
       pSnp->ensnp = bitval ? TRUE:FALSE;
       rtl8363_getAsicRegBit(2, 19, 7, MACPAG, 0, &bitval);
       pSnp->enfastlv = bitval ? TRUE:FALSE;
       rtl8363_getAsicReg(2, 19, MACPAG, 0, &bitval);
       pSnp->adpm = (bitval & MLD_ADPM_MASK) >> MLD_ADPM_OFFSET;
    }
    /*check if enable ENSIPSR*/
    rtl8363_getAsicRegBit(2, 17, 5, MACPAG, 0, &bitval);
    if (bitval)
    {
        pSnp->ensnp = FALSE;
    }
    return SUCCESS;

}


/*
@func int8 | rtl8363_setAsicHWSNP | set hardware snooping parameter
@parm uint8 | type | Specify RTL8363_IGMPSNP(hardware IGMPv1/v2 snooping) or RTL8363_MLDSNP(hardware MLDv1 snooping )
@parm SNPPara_t | snpcfg
@struct SNPPara_t | This structure hardware snooping parameter
@field uint8 | GQI | General query interval in unit of second, default is 125s
@field uint8 | RV | Robustness value(1~4), default is 2.
@field uint8 | LMQC | Last Member Query(1~4) Count, default is 2
@field uint8 | ENTAGIM |enable or disable snooping the CPU tagged IGMP/MLD packet in the hardware snooping, default is enabled;
@field uint8 | CNTPRD | Group member interval granularity 4s(CNTPRD_4S) or 8s(CNTPRD_8S), default is 4s
@field uint8 | enUCMLD | enable regard the uncertain MLD packet as MLD packet for forwarding control, default is disabled
@field uint8 | V1MAXRSP | default MAX response time in unit of 100ms, default is 100
@rvalue SUCCESS
@rvalue FAILED
@comm
This API is used to modify igmp/mld protocol varable, ic default setting is accroding to igmp/mld procol default
value. so if you have no special requirement, rtl8363_setAsicHWSNP need not be called.
*/
int8 rtl8363_setAsicHWSNP(uint8 type, SNPPara_t snpcfg)
{
    uint16 regval;


    if (type == RTL8363_IGMPSNP)
    {
        rtl8363_getAsicReg(2, 17, MACPAG, 0, &regval);
        regval &= 0xF0E7;
        regval |= (snpcfg.ENTAGIM ? HWSNP_ENTAGIM :0) | (snpcfg.CNTPRD ? HWSNP_CNTPRD:0);
        regval |= ((snpcfg.RV-1) & 0x3) << IGMP_RV_OFFSET;
        regval |= ((snpcfg.LMQC-1) & 0x3) << IGMP_LMQC_OFFSET;
        rtl8363_setAsicReg(2, 17, MACPAG, 0, regval);
        regval = snpcfg.V1MAXRSP | (snpcfg.GQI << IGMP_GQI_OFFSET);
        rtl8363_setAsicReg(2, 18, MACPAG, 0, regval);
    }
    else if (type == RTL8363_MLDSNP)
    {

        rtl8363_getAsicReg(2, 17, MACPAG, 0, &regval);
        regval &= 0xFFE3;
        regval |= (snpcfg.enUCMLD ? 0:(HWSNP_DISUCMLD)) | (snpcfg.CNTPRD ? HWSNP_CNTPRD:0);
        rtl8363_setAsicReg(2, 17, MACPAG, 0, regval);
        rtl8363_getAsicReg(2, 19, MACPAG, 0, &regval);
        regval &= 0x70;
        regval |= ((snpcfg.RV-1) & 0x3) <<  MLD_RV_OFFSET;
        regval |= ((snpcfg.LMQC -1) & 0x3) << MLD_LMQC_OFFSET;
        regval |= (snpcfg.GQI << MLD_GQI_OFFSET );
        rtl8363_setAsicReg(2, 19, MACPAG, 0, regval);

    }
    return SUCCESS;
}

/*
@func int8 | rtl8363_getAsicHWSNP | get hardware snooping parameter
@parm uint8 | type | Specify RTL8363_IGMPSNP(hardware IGMPv1/v2 snooping) or RTL8363_MLDSNP(hardware MLDv1 snooping )
@parm SNPPara_t | snpcfg
@struct SNPPara_t *| This structure hardware snooping parameter
@field uint8 | GQI | General query interval in unit of second, default is 125s
@field uint8 | RV | Robustness value(1~4), default is 2.
@field uint8 | LMQC | Last Member Query(1~4) Count, default is 2
@field uint8 | ENTAGIM |enable or disable snooping the CPU tagged IGMP/MLD packet in the hardware snooping, default is enabled;
@field uint8 | CNTPRD | Group member interval granularity 4s(CNTPRD_4S) or 8s(CNTPRD_8S), default is 4s
@field uint8 | enUCMLD | enable regard the uncertain MLD packet as MLD packet for forwarding control, default is disabled
@field uint8 | V1MAXRSP | default MAX response time in unit of 100ms, default is 100
@rvalue SUCCESS
@rvalue FAILED
@comm
This API is used to modify igmp/mld protocol varable, ic default setting is accroding to igmp/mld procol default
value. so if you have no special requirement, rtl8363_setAsicHWSNP need not be called.
*/

int8 rtl8363_getAsicHWSNP(uint8 type, SNPPara_t* pSnpcfg)
{
    uint16 regval;

    if (type == RTL8363_IGMPSNP)
    {
        rtl8363_getAsicReg(2, 17, MACPAG, 0, &regval);
        pSnpcfg->ENTAGIM = (regval & HWSNP_ENTAGIM) ? TRUE:FALSE;
        pSnpcfg->CNTPRD = (regval & HWSNP_CNTPRD) ? 1:0;
        pSnpcfg->RV = ((regval & IGMP_RV_MASK) >> (IGMP_RV_OFFSET)) + 1;
        pSnpcfg->LMQC = ((regval & IGMP_LMQC_MASK) >> IGMP_LMQC_OFFSET) + 1;
        rtl8363_getAsicReg(2, 18, MACPAG, 0, &regval);
        pSnpcfg->GQI = (regval & IGMP_GQI_MASK) >> IGMP_GQI_OFFSET;
        pSnpcfg->V1MAXRSP = regval & IGMP_V1MAXRSP;

    }
    else if (type == RTL8363_MLDSNP)
    {
        rtl8363_getAsicReg(2, 17, MACPAG, 0, &regval);
        pSnpcfg->CNTPRD = (regval & HWSNP_CNTPRD) ? 1:0;
        pSnpcfg->enUCMLD = (regval & HWSNP_DISUCMLD) ? FALSE:TRUE;
        rtl8363_getAsicReg(2, 19, MACPAG, 0, &regval);
        pSnpcfg->RV = ((regval & MLD_RV_MASK) >> MLD_RV_OFFSET) + 1;
        pSnpcfg->LMQC = ((regval & MLD_LMQC_MASK) >> MLD_LMQC_OFFSET) + 1;
        pSnpcfg->GQI = (regval & MLD_GQI_MASK) >> MLD_GQI_OFFSET;
    }

    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicQosPortRate | Set asic port bandwidth control
@parm uint8 | port | Specify port number(0~2)
@parm uint8 | direction | input or output port bandwidth control
@parm QosPRatePara_t | portRate | Specify port rate parameter
@struct QosPRatePara_t | This structure describe port rate parameter
@field uint8 | enabled | Enable or disable rate limit
@field uint16 | rate | Rate of the port in times of 64Kbps(1~0x4000)
@rvalue SUCCESS
@rvalue FAILED
@comm
two MACRO to define direction:RTL8363_PORT_RX means input port bandwidth control, RTL8363_PORT_RX
means output port bandwidth control.
*/
int8 rtl8363_setAsicQosPortRate(uint8 port, uint8 direction, QosPRatePara_t portRate)
{
    uint16 regval;

    if ((port > RTL8363_PORT2) || (portRate.rate> RTL8363_MAXRATE) || ((direction > RTL8363_PORT_TX)))
        return FAILED;

    /*register value 0 means 1*64kbps*/
    portRate.rate --;

    if (direction == RTL8363_PORT_RX)
    {

         if (!portRate.enabled)
        {
            rtl8363_setAsicRegBit(port, 29, 15, MACPAG, 2, 1);
        }
         else
        {
            rtl8363_getAsicReg(port, 29, MACPAG, 2, &regval);
            regval &= ~0x3FFF;
            regval |=   portRate.rate & 0x3FFF;
            rtl8363_setAsicReg(port, 29, MACPAG, 2, regval);
            rtl8363_setAsicRegBit(port, 29, 15, MACPAG, 2, 0);
         }

    }
    else if (direction == RTL8363_PORT_TX)
    {
        if (!portRate.enabled)
        {
            rtl8363_setAsicRegBit(port, 26, 2, MACPAG, 2, 0);
        }
        else
        {
            rtl8363_getAsicReg(port, 25, MACPAG, 2, &regval);
            regval &= ~0x3FFF;
            regval |= portRate.rate & 0x3FFF;
            rtl8363_setAsicReg(port, 25, MACPAG, 2, regval);
            rtl8363_setAsicRegBit(port, 26, 2, MACPAG, 2, 1);
        }

    }
    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicQosPortRate | Get asic port bandwidth control
@parm uint8 | port | Specify port number(0~2)
@parm uint8 | direction | input or output port bandwidth control
@parm QosPRatePara_t* | pPortRate | Specify port rate parameter
@struct QosPRatePara_t | This structure describe port rate parameter
@field uint8 | enabled | Enable or disable rate limit
@field uint16 | rate | Rate of the port in times of 64Kbps
@rvalue SUCCESS
@rvalue FAILED
@comm
two MACRO to define direction:RTL8363_PORT_RX means input port bandwidth control, RTL8363_PORT_RX
means output port bandwidth control.
*/

int8 rtl8363_getAsicQosPortRate(uint8 port, uint8 direction, QosPRatePara_t *pPortRate )
{
    uint16 regval;

    if (pPortRate == NULL)
        return FAILED;

    if (direction == RTL8363_PORT_RX)
    {

        rtl8363_getAsicReg(port, 29, MACPAG, 2, &regval);
        pPortRate->enabled = (regval & (0x1 << 15)) ? FALSE:TRUE;
        //pPortRate->enIPGPRE = (regval & (0x1 << 14)) ? TRUE:FALSE;
        pPortRate->rate = regval & 0x3FFF;

    }
    else if (direction == RTL8363_PORT_TX)
    {

        rtl8363_getAsicReg(port, 26, MACPAG, 2, &regval);
        pPortRate->enabled = (regval & (0x1 << 2)) ? TRUE:FALSE;
        //pPortRate->enIPGPRE = (regval & (0x1 << 3)) ? FALSE:TRUE;
        rtl8363_getAsicReg(port, 25, MACPAG, 2, &regval);
        pPortRate->rate = regval & 0x3FFF;

    }
   /*register value 0 means 1*64kbps*/
   pPortRate->rate ++;

    return SUCCESS;
}


/*
@func int8 | rtl8363_setAsicQosPortScheduling | Set Tx port scheduling parameter
@parm uint8 | port | Specify port number(0~2)
@parm QosPSchePara_t* | portSch | Specify port scheduling  parameter
@struct QosPSchePara_t | This structure describe port scheduling parameter
@field uint8 | q3_enstrpri | enable q3 strict priority
@field uint8 | q2_enstrpri | enable q3 strict priority
@field uint8 | q3_wt | queue 3 weight(1~127)
@field uint8 | q2_wt | queue 2 weight(1~127)
@field uint8 | q1_wt | queue 1 weight(1~127)
@field uint8 | q0_wt | queue 0 weight(1~127)
@field uint8 | q3_enLB | enable queue 3 rate limit
@field uint8 | q2_enLB | enable queue 2 rate limit
@field uint8 | q3_bursize | set queue 3 burst size(0~48), burstsize uinit is KB, so Max burst size is 48KB
@field uint8 | q2_bursize | set queue 2 burst size(0~48), burstsize uinit is KB, so Max burst size is 48KB
@field uint16 | q3_rate | set queue 3 rate, in times of 64Kbps(1~0x4000, 1Gbps, 1 means 64kbps)
@field uint16 | q2_rate | set queue 3 rate, in times of 64Kbps(1~0x4000, 1Gbps, 1means 64kbps)
@rvalue SUCCESS
@rvalue FAILED
@comm
*/
int8 rtl8363_setAsicQosPortScheduling(uint8 port, QosPSchePara_t portSch)
{
    uint16 regval;

    /*set queue weight and strict priority*/
    regval = (portSch.q3_wt & 0x7F) | (portSch.q3_enstrpri ? (0x1 << 7) :0);
    regval |=  (portSch.q2_wt & 0x7F) << 8 | (portSch.q2_enstrpri ? (0x1 << 15) :0);
    rtl8363_setAsicReg(port, 17, MACPAG, 2, regval);

    rtl8363_getAsicReg(port, 18, MACPAG, 2, &regval);
    regval &= 0x8080;
    regval |= (portSch.q1_wt & 0x7F) | ((portSch.q0_wt & 0x7F) << 8);
    rtl8363_setAsicReg(port, 18, MACPAG, 2, regval);

    /*set q3,q2 queue rate*/
    if (!portSch.q3_enLB)
    {
        rtl8363_setAsicRegBit(port, 26, 1, MACPAG, 2, 0);
    }
    else
    {
        portSch.q3_rate --; /*hardware 0 represent 1*64Kbps*/
        rtl8363_setAsicRegBit(port, 26, 1, MACPAG, 2, 1);
        rtl8363_getAsicReg(port, 20, MACPAG, 2, &regval);
        regval &= 0xC000;
        regval |= portSch.q3_rate & 0x3FFF;
        rtl8363_setAsicReg(port, 20, MACPAG, 2, regval);

        rtl8363_getAsicReg(port, 19, MACPAG, 2,&regval);
        regval &= ~0x3F;
        regval |= portSch.q3_bursize & 0x3F;
        rtl8363_setAsicReg(port, 19, MACPAG, 2, regval);
    }

    if (!portSch.q2_enLB)
    {
        rtl8363_setAsicRegBit(port, 26, 0, MACPAG, 2, 0);
    }
    else
    {
        portSch.q2_rate --; /*hardware 0 represent 1*64Kbps*/
        rtl8363_setAsicRegBit(port, 26, 0, MACPAG, 2, 1);
        rtl8363_getAsicReg(port, 21, MACPAG, 2, &regval);
        regval &= 0xC000;
        regval |= portSch.q2_rate & 0x3FFF;
        rtl8363_setAsicReg(port, 21, MACPAG, 2, regval);

        rtl8363_getAsicReg(port, 19, MACPAG, 2,&regval);
        regval &= ~(0x3F << 8);
        regval |= (portSch.q2_bursize & 0x3F) << 8;
        rtl8363_setAsicReg(port, 19, MACPAG, 2, regval);

    }

    return SUCCESS;
}


/*
@func int8 | rtl8363_setAsicQosPortScheduling | Set Tx port scheduling parameter
@parm uint8 | port | Specify port number(0~2)
@parm QosPSchePara_t* | portSch | Specify port scheduling  parameter
@struct QosPSchePara_t | This structure describe port scheduling parameter
@field uint8 | q3_enstrpri | enable q3 strict priority
@field uint8 | q2_enstrpri | enable q3 strict priority
@field uint8 | q3_wt | queue 3 weight(1~127)
@field uint8 | q2_wt | queue 2 weight(1~127)
@field uint8 | q1_wt | queue 1 weight(1~127)
@field uint8 | q0_wt | queue 0 weight(1~127)
@field uint8 | q3_enLB | enable queue 3 rate limit
@field uint8 | q2_enLB | enable queue 2 rate limit
@field uint8 | q3_bursize | set queue 3 burst size(0~48), burstsize uinit is KB, so Max burst size is 48KB
@field uint8 | q2_bursize | set queue 2 burst size(0~48), burstsize uinit is KB, so Max burst size is 48KB
@field uint16 | q3_rate | set queue 3 rate, in times of 64Kbps(1~0x4000, 1Gbps)
@field uint16 | q2_rate | set queue 3 rate, in times of 64Kbps(1~0x4000, 1Gbps)
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_getAsicQosPortScheduling(uint8 port, QosPSchePara_t* pPortSch )
{
    uint16 regval;

    rtl8363_getAsicReg(port, 17, MACPAG, 2, &regval);
    pPortSch->q3_enstrpri = (regval & (0x1 << 7)) ? TRUE:FALSE;
    pPortSch->q3_wt = (regval & 0x7F);
    pPortSch->q2_enstrpri = (regval & (0x1 << 15)) ? TRUE:FALSE;
    pPortSch->q2_wt = (regval & 0x7F00) >> 8;

    rtl8363_getAsicReg(port, 18, MACPAG, 2, &regval);
    pPortSch->q1_wt = regval & 0x7F;
    pPortSch->q0_wt = (regval & 0x7F00) >> 8;

    rtl8363_getAsicReg(port, 19, MACPAG, 2, &regval);
    pPortSch->q3_bursize = regval & 0x3F;
    pPortSch->q2_bursize = (regval & 0x3F00) >> 8;

    rtl8363_getAsicReg(port, 20, MACPAG, 2, &regval);
    pPortSch->q3_rate = regval & 0x3FFF;
    pPortSch->q3_rate ++;/*hardware 0 represent 1*64Kbps*/
    rtl8363_getAsicReg(port, 21, MACPAG, 2, &regval);
    pPortSch->q2_rate = regval & 0x3FFF;
    pPortSch->q2_rate ++;

    rtl8363_getAsicReg(port, 26, MACPAG, 2, &regval);
    pPortSch->q3_enLB = (regval & 0x2) ? TRUE:FALSE;
    pPortSch->q2_enLB = (regval & 0x1) ? TRUE:FALSE;


    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicQosPortQueueNum | Set port Tx queue number
@parm uint8 | num | Tx queue numbers (1 ~ 4)
@rvalue SUCCESS
@rvalue FAILED
@comm
*/
int8 rtl8363_setAsicQosPortQueueNum(uint8 port, uint8 qnum)
{
    uint16 regval;

    if ((port > RTL8363_PORT2) || (qnum > 4))
        return FAILED;
    qnum --;
    rtl8363_getAsicReg(1, 18, MACPAG, 4, &regval);
    regval &= ~(0x3 << (port*2));
    regval |= (qnum & 0x3) << (port*2);
    rtl8363_setAsicReg(1, 18, MACPAG, 4, regval);

    /*software reset*/
    rtl8363_setAsicRegBit(0, 16, 15, MACPAG, 0, 1);
    return SUCCESS;
}

/*
@func int8 | rtl8363_getAsicQosPortQueueNum | Get port Tx queue number
@parm uint8* | num | Tx queue numbers
*/

int8 rtl8363_getAsicQosPortQueueNum(uint8 port, uint8 *pQnum)
{
    uint16 regval;

    rtl8363_getAsicReg(1, 18, MACPAG, 4, &regval);
    *pQnum = (regval & (0x3 << (port*2))) >> (port*2);
    *pQnum += 1;
    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicQosPrioritytoQIDMapping | Set packet priority to QID mapping
@parm uint8 | port | port number(0~2)
@parm QosPri2QIDMap_t | priqid | Specify priority to QID mapping
@struct QosPri2QIDMap_t | This structure describe priority to QID mapping
@field uint8 | qid_pri3 | packet with priority 3 mapping to which queue(0~3)
@field uint8 | qid_pri2 | packet with priority 2 mapping to which queue(0~3)
@field uint8 | qid_pri1 | packet with priority 1 mapping to which queue(0~3)
@field uint8 | qid_pri0 | packet with priority 0 mapping to which queue(0~3)
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_setAsicQosPrioritytoQIDMapping(uint8 port, QosPri2QIDMap_t priqid)
{
    uint16 regval;

    switch(port)
    {
        case RTL8363_PORT0:
            rtl8363_getAsicReg(1, 24, MACPAG, 4, &regval);
            regval &= 0xFF00;
            regval |= (priqid.qid_pri0 & 0x3) | (priqid.qid_pri1 & 0x3) << 2;
            regval |= (priqid.qid_pri2 & 0x3) << 4 | (priqid.qid_pri3 & 0x3) << 6;
            rtl8363_setAsicReg(1, 24, MACPAG, 4, regval);
            break;
        case RTL8363_PORT1:
            rtl8363_getAsicReg(1, 24, MACPAG, 4, &regval);
            regval &= 0xFF;
            regval |= (priqid.qid_pri0 & 0x3) << 8 | (priqid.qid_pri1 & 0x3) << 10;
            regval |= (priqid.qid_pri2 & 0x3) << 12 | (priqid.qid_pri3 & 0x3) << 14;
            rtl8363_setAsicReg(1, 24, MACPAG, 4, regval);
            break;
        case RTL8363_PORT2:
            rtl8363_getAsicReg(1, 25, MACPAG, 4, &regval);
            regval &= 0xFF00;
            regval |= (priqid.qid_pri0 & 0x3) | (priqid.qid_pri1 & 0x3) << 2;
            regval |= (priqid.qid_pri2 & 0x3) << 4 | (priqid.qid_pri3 & 0x3) << 6;
            rtl8363_setAsicReg(1, 25, MACPAG, 4, regval);
            break;
        default :
            return FAILED;

    }

    return SUCCESS;
}

/*
@func int8 | rtl8363_getAsicQosPrioritytoQIDMapping | Get packet priority to QID mapping
@parm uint8 | port | port number(0~2)
@parm QosPri2QIDMap_t* | pPriqid | Specify priority to QID mapping
@struct QosPri2QIDMap_t | This structure describe priority to QID mapping
@field uint8 | qid_pri3 | packet with priority 3 mapping to which queue(0~3)
@field uint8 | qid_pri2 | packet with priority 2 mapping to which queue(0~3)
@field uint8 | qid_pri1 | packet with priority 1 mapping to which queue(0~3)
@field uint8 | qid_pri0 | packet with priority 0 mapping to which queue(0~3)
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_getAsicQosPrioritytoQIDMapping(uint8 port, QosPri2QIDMap_t *priqid)
{
    uint16 regval;

    switch(port)
    {
        case RTL8363_PORT0:
            rtl8363_getAsicReg(1, 24, MACPAG, 4, &regval);
            priqid->qid_pri0 = regval & 0x3;
            priqid->qid_pri1 = (regval & (0x3 << 2)) >> 2;
            priqid->qid_pri2 = (regval & (0x3 << 4)) >> 4;
            priqid->qid_pri3 = (regval & (0x3 << 6)) >> 6;
            break;
        case RTL8363_PORT1:
            rtl8363_getAsicReg(1, 24, MACPAG, 4, &regval);
            priqid->qid_pri0 = (regval & (0x3 << 8)) >> 8;
            priqid->qid_pri1 = (regval & (0x3 << 10)) >> 10;
            priqid->qid_pri2 = (regval & (0x3 << 12)) >> 12;
            priqid->qid_pri3 = (regval & (0x3 << 14)) >> 14;
            break;
        case RTL8363_PORT2:
            rtl8363_getAsicReg(1, 25, MACPAG, 4, &regval);
            priqid->qid_pri0 = regval & 0x3;
            priqid->qid_pri1 = (regval & (0x3 << 2)) >> 2;
            priqid->qid_pri2 = (regval & (0x3 << 4)) >> 4;
            priqid->qid_pri3 = (regval & (0x3 << 6)) >> 6;
            break;

        default:
            return FAILED;
    }

    return SUCCESS;
}

/*
@func int8 | rtl8363_setPrioritySourceArbit | Set priority source arbitration level
@parm QosPriAssign_t | priassign
@struct QosPriAssign_t | The structure describe levels of 4 kinds of priority
@field uint8 | lev_pbp | ACL-Based priority's level
@field uint8 | lev_dscp | DSCP-Based priority's level
@field uint8 | lev_1qbp | 1Q-based priority's level
@field uint8 | lev_pbp | Port-based priority's level
@rvalue SUCCESS
@rvalue FAILED
@comm
there are 4 type priorities could be set priority level, they are ACL-based  priority,
DSCP-based priority, 1Q-based priority,Port-based priority, each one could be set level from 0 to 4,
arbitration module will decide their sequece to take, the highest level priority will be adopted at first,
then  priority type of the sencond highest level. priority with level 0 will not be recognized any more.
*/

int8 rtl8363_setAsicQosPrioritySourceArbit(QosPriAssign_t priassign)
{
    uint16 regval;

    rtl8363_getAsicReg(1, 23, MACPAG, 4, &regval);
    regval &= 0x00FF;
    priassign.lev_pbp &= 0x3;
    priassign.lev_1qbp &= 0x3;
    priassign.lev_dscp &= 0x3;
    priassign.lev_acl &= 0x3;
    regval |= (priassign.lev_pbp << 14 ) | (priassign.lev_1qbp << 12) ;
    regval |= (priassign.lev_dscp << 10) | (priassign.lev_acl << 8);
    rtl8363_setAsicReg(1, 23, MACPAG, 4, regval);

    return SUCCESS;
}

/*
@func int8 | rtl8363_setPrioritySourceArbit | Set priority source arbitration level
@parm QosPriAssign_t | priassign
@struct QosPriAssign_t | The structure describe levels of 4 kinds of priority
@field uint8 | lev_pbp | ACL-Based priority's level
@field uint8 | lev_dscp | DSCP-Based priority's level
@field uint8 | lev_1qbp | 1Q-based priority's level
@field uint8 | lev_pbp | Port-based priority's level
@rvalue SUCCESS
@rvalue FAILED
@comm
there are 4 type priorities could be set priority level, they are ACL-based  priority,
DSCP-based priority, 1Q-based priority,Port-based priority, each one could be set level from 0 to 4,
arbitration module will decide their sequece to take, the highest level priority will be adopted at first,
then  priority type of the sencond highest level. priority with level 0 will not be recognized any more.
*/

int8 rtl8363_getAsicQosPrioritySourceArbit(QosPriAssign_t *pPriassign)
{
    uint16 regval;

    rtl8363_getAsicReg(1, 23, MACPAG, 4, &regval);
    pPriassign->lev_pbp = (regval & 0xC000) >> 14;
    pPriassign->lev_1qbp = (regval & 0x3000) >> 12;
    pPriassign->lev_dscp = (regval & 0xC00) >> 10;
    pPriassign->lev_acl = (regval & 0x300) >> 8;

    return SUCCESS;
}


/*
@func int8 | rtl8363_setPrioritySourcePriority | Set priority source value
@parm uint8 | pri_type | Specify priority source type
@parm uint8 | pri_obj | Specify priority object
@parm uint8 | pri_val | Priority value:RTL8363_PRIO0~RTL8363_PRIO3
@rvalue | SUCCESS
@rvalue | FAILED
@comm
This function could set port-based priority for port n(RTL8363_PBP_PRIO)<nl>
802.1Q based default priority for port n(RTL8363_1QDEFAULT_PRIO)<nl>
802.1Q tag 3 bit priority to 2 bit priority mapping(RTL8363_1QTAG_PRIO)<nl>
DSCP based prioriy (RTL8363_DSCP_PRIO)<nl>
IP  based priority(RTL8363_IPB_PRIO)<nl>
for RTL8363_PBP_PRIO, pri_obj is port number 0 ~2;<nl>
for RTL8363_1QDEFAULT_PRIO, pri_obj is also port number;<nl>
for RTL8363_1QTAG_PRIO, pri_obj is 0 ~7(priority in vlan tag),
because 1Q tag priority is 3 bit value, so it should be mapped to 2 bit priority value at
first;<nl>
for RTL8363_PRI_DSCP, pri_obj could be following DSCP priority type<nl>
	RTL8363_DSCP_EF<nl>
	RTL8363_DSCP_AFL1<nl>
	RTL8363_DSCP_AFM1<nl>
	RTL8363_DSCP_AFH1<nl>
	RTL8363_DSCP_AFL2<nl>
	RTL8363_DSCP_AFM2<nl>
	RTL8363_DSCP_AFH2<nl>
	RTL8363_DSCP_AFL3<nl>
	RTL8363_DSCP_AFM3<nl>
	RTL8363_DSCP_AFH3<nl>
	RTL8363_DSCP_AFL4<nl>
	RTL8363_DSCP_AFM4<nl>
	RTL8363_DSCP_AFH4<nl>
	RTL8363_DSCP_NC<nl>
	RTL8363_DSCP_BF<nl>
for RTL8363_IPB_PRIO, pri_obj could be RTL8363_IPADD_A or RTL8363_IPADD_B, represent two ip address<nl>
*/

int8 rtl8363_setAsicQosPrioritySourcePriority(uint8 pri_type, uint8 pri_obj, uint8 pri_val)
{
    uint16 regval;

    switch(pri_type)
    {
        case RTL8363_PBP_PRIO:
            rtl8363_getAsicReg(1, 19, MACPAG, 4, &regval);
            pri_obj &= 0x3;
            regval &= ~(0x3 << (pri_obj*2));
            pri_val &= 0x3;
            regval |= (pri_val << (pri_obj*2));
            rtl8363_setAsicReg(1, 19, MACPAG, 4, regval);
            break;

        case RTL8363_1QDEFAULT_PRIO:
            rtl8363_getAsicReg(1, 19, MACPAG, 4, &regval);
            pri_obj &= 0x3;
            regval &= ~( 0x3 << (8 + pri_obj*2));
            pri_val &= 0x3;
            regval |= (pri_val << (8 + pri_obj*2));
            rtl8363_setAsicReg(1, 19, MACPAG, 4, regval);
            break;

        case RTL8363_1QTAG_PRIO:
            rtl8363_getAsicReg(1, 20, MACPAG, 4, &regval);
            pri_obj &= 0x7;
            regval &= ~(0x3 << (pri_obj *2));
            pri_val &= 0x3;
            regval |= (pri_val << (pri_obj*2));
            rtl8363_setAsicReg(1, 20, MACPAG, 4, regval);
            break;

        case RTL8363_DSCP_PRIO:
            if (pri_obj <= RTL8363_DSCP_AFL3 )
            {
                rtl8363_getAsicReg(1, 30, MACPAG, 4, &regval);
                regval &= ~(0x3 << (2*pri_obj));
                pri_val &= 0x3;
                regval |= pri_val << (2*pri_obj);
                rtl8363_setAsicReg(1, 30, MACPAG, 4, regval);
            }
            else if (pri_obj <= RTL8363_DSCP_BF )
            {
                rtl8363_getAsicReg(1, 29, MACPAG, 4, &regval);
                regval &= ~(0x3 << (2*(pri_obj - 8)));
                pri_val &= 0x3;
                regval |= pri_val << (2*(pri_obj - 8));
                rtl8363_setAsicReg(1, 29, MACPAG, 4, regval);
            }
            break;
        case  RTL8363_IPB_PRIO:
            rtl8363_getAsicReg(1, 23, MACPAG, 4, &regval);
            if (pri_obj == RTL8363_IPADD_A)
            {
                regval &= ~(0x3 << 3);
                regval |= (pri_val & 0x3) << 3;
            }
            else if (pri_obj == RTL8363_IPADD_B)
            {
                regval &= ~0x3;
                regval |= (pri_val & 0x3);
            }
            rtl8363_setAsicReg(1, 23, MACPAG, 4, regval);
            break;
        default:
            return FAILED;
    }

    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicQosPrioritySourcePriority | Set priority source value
@parm uint8 | pri_type | Specify priority source type
@parm uint8 | pri_obj | Specify priority object
@parm uint8* | pPri_val | Priority value:RTL8363_PRIO0~RTL8363_PRIO3
@rvalue | SUCCESS
@rvalue | FAILED
@comm
This function could set port-based priority for port n(RTL8363_PBP_PRIO),
802.1Q based default priority for port n(RTL8363_1QDEFAULT_PRIO),
802.1Q tag 3 bit priority to 2 bit priority mapping(RTL8363_1QTAG_PRIO),
DSCP based prioriy (RTL8363_DSCP_PRIO),
IP based priority (RTL8363_IPB_PRIO)<nl>
for RTL8363_PBP_PRIO, pri_obj is port number 0 ~2;<nl>
for RTL8363_1QDEFAULT_PRIO, pri_obj is also port number;<nl>
for RTL8363_1QTAG_PRIO, pri_obj is 0 ~ 7(priority in vlan tag),
because 1Q tag priority is 3 bit value, so it should be mapped to 2 bit priority value at
first;<nl>
for RTL8363_DSCP_PRIO, pri_obj could be following DSCP priority type<nl>
	RTL8363_DSCP_EF<nl>
	RTL8363_DSCP_AFL1<nl>
	RTL8363_DSCP_AFM1<nl>
	RTL8363_DSCP_AFH1<nl>
	RTL8363_DSCP_AFL2<nl>
	RTL8363_DSCP_AFM2<nl>
	RTL8363_DSCP_AFH2<nl>
	RTL8363_DSCP_AFL3<nl>
	RTL8363_DSCP_AFM3<nl>
	RTL8363_DSCP_AFH3<nl>
	RTL8363_DSCP_AFL4<nl>
	RTL8363_DSCP_AFM4<nl>
	RTL8363_DSCP_AFH4<nl>
	RTL8363_DSCP_NC<nl>
	RTL8363_DSCP_BF<nl>
for RTL8363_IPB_PRIO, pri_obj could be RTL8363_IPADD_A or RTL8363_IPADD_B<nl>
*/

int8 rtl8363_getAsicQosPrioritySourcePriority(uint8 pri_type, uint8 pri_obj, uint8 *pPri_val)
{
    uint16 regval;

    switch(pri_type)
    {
        case RTL8363_PBP_PRIO:
            rtl8363_getAsicReg(1, 19, MACPAG, 4, &regval);
            pri_obj &= 0x3;
            *pPri_val = (regval & (0x3 << (2*pri_obj)) ) >> (2*pri_obj);
            break;

        case RTL8363_1QDEFAULT_PRIO:
            rtl8363_getAsicReg(1, 19, MACPAG, 4, &regval);
            pri_obj &= 0x3;
            *pPri_val = (regval & (0x3 << (8 + pri_obj *2))) >> (8 + pri_obj*2);
            break;

        case RTL8363_1QTAG_PRIO:
            rtl8363_getAsicReg(1, 20, MACPAG, 4, &regval);
            pri_obj &= 0x7;
            *pPri_val = (regval & (0x3 << (pri_obj *2))) >> (pri_obj *2);
            break;

        case RTL8363_DSCP_PRIO:
            if (pri_obj <= RTL8363_DSCP_AFL3 )
            {
                rtl8363_getAsicReg(1, 30, MACPAG, 4, &regval);
                *pPri_val = (regval & (0x3 << (2*pri_obj)) ) >> (2*pri_obj);
            }
            else if (pri_obj <= RTL8363_DSCP_BF )
            {
                rtl8363_getAsicReg(1, 29, MACPAG, 4, &regval);
                *pPri_val = (regval & (0x3 << (2*(pri_obj - 8)))) >> (2*(pri_obj - 8));
            }
            break;

        case RTL8363_IPB_PRIO:
            rtl8363_getAsicReg(1, 23, MACPAG, 4, &regval);
            if (pri_obj == RTL8363_IPADD_A)
            {
                *pPri_val = (regval & (0x3 << 3)) >> 3;
            }
            else if (pri_obj == RTL8363_IPADD_B)
            {
                *pPri_val = regval & 0x3;
            }
            break;

        default:
            return FAILED;
    }

    return SUCCESS;
}


/*
@func int8 | rtl8363_setAsicQosIPAddress | Set IP address of IP based priority
@parm uint8 | entry | RTL8363_IPADD_A or RTL8363_IPADD_B
@parm QosIPPri_t | ippri | Specify ip address
@struct QosIPPri_t | This structure describe ip address of IP based priority
@field uint32 | ip | ip address
@field uint32 | ipmask | ip mask
@field uint8 | enabled  | enable the entry
@rvalue SUCCESS
@rvalue FAILED
@comm
There are total two entries to set IP address
*/
int8 rtl8363_setAsicQosIPAddress(uint8 entry, QosIPPri_t ippri)
{
    uint16 regval;


    switch (entry)
    {
        case RTL8363_IPADD_A:
            regval = ippri.ip & 0xFFFF;
            rtl8363_setAsicReg(1, 21, MACPAG, 10, regval);
            regval = (ippri.ip & 0xFFFF0000) >> 16;
            rtl8363_setAsicReg(1, 22, MACPAG, 10, regval);
            regval = ippri.ipmask & 0xFFFF;
            rtl8363_setAsicReg(1, 25, MACPAG, 10, regval);
            regval = (ippri.ipmask & 0xFFFF0000) >> 16;
            rtl8363_setAsicReg(1, 26, MACPAG, 10, regval);
            rtl8363_setAsicRegBit(1, 23, 5, MACPAG, 4, ippri.enabled ? 1:0);
            break;
        case RTL8363_IPADD_B:
            regval = ippri.ip & 0xFFFF;
            rtl8363_setAsicReg(1, 23, MACPAG, 10, regval);
            regval = (ippri.ip & 0xFFFF0000) >> 16;
            rtl8363_setAsicReg(1, 24, MACPAG, 10, regval);
            regval = ippri.ipmask & 0xFFFF;
            rtl8363_setAsicReg(1, 27, MACPAG, 10, regval);
            regval = (ippri.ipmask & 0xFFFF0000) >> 16;
            rtl8363_setAsicReg(1, 28, MACPAG, 10, regval);
            rtl8363_setAsicRegBit(1, 23, 2, MACPAG, 4, ippri.enabled ? 1:0);
            break;
        default:
            return FAILED;
    }
    return SUCCESS;
}

/*
@func int8 | rtl8363_getAsicQosIPAddress | Get IP address of IP based priority
@parm uint8 | entry | RTL8363_IPADD_A or RTL8363_IPADD_B
@parm QosIPPri_t* | pIppri | Specify ip address
@struct QosIPPri_t | This structure describe ip address of IP based priority
@field uint32 | ip | ip address
@field uint32 | ipmask | ip mask
@field uint8 | enabled  | enable the entry
@rvalue SUCCESS
@rvalue FAILED
@comm
There are total two entries to set IP address
*/

int8 rtl8363_getAsicQosIPAddress(uint8 entry, QosIPPri_t *pIppri)
{
    uint16 regval;

    switch(entry)
    {
        case RTL8363_IPADD_A:
            rtl8363_getAsicReg(1, 21, MACPAG, 10, &regval);
            pIppri->ip = regval & 0xFFFF;
            rtl8363_getAsicReg(1, 22, MACPAG, 10, &regval);
            pIppri->ip |= (regval & 0xFFFF) << 16;;
            rtl8363_getAsicReg(1, 25, MACPAG, 10, &regval);
            pIppri->ipmask = regval & 0xFFFF;
            rtl8363_getAsicReg(1, 26, MACPAG, 10, &regval);
            pIppri->ipmask |= (regval & 0xFFFF) << 16;
            rtl8363_getAsicRegBit(1, 23, 5, MACPAG, 4, &regval);
            pIppri->enabled = regval ? TRUE :FALSE;
            break;
        case RTL8363_IPADD_B:
            rtl8363_getAsicReg(1, 23, MACPAG, 10, &regval);
            pIppri->ip = regval & 0xFFFF;
            rtl8363_getAsicReg(1, 24, MACPAG, 10, &regval);
            pIppri->ip |= (regval & 0xFFFF) << 16;;
            rtl8363_getAsicReg(1, 27, MACPAG, 10, &regval);
            pIppri->ipmask = regval & 0xFFFF;
            rtl8363_getAsicReg(1, 28, MACPAG, 10, &regval);
            pIppri->ipmask |= (regval & 0xFFFF) << 16;
            rtl8363_getAsicRegBit(1, 23, 2, MACPAG, 4, &regval);
            pIppri->enabled = regval ? TRUE :FALSE;
            break;

        default:
            return FAILED;
    }

    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicQosPrioritySourceEnable | Per-port enable/disable some priority type
@parm uint8 | port | Specify physical port (0~2)
@parm QosPriEnable_t | enQos |  per-port configured priority
@struct QosPriEnable_t | enQos | This structure describe priorities enable/disable
@field uint8 | enDSCPB | DSCP-based priority enable/disable
@field uint8 | en1QB | 1Q-based priority enable/disable
@field uint8 | enPB | Port-based priority enable/disable
@field uint8 | enCPUTAGB |cpu tag priority enable/disable
@field uint8 | enIPB | ip based priority enable/disable
@rvalue SUCCESS
@rvalue FAILED
@comm
There are 5 types of  priority which could be per-port enable/disable, they are DSCP-based priority,
1Q-based, Port-based, cpu tag priority and ip-based priority. When the priority of the specified port is disabled, the priority
will not recognized for packets received from that port.
*/

int8 rtl8363_setAsicQosPrioritySourceEnable(uint8 port, QosPriEnable_t enQos)
{
    uint16 regval;

    switch (port)
    {
        case RTL8363_PORT0:
            rtl8363_getAsicReg(1, 27, MACPAG, 4, &regval);
            regval &= 0xFF00;
            regval |= (enQos.enCPUTAGB ? 0:0x1) | (enQos.enIPB ? 0 : 0x2);
            regval |= (enQos.enDSCPB ? 0: 0x4) | (enQos.en1QB ? 0: 0x8);
            regval |= (enQos.enPB ? 0: 0x10);
            rtl8363_setAsicReg(1, 27, MACPAG, 4, regval);
            break;

        case RTL8363_PORT1:
            rtl8363_getAsicReg(1, 27, MACPAG, 4, &regval);
            regval &= 0xFF;
            regval |= (enQos.enCPUTAGB ? 0:0x100) | (enQos.enIPB ? 0 : 0x200);
            regval |= (enQos.enDSCPB ? 0: 0x400) | (enQos.en1QB ? 0: 0x800);
            regval |= (enQos.enPB ? 0: 0x1000);
            rtl8363_setAsicReg(1, 27, MACPAG, 4, regval);
            break;

        case RTL8363_PORT2:
            rtl8363_getAsicReg(1, 28, MACPAG, 4, &regval);
            regval &= 0xFF00;
            regval |= (enQos.enCPUTAGB ? 0:0x1) | (enQos.enIPB ? 0 : 0x2);
            regval |= (enQos.enDSCPB ? 0: 0x4) | (enQos.en1QB ? 0: 0x8);
            regval |= (enQos.enPB ? 0: 0x10);
            rtl8363_setAsicReg(1, 28, MACPAG, 4, regval);
            break;

        default:
            return FAILED;

    }

    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicQosPrioritySourceEnable | Get Per-port set priority enabled/disabled
@parm uint8 | port | Specify physical port (0~2)
@parm QosPriEnable_t | enQos |  per-port configured priority
@struct QosPriEnable_t | enQos | This structure describe priorities enable/disable
@field uint8 | enDSCPB | DSCP-based priority enable/disable
@field uint8 | en1QB | 1Q-based priority enable/disable
@field uint8 | enPB | Port-based priority enable/disable
@field uint8 | enCPUTAGB |cpu tag priority enable/disable
@field uint8 | enIPB | ip based priority enable/disable
@rvalue SUCCESS
@rvalue FAILED
@comm
There are 5 types of  priority which could be per-port enable/disable, they are DSCP-based priority,
1Q-based, Port-based, cpu tag priority and ip-based priority. When the priority of the specified port is disabled, the priority
will not recognized for packets received from that port.
*/

int8 rtl8363_getAsicQosPrioritySourceEnable(uint8 port, QosPriEnable_t *pEnQos)
{
    uint16 regval;

    switch(port)
    {
        case RTL8363_PORT0:
            rtl8363_getAsicReg(1, 27, MACPAG, 4, &regval);
            pEnQos->enCPUTAGB = (regval & 0x1) ? FALSE:TRUE;
            pEnQos->enIPB = (regval & 0x2) ? FALSE:TRUE;
            pEnQos->enDSCPB = (regval & 0x4) ? FALSE:TRUE;
            pEnQos->en1QB = (regval & 0x8) ? FALSE:TRUE;
            pEnQos->enPB = (regval & 0x10) ? FALSE:TRUE;
            break;

        case RTL8363_PORT1:
            rtl8363_getAsicReg(1, 27, MACPAG, 4, &regval);
            pEnQos->enCPUTAGB = (regval & 0x100) ? FALSE:TRUE;
            pEnQos->enIPB = (regval & 0x200) ? FALSE:TRUE;
            pEnQos->enDSCPB = (regval & 0x400) ? FALSE:TRUE;
            pEnQos->en1QB = (regval & 0x800) ? FALSE:TRUE;
            pEnQos->enPB = (regval & 0x1000) ? FALSE:TRUE;
            break;

        case RTL8363_PORT2:
            rtl8363_getAsicReg(1, 28, MACPAG, 4, &regval);
            pEnQos->enCPUTAGB = (regval & 0x1) ? FALSE:TRUE;
            pEnQos->enIPB = (regval & 0x2) ? FALSE:TRUE;
            pEnQos->enDSCPB = (regval & 0x4) ? FALSE:TRUE;
            pEnQos->en1QB = (regval & 0x8) ? FALSE:TRUE;
            pEnQos->enPB = (regval & 0x10) ? FALSE:TRUE;
            break;
        default:
            return FAILED;

    }

    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicQosFCEnable | Set asic port-based/queue-based flow control enable/disable.
@parm uint8 | port | Specify port number
@parm QosFCEn_t | enfc
@struct QosFCEn_t | This structure describe port-based/queue-based flow control enable/disable setting
@field uint8 | enpfc | enable/disable port-based output flow control
@field uint8 | qfcmask | enable queue-based flow control mask, each bit represents one queue
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_setAsicQosFCEnable(uint8 port, QosFCEn_t enfc )
{
    uint16 regval;

    if (port > RTL8363_PORT2)
        return FAILED;

    /*config queue-based flow control*/
    rtl8363_getAsicReg(port, 29, MACPAG, 3, &regval);
    regval &= ~0xF;
    regval |= ((~enfc.qfcmask) & 0xF);
    rtl8363_setAsicReg(port, 29, MACPAG, 3, regval);

    /*config port-based output flow control*/
    rtl8363_setAsicRegBit(0, 25, (11+ port), MACPAG, 14, enfc.enpfc ? 0:1);
    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicQosFCEnable | Get asic port-based/queue-based flow control enable/disable.
@parm uint8 | port | Specify port number
@parm QosFCEn_t* | pEnfc
@struct QosFCEn_t | This structure describe port-based/queue-based flow control enable/disable setting
@field uint8 | enpfc | enable/disable port-based output flow control
@field uint8 | qfcmask | enable queue-based flow control mask, each bit represents one queue
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_getAsicQosFCEnable(uint8 port, QosFCEn_t *pEnfc)
{
    uint16 regval;

    if (port > RTL8363_PORT2)
        return FAILED;

    rtl8363_getAsicReg(port, 29, MACPAG, 3, &regval);
    pEnfc->qfcmask= (~regval) & 0xF;

    rtl8363_getAsicRegBit(0, 25, (11+port), MACPAG, 14, &regval);
    pEnfc->enpfc = regval ? FALSE:TRUE;

    return SUCCESS;
}

int8 rtl8363_setAsicQosQueueFCThr(uint8 port, uint8 type, QosQFC_t qfcth)
{
    uint16 regval;

    if (type == RTL8363_FCO_QLEN)
    {
        rtl8363_getAsicReg(port, 24, MACPAG, 3, &regval);
        regval &= ~0x7F7F;
        regval |= (qfcth.q0_ON & 0x7F) | ((qfcth.q0_OFF & 0x7F) << 8);
        rtl8363_setAsicReg(port, 24, MACPAG, 3, regval);

        rtl8363_getAsicReg(port, 23, MACPAG, 3, &regval);
        regval &= ~0x7F7F;
        regval |= (qfcth.q1_ON & 0x7F) | ((qfcth.q1_OFF & 0x7F) << 8);
        rtl8363_setAsicReg(port, 23, MACPAG, 3, regval);

        rtl8363_getAsicReg(port, 22, MACPAG, 3, &regval);
        regval &= ~0x7F7F;
        regval |= (qfcth.q2_ON & 0x7F) | ((qfcth.q2_OFF & 0x7F) << 8);
        rtl8363_setAsicReg(port, 22, MACPAG, 3, regval);

        rtl8363_getAsicReg(port, 21, MACPAG, 3, &regval);
        regval &= ~0x7F7F;
        regval |= (qfcth.q3_ON & 0x7F) | ((qfcth.q3_OFF & 0x7F) << 8);
        rtl8363_setAsicReg(port, 21, MACPAG, 3, regval);

        switch (port)
        {
            case RTL8363_PORT0:
                rtl8363_getAsicReg(0, 28, MACPAG, 14, &regval);
                regval &= ~0x7F7F;
                regval |= ((qfcth.q3_DROP & 0x7F) << 8) | (qfcth.q2_DROP & 0x7F);
                rtl8363_setAsicReg(0, 28, MACPAG, 14, regval);
                rtl8363_getAsicReg(0, 29, MACPAG, 14, &regval);
                regval &= ~0x7F7F;
                regval |= ((qfcth.q1_DROP & 0x7F) << 8) | (qfcth.q0_DROP & 0x7F);
                rtl8363_setAsicReg(0, 29, MACPAG, 14, regval);
                break;

            case RTL8363_PORT1:
                rtl8363_getAsicReg(1, 23, MACPAG, 14, &regval);
                regval &= ~0x7F7F;
                regval |= ((qfcth.q3_DROP & 0x7F) << 8) | (qfcth.q2_DROP & 0x7F);
                rtl8363_setAsicReg(1, 23, MACPAG, 14, regval);
                rtl8363_getAsicReg(1, 24, MACPAG, 14, &regval);
                regval &= ~0x7F7F;
                regval |= ((qfcth.q1_DROP & 0x7F) << 8) | (qfcth.q0_DROP & 0x7F);
                rtl8363_setAsicReg(1, 24, MACPAG, 14, regval);
                break;

            case RTL8363_PORT2:
                rtl8363_getAsicReg(1, 28, MACPAG, 14, &regval);
                regval &= ~0x7F7F;
                regval |= ((qfcth.q3_DROP & 0x7F) << 8) | (qfcth.q2_DROP & 0x7F);
                rtl8363_setAsicReg(1, 28, MACPAG, 14, regval);
                rtl8363_getAsicReg(1, 29, MACPAG, 14, &regval);
                regval &= ~0x7F7F;
                regval |= ((qfcth.q1_DROP & 0x7F) << 8) | (qfcth.q0_DROP & 0x7F);
                rtl8363_setAsicReg(1, 29, MACPAG, 14, regval);

                break;
            default:
                return FAILED;
        }


    }
    else if (type == RTL8363_FCO_DSC)
    {
        regval = qfcth.q0_ON | (qfcth.q0_OFF << 8 );
        rtl8363_setAsicReg(port, 20, MACPAG, 3, regval);

        regval = qfcth.q1_ON | (qfcth.q1_OFF << 8 );
        rtl8363_setAsicReg(port, 19, MACPAG, 3, regval);

        regval = qfcth.q2_ON | (qfcth.q2_OFF << 8 );
        rtl8363_setAsicReg(port, 18, MACPAG, 3, regval);

        regval = qfcth.q3_ON | (qfcth.q3_OFF << 8 );
        rtl8363_setAsicReg(port, 17, MACPAG, 3, regval);

        switch(port)
        {
            case RTL8363_PORT0:
                regval = (qfcth.q3_DROP << 8) | (qfcth.q2_DROP);
                rtl8363_setAsicReg(0, 26, MACPAG, 14, regval);
                regval = (qfcth.q1_DROP << 8) | (qfcth.q0_DROP);
                rtl8363_setAsicReg(0, 27, MACPAG, 14, regval);
                break;

            case RTL8363_PORT1:
                regval = (qfcth.q3_DROP << 8) | (qfcth.q2_DROP);
                rtl8363_setAsicReg(1, 21, MACPAG, 14, regval);
                regval = (qfcth.q1_DROP << 8) | (qfcth.q0_DROP);
                rtl8363_setAsicReg(1, 22, MACPAG, 14, regval);
                break;

            case RTL8363_PORT2:
                regval = (qfcth.q3_DROP << 8) | (qfcth.q2_DROP);
                rtl8363_setAsicReg(1, 26, MACPAG, 14, regval);
                regval = (qfcth.q1_DROP << 8) | (qfcth.q0_DROP);
                rtl8363_setAsicReg(1, 27, MACPAG, 14, regval);
                break;
            default:
                return FAILED;
        }

    }

    return SUCCESS;
}

int8 rtl8363_getAsicQosQueueFCThr(uint8 port, uint8 type, QosQFC_t *pQfcth)
{
    uint16 regval;

    if (type == RTL8363_FCO_QLEN)
    {
        rtl8363_getAsicReg(port, 24, MACPAG, 3, &regval);
        pQfcth->q0_ON = regval & 0x7F;
        pQfcth->q0_OFF = (regval & 0x7F00) >> 8;

        rtl8363_getAsicReg(port, 23, MACPAG, 3, &regval);
        pQfcth->q1_ON = regval & 0x7F;
        pQfcth->q1_OFF = (regval & 0x7F00) >> 8;

        rtl8363_getAsicReg(port, 22, MACPAG, 3, &regval);
        pQfcth->q2_ON = regval & 0x7F;
        pQfcth->q2_OFF = (regval & 0x7F00) >> 8;

        rtl8363_getAsicReg(port, 21, MACPAG, 3, &regval);
        pQfcth->q3_ON = regval & 0x7F;
        pQfcth->q3_OFF = (regval & 0x7F00) >> 8;
        switch(port)
        {
            case RTL8363_PORT0:
                rtl8363_getAsicReg(0, 28, MACPAG, 14, &regval);
                pQfcth->q3_DROP = (regval & 0x7F00) >> 8;
                pQfcth->q2_DROP = regval & 0x7F;
                rtl8363_getAsicReg(0, 29, MACPAG, 14, &regval);
                pQfcth->q1_DROP = (regval & 0x7F00) >> 8;
                pQfcth->q0_DROP = regval & 0x7F;
                break;

            case RTL8363_PORT1:
                rtl8363_getAsicReg(1, 23, MACPAG, 14, &regval);
                pQfcth->q3_DROP = (regval & 0x7F00) >> 8;
                pQfcth->q2_DROP = regval & 0x7F;
                rtl8363_getAsicReg(1, 24, MACPAG, 14, &regval);
                pQfcth->q1_DROP = (regval & 0x7F00) >> 8;
                pQfcth->q0_DROP = regval & 0x7F;
                break;

            case RTL8363_PORT2:
                rtl8363_getAsicReg(1, 28, MACPAG, 14, &regval);
                pQfcth->q3_DROP = (regval & 0x7F00) >> 8;
                pQfcth->q2_DROP = regval & 0x7F;
                rtl8363_getAsicReg(1, 29, MACPAG, 14, &regval);
                pQfcth->q1_DROP = (regval & 0x7F00) >> 8;
                pQfcth->q0_DROP = regval & 0x7F;
                break;

            default:
                return FAILED;
        }

    }
    else if (type == RTL8363_FCO_DSC)
    {
        rtl8363_getAsicReg(port, 20, MACPAG, 3, &regval);
        pQfcth->q0_ON = regval & 0xFF;
        pQfcth->q0_OFF = (regval & 0xFF00) >> 8;

        rtl8363_getAsicReg(port, 19, MACPAG, 3, &regval);
        pQfcth->q1_ON = regval & 0xFF;
        pQfcth->q1_OFF = (regval & 0xFF00) >> 8;

        rtl8363_getAsicReg(port, 18, MACPAG, 3, &regval);
        pQfcth->q2_ON = regval & 0xFF;
        pQfcth->q2_OFF = (regval & 0xFF00) >> 8;

        rtl8363_getAsicReg(port, 17, MACPAG, 3, &regval);
        pQfcth->q3_ON = regval & 0xFF;
        pQfcth->q3_OFF = (regval & 0xFF00) >> 8;
        switch(port)
        {
            case RTL8363_PORT0:
                rtl8363_getAsicReg(0, 26, MACPAG, 14, &regval);
                pQfcth->q3_DROP = (regval & 0xFF00) >> 8;
                pQfcth->q2_DROP = regval & 0xFF;
                rtl8363_getAsicReg(0, 27, MACPAG, 14, &regval);
                pQfcth->q1_DROP = (regval & 0xFF00) >> 8;
                pQfcth->q0_DROP = regval & 0xFF;
                break;

            case RTL8363_PORT1:
                rtl8363_getAsicReg(1, 21, MACPAG, 14, &regval);
                pQfcth->q3_DROP = (regval & 0xFF00) >> 8;
                pQfcth->q2_DROP = regval & 0xFF;
                rtl8363_getAsicReg(1, 22, MACPAG, 14, &regval);
                pQfcth->q1_DROP = (regval & 0xFF00) >> 8;
                pQfcth->q0_DROP = regval & 0xFF;
                break;

            case RTL8363_PORT2:
                rtl8363_getAsicReg(1, 26, MACPAG, 14, &regval);
                pQfcth->q3_DROP = (regval & 0xFF00) >> 8;
                pQfcth->q2_DROP = regval & 0xFF;
                rtl8363_getAsicReg(1, 27, MACPAG, 14, &regval);
                pQfcth->q1_DROP = (regval & 0xFF00) >> 8;
                pQfcth->q0_DROP = regval & 0xFF;
                break;

            default:
                return FAILED;

        }

    }

    return SUCCESS;
}

int8 rtl8363_setAsicQosPortFCThr(uint8 port, uint8 direction, QosPFC_t pfc)
{
    uint16 regval;

    if (port > RTL8363_PORT2)
        return FAILED;

    if (direction == RTL8363_PORT_TX)
    {
        regval = (pfc.dscon & 0xFF) | ((pfc.dscoff & 0xFF) << 8);
        rtl8363_setAsicReg(port, 30, MACPAG, 3, regval);
        switch(port)
        {
            case RTL8363_PORT0:
                rtl8363_getAsicReg(0, 30, MACPAG, 14, &regval);
                regval &= ~0xFF;
                regval |= pfc.dscdrop;
                rtl8363_setAsicReg(0, 30, MACPAG, 14, regval);
                break;

            case RTL8363_PORT1:
                rtl8363_getAsicReg(1, 25, MACPAG, 14, &regval);
                regval &= ~0xFF;
                regval |= pfc.dscdrop;
                rtl8363_setAsicReg(1, 25, MACPAG, 14, regval);
                break;

            case RTL8363_PORT2:
                rtl8363_getAsicReg(1, 30, MACPAG, 14, &regval);
                regval &= ~0xFF;
                regval |= pfc.dscdrop;
                rtl8363_setAsicReg(1, 30, MACPAG, 14, regval);
                break;

            default:
                return FAILED;
        }
    }
    else if (direction == RTL8363_PORT_RX)
    {
        regval = (pfc.dscon & 0xFF) | ((pfc.dscoff & 0xFF) << 8);
        rtl8363_setAsicReg(1, 20 + port, MACPAG, 5, regval);
    }
    return SUCCESS;
}

int8 rtl8363_getAsicQosPortFCThr(uint8 port, uint16 direction, QosPFC_t *pPfc)
{
    uint16 regval;

    if (direction == RTL8363_PORT_TX)
    {
        rtl8363_getAsicReg(port, 30, MACPAG, 3, &regval);
        pPfc->dscon = regval & 0xFF;
        pPfc->dscoff = (regval & 0xFF00) >> 8;
        switch(port)
        {
            case RTL8363_PORT0:
                rtl8363_getAsicReg(0, 30, MACPAG, 14, &regval);
                pPfc->dscdrop = regval & 0xFF;
                break;
            case RTL8363_PORT1:
                rtl8363_getAsicReg(1, 25, MACPAG, 14, &regval);
                pPfc->dscdrop = regval & 0xFF;
                break;
            case RTL8363_PORT2:
                rtl8363_getAsicReg(1, 30, MACPAG, 14, &regval);
                pPfc->dscdrop = regval & 0xFF;
                break;
            default:
                return FAILED;
        }

    }
    else if (direction == RTL8363_PORT_RX)
    {
        rtl8363_getAsicReg(1, 20 + port, MACPAG, 5, &regval);
        pPfc->dscon = regval & 0xFF;
        pPfc->dscoff = (regval & 0xFF00) >> 8;
    }

    return SUCCESS;
}


int8 rtl8363_setAsicQosQueueSize(uint8 port, uint8 qnum, QosQsize_t qsize)
{
      uint16 regval;

      switch (qnum)
      {
         case 4:
            rtl8363_getAsicReg(port, 26, MACPAG, 3, &regval);
            regval &= ~0x7F;
            regval |= qsize.q0_size & 0x7F;
            rtl8363_setAsicReg(port, 26, MACPAG, 3, regval);
            rtl8363_getAsicReg(port, 25, MACPAG, 3, &regval);
            regval &= 0x8080;
            regval |= (qsize.q1_size & 0x7F << 8) | (qsize.q2_size & 0x7F);
            rtl8363_setAsicReg(port, 25, MACPAG, 3, regval);
            break;

         case 3:
            rtl8363_getAsicReg(port, 27, MACPAG, 3, &regval);
            regval &= 0x8080;
            regval |= (qsize.q0_size & 0x7F << 8) | (qsize.q1_size & 0x7F);
            rtl8363_setAsicReg(port, 27, MACPAG, 3, regval);
            break;

         case 2:
            rtl8363_getAsicReg(port, 28, MACPAG, 3, &regval);
            regval &= 0x7F;
            regval |= qsize.q0_size & 0x7F;
            rtl8363_setAsicReg(port, 28, MACPAG, 3, regval);
            break;

         default:
            return FAILED;
      }

      return SUCCESS;

}

int8 rtl8363_getAsicQosQueueSize(uint8 port, uint8 qnum, QosQsize_t* pQsize)
{
    uint16 regval;

    switch(qnum)
    {
        case 4:
            rtl8363_getAsicReg(port, 26, MACPAG, 3, &regval);
            pQsize->q0_size = regval & 0x7F;
            rtl8363_getAsicReg(port, 25, MACPAG, 3, &regval);
            pQsize->q1_size = (regval & 0x7F00) >> 8;
            pQsize->q2_size = regval & 0x7F;
            pQsize->q3_size = 128 - pQsize->q0_size- pQsize->q1_size - pQsize->q2_size;
            break;

        case 3:
            rtl8363_getAsicReg(port, 27, MACPAG, 3, &regval);
            pQsize->q0_size = (regval & 0x7F00) >> 8;
            pQsize->q1_size = regval & 0x7F;
            pQsize->q2_size = 128 - pQsize->q0_size- pQsize->q1_size;
            break;

        case 2:
            rtl8363_getAsicReg(port, 27, MACPAG, 3, &regval);
            pQsize->q0_size = regval & 0x7F;
            pQsize->q1_size = 128 - pQsize->q1_size;
            break;

        default:
            return FAILED;

    }

    return SUCCESS;
}

int8 rtl8363_setAsicQosDebugCntClr(void)
{
    uint16 i;

    rtl8363_setAsicRegBit(1, 17, 15, MACPAG, 13, 1);
    RTL8363_DELAY(i, 500);
    rtl8363_setAsicRegBit(1, 17, 15, MACPAG, 13, 0);
    return SUCCESS;
}
int8 rtl8363_getAsicQosDebugCnt(QosDebug_t *pQosdbg)
{
    uint16 regval;

    rtl8363_getAsicRegBit(1, 17, 0, MACPAG, 13, &regval);
    pQosdbg->DscrunoutFlag = regval ? TRUE:FALSE;
    rtl8363_getAsicReg(1, 18, MACPAG, 13, &regval);
    pQosdbg->MaxpktBuf = regval & 0xFF;
    rtl8363_getAsicReg(1, 20, MACPAG, 13, &regval);
    pQosdbg->Pn_MaxRxDscCount[0] = regval & 0xFF;
    pQosdbg->Pn_MaxTxDscCount[0] = (regval & 0xFF00) >> 8;
    rtl8363_getAsicReg(1, 21, MACPAG, 13, &regval);
    pQosdbg->Pn_MaxRxDscCount[1] = regval & 0xFF;
    pQosdbg->Pn_MaxTxDscCount[1] = (regval & 0xFF00) >> 8;
    rtl8363_getAsicReg(1, 22, MACPAG, 13, &regval);
    pQosdbg->Pn_MaxRxDscCount[2] = regval & 0xFF;
    pQosdbg->Pn_MaxTxDscCount[2] = (regval & 0xFF00) >> 8;
    rtl8363_getAsicReg(1, 23, MACPAG, 13, &regval);
    pQosdbg->Pn_MaxPktCount[0] = (regval & 0xFF00) >> 8;
    pQosdbg->Pn_MaxPktCount[1] = regval & 0xFF;
    rtl8363_getAsicReg(1, 19, MACPAG, 13, &regval);
    pQosdbg->Pn_MaxPktCount[2] = regval & 0xFF;
    rtl8363_getAsicReg(1, 17, MACPAG, 15, &regval);
    pQosdbg->PnQ0_MaxDscCount[0] = (regval & 0xFF00) >> 8;
    pQosdbg->PnQ0_MaxPktCount[0] = regval & 0xFF;
    rtl8363_getAsicReg(1, 18, MACPAG, 15, &regval);
    pQosdbg->PnQ1_MaxDscCount[0] = (regval & 0xFF00) >> 8;
    pQosdbg->PnQ1_MaxPktCount[0] = regval & 0xFF;
    rtl8363_getAsicReg(1, 19, MACPAG, 15, &regval);
    pQosdbg->PnQ2_MaxDscCount[0] = (regval & 0xFF00) >> 8;
    pQosdbg->PnQ2_MaxPktCount[0] = regval & 0xFF;
    rtl8363_getAsicReg(1, 20, MACPAG, 15, &regval);
    pQosdbg->PnQ3_MaxDscCount[0] = (regval & 0xFF00) >> 8;
    pQosdbg->PnQ3_MaxPktCount[0] = regval & 0xFF;
    rtl8363_getAsicReg(1, 21, MACPAG, 15, &regval);
    pQosdbg->PnQ0_MaxDscCount[1] = (regval & 0xFF00) >> 8;
    pQosdbg->PnQ0_MaxPktCount[1] = regval & 0xFF;
    rtl8363_getAsicReg(1, 22, MACPAG, 15, &regval);
    pQosdbg->PnQ1_MaxDscCount[1] = (regval & 0xFF00) >> 8;
    pQosdbg->PnQ1_MaxPktCount[1] = regval & 0xFF;
    rtl8363_getAsicReg(1, 23, MACPAG, 15, &regval);
    pQosdbg->PnQ2_MaxDscCount[1] = (regval & 0xFF00) >> 8;
    pQosdbg->PnQ2_MaxPktCount[1] = regval & 0xFF;
    rtl8363_getAsicReg(1, 24, MACPAG, 15, &regval);
    pQosdbg->PnQ3_MaxDscCount[1] = (regval & 0xFF00) >> 8;
    pQosdbg->PnQ3_MaxPktCount[1] = regval & 0xFF;
    rtl8363_getAsicReg(1, 25, MACPAG, 15, &regval);
    pQosdbg->PnQ0_MaxDscCount[2] = (regval & 0xFF00) >> 8;
    pQosdbg->PnQ0_MaxPktCount[2] = regval & 0xFF;
    rtl8363_getAsicReg(1, 26, MACPAG, 15, &regval);
    pQosdbg->PnQ1_MaxDscCount[2] = (regval & 0xFF00) >> 8;
    pQosdbg->PnQ1_MaxPktCount[2] = regval & 0xFF;
    rtl8363_getAsicReg(1, 27, MACPAG, 15, &regval);
    pQosdbg->PnQ2_MaxDscCount[2] = (regval & 0xFF00) >> 8;
    pQosdbg->PnQ2_MaxPktCount[2] = regval & 0xFF;
    rtl8363_getAsicReg(1, 28, MACPAG, 15, &regval);
    pQosdbg->PnQ3_MaxDscCount[2] = (regval & 0xFF00) >> 8;
    pQosdbg->PnQ3_MaxPktCount[2] = regval & 0xFF;

    return SUCCESS;
}



/*
@func int8 | rtl8363_setAsicSpecialMACFWD | Set the special MAC frame forwarding behaviour.
@parm enum SpecialMac | SpecialMac | Specify the special mac frame
@parm FwdSpecialMac_t | fwdcfg
@struct FwdSpecialMac_t | This structure describe special MAC frame forwarding behaviour
@field uint8 | fwd_bhv | there are four option
@field uint8 | rgnPri | reassign the frame priority value 0~3
@field uint8 | enRgnPri |whether enable reassign the frame priority
@field uint8 | enUDA |whether enable the User Defined mac Address, it is only use for User Defined mac Address
@rvalue SUCCESS
@rvalue FAILED
@comm
special mac frame could be <nl>
NIPMLT      non-IP multicast <nl>
BRD           broadcast <nl>
PIGMP        IGMP in PPPoE frame <nl>
PMLD         MLD(IPv6 ICMP) in PPPoE frame <nl>
IGMP          IGMP packet but not PPPoE frame <nl>
MLD           MLD(IPv6 ICMP) but not PPPoE frame
RSV_XX      reserved multicast 01-80-c2-00-00-xx (xx != 00,01,02,03,10,20,21,04~0F,22~2F) <nl>
RSV_22       reserved multicast 01-80-c2-00-00-22~2F <nl>
RSV_04       reserved multicast 01-80-c2-00-00-04~0F <nl>
RSV_21       reserved multicast 01-80-c2-00-00-21 <nl>
RSV_20       reserved multicast 01-80-c2-00-00-20 <nl>
RSV_10       reserved multicast 01-80-c2-00-00-10 <nl>
RSV_03       reserved multicast 01-80-c2-00-00-03 <nl>
RSV_02       reserved multicast 01-80-c2-00-00-02 <nl>
RSV_01       reserved multicast 01-80-c2-00-00-01 <nl>
RSV_00       reserved multicast 01-80-c2-00-00-00 <nl>
PIP6MLT      IPv6 multicast in PPPoE frame <nl>
PIP4MLT      IPv4 multicast in PPPoE frame <nl>
KIP6MLT      Known IPv6 multicast <nl>
KIP4MLT      Known IPv4 multicast <nl>
UDA1          user defined address 1, use rtl8363_setAsicUsrDefMac set it <nl>
UDA2          user defined address 2, use rtl8363_setAsicUsrDefMac set it <nl>
UDA3          user defined address 3, use rtl8363_setAsicUsrDefMac set it <nl>
UDA4          user defined address 4, use rtl8363_setAsicUsrDefMac set it <nl>
UKU            unkown DA unicast <nl>
UKIP4MLT    unkown IPv4 multicast address <nl>
UKIP6MLT    unkown IPv6 multicast address <nl>

fwd_bhv could be: <nl>
RTL8363_FWD_NORMAL    normal forwarding <nl>
RTL8363_FWD_EXCPU      normal forwarding but excluding CPU port <nl>
RTL8363_FWD_TRAPCPU   trap to cpu only <nl>
RTL8363_FWD_DROP 3      drop <nl>

*/

int8 rtl8363_setAsicSpecialMACFWD(enum SpecialMac SpecialMac, FwdSpecialMac_t  fwdcfg)
{
    uint16 regval;

    switch(SpecialMac)
    {

        case NIPMLT:
            rtl8363_getAsicReg(1, 21, MACPAG, 0, &regval);
            regval &= ~(0x3 << 4);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 4;
            rtl8363_setAsicReg(1, 21, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 27, MACPAG, 0, &regval);
            regval &= ~(0x3 << 12);
            regval |= ((fwdcfg.rgnPri & 0x3) << 12);
            rtl8363_setAsicReg(1, 27, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 29, 0, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);
            break;

        case  BRD:
            rtl8363_getAsicReg(1, 21, MACPAG, 0, &regval);
            regval &= ~(0x3 << 6);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 6;
            rtl8363_setAsicReg(1, 21, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 27, MACPAG, 0, &regval);
            regval &= ~(0x3 << 14);
            regval |= ((fwdcfg.rgnPri & 0x3) << 14);
            rtl8363_setAsicReg(1, 27, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 29, 1, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

        case PIGMP:
            rtl8363_getAsicReg(1, 20, MACPAG, 0, &regval);
            regval &= ~(0x3 << 2);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 2;
            rtl8363_setAsicReg(1, 20, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 26, MACPAG, 0, &regval);
            regval &= ~(0x3 << 8);
            regval |= ((fwdcfg.rgnPri & 0x3) << 8);
            rtl8363_setAsicReg(1, 26, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 29, 2, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

        case PMLD:
            rtl8363_getAsicReg(1, 20, MACPAG, 0, &regval);
            regval &= ~(0x3 << 0);
            regval |= (fwdcfg.fwd_bhv & 0x3) ;
            rtl8363_setAsicReg(1, 20, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 26, MACPAG, 0, &regval);
            regval &= ~(0x3 << 10);
            regval |= ((fwdcfg.rgnPri & 0x3) << 10);
            rtl8363_setAsicReg(1, 26, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 29, 3, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

         case IGMP:
            rtl8363_getAsicReg(1, 20, MACPAG, 0, &regval);
            regval &= ~(0x3 << 4);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 4 ;
            rtl8363_setAsicReg(1, 20, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 26, MACPAG, 0, &regval);
            regval &= ~(0x3 << 12);
            regval |= ((fwdcfg.rgnPri & 0x3) << 12);
            rtl8363_setAsicReg(1, 26, MACPAG, 0, regval);

             rtl8363_setAsicRegBit(1, 29, 4, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);


            break;

         case MLD:
            rtl8363_getAsicReg(1, 20, MACPAG, 0, &regval);
            regval &= ~(0x3 << 6);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 6 ;
            rtl8363_setAsicReg(1, 20, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 26, MACPAG, 0, &regval);
            regval &= ~(0x3 << 14);
            regval |= ((fwdcfg.rgnPri & 0x3) << 14);
            rtl8363_setAsicReg(1, 26, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 29, 5, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

          case RSV_XX:
            rtl8363_getAsicReg(1, 19, MACPAG, 0, &regval);
            regval &= ~(0x3 << 12);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 12 ;
            rtl8363_setAsicReg(1, 19, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 26, MACPAG, 0, &regval);
            regval &= ~(0x3 << 4);
            regval |= ((fwdcfg.rgnPri & 0x3) << 4);
            rtl8363_setAsicReg(1, 26, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 29, 6, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);


            break;

          case RSV_22:
            rtl8363_getAsicReg(1, 19, MACPAG, 0, &regval);
            regval &= ~(0x3 << 14);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 14 ;
            rtl8363_setAsicReg(1, 19, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 26, MACPAG, 0, &regval);
            regval &= ~(0x3 << 6);
            regval |= ((fwdcfg.rgnPri & 0x3) << 6);
            rtl8363_setAsicReg(1, 26, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 29, 7, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

          case RSV_04:
            rtl8363_getAsicReg(1, 19, MACPAG, 0, &regval);
            regval &= ~(0x3 << 0);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 0 ;
            rtl8363_setAsicReg(1, 19, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 25, MACPAG, 0, &regval);
            regval &= ~(0x3 << 8);
            regval |= ((fwdcfg.rgnPri & 0x3) << 8);
            rtl8363_setAsicReg(1, 25, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 28, 8, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);
            break;

          case RSV_21:
            rtl8363_getAsicReg(1, 19, MACPAG, 0, &regval);
            regval &= ~(0x3 << 2);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 2 ;
            rtl8363_setAsicReg(1, 19, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 25, MACPAG, 0, &regval);
            regval &= ~(0x3 << 10);
            regval |= ((fwdcfg.rgnPri & 0x3) << 10);
            rtl8363_setAsicReg(1, 25, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 28, 9, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

          case RSV_20:
            rtl8363_getAsicReg(1, 19, MACPAG, 0, &regval);
            regval &= ~(0x3 << 4);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 4 ;
            rtl8363_setAsicReg(1, 19, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 25, MACPAG, 0, &regval);
            regval &= ~(0x3 << 12);
            regval |= ((fwdcfg.rgnPri & 0x3) << 12);
            rtl8363_setAsicReg(1, 25, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 28, 10, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

          case RSV_10:
            rtl8363_getAsicReg(1, 19, MACPAG, 0, &regval);
            regval &= ~(0x3 << 6);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 6 ;
            rtl8363_setAsicReg(1, 19, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 25, MACPAG, 0, &regval);
            regval &= ~(0x3 << 14);
            regval |= ((fwdcfg.rgnPri & 0x3) << 14);
            rtl8363_setAsicReg(1, 25, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 28, 11, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

          case RSV_03:
            rtl8363_getAsicReg(1, 18, MACPAG, 0, &regval);
            regval &= ~(0x3 << 8);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 8 ;
            rtl8363_setAsicReg(1, 18, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 25, MACPAG, 0, &regval);
            regval &= ~(0x3 << 0);
            regval |= ((fwdcfg.rgnPri & 0x3) << 0);
            rtl8363_setAsicReg(1, 25, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 28, 12, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

          case RSV_02:
            rtl8363_getAsicReg(1, 18, MACPAG, 0, &regval);
            regval &= ~(0x3 << 10);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 10 ;
            rtl8363_setAsicReg(1, 18, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 25, MACPAG, 0, &regval);
            regval &= ~(0x3 << 2);
            regval |= ((fwdcfg.rgnPri & 0x3) << 2);
            rtl8363_setAsicReg(1, 25, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 28, 13, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

          case RSV_01:
            rtl8363_getAsicReg(1, 18, MACPAG, 0, &regval);
            regval &= ~(0x3 << 12);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 12 ;
            rtl8363_setAsicReg(1, 18, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 25, MACPAG, 0, &regval);
            regval &= ~(0x3 << 4);
            regval |= ((fwdcfg.rgnPri & 0x3) << 4);
            rtl8363_setAsicReg(1, 25, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 28, 14, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

          case RSV_00:
            rtl8363_getAsicReg(1, 18, MACPAG, 0, &regval);
            regval &= ~(0x3 << 14);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 14 ;
            rtl8363_setAsicReg(1, 18, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 25, MACPAG, 0, &regval);
            regval &= ~(0x3 << 6);
            regval |= ((fwdcfg.rgnPri & 0x3) << 6);
            rtl8363_setAsicReg(1, 25, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 28, 15, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);
            break;

          case PIP6MLT:
            rtl8363_getAsicReg(1, 21, MACPAG, 0, &regval);
            regval &= ~(0x3 << 8);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 8 ;
            rtl8363_setAsicReg(1, 21, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 28, MACPAG, 0, &regval);
            regval &= ~(0x3 << 0);
            regval |= ((fwdcfg.rgnPri & 0x3) << 0);
            rtl8363_setAsicReg(1, 28, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 29, 8, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

          case PIP4MLT:
            rtl8363_getAsicReg(1, 21, MACPAG, 0, &regval);
            regval &= ~(0x3 << 10);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 10 ;
            rtl8363_setAsicReg(1, 21, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 28, MACPAG, 0, &regval);
            regval &= ~(0x3 << 2);
            regval |= ((fwdcfg.rgnPri & 0x3) << 2);
            rtl8363_setAsicReg(1, 28, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 29, 9, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

          case KIP6MLT:
            rtl8363_getAsicReg(1, 21, MACPAG, 0, &regval);
            regval &= ~(0x3 << 12);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 12 ;
            rtl8363_setAsicReg(1, 21, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 28, MACPAG, 0, &regval);
            regval &= ~(0x3 << 4);
            regval |= ((fwdcfg.rgnPri & 0x3) << 4);
            rtl8363_setAsicReg(1, 28, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 29, 10, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);
            break;

          case KIP4MLT:
            rtl8363_getAsicReg(1, 21, MACPAG, 0, &regval);
            regval &= ~(0x3 << 14);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 14 ;
            rtl8363_setAsicReg(1, 21, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 28, MACPAG, 0, &regval);
            regval &= ~(0x3 << 6);
            regval |= ((fwdcfg.rgnPri & 0x3) << 6);
            rtl8363_setAsicReg(1, 28, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 29, 11, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

          case UDA1:
            rtl8363_getAsicReg(1, 20, MACPAG, 0, &regval);
            regval &= ~(0x3 << 8);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 8 ;
            rtl8363_setAsicReg(1, 20, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 27, MACPAG, 0, &regval);
            regval &= ~(0x3 << 0);
            regval |= ((fwdcfg.rgnPri & 0x3) << 0);
            rtl8363_setAsicReg(1, 27, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 19, 8, MACPAG, 0, fwdcfg.enUDA ? 1:0);
            rtl8363_setAsicRegBit(1, 29, 12, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

          case UDA2:
            rtl8363_getAsicReg(1, 20, MACPAG, 0, &regval);
            regval &= ~(0x3 << 10);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 10 ;
            rtl8363_setAsicReg(1, 20, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 27, MACPAG, 0, &regval);
            regval &= ~(0x3 << 2);
            regval |= ((fwdcfg.rgnPri & 0x3) << 2);
            rtl8363_setAsicReg(1, 27, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 19, 9, MACPAG, 0, fwdcfg.enUDA ? 1:0);
            rtl8363_setAsicRegBit(1, 29, 13, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);


            break;

          case UDA3:
            rtl8363_getAsicReg(1, 20, MACPAG, 0, &regval);
            regval &= ~(0x3 << 12);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 12 ;
            rtl8363_setAsicReg(1, 20, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 27, MACPAG, 0, &regval);
            regval &= ~(0x3 << 4);
            regval |= ((fwdcfg.rgnPri & 0x3) << 4);
            rtl8363_setAsicReg(1, 27, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 19, 10, MACPAG, 0, fwdcfg.enUDA ? 1:0);
            rtl8363_setAsicRegBit(1, 29, 14, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

          case UDA4:
            rtl8363_getAsicReg(1, 20, MACPAG, 0, &regval);
            regval &= ~(0x3 << 14);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 14 ;
            rtl8363_setAsicReg(1, 20, MACPAG, 0, regval);

            rtl8363_getAsicReg(1, 27, MACPAG, 0, &regval);
            regval &= ~(0x3 << 6);
            regval |= ((fwdcfg.rgnPri & 0x3) << 6);
            rtl8363_setAsicReg(1, 27, MACPAG, 0, regval);

            rtl8363_setAsicRegBit(1, 19, 11, MACPAG, 0, fwdcfg.enUDA ? 1:0);
            rtl8363_setAsicRegBit(1, 29, 15, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

            break;

           case UKU:
            rtl8363_getAsicReg(1, 21, MACPAG, 0, &regval);
            regval &= ~(0x3 << 2);
            regval |= (fwdcfg.fwd_bhv & 0x3) << 2;
            rtl8363_setAsicReg(1, 21, MACPAG, 0, regval);

            break;

            case UKIP4MLT:
		 rtl8363_getAsicReg(1, 22, MACPAG, 0, &regval);
		 regval &= ~(0x3 << 6);
		 regval |= (fwdcfg.fwd_bhv & 0x3) << 6 ;
              rtl8363_setAsicReg(1, 22, MACPAG, 0, regval);

	       rtl8363_getAsicReg(1, 26, MACPAG, 0, &regval);
              regval &= ~(0x3 << 2);
              regval |= ((fwdcfg.rgnPri & 0x3) << 2);
              rtl8363_setAsicReg(1, 26, MACPAG, 0, regval);

		 rtl8363_setAsicRegBit(1, 22, 1, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);

		 break;

	     case UKIP6MLT:
		 rtl8363_getAsicReg(1, 22, MACPAG, 0, &regval);
		 regval &= ~(0x3 << 4);
		 regval |= (fwdcfg.fwd_bhv & 0x3) << 4 ;
              rtl8363_setAsicReg(1, 22, MACPAG, 0, regval);

	       rtl8363_getAsicReg(1, 26, MACPAG, 0, &regval);
              regval &= ~(0x3 << 0);
              regval |= ((fwdcfg.rgnPri & 0x3) << 0);
              rtl8363_setAsicReg(1, 26, MACPAG, 0, regval);

		 rtl8363_setAsicRegBit(1, 22, 0, MACPAG, 0, fwdcfg.enRgnPri ? 1:0);
		 break;


          default:
            return FAILED;

    }


    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicSpecialMACFWD | Get special MAC frame forwarding behaviour.
@parm enum SpecialMac | SpecialMac | Specify the special mac frame
@parm FwdSpecialMac_t* | fwdcfg
@struct FwdSpecialMac_t | This structure describe special MAC frame forwarding behaviour
@field uint8 | fwd_bhv | there are four option
@field uint8 | rgnPri | reassign the frame priority value 0~3
@field uint8 | enRgnPri |whether enable reassign the frame priority
@field uint8 | enUDA |whether enable the User Defined mac Address, it is only use for User Defined mac Address
@rvalue SUCCESS
@rvalue FAILED
@comm
special mac frame could be <nl>
NIPMLT      non-IP multicast <nl>
BRD           broadcast <nl>
PIGMP        IGMP in PPPoE frame <nl>
PMLD         MLD(IPv6 ICMP) in PPPoE frame <nl>
IGMP          IGMP packet but not PPPoE frame <nl>
MLD           MLD(IPv6 ICMP) but not PPPoE frame
RSV_XX      reserved multicast 01-80-c2-00-00-xx (xx != 00,01,02,03,10,20,21,04~0F,22~2F) <nl>
RSV_22       reserved multicast 01-80-c2-00-00-22~2F <nl>
RSV_04       reserved multicast 01-80-c2-00-00-04~0F <nl>
RSV_21       reserved multicast 01-80-c2-00-00-21 <nl>
RSV_20       reserved multicast 01-80-c2-00-00-20 <nl>
RSV_10       reserved multicast 01-80-c2-00-00-10 <nl>
RSV_03       reserved multicast 01-80-c2-00-00-03 <nl>
RSV_02       reserved multicast 01-80-c2-00-00-02 <nl>
RSV_01       reserved multicast 01-80-c2-00-00-01 <nl>
RSV_00       reserved multicast 01-80-c2-00-00-00 <nl>
PIP6MLT      IPv6 multicast in PPPoE frame <nl>
PIP4MLT      IPv4 multicast in PPPoE frame <nl>
KIP6MLT      Known IPv6 multicast <nl>
KIP4MLT      Known IPv4 multicast <nl>
UDA1          user defined address 1, use rtl8363_setAsicUsrDefMac set it <nl>
UDA2          user defined address 2, use rtl8363_setAsicUsrDefMac set it <nl>
UDA3          user defined address 3, use rtl8363_setAsicUsrDefMac set it <nl>
UDA4          user defined address 4, use rtl8363_setAsicUsrDefMac set it <nl>
UKU            unkown DA unicast <nl>
UKIP4MLT    unkown IPv4 multicast address <nl>
UKIP6MLT    unkown IPv6 multicast address <nl>

fwd_bhv could be: <nl>
RTL8363_FWD_NORMAL    normal forwarding <nl>
RTL8363_FWD_EXCPU      normal forwarding but excluding CPU port <nl>
RTL8363_FWD_TRAPCPU   trap to cpu only <nl>
RTL8363_FWD_DROP 3      drop <nl>

*/

int8 rtl8363_getAsicSpecialMACFWD(enum SpecialMac SpecialMac, FwdSpecialMac_t  *pFwdcfg)
{

    uint16 regval;

    switch(SpecialMac)
    {

        case NIPMLT:
            rtl8363_getAsicReg(1, 21, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 4)) >> 4;
            rtl8363_getAsicReg(1, 27, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 12)) >> 12;
            rtl8363_getAsicRegBit(1, 29, 0, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

        case  BRD:
            rtl8363_getAsicReg(1, 21, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 6)) >> 6;
            rtl8363_getAsicReg(1, 27, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 14)) >> 14;
            rtl8363_getAsicRegBit(1, 29, 1, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

        case PIGMP:
            rtl8363_getAsicReg(1, 20, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 2)) >> 2;
            rtl8363_getAsicReg(1, 26, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 8)) >> 8;
            rtl8363_getAsicRegBit(1, 29, 2, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

        case PMLD:
            rtl8363_getAsicReg(1, 20, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = regval & 0x3;
            rtl8363_getAsicReg(1, 26, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 10)) >> 10;
            rtl8363_getAsicRegBit(1, 29, 3, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

         case IGMP:
            rtl8363_getAsicReg(1, 20, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 4)) >> 4;
            rtl8363_getAsicReg(1, 26, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 12)) >> 12;
            rtl8363_getAsicRegBit(1, 29, 4, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

         case MLD:
            rtl8363_getAsicReg(1, 20, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 6)) >> 6;
            rtl8363_getAsicReg(1, 26, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 14)) >> 14;
            rtl8363_getAsicRegBit(1, 29, 5, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

          case RSV_XX:
            rtl8363_getAsicReg(1, 19, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 12)) >> 12;
            rtl8363_getAsicReg(1, 26, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 4)) >> 4;
            rtl8363_getAsicRegBit(1, 29, 6, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

          case RSV_22:
            rtl8363_getAsicReg(1, 19, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 14)) >> 14;
            rtl8363_getAsicReg(1, 26, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 6)) >> 6;
            rtl8363_getAsicRegBit(1, 29, 7, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

          case RSV_04:
            rtl8363_getAsicReg(1, 19, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = regval & 0x3 ;
            rtl8363_getAsicReg(1, 25, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 8)) >> 8;
            rtl8363_getAsicRegBit(1, 29, 8, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

          case RSV_21:
            rtl8363_getAsicReg(1, 19, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 2)) >> 2;
            rtl8363_getAsicReg(1, 25, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 10)) >> 10;
            rtl8363_getAsicRegBit(1, 29, 9, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

          case RSV_20:
            rtl8363_getAsicReg(1, 19, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 4)) >> 4;
            rtl8363_getAsicReg(1, 25, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 12)) >> 12;
            rtl8363_getAsicRegBit(1, 29, 10, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

          case RSV_10:
            rtl8363_getAsicReg(1, 19, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 6)) >> 6;
            rtl8363_getAsicReg(1, 25, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 10)) >> 10;
            rtl8363_getAsicRegBit(1, 29, 11, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

          case RSV_03:
            rtl8363_getAsicReg(1, 18, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 8)) >> 8;
            rtl8363_getAsicReg(1, 25, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = regval & 0x3;
            rtl8363_getAsicRegBit(1, 29, 12, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

          case RSV_02:
            rtl8363_getAsicReg(1, 18, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 10)) >> 10;
            rtl8363_getAsicReg(1, 25, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 2)) >> 2;
            rtl8363_getAsicRegBit(1, 29, 13, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

          case RSV_01:
            rtl8363_getAsicReg(1, 18, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 12)) >> 12;
            rtl8363_getAsicReg(1, 25, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 4)) >> 4;
            rtl8363_getAsicRegBit(1, 29, 14, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

          case RSV_00:
            rtl8363_getAsicReg(1, 18, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 14)) >> 14;
            rtl8363_getAsicReg(1, 25, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 6)) >> 6;
            rtl8363_getAsicRegBit(1, 29, 15, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

          case PIP6MLT:
            rtl8363_getAsicReg(1, 21, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 8)) >> 8;
            rtl8363_getAsicReg(1, 28, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = regval & 0x3 ;
            rtl8363_getAsicRegBit(1, 29, 16, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

          case PIP4MLT:
            rtl8363_getAsicReg(1, 21, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 10)) >> 10;
            rtl8363_getAsicReg(1, 28, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 2)) >> 2;
            rtl8363_getAsicRegBit(1, 29, 17, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

          case KIP6MLT:
            rtl8363_getAsicReg(1, 21, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 12)) >> 12;
            rtl8363_getAsicReg(1, 28, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 4)) >> 4;
            rtl8363_getAsicRegBit(1, 29, 18, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

          case KIP4MLT:
            rtl8363_getAsicReg(1, 21, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 14)) >> 14;
            rtl8363_getAsicReg(1, 28, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 6)) >> 6;
            rtl8363_getAsicRegBit(1, 29, 19, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

          case UDA1:
            rtl8363_getAsicReg(1, 20, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 8)) >> 8;
            rtl8363_getAsicReg(1, 27, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = regval & 0x3 ;
            rtl8363_getAsicRegBit(1, 29, 12, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            rtl8363_getAsicRegBit(1, 19, 8, MACPAG, 0, &regval);
            pFwdcfg->enUDA = regval ? TRUE:FALSE;
            break;

          case UDA2:
            rtl8363_getAsicReg(1, 20, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 10)) >> 10;
            rtl8363_getAsicReg(1, 27, MACPAG, 0, &regval);
            pFwdcfg->rgnPri= (regval & (0x3 << 2)) >> 2;
            rtl8363_getAsicRegBit(1, 29, 13, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            rtl8363_getAsicRegBit(1, 19, 9, MACPAG, 0, &regval);
            pFwdcfg->enUDA = regval ? TRUE:FALSE;
            break;

          case UDA3:
            rtl8363_getAsicReg(1, 20, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 12)) >> 12;
            rtl8363_getAsicReg(1, 27, MACPAG, 0, &regval);
            pFwdcfg->rgnPri= (regval & (0x3 << 4)) >> 4;
            rtl8363_getAsicRegBit(1, 29, 14, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            rtl8363_getAsicRegBit(1, 19, 10, MACPAG, 0, &regval);
            pFwdcfg->enUDA = regval ? TRUE:FALSE;
            break;

          case UDA4:
            rtl8363_getAsicReg(1, 20, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 14)) >> 14;
            rtl8363_getAsicReg(1, 27, MACPAG, 0, &regval);
            pFwdcfg->rgnPri= (regval & (0x3 << 6)) >> 6;
            rtl8363_getAsicRegBit(1, 29, 15, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            rtl8363_getAsicRegBit(1, 19, 11, MACPAG, 0, &regval);
            pFwdcfg->enUDA = regval ? TRUE:FALSE;
            break;

           case UKU:
            rtl8363_getAsicReg(1, 21, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 2)) >> 2;
            break;

	    case UKIP4MLT:
            rtl8363_getAsicReg(1, 22, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 6)) >> 6;
	     rtl8363_getAsicReg(1, 26, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 2)) >> 2;
            rtl8363_getAsicRegBit(1, 22, 1, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

	    case UKIP6MLT:
            rtl8363_getAsicReg(1, 22, MACPAG, 0, &regval);
            pFwdcfg->fwd_bhv = (regval & (0x3 << 4)) >> 4;
	     rtl8363_getAsicReg(1, 26, MACPAG, 0, &regval);
            pFwdcfg->rgnPri = (regval & (0x3 << 0)) >> 0;
            rtl8363_getAsicRegBit(1, 22, 0, MACPAG, 0, &regval);
            pFwdcfg->enRgnPri = regval ? TRUE:FALSE;
            break;

          default:
            return FAILED;

    }

    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicUsrDefMac | Set user defined mac address
@parm enum SpecialMac | SpecialMac |  which entry, UDA1 ~UDA4
@parm uint8* | pMac | the mac address
@rvalue SUCCESS
@rvalue FAILED
@comm
There are total 4 entry to set user defined mac address:UDA1, UDA2, UDA3, UDA4.
The API should be used with rtl8363_setAsicSpecialMACFWD.
*/

int8 rtl8363_setAsicUsrDefMac(enum SpecialMac SpecialMac, uint8* pMac)
{
    uint16 regval;
    uint8 regbase;

    switch(SpecialMac)
    {
        case UDA1:
            regbase = 17;
            break;

        case UDA2:
            regbase = 20;
            break;
        case UDA3:
            regbase = 23;
            break;
        case UDA4:
            regbase = 26;
            break;
        default:
            return FAILED;
    }
    regval = *pMac | ((*(pMac + 1)) << 8);
    rtl8363_setAsicReg(1, regbase, MACPAG, 1, regval);
    regval = *(pMac + 2) | ((*(pMac + 3)) << 8);
    rtl8363_setAsicReg(1, regbase + 1, MACPAG, 1, regval);
    regval = *(pMac + 4) | ((*(pMac + 5)) << 8);
    rtl8363_setAsicReg(1, regbase + 2, MACPAG, 1, regval);

    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicUsrDefMac | Get user defined mac address
@parm enum SpecialMac | SpecialMac |  which entry, UDA1 ~UDA4
@parm uint8* | pMac | the mac address
@rvalue SUCCESS
@rvalue FAILED
@comm
There are total 4 entry to set user defined mac address:UDA1, UDA2, UDA3, UDA4.
The API should be used with rtl8363_setAsicSpecialMACFWD.
*/

int8 rtl8363_getAsicUsrDefMac(enum SpecialMac SpecialMac, uint8* pMac)
{
    uint16 regval;
    uint8 regbase;

    switch(SpecialMac)
    {
        case UDA1:
            regbase = 17;
            break;

        case UDA2:
            regbase = 20;
            break;
        case UDA3:
            regbase = 23;
            break;
        case UDA4:
            regbase = 26;
            break;
        default:
            return FAILED;
    }
    rtl8363_getAsicReg(1, regbase, MACPAG, 1, &regval);
    *pMac = regval & 0xFF;
    *(pMac + 1) = (regval & 0xFF00) >> 8;
    rtl8363_getAsicReg(1, regbase + 1, MACPAG, 1, &regval);
    *(pMac + 2) = regval & 0xFF;
    *(pMac + 3) = (regval & 0xFF00) >> 8;
    rtl8363_getAsicReg(1, regbase + 2, MACPAG, 1, &regval);
    *(pMac + 4) = regval & 0xFF;
    *(pMac + 5) = (regval & 0xFF00) >> 8;
    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicMIB | Configure MIB counter
@parm uint8 | port | Specify port number
@parm enum MIBOpType | mibop
@rvalue SUCCESS
@rvalue FAILED
@comm
port could be RTL8363_PORT0,RTL8363_PORT1, RTL8363_PORT2, RTL8363_ANYPORT(all port);<nl>
mibop could be RELEASE(begin counting), STOP(stop counting), RESET(reset couter);<nl>
*/

int8 rtl8363_setAsicMIB(uint8 port, enum MIBOpType mibop)
{
    uint16 bitval;
    uint32 pollcnt;

    if (port > RTL8363_ANYPORT)
        return FAILED;

    switch(mibop)
    {
        case RELEASE:
            rtl8363_setAsicRegBit(0, 17, 8 + port, MACPAG, 4, 0);
            break;

        case STOP:
            rtl8363_setAsicRegBit(0, 17, 8 + port, MACPAG, 4, 1);
            break;

        case RESET:
            rtl8363_setAsicRegBit(0, 17, port, MACPAG, 4, 1);
            for (pollcnt = 0; pollcnt < RTL8363_IDLE_TIMEOUT; pollcnt ++)
            {
                rtl8363_getAsicRegBit(0, 17, port, MACPAG, 4, &bitval);
                if(!bitval)
                    break;
            }
            if (pollcnt == RTL8363_IDLE_TIMEOUT)
                return FAILED;
            break;

        default:
            return FAILED;

    }

    return SUCCESS;
}

/*
@func int8 | rtl8363_getAsicMIB | Get MIB counter
@parm uint8 | MIBaddr | Specify which MIB counter
@parm uint8* | pMIBcnt | counter value
@rvalue SUCCESS
@rvalue FAILED
@comm
please refence datasheet to get MIB counter address
*/

int8 rtl8363_getAsicMIB(uint8 MIBaddr, uint8* pMIBcnt)
{
    uint16 regval;
    uint32 pollcnt;

    if (pMIBcnt == NULL)
        return FAILED;

    rtl8363_getAsicReg(0, 18, MACPAG, 4, &regval);
    regval &= 0xFF;
    regval |= (0x80 | (MIBaddr & 0x7F)) << 8;
    rtl8363_setAsicReg(0, 18, MACPAG, 4, regval);
    for (pollcnt = 0; pollcnt < RTL8363_IDLE_TIMEOUT; pollcnt ++)
    {
        rtl8363_getAsicRegBit(0, 18, 15, MACPAG, 4, &regval);
        if (!regval)
            break;
    }
    if (pollcnt == RTL8363_IDLE_TIMEOUT)
        return FAILED;
    /*Get MIB counter value*/
    if (MIBaddr <= 0x8)
    {
        /*64 bit counter*/
        rtl8363_getAsicReg(0, 22, MACPAG, 4, &regval);
        *pMIBcnt = (regval & 0xFF00) >> 8;
        *(pMIBcnt + 1) = regval & 0xFF;
        rtl8363_getAsicReg(0, 21, MACPAG, 4, &regval);
        *(pMIBcnt + 2) = (regval & 0xFF00) >> 8;
        *(pMIBcnt + 3) = regval & 0xFF;
        rtl8363_getAsicReg(0, 20, MACPAG, 4, &regval);
        *(pMIBcnt + 4) = (regval & 0xFF00) >> 8;
        *(pMIBcnt + 5) = regval & 0xFF;
        rtl8363_getAsicReg(0, 19, MACPAG, 4, &regval);
        *(pMIBcnt + 6) = (regval & 0xFF00) >> 8;
        *(pMIBcnt + 7) = regval & 0xFF;
    }
    else if (MIBaddr >= 0x12)
    {
        /*32 bit counter*/
        rtl8363_getAsicReg(0, 20, MACPAG, 4, &regval);
        *pMIBcnt  = (regval & 0xFF00) >> 8;
        *(pMIBcnt + 1) = regval & 0xFF;
        rtl8363_getAsicReg(0, 19, MACPAG, 4, &regval);
        *(pMIBcnt + 2) = (regval & 0xFF00) >> 8;
        *(pMIBcnt + 3) = regval & 0xFF;
    }

    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsic1dPortState | Set IEEE 802.1d spanning tree port state
@parm uint8 | port | Specify port number(0~2)
@parm enum SPTPortState | state
@rvalue SUCCESS
@rvalue FAILED
@comm
There are 4 port state:<nl>
DISABLE  - Disable state<nl>
BLOCK    - Blocking state<nl>
LEARN    - Learning state<nl>
FORWARD	- Forwarding state<nl>
*/

int8 rtl8363_setAsic1dPortState(uint8 port, enum SPTPortState state)
{
    uint16 regval;

    if (port > RTL8363_PORT2)
        return FAILED;

    rtl8363_getAsicReg(2, 21, MACPAG, 1, &regval);
    regval &= ~(0x3 << (2*port));
    regval |= (state << (2*port));
    rtl8363_setAsicReg(2, 21, MACPAG, 1, regval);

    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsic1dPortState | Get IEEE 802.1d spanning tree port state
@parm uint8 | port | Specify port number(0~2)
@parm enum SPTPortState | state
@rvalue SUCCESS
@rvalue FAILED
@comm
There are 4 port state:<nl>
DISABLE  - Disable state<nl>
BLOCK    - Blocking state<nl>
LEARN    - Learning state<nl>
FORWARD	- Forwarding state<nl>
*/
int8 rtl8363_getAsic1dPortState(uint8 port, enum SPTPortState *pState)
{
    uint16 regval;

    if (port > RTL8363_PORT2)
        return FAILED;

    rtl8363_getAsicReg(2, 21, MACPAG, 1, &regval);
    *pState = (enum SPTPortState)((regval & (0x3 << (2*port))) >> (2*port));

    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicMirror | Set asic mirror ability
@parm mirrorPara_t | mir | Specify mirror ability
@struct mirrorPara_t | This structure describe mirror parameter
@field uint8 | mirport | specify mirror port, 0x3 means no mirror port
@field uint8 | rxport | Rx mirrored port mask
@field uint8 | txport | Rx mirrored port mask
@field uint8 | enMirMac | enable mirror port to mirror pkt. by its SA or DA
@field uint8 | mac[6] | Source and destination address of mirrored pkt
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_setAsicMirror(mirrorPara_t mir)
{
    uint16 regval;

    rtl8363_getAsicReg(2, 17, MACPAG, 1, &regval);
    regval &= 0x7F00;
    regval |= ((mir.mirport & 0x3 ) << 6) | ((mir.rxport & 0x7) << 3) | ((mir.txport & 0x7));
    regval |= (mir.enMirMac ? (0x1 << 15):0 );
    rtl8363_setAsicReg(2, 17, MACPAG, 1, regval);

    if (mir.enMirMac)
    {
        regval = mir.mac[0] | (mir.mac[1] << 8);
        rtl8363_setAsicReg(2, 18, MACPAG, 1, regval);
        regval = mir.mac[2] | (mir.mac[3] << 8);
        rtl8363_setAsicReg(2, 19, MACPAG, 1, regval);
        regval = mir.mac[4] | (mir.mac[5] << 8);
        rtl8363_setAsicReg(2, 20, MACPAG, 1, regval);
    }

    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicMirror | Get asic mirror ability
@parm mirrorPara_t* | pMir |  mirror ability
@struct mirrorPara_t | This structure describe mirror parameter
@field uint8 | mirport | specify mirror port, 0x3 means no mirror port
@field uint8 | rxport | Rx mirrored port mask
@field uint8 | txport | Rx mirrored port mask
@field uint8 | enMirMac | enable mirror port to mirror pkt. by its SA or DA
@field uint8 | mac[6] | Source and destination address of mirrored pkt
@rvalue SUCCESS
@rvalue FAILED
@comm
*/

int8 rtl8363_getAsicMirror(mirrorPara_t *pMir)
{
    uint16 regval;

    rtl8363_getAsicReg(2, 17, MACPAG, 1, &regval);
    pMir->mirport = (regval & (0x3 << 6)) >> 6;
    pMir->rxport = (regval & (0x7 << 3)) >> 3;
    pMir->txport = regval & 0x7;
    pMir->enMirMac = (regval & 0x8000) ? TRUE:FALSE;
    rtl8363_getAsicReg(2, 18, MACPAG, 1, &regval);
    pMir->mac[0] = regval & 0xFF;
    pMir->mac[1] = (regval & 0xFF00) >> 8;
    rtl8363_getAsicReg(2, 19, MACPAG, 1, &regval);
    pMir->mac[2] = regval & 0xFF;
    pMir->mac[3] = (regval & 0xFF00) >> 8;
    rtl8363_getAsicReg(2, 20, MACPAG, 1, &regval);
    pMir->mac[4] = regval & 0xFF;
    pMir->mac[5] = (regval & 0xFF00) >> 8;

    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicStormFilter | Get asic storm filter
@parm stormPara_t | stm | storm filter ability
@struct stormPara_t | This structure describe storm filter parameter
@field uint8 | enBRD | Enable/disable Broadcast Storm Control
@field uint8 | enMUL | Enable/disable multicast storm filter
@field uint8 | enUDA | Enable/disable unknown DA unicast storm filter
@field uint8 | trigNum | how many continuous broadcast/multicast/unkown DA packet will be treat as storm
@field uint8 | filTime | how much time filter traffic after trigger storm filter
@field uint8 | onlyTmClr | only timer can reset storm filter or both timer and other kind of the packets which is different from the filtered packet can reset storm filter
@rvalue SUCCESS
@rvalue FAILED
@comm
trigNum could be set as one of four macros:<nl>
    RTL8363_STM_FILNUM64  - continuous 64 pkts will trigger storm fileter<nl>
    RTL8363_STM_FILNUM32  -  continuous 32 pkts will trigger storm fileter<nl>
    RTL8363_STM_FILNUM16  -  continuous 16 pkts will trigger storm fileter<nl>
    RTL8363_STM_FILNUM8    -  continuous 8 pkts will trigger storm fileter<nl>

filTime could be set as one of four macros:<nl>
    RTL8363_STM_FIL800MS - filter 800ms after trigger storm filter<nl>
    RTL8363_STM_FIL400MS - filter 400ms after trigger storm filter<nl>
    RTL8363_STM_FIL200MS - filter 200ms after trigger storm filter<nl>
    RTL8363_STM_FIL100MS - filter 100ms after trigger storm filter<nl>
*/

int8 rtl8363_setAsicStormFilter(stormPara_t stm)
{
   uint16 regval;

   rtl8363_getAsicReg(1, 16, MACPAG, 0, &regval);
   regval &= 0xFF00;
   regval |= (stm.enBRD ? 0:(0x1 << 7)) | (stm.enMUL ? 0:(0x1 << 6)) | (stm.enUDA ? 0:(0x1 << 5 ));
   regval |= ((stm.trigNum & 0x3) << 3) | (stm.onlyTmClr ? (0x1 << 2):0) | (stm.filTime & 0x3);
   rtl8363_setAsicReg(1, 16, MACPAG, 0, regval);
    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicStormFilter | Get asic storm filter
@parm stormPara_t* | pStm | storm filter ability
@struct stormPara_t | This structure describe storm filter parameter
@field uint8 | enBRD | Enable/disable Broadcast Storm Control
@field uint8 | enMUL | Enable/disable multicast storm filter
@field uint8 | enUDA | Enable/disable unknown DA unicast storm filter
@field uint8 | trigNum | how many continuous broadcast/multicast/unkown DA packet will be treat as storm
@field uint8 | filTime | how much time filter traffic after trigger storm filter
@field uint8 | onlyTmClr | only timer can reset storm filter or both timer and other kind of the packets which is different from the filtered packet can reset storm filter
@rvalue SUCCESS
@rvalue FAILED
@comm
trigNum could be set as one of four macros:<nl>
    RTL8363_STM_FILNUM64  - continuous 64 pkts will trigger storm fileter<nl>
    RTL8363_STM_FILNUM32  -  continuous 32 pkts will trigger storm fileter<nl>
    RTL8363_STM_FILNUM16  -  continuous 16 pkts will trigger storm fileter<nl>
    RTL8363_STM_FILNUM8    -  continuous 8 pkts will trigger storm fileter<nl>

filTime could be set as one of four macros:<nl>
    RTL8363_STM_FIL800MS - filter 800ms after trigger storm filter<nl>
    RTL8363_STM_FIL400MS - filter 400ms after trigger storm filter<nl>
    RTL8363_STM_FIL200MS - filter 200ms after trigger storm filter<nl>
    RTL8363_STM_FIL100MS - filter 100ms after trigger storm filter<nl>
*/

int8 rtl8363_getAsicStormFilter(stormPara_t *pStm)
{
    uint16 regval;

    rtl8363_getAsicReg(1, 16, MACPAG, 0, &regval);
    pStm->enBRD = (regval & (0x1 << 7)) ? FALSE:TRUE;
    pStm->enMUL = (regval & (0x1 << 6)) ? FALSE:TRUE;
    pStm->enUDA = (regval & (0x1 << 5)) ? FALSE:TRUE;
    pStm->trigNum = (regval & (0x3 << 3)) >> 3;
    pStm->onlyTmClr = regval & (0x1 << 2) ? TRUE:FALSE;
    pStm->trigNum = regval & 0x3;
    return SUCCESS;
}


int8 rtl8363_getAsicChipID(enum ChipID * id, enum RLNum *rlnum,enum VerNum * ver)
{
uint16  regval;


    /*enable internal read */
    rtl8363_setAsicRegBit(1, 21, 9, MACPAG, 9, 1);

    rtl8363_getAsicReg(1, 20, MACPAG, 9, &regval);
    if (regval & (0x1 << 5))
    {
        if (((regval & (0x7 << 10)) >> 10) == 0x1)
        {
            *id = RTL8203M;
        }
        else if (((regval & (0x7 << 10)) >> 10) == 0x3)
        {
            *id = RTL8303H;
        }
        else
        {
            return FAILED;
        }

    }
    else
    {
        if (((regval & (0x7 << 10)) >> 10) == 0x1)
        {
            *id = RTL8213M;
        }
        else if (((regval & (0x7 << 10)) >> 10) == 0x2)
        {
            *id = RTL8363S;
        }
        else if (((regval & (0x7 << 10)) >> 10) == 0x3)
        {
            *id = RTL8363H;
        }
        else
        {
            return FAILED;
        }

    }

        rtl8363_getAsicReg(0, 13, SERLPAG, 6, &regval);
        if (regval == 0x6098)
        {
          *rlnum = RTL6098;
        }
        else
            *rlnum = RTL6021;

    rtl8363_getAsicReg(1, 27, MACPAG, 9, &regval);
	if (*rlnum==RTL6021)
	{
		    switch (regval)
		{
		case 0:
			*ver = VER_A;
			break;
		case 1:
			*ver = VER_B;
			break;
		default:
		 return FAILED;
		}
	}
else if (*rlnum==RTL6098)
{
	    if (regval== 2)
		*ver = VER_A;
	    else
		return FAILED;
}
else
	return FAILED;



    /*disable internal read */
    rtl8363_setAsicRegBit(1, 21, 9, MACPAG, 9, 0);

    return SUCCESS;

}

int8 rtl8363_setAsicGPIO(enum GPIO gpio_pin, GPIOcfg_t *pCfg)
{
    if (gpio_pin == GB3)
        gpio_pin++;

    rtl8363_setAsicRegBit(0, 23, gpio_pin, MACPAG, 4, pCfg->dir);
    rtl8363_setAsicRegBit(0, 26, gpio_pin, MACPAG, 4, pCfg->enint);
    if (gpio_pin < GB0)
    {
        rtl8363_setAsicRegBit(0, 25, 11, MACPAG, 4, (pCfg->intsig & 0x2) ? (0x1 << 11):0);
        rtl8363_setAsicRegBit(0, 25, 10, MACPAG, 4, (pCfg->intsig & 0x1) ? (0x1 << 10):0);
    }
    else
    {
        rtl8363_setAsicRegBit(0, 25, 13, MACPAG, 4, (pCfg->intsig & 0x2) ? (0x1 << 13):0);
        rtl8363_setAsicRegBit(0, 25, 12, MACPAG, 4, (pCfg->intsig & 0x1) ? (0x1 << 12):0);
    }
   return SUCCESS;
}


int8 rtl8363_setAsicGpioPin(enum GPIO gpio_pin, uint8 val)
{
    if (gpio_pin == GB3)
        gpio_pin++;
    rtl8363_setAsicRegBit(0, 24, gpio_pin, MACPAG, 4, (uint16)val);
    return SUCCESS;
}


int8 rtl8363_getAsicGpioPin(enum GPIO gpio_pin, uint8 *pData)
{
    uint16 bitval;

    if (gpio_pin == GB3)
        gpio_pin++;
    rtl8363_getAsicRegBit(0, 24, gpio_pin, MACPAG, 4, &bitval);
    *pData = bitval ? 1:0;
    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicWolEnable | Configure WOL function
@parm wolCfg_t* | pWolCfg | wol configurations
@struct wolCfg_t | this structure defines wol configuations
@field uint8 | WOLEn | enable/disable WOL mode
@field uint8 | LUEn | whether cpu could wake up and switch go into normal mode when the cable connection is re-established
@field uint8 | MPEn | whether magic packet can wakeup the system.
@field uint8 | WOLSerdes | whether Serdes can work in WOL mode, set it ture when using fiber
@field uint8 | WSig | wake-up signal property
@field uint8 | nodeid[6] | node ID in magic packet, it is switch NIC MAC address
@rvalue SUCCESS
@rvalue FAILED
@comm
*/
int8 rtl8363_setAsicWolEnable(wolCfg_t *pWolCfg)
{
    uint16 regval;

    rtl8363_setAsicRegBit(0, 17, 7, MACPAG, 8, pWolCfg->WOLEn ? 1:0);
    rtl8363_setAsicRegBit(0, 17, 6, MACPAG, 8, pWolCfg->LUEn? 1:0);
    rtl8363_setAsicRegBit(0, 17, 5, MACPAG, 8, pWolCfg->MPEn? 1:0);
    rtl8363_setAsicRegBit(0, 17, 4, MACPAG, 8, pWolCfg->WFEn? 1:0);
    rtl8363_setAsicRegBit(0, 17, 8, MACPAG, 8 , (pWolCfg->WSig == RTL8363_SIG_NONE) ?1:0 );
    rtl8363_setAsicRegBit(0, 17, 2, MACPAG, 8, (pWolCfg->WSig & 0x2) ? 1:0);
    rtl8363_setAsicRegBit(0, 17, 3, MACPAG, 8, (pWolCfg->WSig & 0x1) ? 1:0);
    rtl8363_setAsicRegBit(0, 17, 0, MACPAG, 8, pWolCfg->WOLSerdes ? 1:0);
    regval = ((pWolCfg->nodeID[1]) << 8 ) | (pWolCfg->nodeID[0]);
    rtl8363_setAsicReg(0, 21, MACPAG, 13, regval);
    regval = ((pWolCfg->nodeID[3]) << 8 ) | (pWolCfg->nodeID[2]);
    rtl8363_setAsicReg(0, 22, MACPAG, 13, regval);
    regval = ((pWolCfg->nodeID[5]) << 8 ) | (pWolCfg->nodeID[4]);
    rtl8363_setAsicReg(0, 23, MACPAG, 13, regval);
    return SUCCESS;
}


/*
@func int8 | rtl8363_setAsicWolWakeupFrame | set WOL wake up frame
@parm uint8 | num | 0~7, there are 8 sample wake-up frame at most
@parm wakeupFrameMask_t* | pMask | wake up frame mask value
@sturct wakeupFrameMask_t | The structure defines wake up sample frame
@field uint16 | crc | crc value of masked bytes of sample frame
@field uint16 | Wakeupmask | each bit represents one byte mask, Wakeupmask[0].[15-0] = wfmask0[127-112], ...,Wakeupmask[7].[15-0] = wfmask0[15-0]
@rvalue SUCCESS
@rvalue FAILED
@comm
*/
int8 rtl8363_setAsicWolWakeupFrame(uint8 num, wakeupFrameMask_t *pMask)
{
    /*crc*/
    rtl8363_setAsicReg(0, 19 + num, MACPAG, 8, pMask->crc);
    /*wake up frame*/
    switch (num)
    {
        case 0:
            rtl8363_setAsicReg(0, 20, MACPAG, 9, pMask->Wakeupmask[0]);
            rtl8363_setAsicReg(0, 19, MACPAG, 9, pMask->Wakeupmask[1]);
            rtl8363_setAsicReg(0, 18, MACPAG, 9, pMask->Wakeupmask[2]);
            rtl8363_setAsicReg(0, 17, MACPAG, 9, pMask->Wakeupmask[3]);
            rtl8363_setAsicReg(0, 30, MACPAG, 8, pMask->Wakeupmask[4]);
            rtl8363_setAsicReg(0, 29, MACPAG, 8, pMask->Wakeupmask[5]);
            rtl8363_setAsicReg(0, 28, MACPAG, 8, pMask->Wakeupmask[6]);
            rtl8363_setAsicReg(0, 27, MACPAG, 8, pMask->Wakeupmask[7]);
            break;
        case 1:
            rtl8363_setAsicReg(0, 28, MACPAG, 9, pMask->Wakeupmask[0]);
            rtl8363_setAsicReg(0, 27, MACPAG, 9, pMask->Wakeupmask[1]);
            rtl8363_setAsicReg(0, 26, MACPAG, 9, pMask->Wakeupmask[2]);
            rtl8363_setAsicReg(0, 25, MACPAG, 9, pMask->Wakeupmask[3]);
            rtl8363_setAsicReg(0, 24, MACPAG, 9, pMask->Wakeupmask[4]);
            rtl8363_setAsicReg(0, 23, MACPAG, 9, pMask->Wakeupmask[5]);
            rtl8363_setAsicReg(0, 22, MACPAG, 9, pMask->Wakeupmask[6]);
            rtl8363_setAsicReg(0, 21, MACPAG, 9, pMask->Wakeupmask[7]);
            break;
        case 2:
            rtl8363_setAsicReg(0, 22, MACPAG, 10, pMask->Wakeupmask[0]);
            rtl8363_setAsicReg(0, 21, MACPAG, 10, pMask->Wakeupmask[1]);
            rtl8363_setAsicReg(0, 20, MACPAG, 10, pMask->Wakeupmask[2]);
            rtl8363_setAsicReg(0, 19, MACPAG, 10, pMask->Wakeupmask[3]);
            rtl8363_setAsicReg(0, 18, MACPAG, 10, pMask->Wakeupmask[4]);
            rtl8363_setAsicReg(0, 17, MACPAG, 10, pMask->Wakeupmask[5]);
            rtl8363_setAsicReg(0, 30, MACPAG, 9, pMask->Wakeupmask[6]);
            rtl8363_setAsicReg(0, 29, MACPAG, 9, pMask->Wakeupmask[7]);
            break;
        case 3:
            rtl8363_setAsicReg(0, 30, MACPAG, 10, pMask->Wakeupmask[0]);
            rtl8363_setAsicReg(0, 29, MACPAG, 10, pMask->Wakeupmask[1]);
            rtl8363_setAsicReg(0, 28, MACPAG, 10, pMask->Wakeupmask[2]);
            rtl8363_setAsicReg(0, 27, MACPAG, 10, pMask->Wakeupmask[3]);
            rtl8363_setAsicReg(0, 26, MACPAG, 10, pMask->Wakeupmask[4]);
            rtl8363_setAsicReg(0, 25, MACPAG, 10, pMask->Wakeupmask[5]);
            rtl8363_setAsicReg(0, 24, MACPAG, 10, pMask->Wakeupmask[6]);
            rtl8363_setAsicReg(0, 23, MACPAG, 10, pMask->Wakeupmask[7]);
            break;
        case 4:
            rtl8363_setAsicReg(0, 24, MACPAG, 11, pMask->Wakeupmask[0]);
            rtl8363_setAsicReg(0, 23, MACPAG, 11, pMask->Wakeupmask[1]);
            rtl8363_setAsicReg(0, 22, MACPAG, 11, pMask->Wakeupmask[2]);
            rtl8363_setAsicReg(0, 21, MACPAG, 11, pMask->Wakeupmask[3]);
            rtl8363_setAsicReg(0, 20, MACPAG, 11, pMask->Wakeupmask[4]);
            rtl8363_setAsicReg(0, 19, MACPAG, 11, pMask->Wakeupmask[5]);
            rtl8363_setAsicReg(0, 18, MACPAG, 11, pMask->Wakeupmask[6]);
            rtl8363_setAsicReg(0, 17, MACPAG, 11, pMask->Wakeupmask[7]);
            break;
        case 5:
            rtl8363_setAsicReg(0, 18, MACPAG, 12, pMask->Wakeupmask[0]);
            rtl8363_setAsicReg(0, 17, MACPAG, 12, pMask->Wakeupmask[1]);
            rtl8363_setAsicReg(0, 30, MACPAG, 11, pMask->Wakeupmask[2]);
            rtl8363_setAsicReg(0, 29, MACPAG, 11, pMask->Wakeupmask[3]);
            rtl8363_setAsicReg(0, 28, MACPAG, 11, pMask->Wakeupmask[4]);
            rtl8363_setAsicReg(0, 27, MACPAG, 11, pMask->Wakeupmask[5]);
            rtl8363_setAsicReg(0, 26, MACPAG, 11, pMask->Wakeupmask[6]);
            rtl8363_setAsicReg(0, 25, MACPAG, 11, pMask->Wakeupmask[7]);
            break;
        case 6:
            rtl8363_setAsicReg(0, 26, MACPAG, 12, pMask->Wakeupmask[0]);
            rtl8363_setAsicReg(0, 25, MACPAG, 12, pMask->Wakeupmask[1]);
            rtl8363_setAsicReg(0, 24, MACPAG, 12, pMask->Wakeupmask[2]);
            rtl8363_setAsicReg(0, 23, MACPAG, 12, pMask->Wakeupmask[3]);
            rtl8363_setAsicReg(0, 22, MACPAG, 12, pMask->Wakeupmask[4]);
            rtl8363_setAsicReg(0, 21, MACPAG, 12, pMask->Wakeupmask[5]);
            rtl8363_setAsicReg(0, 20, MACPAG, 12, pMask->Wakeupmask[6]);
            rtl8363_setAsicReg(0, 19, MACPAG, 12, pMask->Wakeupmask[7]);
            break;
         case 7:
            rtl8363_setAsicReg(0, 20, MACPAG, 13, pMask->Wakeupmask[0]);
            rtl8363_setAsicReg(0, 19, MACPAG, 13, pMask->Wakeupmask[1]);
            rtl8363_setAsicReg(0, 18, MACPAG, 13, pMask->Wakeupmask[2]);
            rtl8363_setAsicReg(0, 17, MACPAG, 13, pMask->Wakeupmask[3]);
            rtl8363_setAsicReg(0, 30, MACPAG, 12, pMask->Wakeupmask[4]);
            rtl8363_setAsicReg(0, 29, MACPAG, 12, pMask->Wakeupmask[5]);
            rtl8363_setAsicReg(0, 28, MACPAG, 12, pMask->Wakeupmask[6]);
            rtl8363_setAsicReg(0, 27, MACPAG, 12, pMask->Wakeupmask[7]);
            break;
         default:
            return FAILED;
    }
    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicEnterSleepMode | asic enter sleep mode and pkts queues in cpu port
@parm uint8 | port | the port connet with cpu
@rvalue SUCCESS
@rvalue FAILED
@comm
This sleep mode is a special sleep mode, the port will not forward pkts but queue them in
buffer, you should call rtl8363_setAsicExitSleepMode to exit this mode.
*/
int8 rtl8363_setAsicEnterSleepMode(uint8 port)
{
    uint16 i;
    rtl8363_setAsicReg(port, 17, MACPAG, 2, 0);
    rtl8363_setAsicReg(port, 18, MACPAG, 2, 0);
    /*clr qos counter*/
    rtl8363_setAsicRegBit(1, 17, 15, MACPAG, 13, 1);
    for (i = 0; i < 500; i++ );
    rtl8363_setAsicRegBit(1, 17, 15, MACPAG, 13, 0);

    return SUCCESS;
}

/*
@func int8 | rtl8363_getAiscWolQueEmpty | polling whether there are pkts queued in the port
@parm uint8 | port | the port connet with cpu
@parm uint8* | pIsEmpty | the buffer is empty
@rvalue SUCCESS
@rvalue FAILED
@comm
*/
int8 rtl8363_getAiscWolQueEmpty(uint8 port , uint8 * pIsEmpty)
{
    uint16 regval;
    switch(port)
    {
        case 0:
            rtl8363_getAsicReg(1, 23, MACPAG, 13, &regval);
            *pIsEmpty = (regval & 0xFF00) ? 0:1;
            break;
        case 1:
            rtl8363_getAsicReg(1, 23, MACPAG, 13, &regval);
            *pIsEmpty = (regval & 0xFF) ? 0:1;
            break;
        case 2:
            rtl8363_getAsicReg(1, 19, MACPAG, 13, &regval);
            *pIsEmpty = (regval & 0xFF) ? 0:1;
            break;
        default:
            return FAILED;
    }
    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicExitSleepMode | asic exit sleep mode, pkts will not queu in the ports
@parm uint8 | port | the port connet with cpu
@parm WRRPara_t | *pWrr | the port scheduling parameter
@struct WRRPara_t | the structure defines scheduling parameter of the cpu port
@field uint8 | q3_wt | weight of queue 3
@field uint8 | q2_wt | weight of queue 2
@field uint8 | q1_wt | weight of queue 1
@field uint8 | q0_wt | weight of queue 0
@field uint8 | q3_enstrpri | whether q3 enable strict priority
@field uint8 | q2_enstrpri | whether q2 enable strict priority
@rvalue SUCCESS
@rvalue FAILED
@comm
*/
int8 rtl8363_setAsicExitSleepMode(uint8 port, WRRPara_t *pWrr )
{
    uint16 regval;

    /*set queue weight and strict priority*/
    regval = (pWrr->q3_wt & 0x7F) | (pWrr->q3_enstrpri ? (0x1 << 7) :0);
    regval |=  (pWrr->q2_wt & 0x7F) << 8 | (pWrr->q2_enstrpri ? (0x1 << 15) :0);
    rtl8363_setAsicReg(port, 17, MACPAG, 2, regval);

    rtl8363_getAsicReg(port, 18, MACPAG, 2, &regval);
    regval &= 0x8080;
    regval |= (pWrr->q1_wt & 0x7F) | ((pWrr->q0_wt & 0x7F) << 8);
    rtl8363_setAsicReg(port, 18, MACPAG, 2, regval);

    return SUCCESS;
}

/*
@func int8 | rtl8363_setAsicInit | Init Asic
@rvalue SUCCESS
@rvalue FAILED
@comm
This function must be called during system initialization process
*/

int8 rtl8363_setAsicInit(void)
{
    enum ChipID id;
    enum VerNum ver;
    enum RLNum rlnum;

 if (SUCCESS != rtl8363_getAsicChipID(&id, &rlnum,&ver))
        return FAILED;
   /*printf("Chip id = 0x%x, Version = 0x%x", id, ver);*/
    if (((rlnum == RTL6098) && (ver == VER_A))||(rlnum == RTL6021))
    {
	rtl8363_setAsicRegBit(0, 20, 15, UTPPAG, 0, 0);
	rtl8363_setAsicRegBit(1, 20, 15, UTPPAG, 0, 0);
    }

    return SUCCESS;
}


/*
@func int8| rtl8363_resetSwitch | HW reset. It will all blocks and  load configurations from EEPROM or strapping pins.
@rvalue SUCCESS
@rvalue FAILED
@comm
*/
int8 rtl8363_resetSwitch(void)
{
     rtl8363_setAsicRegBit(0, 16, 10, MACPAG, 0, 1);
     return  SUCCESS;
}


/*
@func int8 | rtl8363_setAsicLocalLbEnable | Enable/disbale "local loopback", i.e. loop MAC's RX back to TX.
@parm uint8 |port | prot number
@parm uint8 | enabled | TRUE or FALSE
@rvalue SUCCESS
@rvalue FAILED
@comm
*/
int8 rtl8363_setAsicLocalLbEnable(uint8 port, uint8 enabled)
{
     if (port == 0)
     {
         rtl8363_setAsicRegBit(2, 29, 7, MACPAG, 0, enabled ? 0:1);
         return SUCCESS;
     }
     else if (port == 1)
     {
         rtl8363_setAsicRegBit(2, 29, 15, MACPAG, 0, enabled ? 0:1);
         return SUCCESS;
     }
     else if (port == 2)
     {
         rtl8363_setAsicRegBit(2, 30, 7, MACPAG, 0, enabled ? 0:1);
         return SUCCESS;
     }
     else
	  return FAILED;
}

/*
@func int8 | rtl8363_getAsicLocalLbEnable | Get Local loopback function ability
@parm uint8 |port | prot number
@parm uint8 * |pEnabled | TRUE or FALSE
@rvalue SUCCESS
@rvalue FAILED
@comm
*/
int8 rtl8363_getAsicLocalLbEnable(uint8 port, uint8* pEnabled)
{
     uint16 bitValue;

     if (port == 0)
     {
         rtl8363_getAsicRegBit(2, 29, 7, MACPAG, 0, &bitValue);
	  *pEnabled = (bitValue ? FALSE : TRUE);
         return SUCCESS;
     }
     else if (port == 1)
     {
         rtl8363_getAsicRegBit(2, 29, 15, MACPAG, 0, &bitValue);
	  *pEnabled = (bitValue ? FALSE : TRUE);
         return SUCCESS;
     }
     else if (port == 2)
     {
         rtl8363_getAsicRegBit(2, 30, 7, MACPAG, 0, &bitValue);
	  *pEnabled = (bitValue ? FALSE : TRUE);
         return SUCCESS;
     }
     else
	  return FAILED;
}



/*
@func int8 | rtl8363_setAsicMaxPktLen | set Max packet length.
@parm uint8 |maxLen | max packet length
@rvalue SUCCESS
@rvalue FAILED
@comm
maxLen could be set :
RTL8363_MAX_PKT_LEN_1518 -1518 bytes without any tag; 1522 bytes: with VLAN tag or CPU tag, 1526 bytes with CPU and VLAN tag;
RTL8363_MAX_PKT_LEN_2048 - 2048 bytes (all tags counted);
RTL8363_MAX_PKT_LEN_16K - 16K bytes (all tags counted);
user defined between 64 and 2048 bytes(all tags counted).
*/
int8 rtl8363_setAsicMaxPktLen(uint16 maxLen)
{
     uint16 regval;

     rtl8363_getAsicReg(1, 30, MACPAG, 0, &regval);
     regval &= 0x3FFF;
     if (maxLen == RTL8363_MAX_PKT_LEN_1518)
     {
	    regval |= 0x00 << 14;
	    rtl8363_setAsicReg(1, 30, MACPAG, 0, regval);
     }
     else if(maxLen == RTL8363_MAX_PKT_LEN_2048)
     {
		regval |= 0x1 << 14;
		rtl8363_setAsicReg(1, 30, MACPAG, 0, regval);
     }
     else if(maxLen == RTL8363_MAX_PKT_LEN_16K)
     {
		regval |= 0x3 << 14;
		rtl8363_setAsicReg(1, 30, MACPAG, 0, regval);
             /*enable modified cut through*/
             rtl8363_setAsicRegBit(1, 17, 9, MACPAG, 0, 0);
             rtl8363_setAsicRegBit(1, 17, 8, MACPAG, 0, 1);
             /*latency threshold 1536*/
             rtl8363_setAsicRegBit(1, 17, 12, MACPAG, 0, 1);
             rtl8363_setAsicRegBit(1, 17, 11, MACPAG, 0, 0);

     }
     else if(maxLen>=64 && maxLen<= 2048)
     {
	       regval &= 0x3000;
		regval |= (0x2 << 14 |maxLen);
		rtl8363_setAsicReg(1, 30, MACPAG, 0, regval);
     }
     else
	  return FAILED;

     return SUCCESS;

}


/*
@func int8 | rtl8363_getAsicMaxPktLen | get Max packet length Asic has set.
@parm uint16 * |pMaxLen | max packet length.
@rvalue SUCCESS
@rvalue FAILED
@comm
*pMaxLen could be :
RTL8363_MAX_PKT_LEN_1518 -1518 bytes without any tag; 1522 bytes: with VLAN tag or CPU tag, 1526 bytes with CPU and VLAN tag;
RTL8363_MAX_PKT_LEN_2048 - 2048 bytes (all tags counted);
RTL8363_MAX_PKT_LEN_16K - 16K bytes (all tags counted);
user defined between 64 and 2048 bytes(all tags counted).
*/
int8 rtl8363_getAsicMaxPktLen(uint16 *pMaxLen)
{
      uint16 regval;

      rtl8363_getAsicReg(1, 30, MACPAG, 0, &regval);
      switch( (regval & 0xc000) >> 14 )
      {
          case 0:
		 *pMaxLen = RTL8363_MAX_PKT_LEN_1518;
		 break;
	   case 1:
		 *pMaxLen = RTL8363_MAX_PKT_LEN_2048;
		 break;
	   case 2:  /*user defined between 64 and 2048 bytes(all tags counted)*/
	       *pMaxLen = (regval & 0xFFF);
		 break;
	   case 3:
	       *pMaxLen = RTL8363_MAX_PKT_LEN_16K;
		 break;
	   default:
		 return FAILED;
	}
       return SUCCESS;
}


/*
@func int8 | rtl8363_setAsicMacAddress  | Set the switch MAC address.
@parm uint8 * |macAddress | Specify the switch MAC address.
@rvalue SUCCESS
@rvalue FAILED
@comm
*/
int8 rtl8363_setAsicMacAddress(uint8 *macAddress)
{
     uint16 regValue;

     regValue = macAddress[0]|(macAddress[1]<<8);
     rtl8363_setAsicReg(2, 25, MACPAG, 1, regValue);

     regValue = macAddress[3]|(macAddress[4]<<8);
     rtl8363_setAsicReg(2, 26, MACPAG, 1, regValue);

     regValue = macAddress[4]|(macAddress[5]<<8);
     rtl8363_setAsicReg(2, 27, MACPAG, 1, regValue);

     return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicMacAddress  | Get the switch MAC address.
@parm uint8 * |macAddress | Specify the switch MAC address.
@rvalue SUCCESS
@rvalue FAILED
@comm
*/
int8 rtl8363_getAsicMacAddress(uint8 *macAddress)
{
     uint16 regValue;

     rtl8363_getAsicReg(2, 25, MACPAG, 1, &regValue);
     macAddress[0] = (uint8)regValue;
     macAddress[1] = (uint8)(regValue>>8);

     rtl8363_getAsicReg(2, 26, MACPAG, 1, &regValue);
     macAddress[2] = (uint8)regValue;
     macAddress[3] = (uint8)(regValue>>8);

     rtl8363_getAsicReg(2, 27, MACPAG, 1, &regValue);
     macAddress[4] = (uint8)regValue;
     macAddress[5] = (uint8)(regValue>>8);

     return SUCCESS;
}


/*
@func int8 | rtl8363_setAsicLUTFastAgingEnable |Enable or disabled fast aging function
@parm uint8 | enabled | TRUE or FALSE
@rvalue SUCCESS
@rvalue FAILED
@comm
*/
int8 rtl8363_setAsicLUTFastAgingEnable( uint8 enabled)
{
      rtl8363_setAsicRegBit(1, 21, 11, MACPAG, 9, enabled ? 1:0);
      return SUCCESS;
}



/*
@func int8 | rtl8363_setAsic1xUnauthPktAct | Set action for IEEE802.1x unauthorized pkt
@parm uint8 | action | action type
@rvalue SUCCESS
@rvalue FAILED
@comm
There are two actions for unauthorized pkt: <nl>
	RTL8363_ACT_DROP 	     - Drop the packet <nl>
	RTL8363_ACT_TRAPCPU	     - Trap the packet to cpu <nl>
*/
int8 rtl8363_setAsicDot1xUnauthPktAct(uint8 action)
{

	if (action == RTL8363_ACT_DROP)
        rtl8363_setAsicRegBit(0, 19, 14, MACPAG, 0, 0);
	else if (action == RTL8363_ACT_TRAPCPU)
        rtl8363_setAsicRegBit(0, 19, 14, MACPAG, 0, 1);
	else
		return FAILED;
	return SUCCESS;
}

/*
@func int8 | rtl8363_getAsic1xUnauthPktAct | Get action for IEEE802.1x unauthorized pkt
@parm uint8* | pAction | action type
@rvalue SUCCESS
@rvalue FAILED
@comm
There are two actions for unauthorized pkt: <nl>
	RTL8363_ACT_DROP 		- Drop the packet <nl>
	RTL8363_ACT_TRAPCPU	      - Trap the packet to cpu <nl>
*/

int8 rtl8363_getAsicDot1xUnauthPktAct(uint8 *pAction)
{
	uint16 bitval;

	if (pAction == NULL)
		return FAILED;

       rtl8363_getAsicRegBit(0, 19, 14, MACPAG, 0, &bitval);
       *pAction = bitval ? RTL8363_ACT_TRAPCPU : RTL8363_ACT_DROP;
	return SUCCESS;
}



/*
@func int8 | rtl8363_setAsicDot1x | Set IEEE802.1x access control function
@parm uint8 | type | Specify port-based or mac-based 802.1x control
@parm uint8 | port | Specify port number (0 ~ 2)
@parm Dot1xPara_t | pDot1x | Specify port 802.1x control parameter
@struct Dot1xPara_t | This structure describe 802.1x  parameter
@field uint8 | enabled | enable/disable  port-based or mac-based access control
@field uint8 | direction | set IEEE802.1x port-based or mac-based control direction
@field uint8 | isAuth | only for port-based dot1x, set port authorization status
@rvalue SUCCESS
@rvalue FAILED
@comm
Three are two type  IEEE802.1x access control method:<nl>
     RTL8363_DOT1X_PORTBASE - port-Based Access Control <nl>
     RTL8363_DOT1X_MACBASE  - mac-Based Access control <nl>
There are two IEEE802.1x port state:<nl>
	RTL8363_DOT1X_AUTH - authorized<nl>
	RTL8363_DOT1X_UNAUTH - unauthorized<nl>
There are also two 802.1x control direction: RTL8363_DOT1X_BOTHDIR or RTL8363_DOT1X_INDIR.
if control direction is input and output, packet's authorization status depends on both
souce(source port for port-base access control,source Mac for mac-basess control)
and destination(destination port for port-base access control,destination Mac for mac-basess control)
authorization status; if control direction is only input, packet's authorization status depends only source
status. for port-base access control, direction is per port setting, for mac-base access control,
dirction is global setting.
*/
int8 rtl8363_setAsicDot1x(uint8 type, uint8 port,  Dot1xPara_t* pDot1x)
{

   if ((port > RTL8363_PORT2)  || (pDot1x == NULL))
        return FAILED;

   if (type  == RTL8363_DOT1X_PORTBASE)
   {
        rtl8363_setAsicRegBit(0, 19, 8+port, MACPAG, 0, pDot1x->enabled ? 1:0);
        rtl8363_setAsicRegBit(0, 19, 3+port, MACPAG, 0, pDot1x->dirction ? 1:0);
        rtl8363_setAsicRegBit(0, 19, port, MACPAG, 0, pDot1x->isAuth ? 1:0);

   }
   else if (type == RTL8363_DOT1X_MACBASE)
   {
        rtl8363_setAsicRegBit(0, 19, 11+port, MACPAG, 0, pDot1x->enabled ? 1:0);
        rtl8363_setAsicRegBit(0, 19, 15, MACPAG, 0, pDot1x->dirction ? 1:0);
   }

    return SUCCESS;
}


/*
@func int8 | rtl8363_getAsicDot1x | Set IEEE802.1x access control function
@parm uint8 | type | Specify port-based or mac-based 802.1x control
@parm uint8 | port | Specify port number (0 ~ 2)
@parm Dot1xPara_t | pDot1x | Specify port 802.1x control parameter
@struct Dot1xPara_t | This structure describe 802.1x  parameter
@field uint8 | enabled | enable/disable  port-based or mac-based access control
@field uint8 | direction | set IEEE802.1x port-based or mac-based control direction
@field uint8 | isAuth | only for port-based dot1x, set port authorization status
@rvalue SUCCESS
@rvalue FAILED
@comm
Three are two type  IEEE802.1x access control method:<nl>
     RTL8363_DOT1X_PORTBASE - port-Based Access Control <nl>
     RTL8363_DOT1X_MACBASE  - mac-Based Access control <nl>
There are two IEEE802.1x port state:<nl>
	RTL8363_DOT1X_AUTH - authorized<nl>
	RTL8363_DOT1X_UNAUTH - unauthorized<nl>
There are also two 802.1x control direction: RTL8363_DOT1X_BOTHDIR or RTL8363_DOT1X_INDIR.
if control direction is input and output, packet's authorization status depends on both
souce(source port for port-base access control,source Mac for mac-basess control)
and destination(destination port for port-base access control,destination Mac for mac-basess control)
authorization status; if control direction is only input, packet's authorization status depends only source
status. for port-base access control, direction is per port setting, for mac-base access control,
dirction is global setting.
*/

int8 rtl8363_getAsicDot1x(uint8 type, uint8 port, Dot1xPara_t* pDot1x)
{
   uint16 bitval;

   if ((port > RTL8363_PORT2)  || (pDot1x == NULL))
        return FAILED;
   if (type == RTL8363_DOT1X_PORTBASE )
   {
       rtl8363_getAsicRegBit(0, 19, 8+port, MACPAG, 0, &bitval);
       pDot1x->enabled = bitval ? 1:0;
       rtl8363_getAsicRegBit(0, 19, 3+port, MACPAG, 0, &bitval);
       pDot1x->dirction =  bitval ? (RTL8363_DOT1X_INDIR) : (RTL8363_DOT1X_BOTHDIR);
       rtl8363_getAsicRegBit(0, 19, port, MACPAG, 0, &bitval);
       pDot1x->isAuth = bitval ? (RTL8363_DOT1X_AUTH):(RTL8363_DOT1X_UNAUTH);

   }

   if (type == RTL8363_DOT1X_MACBASE)
   {
       rtl8363_getAsicRegBit(0, 19, 11+port, MACPAG, 0, &bitval);
       pDot1x->enabled = bitval ? 1:0;
       rtl8363_getAsicRegBit(0, 19, 15, MACPAG, 0, &bitval);
       pDot1x->dirction =  bitval ? (RTL8363_DOT1X_INDIR) : (RTL8363_DOT1X_BOTHDIR);
   }

    return SUCCESS;
}
