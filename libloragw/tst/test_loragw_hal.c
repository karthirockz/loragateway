/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
	Minimum test program for the loragw_hal 'library'

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Sylvain Miermont
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* fix an issue between POSIX and C99 */
#if __STDC_VERSION__ >= 199901L
	#define _XOPEN_SOURCE 600
#else
	#define _XOPEN_SOURCE 500
#endif

#include <stdint.h>		/* C99 types */
#include <stdbool.h>	/* bool type */
#include <stdio.h>		/* printf */
#include <string.h>		/* memset */
#include <signal.h>		/* sigaction */

#include "loragw_hal.h"
#include "loragw_reg.h"
#include "loragw_aux.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#if ((CFG_BAND_868 == 1) || ((CFG_BAND_FULL == 1) && (CFG_RADIO_1257 == 1)))
	#define	F_RX_0	868500000
	#define	F_RX_1	869500000
	#define	F_TX	869000000
#elif (CFG_BAND_780 == 1)
	#define	F_RX_0	780500000
	#define	F_RX_1	781500000
	#define	F_TX	780000000
#elif (CFG_BAND_915 == 1)
	#define	F_RX_0	914500000
	#define	F_RX_1	915500000
	#define	F_TX	915000000
#elif ((CFG_BAND_470 == 1) || ((CFG_BAND_FULL == 1) && (CFG_RADIO_1255 == 1)))
	#define	F_RX_0	471500000
	#define	F_RX_1	472500000
	#define	F_TX	472000000
#elif (CFG_BAND_433 == 1)
	#define	F_RX_0	433500000
	#define	F_RX_1	434500000
	#define	F_TX	434000000
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

static int exit_sig = 0; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
static int quit_sig = 0; /* 1 -> application terminates without shutting down the hardware */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

static void sig_handler(int sigio);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

static void sig_handler(int sigio) {
	if (sigio == SIGQUIT) {
		quit_sig = 1;;
	} else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
		exit_sig = 1;
	}
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main()
{
	struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
	
	struct lgw_conf_rxrf_s rfconf;
	struct lgw_conf_rxif_s ifconf;
	
	struct lgw_pkt_rx_s rxpkt[4]; /* array containing up to 4 inbound packets metadata */
	struct lgw_pkt_tx_s txpkt; /* configuration and metadata for an outbound packet */
	struct lgw_pkt_rx_s *p; /* pointer on a RX packet */
	
	int i, j;
	int nb_pkt;
	
	uint32_t tx_cnt = 0;
	unsigned long loop_cnt = 0;
	uint8_t status_var = 0;
	
	uint32_t f_tx, f_rx0, f_rx1;

	/* configure signal handling */
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigact.sa_handler = sig_handler;
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	
	/* beginning of LoRa concentrator-specific code */
	printf("Beginning of test for loragw_hal.c\n");
	
	printf("*** Library version information ***\n%s\n\n", lgw_version_info());
	
	/* set configuration for RF chains */
	memset(&rfconf, 0, sizeof(rfconf));
	
#if (CFG_RADIO_AUTO == 1)
	lgw_auto_check();
	if( lgw_get_radio_id(0) == ID_SX1255 ){
		f_tx = 434000000;
		f_rx0 = 433500000;
	}else if( lgw_get_radio_id(0) == ID_SX1257 ){
		f_tx = 869000000;
		f_rx0 = 868500000;
	}else{
		printf("RF Chain A unknown\n");
		return -1;
	}
	if( lgw_get_radio_id(1) == ID_SX1255 ){
		f_rx1 = 869500000;
	}else if( lgw_get_radio_id(1) == ID_SX1257 ){
		f_rx1 = 869500000;
	}else{
		printf("RF Chain B unknown\n");
		return -1;
	}
#else
	f_tx = F_TX;
	f_rx0 = F_RX_0;
	f_rx1 = F_RX_1;
#endif
	
	rfconf.enable = true;
	rfconf.freq_hz = f_rx0;
	lgw_rxrf_setconf(0, rfconf); /* radio A, f0 */
	
	rfconf.enable = true;
	rfconf.freq_hz = f_rx1;
	lgw_rxrf_setconf(1, rfconf); /* radio B, f1 */
	
	/* set configuration for LoRa multi-SF channels (bandwidth cannot be set) */
	memset(&ifconf, 0, sizeof(ifconf));
	
	ifconf.enable = true;
	ifconf.rf_chain = 0;
	ifconf.freq_hz = -300000;
	ifconf.datarate = DR_LORA_MULTI;
	lgw_rxif_setconf(0, ifconf); /* chain 0: LoRa 125kHz, all SF, on f0 - 0.3 MHz */
	
	ifconf.enable = true;
	ifconf.rf_chain = 0;
	ifconf.freq_hz = 300000;
	ifconf.datarate = DR_LORA_MULTI;
	lgw_rxif_setconf(1, ifconf); /* chain 1: LoRa 125kHz, all SF, on f0 + 0.3 MHz */
	
	ifconf.enable = true;
	ifconf.rf_chain = 1;
	ifconf.freq_hz = -300000;
	ifconf.datarate = DR_LORA_MULTI;
	lgw_rxif_setconf(2, ifconf); /* chain 2: LoRa 125kHz, all SF, on f1 - 0.3 MHz */
	
	ifconf.enable = true;
	ifconf.rf_chain = 1;
	ifconf.freq_hz = 300000;
	ifconf.datarate = DR_LORA_MULTI;
	lgw_rxif_setconf(3, ifconf); /* chain 3: LoRa 125kHz, all SF, on f1 + 0.3 MHz */
	
	#if (LGW_MULTI_NB >= 8)
	ifconf.enable = true;
	ifconf.rf_chain = 0;
	ifconf.freq_hz = -100000;
	ifconf.datarate = DR_LORA_MULTI;
	lgw_rxif_setconf(4, ifconf); /* chain 4: LoRa 125kHz, all SF, on f0 - 0.1 MHz */
	
	ifconf.enable = true;
	ifconf.rf_chain = 0;
	ifconf.freq_hz = 100000;
	ifconf.datarate = DR_LORA_MULTI;
	lgw_rxif_setconf(5, ifconf); /* chain 5: LoRa 125kHz, all SF, on f0 + 0.1 MHz */
	
	ifconf.enable = true;
	ifconf.rf_chain = 1;
	ifconf.freq_hz = -100000;
	ifconf.datarate = DR_LORA_MULTI;
	lgw_rxif_setconf(6, ifconf); /* chain 6: LoRa 125kHz, all SF, on f1 - 0.1 MHz */
	
	ifconf.enable = true;
	ifconf.rf_chain = 1;
	ifconf.freq_hz = 100000;
	ifconf.datarate = DR_LORA_MULTI;
	lgw_rxif_setconf(7, ifconf); /* chain 7: LoRa 125kHz, all SF, on f1 + 0.1 MHz */
	#endif
	
	/* set configuration for LoRa 'stand alone' channel */
	memset(&ifconf, 0, sizeof(ifconf));
	ifconf.enable = true;
	ifconf.rf_chain = 0;
	ifconf.freq_hz = 0;
	ifconf.bandwidth = BW_250KHZ;
	ifconf.datarate = DR_LORA_SF10;
	lgw_rxif_setconf(8, ifconf); /* chain 8: LoRa 250kHz, SF10, on f0 MHz */
	
	/* set configuration for FSK channel */
	memset(&ifconf, 0, sizeof(ifconf));
	ifconf.enable = true;
	ifconf.rf_chain = 1;
	ifconf.freq_hz = 0;
	ifconf.bandwidth = BW_250KHZ;
	ifconf.datarate = 64000;
	lgw_rxif_setconf(9, ifconf); /* chain 9: FSK 64kbps, on f1 MHz */
	
	/* set configuration for TX packet */
	
	memset(&txpkt, 0, sizeof(txpkt));
	txpkt.freq_hz = f_tx;
	txpkt.tx_mode = IMMEDIATE;
	txpkt.rf_power = 10;
	txpkt.modulation = MOD_LORA;
	txpkt.bandwidth = BW_250KHZ;
	txpkt.datarate = DR_LORA_SF10;
	txpkt.coderate = CR_LORA_4_5;
	strcpy((char *)txpkt.payload, "TX.TEST.LORA.GW.????" );
	txpkt.size = 20;
	txpkt.preamble = 6;
	txpkt.rf_chain = 0;
/*	
	memset(&txpkt, 0, sizeof(txpkt));
	txpkt.freq_hz = F_TX;
	txpkt.tx_mode = IMMEDIATE;
	txpkt.rf_power = 10;
	txpkt.modulation = MOD_FSK;
	txpkt.f_dev = 50;
	txpkt.datarate = 64000;
	strcpy((char *)txpkt.payload, "TX.TEST.LORA.GW.????" );
	txpkt.size = 20;
	txpkt.preamble = 4;
	txpkt.rf_chain = 0;
*/	
	
	/* connect, configure and start the LoRa concentrator */
	i = lgw_start();
	if (i == LGW_HAL_SUCCESS) {
		printf("*** Concentrator started ***\n");
	} else {
		printf("*** Impossible to start concentrator ***\n");
		return -1;
	}
	
	/* once configured, dump content of registers to a file, for reference */
	// FILE * reg_dump = NULL;
	// reg_dump = fopen("reg_dump.log", "w");
	// if (reg_dump != NULL) {
		// lgw_reg_check(reg_dump);
		// fclose(reg_dump);
	// }
	
	while ((quit_sig != 1) && (exit_sig != 1)) {
		loop_cnt++;
		
		/* fetch N packets */
		nb_pkt = lgw_receive(ARRAY_SIZE(rxpkt), rxpkt);
		
		if (nb_pkt == 0) {
			wait_ms(300);
		} else {
			/* display received packets */
			for(i=0; i < nb_pkt; ++i) {
				p = &rxpkt[i];
				printf("---\nRcv pkt #%d >>", i+1);
				if (p->status == STAT_CRC_OK) {
					printf(" if_chain:%2d", p->if_chain);
					printf(" tstamp:%010u", p->count_us);
					printf(" size:%3u", p->size);
					switch (p-> modulation) {
						case MOD_LORA: printf(" LoRa"); break;
						case MOD_FSK: printf(" FSK"); break;
						default: printf(" modulation?");
					}
					switch (p->datarate) {
						case DR_LORA_SF7: printf(" SF7"); break;
						case DR_LORA_SF8: printf(" SF8"); break;
						case DR_LORA_SF9: printf(" SF9"); break;
						case DR_LORA_SF10: printf(" SF10"); break;
						case DR_LORA_SF11: printf(" SF11"); break;
						case DR_LORA_SF12: printf(" SF12"); break;
						default: printf(" datarate?");
					}
					switch (p->coderate) {
						case CR_LORA_4_5: printf(" CR1(4/5)"); break;
						case CR_LORA_4_6: printf(" CR2(2/3)"); break;
						case CR_LORA_4_7: printf(" CR3(4/7)"); break;
						case CR_LORA_4_8: printf(" CR4(1/2)"); break;
						default: printf(" coderate?");
					}
					printf("\n");
					printf(" RSSI:%+6.1f SNR:%+5.1f (min:%+5.1f, max:%+5.1f) payload:\n", p->rssi, p->snr, p->snr_min, p->snr_max);
					
					for (j = 0; j < p->size; ++j) {
						printf(" %02X", p->payload[j]);
					}
					printf(" #\n");
				} else if (p->status == STAT_CRC_BAD) {
					printf(" if_chain:%2d", p->if_chain);
					printf(" tstamp:%010u", p->count_us);
					printf(" size:%3u\n", p->size);
					printf(" CRC error, damaged packet\n\n");
				} else if (p->status == STAT_NO_CRC){
					printf(" if_chain:%2d", p->if_chain);
					printf(" tstamp:%010u", p->count_us);
					printf(" size:%3u\n", p->size);
					printf(" no CRC\n\n");
				} else {
					printf(" if_chain:%2d", p->if_chain);
					printf(" tstamp:%010u", p->count_us);
					printf(" size:%3u\n", p->size);
					printf(" invalid status ?!?\n\n");
				}
			}
		}
		
		/* send a packet every X loop */
		if (loop_cnt%16 == 0) {
			/* 32b counter in the payload, big endian */
			txpkt.payload[16] = 0xff & (tx_cnt >> 24);
			txpkt.payload[17] = 0xff & (tx_cnt >> 16);
			txpkt.payload[18] = 0xff & (tx_cnt >> 8);
			txpkt.payload[19] = 0xff & tx_cnt;
			i = lgw_send(txpkt); /* non-blocking scheduling of TX packet */
			j = 0;
			printf("+++\nSending packet #%d, rf path %d, return %d\nstatus -> ", tx_cnt, txpkt.rf_chain, i);
			do {
				++j;
				wait_ms(100);
				lgw_status(TX_STATUS, &status_var); /* get TX status */
				printf("%d:", status_var);
			} while ((status_var != TX_FREE) && (j < 100));
			++tx_cnt;
			printf("\nTX finished\n");
		}
	}
	
	if (exit_sig == 1) {
		/* clean up before leaving */
		lgw_stop();
	}
	
	printf("\nEnd of test for loragw_hal.c\n");
	return 0;
}

/* --- EOF ------------------------------------------------------------------ */
