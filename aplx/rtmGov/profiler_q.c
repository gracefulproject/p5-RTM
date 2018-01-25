#include "profiler.h"

/*---------------------------------------------------------------------------*
 *                             Q-Learning Method                             *
 * --------------------------------------------------------------------------*/
#define N_FREQ_ITEM	                N_AVAIL_FREQ
#define	N_STATES                    N_FREQ_ITEM
#define N_ACTION                    3

typedef enum {DOWN, STAY, UP} act_t;

typedef struct st
{
    uchar state;    // from 0 to N_STATES-1
    act_t action;   //
} st_t;

volatile uchar qIsLearning;
static uchar qIsExploring;			// set this value to explore rather than exploit

static int Q[N_STATES][N_STATES] = {0};
st_t currentState;
//st_t nextState;

// state parameters/measurements
ushort fList[N_FREQ_ITEM];
static uchar N_USED_FREQ = N_FREQ_ITEM; // we'll revise later!
REAL CPUperf;				// CPU performance, how to measure it? averaging or max?
uint currentFreq;
REAL currentTempReal;
int currentRewardVal;


// these will control how Q-learning operates:
REAL  alphaVal;				// learning rate
REAL  gammaVal;				// discount factor
REAL  betaVal;					// exloration ratio

uchar getStateFromFreq(uint freq)
{
    uchar i,result;
    for(i=0; i<N_USED_FREQ; i++)
        if (freq==fList[i]) {
            result = i;
            break;
        }
    return result;
}

void initQ()
{
    alphaVal = 0.1;	// learning rage
    gammaVal = 0.75;	// discount factor
    betaVal = 0.5;		// exploration ratio

#if(DEBUG_LEVEL>0)
    io_printf(IO_STD, "[Q-gov] alpha=%k, gamma=%k, beta=%k\n", alphaVal, gammaVal, betaVal);
#endif

    // create a list of frequency
    uchar i=0, j=0;
    for(; i<N_FREQ_ITEM; i++) {
        //fList[i] = 100 + i*10;
        if(memTable[i][0] >= MIN_CPU_FREQ)
            fList[j++] = memTable[i][0];
    }
    // then we revise the number of used frequency
    N_USED_FREQ = j;

    // let's randomly initialize currentState: NO!!!
    //uchar s = sark_rand() % N_STATES;
    //uchar a = sark_rand() % N_ACTION;

    // set the current state based on the current frequency
    currentState.state = getStateFromFreq(readFreq(NULL, NULL));

    qIsLearning = TRUE;
}

/* TODO: how to compute reward value?
 * */
void computeReward()
{
    // currentRewardVal = (avgCPUidle/35) -  tempVal[2];
    //currentRewardVal = (int)(100.00 - avgCPUload - currentTempReal);
    currentRewardVal = (int)(100.00 - CPUperf - currentTempReal);
    //if(currentTempReal < 41) currentRewardVal=1; else currentRewardVal=-1;
}

/* collectMeasurement() computes the average cpu idle and convert temparture
 * from integer to centigrade
 * */
void collectMeasurement()
{
    // read average CPU load
    uint iCPUperf = 0;
#if(CPU_PERF_MEAS_METHOD==METHOD_AVG)
    for(uchar i=2; i<18; i++)
        iCPUperf += virt_cpu_idle_cntr[i];
    CPUperf /= 16;
#else // get the maximum
    iCPUperf = virt_cpu_idle_cntr[2];
    for(uchar i=3; i<18; i++) {
        if(iCPUperf < virt_cpu_idle_cntr[i])
            iCPUperf = virt_cpu_idle_cntr[i];
    }
#endif
    CPUperf = (REAL)iCPUperf;
    // compute real temperature from sensor-2
    currentTempReal = getRealTemp(0);
}

uchar getNexStateFromAction(st_t currSt)
{
    uchar result;
    // if in the boundary, than remain there
    if((currSt.state==0 && currSt.action==DOWN) ||
       (currSt.state==(N_STATES-1) && currSt.action==UP)) {
        result = currSt.state;
    }
    else {
        switch(currSt.action) {
        case DOWN:
            result = currSt.state - 1; break;
        case STAY:
            result = currSt.state; break;
        case UP:
            result = currSt.state + 1; break;
        }
    }
    return result;
}

// find the maximum future Q-value according to the selected action
int maxFutureQVal(st_t currSt)
{
    int maxQ;
    uchar i;
    st_t dummyState;
    uchar possibleNextState[N_ACTION];
    for(i=0; i<N_ACTION; i++) {
        dummyState.state = currSt.state;
        dummyState.action = (act_t)i;
        possibleNextState[i] = getNexStateFromAction(dummyState);
    }
    int possibleQ[N_ACTION];
    for(i=0; i<N_ACTION; i++) {
        possibleQ[i] = Q[possibleNextState[i]][possibleNextState[i]];
    }
    maxQ = possibleQ[0];
    for(i=1; i<N_ACTION; i++) {
        if(possibleQ[i] > maxQ) {
            maxQ = possibleQ[i];
        }
    }
    return maxQ;
}

act_t getActionWithMaxQ(st_t currSt)
{
    int Qval[N_ACTION];
    uchar i;
    act_t a;
    st_t dummyState;
    uchar sIdx;
    int maxQ;
    dummyState.state = currSt.state;
    for(i=0; i<N_ACTION; i++) {
        if(currSt.state==0 || currSt.state==(N_STATES-1))
            a = STAY;
        else
            a = (act_t)i;
        dummyState.action = a;
        sIdx = getNexStateFromAction(dummyState);
        Qval[i] = Q[sIdx][sIdx];
    }
    uchar maxAction = 0;
    maxQ = Qval[maxAction];
    for(i=1; i<N_ACTION; i++) {
        if(Qval[i] > maxQ) {
            maxQ = Qval[i];
            maxAction = i;
        }
    }
    return (act_t)maxAction;
}

void updateQ()
{

    // stage-1: determine if we're exploring or exploiting
    uint rv = sark_rand() % 65535;	// the max value of REAL is 65535
    REAL ex = (REAL)rv / (REAL)65535;
    //io_printf(IO_STD, "ex=%k, betaVal=%k\n", ex, betaVal);
    if(ex > betaVal) {
        qIsExploring = TRUE;
#if(DEBUG_LEVEL>0)
        io_printf(IO_STD, "[Q-gov] Exploring...\n");
#endif
    }
    else {
        qIsExploring = FALSE;
#if(DEBUG_LEVEL>0)
        io_printf(IO_STD, "[Q-gov] Exploiting...\n");
#endif
    }

    // If exploration is active, then next action selection is based on
    // a random probability value. Otherwise, it select the best action
    // that leads to the maximum Q_value/reward.

    // Formula: Q'(s,a) = Q(s,a) + a*{r + g*[max_Q(s',a)] - Q(s,a)}
    // requirement: reward

    // stage-2: select an action, either randomly of with best known value
    if(qIsExploring==TRUE) {
        currentState.action = sark_rand() % N_ACTION;	// DOWN, STAY of UP
    }
    else {
        // in the beginning, if exploitation is chosen, then
        // this algorithm very likely will choose act_t::DOWN, because Q is 0
        currentState.action = getActionWithMaxQ(currentState);
    }

    // stage-3: get the maximum future-Q based on the selected action
    int maxQ = maxFutureQVal(currentState);

    // stage-4: update Q table
    int currentQ = Q[currentState.state][currentState.state];
    REAL deltaQ = alphaVal * ((REAL)currentRewardVal + gammaVal * (REAL)maxQ -
                  (REAL)currentQ);

    // with reward only +1 and -1, deltaQ will be less than 1,
    // hence, nextQ will always 0. Thus, we need to multiply with something
    //int nextQ = currentQ + (int)(deltaQ*1000);
    int nextQ = currentQ + (int)deltaQ;
#if(DEBUG_LEVEL>0)
    io_printf(IO_STD, "a=%d, maxFutureQ=%d, deltaQ=%k, nextQ=%d\n",
              currentState.action, maxQ, deltaQ, nextQ);
#endif


    // then put the nextQ to the Qtable
    Q[currentState.state][currentState.state] = nextQ;

    // stage-5: go to the next state with the selected action
    currentState.state = getNexStateFromAction(currentState);
}


uint getFreqFromState(st_t s)
{
    return fList[s.state];
}


void runQ()
{
    // stage1: collect measurement and compute reward
    //         --> raw values are provided by the profiler
    collectMeasurement(); // here, we do: cpu averaging and temperature conversion

    // the reward is based on measurement and performance
    computeReward();

    // optionally, send the measurement/reward to host
    //sendMReport();

    // stage2: update Q-matrix
    // TODO: when it is in learning mode and when it is in execution mode?
    //       --> sent via SDP, with arg1==0 means execute, arg1==1 means learn

    if(qIsLearning==TRUE) {
#if(DEBUG_LEVEL>0)
        io_printf(IO_STD, "[Q-gov] is learning!\n");
#endif
        updateQ();
    }
    else {
        uint cf = readFreq(NULL, NULL);
        uint f = getFreqFromState(currentState);
        if(f==cf) return;
        // change the frequency based on the current state
        changeFreq(PLL_CPU, f);
    }
}
