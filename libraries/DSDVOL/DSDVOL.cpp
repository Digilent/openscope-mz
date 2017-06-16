/*------------------------------------------------------------------------/
/  MMCv3/SDv1/SDv2 (in SPI mode) control module
/-------------------------------------------------------------------------/
/
/  Copyright (C) 2014, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/ http://elm-chan.org/docs/mmc/mmc_e.html
/-------------------------------------------------------------------------/
/
/ 10/21/2015 Modified by Keith Vogel; Digilent Inc., for use with chipKIT
/ 
/-------------------------------------------------------------------------*/
#include "DSDVOL.h"

/*-----------------------------------------------------------------------*/
/* Power Control  (Platform dependent)                                   */
/*-----------------------------------------------------------------------*/
/* When the target system does not support socket power control, there   */
/* is nothing to do in these functions.                                  */

void DSDVOL::power_on (void)
{
    dSDspi.begin();
 
    dSDspi.setMode(DSPI_MODE0);
    FCLK_FAST();
}

void DSDVOL::power_off (void)
{
    dSDspi.end();
}

/*-----------------------------------------------------------------------*/
/* Transmit/Receive data to/from MMC via SPI  (Platform dependent)       */
/*-----------------------------------------------------------------------*/

/* Single byte SPI transfer */

/* Block SPI transfers */

/*-----------------------------------------------------------------------*/
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/

int DSDVOL::wait_ready (void)
{
	BYTE d;

	tmsSetTimer(2) = 500;	/* Wait for ready in timeout of 500ms */
	do {
		d = xchg_spi(0xFF);
	} while ((d != 0xFF) && tmsTimer(2));

	return (d == 0xFF) ? 1 : 0;
}

/*-----------------------------------------------------------------------*/
/* Deselect the card and release SPI bus                                 */
/*-----------------------------------------------------------------------*/

void DSDVOL::deselect (void)
{
    dSDspi.setSelect(1);
	xchg_spi(0xFF);		/* Dummy clock (force DO hi-z for multiple slave SPI) */
}

/*-----------------------------------------------------------------------*/
/* Select the card and wait ready                                        */
/*-----------------------------------------------------------------------*/

int DSDVOL::select (void)	/* 1:Successful, 0:Timeout */
{
    dSDspi.setSelect(0);
	xchg_spi(0xFF);			// Dummy clock (force DO enabled)

	if (wait_ready()) {
		return 1;			// success
	}

	deselect();

	return 0;				// Timeout
}

/*-----------------------------------------------------------------------*/
/* Receive a data packet from MMC                                        */
/*-----------------------------------------------------------------------*/

int DSDVOL::rcvr_datablock (	/* 1:OK, 0:Failed */
	BYTE *buff,			/* Data buffer to store received data */
	UINT btr			/* Byte count (must be multiple of 4) */
) {
	BYTE token;


	tmsSetTimer(1) = 100;

	do {							// Wait for data packet with timeout of 100ms
		token = xchg_spi(0xFF);
	} while ((token == 0xFF) && tmsTimer(1));

	if (token != 0xFE) {
		return 0;					// If not valid data token, return with error
	}

	rcvr_spi_multi(buff, btr);		// Read the data block into the buffer

	xchg_spi(0xFF);					// Discard CRC
	xchg_spi(0xFF);

	return 1;						// Return with success
}

/*-----------------------------------------------------------------------*/
/* Send a data packet to MMC                                             */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
int DSDVOL::xmit_datablock (	/* 1:OK, 0:Failed */
	const BYTE *buff,	/* 512 byte data block to be transmitted */
	BYTE token			/* Data token */
) {
	BYTE resp;


	if (!wait_ready()) {
		return 0;
	}

	xchg_spi(token);					// Xmit the token

	if (token != 0xFD) {				// Not StopTran token
		xmit_spi_multi(buff, 512);		// Xmit the data block to the MMC
		xchg_spi(0xFF);					// CRC (Dummy)
		xchg_spi(0xFF);
		resp = xchg_spi(0xFF);			// Receive a data response
		if ((resp & 0x1F) != 0x05)		// If not accepted, return with error
			return 0;
	}

	return 1;
}
#endif

/*-----------------------------------------------------------------------*/
/* Send a command packet to MMC                                          */
/*-----------------------------------------------------------------------*/

BYTE DSDVOL::send_cmd (
	BYTE cmd,		/* Command byte */
	DWORD arg		/* Argument */
)
{
	BYTE n, res;


	if ((cmd & 0x80) != 0) {	/* ACMD<n> is the command sequence of CMD55-CMD<n> */
		cmd &= 0x7F;
		res = send_cmd(CMD55, 0);
		if (res > 1) {
			return res;
		}
	}

	/* Select the card and wait for ready except to stop multiple block read */
	if (cmd != CMD12) {
		deselect();
		if (!select()) {
			return 0xFF;
		}
	}

	/* Send command packet */
	xchg_spi(0x40 | cmd);			/* Start + Command index */
	xchg_spi((BYTE)(arg >> 24));	/* Argument[31..24] */
	xchg_spi((BYTE)(arg >> 16));	/* Argument[23..16] */
	xchg_spi((BYTE)(arg >> 8));		/* Argument[15..8] */
	xchg_spi((BYTE)arg);			/* Argument[7..0] */

	n = 0x01;						/* Dummy CRC + Stop */
	if (cmd == CMD0) {
		n = 0x95;		/* Valid CRC for CMD0(0) + Stop */
	}
	if (cmd == CMD8) {
		n = 0x87;		/* Valid CRC for CMD8(0x1AA) + Stop */
	}
	xchg_spi(n);

	/* Receive command response */
	if (cmd == CMD12) {
		xchg_spi(0xFF);	/* Skip a stuff byte on stop to read */
	}

	n = 10;							/* Wait for a valid response in timeout of 10 attempts */
	do {
		res = xchg_spi(0xFF);
	} while ((res & 0x80) && --n);

	return res;			/* Return with the response value */
}

/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/

DSTATUS DSDVOL::disk_status(void)
{
    return Stat;
}

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS DSDVOL::disk_initialize(void)
{
	BYTE	n;
	BYTE	cmd;
	BYTE	ty;
	BYTE	ocr[4];


	if (Stat & STA_NODISK) {
		return Stat;	/* No card in the socket */
	}

	power_on();							/* Initialize memory card interface */
	FCLK_SLOW();
	for (n = 10; n; n--) xchg_spi(0xFF);	/* 80 dummy clocks */

	ty = 0;
	if (send_cmd(CMD0, 0) == 1) {			/* Enter Idle state */
		tmsSetTimer(1) = 1000;						/* Initialization timeout of 1000 msec */
		if (send_cmd(CMD8, 0x1AA) == 1) {	/* SDv2? */
			for (n = 0; n < 4; n++) {
				ocr[n] = xchg_spi(0xFF);			/* Get trailing return value of R7 resp */
			}
			if ((ocr[2] == 0x01) && (ocr[3] == 0xAA)) {				/* The card can work at vdd range of 2.7-3.6V */
				while (tmsTimer(1) && send_cmd(ACMD41, 0x40000000));	/* Wait for leaving idle state (ACMD41 with HCS bit) */
				if (tmsTimer(1) && send_cmd(CMD58, 0) == 0) {			/* Check CCS bit in the OCR */
					for (n = 0; n < 4; n++) {
						ocr[n] = xchg_spi(0xFF);
					}
					ty = (ocr[0] & 0x40) ? CT_SD2|CT_BLOCK : CT_SD2;	/* SDv2 */
				}
			}
		} else {							/* SDv1 or MMCv3 */
			if (send_cmd(ACMD41, 0) <= 1) 	{
				ty = CT_SD1;
				cmd = ACMD41;	/* SDv1 */
			} else {
				ty = CT_MMC;
				cmd = CMD1;	/* MMCv3 */
			}
			while (tmsTimer(1) && send_cmd(cmd, 0));		/* Wait for leaving idle state */
			if (!tmsTimer(1) || send_cmd(CMD16, 512) != 0)	/* Set read/write block length to 512 */
				ty = 0;
		}
	}
	CardType = ty;
	deselect();

	if (ty) {		/* Function succeded */
		Stat &= ~STA_NOINIT;	/* Clear STA_NOINIT */
		FCLK_FAST();
	} else {		/* Function failed */
		power_off();	/* Deinitialize interface */
	}

	return Stat;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT DSDVOL::disk_read(
	uint8_t *buff,		/* Pointer to the data buffer to store read data */
	uint32_t sector,	/* Start sector number (LBA) */
	uint32_t count		/* Sector count (1..128) */
)
{
	if (!count) {
		return RES_PARERR;
	}

	if (Stat & STA_NOINIT) {
		return RES_NOTRDY;
	}

	if (!(CardType & CT_BLOCK)) {
		sector *= 512;						/* Convert to byte address if needed */
	}

	if (count == 1) {						/* Single block read */
		if ((send_cmd(CMD17, sector) == 0)	/* READ_SINGLE_BLOCK */
			&& rcvr_datablock(buff, 512)) {
			count = 0;
		}
	}
	else {				/* Multiple block read */
		if (send_cmd(CMD18, sector) == 0) {	/* READ_MULTIPLE_BLOCK */
			do {
				if (!rcvr_datablock(buff, 512)) {
					break;
				}
				buff += 512;
			} while (--count);
			send_cmd(CMD12, 0);				/* STOP_TRANSMISSION */
		}
	}
	deselect();

	return count ? RES_ERROR : RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT DSDVOL::disk_write (
	const uint8_t *buff,		/* Pointer to the data to be written */
	uint32_t sector,			/* Start sector number (LBA) */
	uint32_t count				/* Sector count (1..128) */
)
{
	if (!count) {
		return RES_PARERR;
	}

	if (Stat & STA_NOINIT) {
		return RES_NOTRDY;
	}

	if (Stat & STA_PROTECT) {
		return RES_WRPRT;
	}

	if (!(CardType & CT_BLOCK)) {
		sector *= 512;						/* Convert to byte address if needed */
	}

	if (count == 1) {						/* Single block write */
		if ((send_cmd(CMD24, sector) == 0)	/* WRITE_BLOCK */
			&& xmit_datablock(buff, 0xFE))
			count = 0;
	}
	else {									/* Multiple block write */
		if (CardType & CT_SDC) {
			send_cmd(ACMD23, count);
		}
		if (send_cmd(CMD25, sector) == 0) {	/* WRITE_MULTIPLE_BLOCK */
			do {
				if (!xmit_datablock(buff, 0xFC)) {
					break;
				}
				buff += 512;
			} while (--count);
			if (!xmit_datablock(0, 0xFD)) {	/* STOP_TRAN token */
				count = 1;
			}
		}
	}
	deselect();

	return count ? RES_ERROR : RES_OK;
}
#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT DSDVOL::disk_ioctl (
	uint8_t cmd,		/* Control code */
	void *buff		/* Buffer to send/receive data block */
)
{
	DRESULT res;
	BYTE	n;
	BYTE	csd[16];
	BYTE *	ptr = (BYTE *)buff;
	DWORD	csz;

	if (Stat & STA_NOINIT) {
		return RES_NOTRDY;
	}

	res = RES_ERROR;
	switch (cmd) {
	case CTRL_SYNC :	/* Flush write-back cache, Wait for end of internal process */
		if (select()) res = RES_OK;
		break;

	case GET_SECTOR_COUNT :	/* Get number of sectors on the disk (WORD) */
		if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
			if ((csd[0] >> 6) == 1) {	/* SDv2? */
				csz = csd[9] + ((WORD)csd[8] << 8) + ((DWORD)(csd[7] & 63) << 16) + 1;
				*(DWORD*)buff = csz << 10;
			} else {					/* SDv1 or MMCv3 */
				n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
				csz = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
				*(DWORD*)buff = csz << (n - 9);
			}
			res = RES_OK;
		}
		break;

	case GET_BLOCK_SIZE :	/* Get erase block size in unit of sectors (DWORD) */
		if (CardType & CT_SD2) {	/* SDv2? */
			if (send_cmd(ACMD13, 0) == 0) {		/* Read SD status */
				xchg_spi(0xFF);
				if (rcvr_datablock(csd, 16)) {				/* Read partial block */
					for (n = 64 - 16; n; n--) {
						xchg_spi(0xFF);	/* Purge trailing data */
					}
					*(DWORD*)buff = 16UL << (csd[10] >> 4);
					res = RES_OK;
				}
			}
		} else {					/* SDv1 or MMCv3 */
			if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {	/* Read CSD */
				if (CardType & CT_SD1) {	/* SDv1 */
					*(DWORD*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
				} else {					/* MMCv3 */
					*(DWORD*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
				}
				res = RES_OK;
			}
		}
		break;

	case MMC_GET_TYPE :		/* Get card type flags (1 byte) */
		*ptr = CardType;
		res = RES_OK;
		break;

	case MMC_GET_CSD :	/* Receive CSD as a data block (16 bytes) */
		if ((send_cmd(CMD9, 0) == 0)	/* READ_CSD */
			&& rcvr_datablock((BYTE *)buff, 16))
			res = RES_OK;
		break;

	case MMC_GET_CID :	/* Receive CID as a data block (16 bytes) */
		if ((send_cmd(CMD10, 0) == 0)	/* READ_CID */
			&& rcvr_datablock((BYTE *)buff, 16))
			res = RES_OK;
		break;

	case MMC_GET_OCR :	/* Receive OCR as an R3 resp (4 bytes) */
		if (send_cmd(CMD58, 0) == 0) {	/* READ_OCR */
			for (n = 0; n < 4; n++)
				*((BYTE*)buff+n) = xchg_spi(0xFF);
			res = RES_OK;
		}
		break;

	case MMC_GET_SDSTAT :	/* Receive SD statsu as a data block (64 bytes) */
		if ((CardType & CT_SD2) && send_cmd(ACMD13, 0) == 0) {	/* SD_STATUS */
			xchg_spi(0xFF);
			if (rcvr_datablock((BYTE *)buff, 64))
				res = RES_OK;
		}
		break;

	case CTRL_POWER_OFF :	/* Power off */
		power_off();
		Stat |= STA_NOINIT;
		res = RES_OK;
		break;

	default:
		res = RES_PARERR;
	}

	deselect();

	return res;
}
#endif
