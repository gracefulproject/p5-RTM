#include "profiler.h"

/* This is the main governor thread */

static void gov_user_def();
static void gov_on_demand();
static void gov_performance();
static void gov_powersave();
static void gov_conservative();
static void gov_pmc();
static void gov_qlearning();


void init_governor()
{
    freqUserDef = readFreq(NULL, NULL); // initially, use current frequency
    current_gov = GOV_USER;
    spin1_schedule_callback(governor, 0, 0, IDLE_PRIORITY_VAL);
}


/* change_governor() is called via hSDP
 * for GOV_USER, user_param will set the desired user_def_freq
 * for GOV_QLEARNING, user_param determines if Q-learning should be reset or not
 *                    when reset, the Q will restart the learning process
 * */
void change_governor(gov_t newGov, uint user_param)
{
#if(DEBUG_LEVEL>0)
    io_printf(IO_BUF, "[INFO] Changing to mode-%d with user_param-%d\n", newGov, user_param);
#endif
    current_gov = newGov; // will take action in the next schedule
    freqUserDef = user_param;
    qIsInitialized = user_param; // In C true is any value != 0 , and false == 0
}

void get_governor_status(uint arg1, uint arg2)
{
    io_printf(IO_BUF, "[INFO] govMode-%d (freqUserDef=%d)\n", current_gov, freqUserDef);
    io_printf(IO_STD, "[INFO] govMode-%d (freqUserDef=%d)\n", current_gov, freqUserDef);
}

void governor(uint arg1, uint arg2)
{
    switch(current_gov) {
    case GOV_USER: gov_user_def(); break;
    case GOV_ONDEMAND: gov_on_demand(); break;
    case GOV_PERFORMANCE: gov_performance(); break;
    case GOV_POWERSAVE: gov_powersave(); break;
    case GOV_CONSERVATIVE: gov_conservative(); break;
    case GOV_PMC: gov_pmc(); break;
    case GOV_QLEARNING: gov_qlearning(); break;
    default: gov_user_def();
    }

    // finally schedule itself
    spin1_schedule_callback(governor, 0,0, IDLE_PRIORITY_VAL);
}

void gov_user_def()
{
    uint f = readFreq(NULL, NULL);
    if (f==freqUserDef) return;
    changeFreq(PLL_CPU, freqUserDef);
}

void gov_on_demand()
{
    uint f = readFreq(NULL, NULL);
    // find if there's a core with maximum cpu load in virt_cpu_idle_cntr
    uint i, found = 0;
    for(i=2; i<18; i++) {
        if(virt_cpu_idle_cntr[i]==100) {
            found = 1;
            break;
        }
    }
    if(found==1) {
        if(f!=MAX_CPU_FREQ)
            changeFreq(PLL_CPU, MAX_CPU_FREQ);
    }
    else {
        if(f!=MIN_CPU_FREQ)
            changeFreq(PLL_CPU, MIN_CPU_FREQ);
    }
}

void gov_performance()
{
    uint f = readFreq(NULL, NULL);
    if(f==MAX_CPU_FREQ) return;
    changeFreq(PLL_CPU, MAX_CPU_FREQ);
}

void gov_powersave()
{
    uint f = readFreq(NULL, NULL);
    if(f==MIN_CPU_FREQ) return;
    changeFreq(PLL_CPU, MIN_CPU_FREQ);
}

void gov_conservative()
{
    uint f = readFreq(NULL, NULL);
    // find if there's a core with maximum cpu load in virt_cpu_idle_cntr
    uint i, found = 0;
    for(i=2; i<18; i++) {
        if(virt_cpu_idle_cntr[i]==100) {
            found = 1;
            break;
        }
    }
    if(found==1) {
        if(f<MAX_CPU_FREQ) {
            // compute freq difference
            uint fd = ((MAX_CPU_FREQ - f)/10)*5; // eq. ((255 - 100)/10)*5 = 75
            fd += f;
            if(fd!=f)
                changeFreq(PLL_CPU, fd);
        }
    }
    else {
        if(f>MIN_CPU_FREQ) {
            // compute freq difference
            uint fd = ((f-MIN_CPU_FREQ)/10)*5; // eq. ((255 - 100)/10)*5 = 75
            fd = f - fd;
            if(fd!=f)
                changeFreq(PLL_CPU, fd);
        }
    }
}

// gov_pmc == improved conservative
void gov_pmc()
{
    uint f = readFreq(NULL, NULL);
    // find what is the maximum cpu load
    uint i,mxCpuLoad=0;
    for(i=2; i<18; i++){
        if(virt_cpu_idle_cntr[i]>mxCpuLoad)
            mxCpuLoad = virt_cpu_idle_cntr[i];
    }
    uint y = (155*mxCpuLoad + 10000)/100;
    // then extrapolate y into the closest factor of 5
    y -= (y%5);
    if(f!=y) changeFreq(PLL_CPU, y);
}

// gov_qlearning functionalities are written in profiler_q.c
extern void initQ();
extern void runQ();

void gov_qlearning()
{
    // qIsInitialized is determined in change_governor()
    if(qIsInitialized==FALSE) {
        initQ(); // and set in learning mode
    } else {
        runQ();
    }
}
