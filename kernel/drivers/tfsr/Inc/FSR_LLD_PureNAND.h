/**
 *   @mainpage   Flex Sector Remapper : LinuStoreIII_1.2.0_b029_FSR_1.2.1_b125_RTM
 *
 *   @section Intro
 *       Flash Translation Layer for Flex-OneNAND and OneNAND
 *
 *    @section  Copyright
 *---------------------------------------------------------------------------*
 *                                                                           *
 * Copyright (C) 2003-2009 Samsung Electronics                               *
 * This program is free software; you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License version 2 as         *
 * published by the Free Software Foundation.                                *
 *                                                                           *
 *---------------------------------------------------------------------------*
 *
 *     @section Description
 *
 */

/**
 * @file      FSR_LLD_PureNAND.h
 * @brief     declarations of exported functions
 * @author    NamOh Hwang, JinHyuck Kim
 * @date      25-SEP-2009
 * @remark
 * REVISION HISTORY
 * @n  20-OCT-2008 [NamOh Hwang]  : first writing
 * @n  15-SEP-2009 [JinHyuck Kim] : Update for FSR 1.2.1
 *
 */


#ifndef _FSR_PURENAND_LLD_H_
#define _FSR_PURENAND_LLD_H_

/******************************************************************************/
/* PNL Command Set                                                            */
/******************************************************************************/
#define     FSR_PND_MAX_LOG                             (32)

/**
 * @brief   data structure for logging LLD operations.
 */
typedef struct
{
    volatile UINT32   nLogHead;
    volatile UINT16   nLogOp      [FSR_PND_MAX_LOG];
    volatile UINT16   nLogPbn     [FSR_PND_MAX_LOG];
    volatile UINT16   nLogPgOffset[FSR_PND_MAX_LOG];
    volatile UINT32   nLogFlag    [FSR_PND_MAX_LOG];
} PureNANDOpLog;

extern volatile PureNANDOpLog        gstPNDOpLog[FSR_MAX_DEVS];

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/******************************************************************************/
/* exported common APIs                                                       */
/******************************************************************************/
INT32   FSR_PND_Init            (UINT32         nFlag);
INT32   FSR_PND_Open            (UINT32         nDev,
                                 VOID          *pParam,
                                 UINT32         nFlag);
INT32   FSR_PND_Close           (UINT32         nDev,
                                 UINT32         nFlag);
INT32   FSR_PND_Read            (UINT32         nDev,
                                 UINT32         nPbn,
                                 UINT32         nPgOffset,
                                 UINT8         *pMBuf,
                                 FSRSpareBuf   *pSBuf,
                                 UINT32         nFlag);
INT32   FSR_PND_ReadOptimal     (UINT32         nDev,
                                 UINT32         nPbn,
                                 UINT32         nPgOffset,
                                 UINT8         *pMBuf,
                                 FSRSpareBuf   *pSBuf,
                                 UINT32         nFlag);
INT32   FSR_PND_Write           (UINT32         nDev,
                                 UINT32         nPbn,
                                 UINT32         nPgOffset,
                                 UINT8         *pMBuf,
                                 FSRSpareBuf   *pSBuf,
                                 UINT32         nFlag);
INT32   FSR_PND_Erase           (UINT32         nDev,
                                 UINT32        *pnPbn,
                                 UINT32         nNumOfBlks,
                                 UINT32         nFlag);
INT32   FSR_PND_CopyBack        (UINT32         nDev,
                                 LLDCpBkArg    *pstCpArg,
                                 UINT32         nFlag);
INT32   FSR_PND_ChkBadBlk       (UINT32         nDev,
                                 UINT32         nPbn,
                                 UINT32         nFlag);
INT32   FSR_PND_FlushOp         (UINT32         nDev,
                                 UINT32         nDieIdx,
                                 UINT32         nFlag);
INT32   FSR_PND_GetBlockInfo    (UINT32         nDev,
                                 UINT32         nPbn,
                                 UINT32        *pBlockType,
                                 UINT32        *pPgsPerBlk);
INT32   FSR_PND_GetDevSpec      (UINT32         nDev,
                                 FSRDevSpec    *pstDevSpec,
                                 UINT32         nFlag);
INT32   FSR_PND_GetPlatformInfo (UINT32         nDev,
                               LLDPlatformInfo *pLLDPltInfo);
INT32   FSR_PND_GetPrevOpData   (UINT32         nDev,
                                 UINT8         *pMBuf,
                                 FSRSpareBuf   *pSBuf,
                                 UINT32         nDieIdx,
                                 UINT32         nFlag);
INT32   FSR_PND_IOCtl           (UINT32         nDev,
                                 UINT32         nCode,
                                 UINT8         *pBufI,
                                 UINT32         nLenI,
                                 UINT8         *pBufO,
                                 UINT32         nLenO,
                                 UINT32        *pByteRet);
INT32   FSR_PND_InitLLDStat     (VOID);
INT32   FSR_PND_GetStat         (FSRLLDStat    *pstStat);
INT32   FSR_PND_GetNANDCtrllerInfo(UINT32             nDev,
                                   LLDPlatformInfo   *pLLDPltInfo);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _FSR_PURENAND_LLD_H_ */
