/****
* PROFILER_VERSION 444
* SUMMARY
*  - Read the SpiNNaker chip states: temperature sensor, clock frequency, 
*    ,number of running cores, and cpu loads.
*  - The master (root) profiler stays in chip <0,0>. All other profilers 
*    report to this master profiler and only the master profiler send
*    the combined profile report to the host.
*
* AUTHOR
*  Indar Sugiarto - indar.sugiarto@manchester.ac.uk
*
* DETAILS
*  Created on       : 3 Sept 2015
*  Version          : $Revision: 1 $
*  Last modified by : $Author: indi $
*
* COPYRIGHT
*  Copyright (c) The University of Manchester, 2011. All rights reserved.
*  SpiNNaker Project
*  Advanced Processor Technologies Group
*  School of Computer Science
*
*******/

/* Version 444 is designed to overcome the problem of modifying clock of the master profiler
 * in version 333:
 *    we used synchronized clock mechanism: root profiler will broadcast MCPL_GLOBAL_TICK
 *    every GLOBAL_TICK_PERIOD_US microseconds. NOTE: the root profiler SHOULD ALWAYS HAVE 200MHz,
 *    otherwise this mechanism won't work!
 *
 * In version 444, we use slow clock timer for cpu load counting.
 * When interrupt happens, each profiler will:
 * - read temperature
 * - read frequency
 * - read how many cores are active
 * - cpu load
 *
 * */


/* SOME IDEAS:
 * -------------
 */

#ifndef PROFILER_H
#define PROFILER_H

// We have several profiler version. For this program, use this version:
#define PROFILER_VERSION			444	// put this version in the vcpu->user0

#include <spin1_api.h>
#include <stdfix.h>

#define DEBUG_LEVEL					1

/*--------- Some functionalities needs fix point representation ----------*/
#define REAL						accum
#define REAL_CONST(x)				(x##k)
/*------------------------------------------------------------------------*/


#define DEF_DELAY_VAL				1000	// used mainly for io_printf
#define DEF_MY_APP_ID				255
#define DEF_PROFILER_CORE			1

// priority setup
// we allocate FIQ for slow timer
#define SCP_PRIORITY_VAL			0
#define MCPL_PRIORITY_VAL			1     
#define APP_PRIORITY_VAL			2
#define LOW_PRIORITY_VAL			2
#define TIMER_PRIORITY_VAL          3
#define NON_CRITICAL_PRIORITY_VAL	4
#define LOWEST_PRIORITY_VAL			4		// I found from In spin1_api_params.h I found this: #define NUM_PRIORITIES    5
#define IDLE_PRIORITY_VAL			LOWEST_PRIORITY_VAL
#define SCHEDULED_PRIORITY_VAL      1


/*--------------------- routing mechanism ------------------------*/
/*
#define MCPL_BCAST_REQ_UPDATE 				0xbca50001	// broadcast from root to all other profilers
#define MCPL_PROFILERS_REPORT_PART1			0x21ead100	// go to leader in chip-0
#define MCPL_PROFILERS_REPORT_PART2			0x21ead200	// go to leader in chip-0
#define MCPL_GLOBAL_TICK					MCPL_BCAST_REQ_UPDATE	// just an alias
*/

/*------------------- SDP-related parameters ---------------------*/
#define DEF_REPORT_TAG              5
#define DEF_REPORT_PORT				40005
#define DEF_ERR_INFO_TAG			6
#define DEF_ERR_INFO_PORT			40006
#define DEF_INTERNAL_SDP_PORT		1
#define DEF_HOST_SDP_PORT           7		// port-7 has a special purpose, usually related with ETH
#define DEF_TIMEOUT					10		// as recommended

// scp sent by host, related to frequecy/PLL
#define HOST_REQ_PLL_INFO			1
#define HOST_REQ_INIT_PLL			2		// host request special PLL configuration
#define HOST_REQ_REVERT_PLL			3
#define HOST_SET_FREQ_VALUE			4		// Note: HOST_SET_FREQ_VALUE assumes that CPUs use PLL1,
											// if this is not the case, then use HOST_SET_CHANGE_PLL
#define HOST_REQ_PROFILER_STREAM	5		// host send this to a particular profiler to start streaming
#define HOST_SET_GOV_MODE           6
#define HOST_REQ_GOV_STATUS         7
#define HOST_TELL_STOP				255


/*--------------------- Reporting Data Structure -------------------------*/
/* The idea is: since this version uses slow recording, then each profiler
 * can report directly to host-PC via SDP.
 *
 * myProfile will be put in the SCP part of SDP, whereas the
 * virt_cpu_idle_cntr will be copied into the data segment of the SDP.
/*------------------------------------------------------------------------*/
typedef struct pro_info {
    ushort v;           // profiler version, in cmd_rc
    ushort pID;         // which chip sends this?, in seq
    uchar cpu_freq;     // in arg1
    uchar rtr_freq;     // in arg1
    uchar ahb_freq;     // in arg1
    uchar nActive;      // in arg1
    ushort  temp1;         // from sensor-1, for arg2
    ushort temp3;         // from sensor-3, for arg2
    uint memfree;       // optional, sdram free (in bytes), in arg3
	//uchar load[18];
} pro_info_t;

pro_info_t myProfile;
ushort my_pID;

/*====================== PLL-related Functions =======================*/
#define N_AVAIL_FREQ    93
#define MAX_CPU_FREQ    255
#define MIN_CPU_FREQ    100
#define MAX_AHB_FREQ    173
#define MIN_AHB_FREQ    130
#define MAX_RTR_FREQ    MAX_AHB_FREQ
#define MIN_RTR_FREQ    MIN_AHB_FREQ
enum PLL_COMP_e {PLL_CPU, PLL_AHB, PLL_RTR};
typedef enum PLL_COMP_e PLL_PART;

void initPLL();                             // set PLLs to fine-grained mode
void changeFreq(PLL_PART component, uint f);
void showPLLinfo(uint output, uint arg1);
void revertPLL();                           // return back PLL configuration
uchar readFreq(uchar *fAHB, uchar *fRTR);		// read freqs of three components

// Let's put frequency list here so that it can be used by the Q-gov as well
//#define lnMemTable					93
#define lnMemTable                  N_AVAIL_FREQ // make it consistent with Q-gov
#define wdMemTable                  3
// memTable format: freq, MS, NS --> with default dv = 2, so that we don't have
// to modify r24
static const uchar memTable[lnMemTable][wdMemTable] = {
{10,1,2},
{11,5,11},
{12,5,12},
{13,5,13},
{14,5,14},
{15,1,3},
{16,5,16},
{17,5,17},
{18,5,18},
{19,5,19},
{20,1,4},
{21,5,21},
{22,5,22},
{23,5,23},
{24,5,24},
{25,1,5},
{26,5,26},
{27,5,27},
{28,5,28},
{29,5,29},
{30,1,6},
{31,5,31},
{32,5,32},
{33,5,33},
{34,5,34},
{35,1,7},
{36,5,36},
{37,5,37},
{38,5,38},
{39,5,39},
{40,1,8},
{41,5,41},
{42,5,42},
{43,5,43},
{44,5,44},
{45,1,9},
{46,5,46},
{47,5,47},
{48,5,48},
{49,5,49},
{50,1,10},
{51,5,51},
{52,5,52},
{53,5,53},
{54,5,54},
{55,1,11},
{56,5,56},
{57,5,57},
{58,5,58},
{59,5,59},
{60,1,12},
{61,5,61},
{62,5,62},
{63,5,63},
{65,1,13},
{70,1,14},
{75,1,15},
{80,1,16},
{85,1,17},
{90,1,18},
{95,1,19},
{100,1,20},
{105,1,21},
{110,1,22},
{115,1,23},
{120,1,24},
{125,1,25},
{130,1,26},
{135,1,27},
{140,1,28},
{145,1,29},
{150,1,30},
{155,1,31},
{160,1,32},
{165,1,33},
{170,1,34},
{175,1,35},
{180,1,36},
{185,1,37},
{190,1,38},
{195,1,39},
{200,1,40},
{205,1,41},
{210,1,42},
{215,1,43},
{220,1,44},
{225,1,45},
{230,1,46},
{235,1,47},
{240,1,48},
{245,1,49},
{250,1,50},
{255,1,51},
};


/*====================== CPUidle-related Functions =======================*/
uchar running_cpu_idle_cntr[18];
uchar stored_cpu_idle_cntr[18];
uchar virt_cpu_idle_cntr[18];
uint idle_cntr_cntr; //master counter that count up to 100
void init_idle_cntr();
void startProfiling(uint null, uint nill);
void stopProfiling(uint null, uint nill);


/*==================== Temperature-related Functions =====================*/
uint tempVal[3];							// there are 3 sensors in each chip
uint readTemp();                            // in addition, sensor-2 will be returned
REAL getRealTemp(uint tVal);                // use sensor-2 only!

/*======================= Event-related Functions =====================*/
void init_Router();                         // sub-profilers report to the root profiler
void init_Handlers();

/*======================== Governors ========================*/
// how CPUperf will be calculated in Q-gov?
// - if many (all) cores are used, then use METHOD_AVG
// - if single (or very few) cores, such as in singe-core JPEG aplx, then use METHOD_MAX
#define METHOD_AVG              0
#define METHOD_MAX              1
#define CPU_PERF_MEAS_METHOD    METHOD_MAX

typedef enum gov_e
{
  GOV_USER,
  GOV_ONDEMAND,
  GOV_PERFORMANCE,
  GOV_POWERSAVE,
  GOV_CONSERVATIVE,
  GOV_PMC,
  GOV_QLEARNING
} gov_t;

static gov_t current_gov = GOV_USER;
uint freqUserDef;
static volatile uchar qIsInitialized = FALSE;
void init_governor();
void change_governor(gov_t newGov, uint user_param); // see profiler_gov.c for the meaning of those arguments
void get_governor_status(uint arg1, uint arg2);
void governor(uint arg1, uint arg2); // callback for main governor thread


/*======================== Misc. Functions ===========================*/
void sanityCheck();
void generateProfilerID();
void print_cntr(uint null, uint nill);
uchar getNumActiveCores();


/*********************** Logging mechanism ****************************
 * For logging, the host may send a streaming request to a certain
 * profiler.
 * */
void collectData(uint None, uint Unused);
volatile uint streaming; // do we need streaming? initially no!

#endif // PROFILER_H
