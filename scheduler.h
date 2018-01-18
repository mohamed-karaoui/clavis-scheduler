/*
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The Clavis user-level scheduler is designed to implement various scheduling algorithms
 * under Linux Operating System running on multicore and NUMA machines. It is written in C
 * to make the integration with the default OS scheduling facilities seamless.
 *
 * Clavis is released under Academic Free License (AFL), which allows you to use the source code
 * in your proprietary projects. We only ask to please cite our Linux Symposium paper that
 * introduces Clavis, in case you will decide to publish your work.
 */

/*
 * user-level scheduler (see README.txt for further details)
 *
 * compile on UMA:  gcc -lm -lutil -pthread scheduler.h signal-handling.c scheduler-tools.c scheduler-algorithms.c -o scheduler.out
 * compile on NUMA: gcc -lnuma -lm -lutil -pthread scheduler.h signal-handling.c scheduler-tools.c scheduler-algorithms.c -o scheduler.out
 * note that some gcc versions require different order of options during gcc invocation:
 *                  gcc scheduler.h signal-handling.c scheduler-tools.c scheduler-algorithms.c -lm -lnuma -lutil -pthread -o scheduler.out 
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/dir.h>
#include <sys/param.h>
#include <sched.h>
#include <math.h>
#include <getopt.h>
#include <inttypes.h>
#include <ctype.h>

#include <sys/resource.h>
#include <sys/time.h>

#include <sys/mman.h>
#include <stdarg.h>

// for numa machines (trocadero)
#include <numa.h>
#include <numaif.h>

#define MAX_LINE_SIZE 8192
#define APP_RUN_SIZE 2048
#define MAX_NUMA_NODES 4


// if we run apache and mysql
#define LAMP_WORKLOAD 0

// if we run mpi (hpcc, imb, etc.). should be 0 for the online detection!
#define MPI_WORKLOAD 0
// if we run mpi, then that is the maximum MPI's iA (for the workloads with both MPI and regular apps like HEP SPEC)
#define MAX_MPI_APP 24

// if the workload has large (more than 20% of the total machine memory per app) we turn off counting memory pages on each node to re-migrate the memory when part of the pages failed to migrate on the first time
#define LARGE_MEMORY_WORKLOAD 1

// 1000 milliseconds - !!! very important parameter - changes the CPU time consumed by the scheduler significantly
#define REFRESH_PERIOD 1000000
#define WAIT_PERIOD 500000
#define PFMON_PERIOD 1000
#define PERIOD_IN_SEC 1

// number of times application has to be restarted
#define SLEEPER_RESTART 4

//#define NETHOGS_PERIOD 1
//#define NETHOGS_INTERFACE "eth4"

// 0 - no memory migration 
// 1 - migrate the whole memory as specified by MIGRATE_WHOLE_MEMORY_TYPE
// 2 - migrate the part of the memory with ibs as specified by PAGES, BACKWARDS, RANDOM
#define MIGRATION_TYPE 0

#define PAGES 4096
#define BACKWARDS 1
#define RANDOM 0

#define IBS_INTERVAL 10000

// do not use runfile
#define DETECT_ONLINE 0
// 0 - workload is determined by the runfile
// 1 - workload is determined using a separate top process (requires a valid configuration file placed in ~ + top compiled with -O0)
#define DETECT_COMPUTE_BOUND_VIA_TOP 0
// detects MPI ranks (if this option is "1" and DETECT_HADOOP_CHILDS, DETECT_COMPUTE_BOUND_VIA_TOP, DETECT_COMMUNICATION_BOUND_VIA_NETHOGS and 
// DETECT_IO_BOUND_VIA_IOTOP are "0", then only MPI ranks will be detected and scheduled and only MPI ranks will show up in the Clavis logs - very convenient for HPC)
#define DETECT_MPI_RANKS 0
// detects Hadoop mappers and reducers (if this option is "1" and DETECT_MPI_RANKS, DETECT_COMPUTE_BOUND_VIA_TOP, DETECT_COMMUNICATION_BOUND_VIA_NETHOGS and 
// DETECT_IO_BOUND_VIA_IOTOP are "0", then only Hadoop childs will be detected and scheduled and only them will show up in the Clavis logs - very convenient for MapReduce)
// note: only Hadoop childs (java VMs that directly perform map or reduce tasks) will be detected. Hadoop JobTrackers, TaskTrackers, NameNodes amd DataNodes will *not* be included.
#define DETECT_HADOOP_CHILDS 0
// requires a modified nethogs
#define DETECT_COMMUNICATION_BOUND_VIA_NETHOGS 0
#define ADD_NETHOGS_TO_APP_ARRAY 0
#define DETECT_COMM_CONTENTION_VIA_NETSTAT 0
#define DETECT_IO_BOUND_VIA_IOTOP 0
#define ADD_IOTOP_TO_APP_ARRAY 0

// collect various information about system workload
#define GENERATE_RESOURCE_VECTOR 1
// whether to truncate resource vector after each scheduling interval
#define TRUNCATE_RESOURCE_VECTOR 0
// adds vm name to resource vector after pid, if the pid is of a vm
// turn off this option if not using kvm as it may slow down the scheduler otherwise
#define ADD_VM_NAME_TO_RESOURCE_VECTOR 0
// adds OpenVZ container ID to resource vector after pid, if the pid is of a container
#define ADD_CONTAINER_ID_TO_RESOURCE_VECTOR 0

// 0 - user-level app migratepages
// 1 - systen call numa_migrate_pages
#define MIGRATE_WHOLE_MEMORY_TYPE 1

// the max number of times memory can be bounced between the nodes
#define BOUNCING_LIMIT 5

#define MAX_TIMEOUT 10000000

#define DEVIL_HORIZON 15
#define SUPER_DEVIL_HORIZON 500

//in KB
#define COMM_BOUND 1000

// as seen in %cpu column of ps or top
// very important parameter for DINO.
// Making it too high with Hadoop will cause DINO to only schedule very compute bound JVMs and so
// they will be placed on cores with, say, 30% CPU bound JVMs which will cause congestion, performance decrease and high deviation.
// For MPI its ok to make it high though, as they all are 100% CPU intensive.
#define COMPUTE_BOUND_HORIZON 90
// as seen in %mem column of ps or top
#define FOOTPRINT_BOUND_HORIZON 10
// as seen in iotopper columns, in Kb/s
#define DISK_TRAFFIC_HORIZON 500
// as seen in nethogs columns, in Kb/s
#define NETWORK_TRAFFIC_HORIZON 1000

#define REPORTS_DIRECTORY "/root/clavis-src-6.5.5/"

#define MAX_SHARED_CACHES_NUMBER 4
#define MAX_CORES_PER_SHARED_CACHE 12
#define MAX_CORE_ID 32
#define MAX_CHIP_ID 4
#define MAX_THREAD_NUMBER 64
#define MAX_SIG_COUNTERS 32
#define MAX_RUN_TIMES 128

#define MAX_MOULD_TIMES 1000

#define lock(x) pthread_mutex_lock(x) 
#define unlock(x) pthread_mutex_unlock(x)

/*#define LOGFILE "/home/sba70/scheduler/user-level-scheduler/scheduler.log"
 #define MOULDFILE "/home/sba70/scheduler/user-level-scheduler/mould.log"
 #define TIMEFILE "/home/sba70/scheduler/user-level-scheduler/time.log"
 #define PFMONFILE "/home/sba70/scheduler/user-level-scheduler/pfmon.log"
 #define ONLINEFILE "/home/sba70/scheduler/user-level-scheduler/online.log"
 #define NUMAFILE "/home/sba70/scheduler/user-level-scheduler/numa.log"
 #define SYSTEMWIDEFILE "/home/sba70/scheduler/user-level-scheduler/online.log"
 */
char LOGFILE[MAX_LINE_SIZE + 5];
char MOULDFILE[MAX_LINE_SIZE + 5];
char MOULDFILEBM[MAX_LINE_SIZE + 5];
char TIMEFILE[MAX_LINE_SIZE + 5];
char PFMONFILE[MAX_LINE_SIZE + 5];
char ONLINEFILE[MAX_LINE_SIZE + 5];
char NUMAFILE[MAX_LINE_SIZE + 5];
char SYSTEMWIDEFILE[MAX_LINE_SIZE + 5];

char NETHOGSFILE[MAX_LINE_SIZE + 5];
char CLUSTERREPORTFILE[MAX_LINE_SIZE + 5];

char line_temp[MAX_LINE_SIZE+5];

char aggregate_results[MAX_LINE_SIZE + 5]; // for <multi-threaded/single-threaded>

struct app
{
  // data from the runfile (static)
  char*   pszName;
  int   iTimeToStart; // when to start (in seconds since prototype launch)
  char*   pszInvocation;
  char*   pszRunDir;
  
  int phase_change[MAX_THREAD_NUMBER];
  int cpu_util_change[MAX_THREAD_NUMBER];
  int old_jiffies[MAX_THREAD_NUMBER];
  short int to_schedule[MAX_THREAD_NUMBER];
  
  long double aldAppsSignatures[MAX_THREAD_NUMBER][MAX_SIG_COUNTERS];
  long double aldOldAppsSignatures[MAX_THREAD_NUMBER][MAX_SIG_COUNTERS];
  
  long double aldOldOnlineMisses[MAX_THREAD_NUMBER];
  long double aldOldOnlineInstructions[MAX_THREAD_NUMBER];
  
  long double aldActualOnlineMisses[MAX_THREAD_NUMBER];
  long double aldActualOnlineInstructions[MAX_THREAD_NUMBER];
  
  // data which changes during execution (dynamic)
  int   iCount; // how many times this program was succesfully relaunched
  int   iPID; // pid for the current run
  int   iMpirunPID; // pid for the mpirun or orted
  int   iShellPID; // pid for the spawning shell (for assigning an MPI process to a cluster job)
  int   aiTIDs[MAX_THREAD_NUMBER]; // thread ids of the current run
  int   aiSharedCacheMasks[MAX_THREAD_NUMBER];
  int   aiOldSharedCacheMasks[MAX_THREAD_NUMBER];
  
  int   aiMigrated[MAX_THREAD_NUMBER]; // how many times each thread was succesfully migrated between shared caches
  int   aiExecTimes[MAX_RUN_TIMES]; // execution times of the different runs
  
  int   aiActualCores[MAX_THREAD_NUMBER]; // the actual core this thread is binded to according to ps's psr field atm
  int   aiOldCores[MAX_THREAD_NUMBER]; // the core this thread was bound to during the previous measurement
  int   aiMouldIntervals[MAX_THREAD_NUMBER]; // the number of intervals this thread was bounded to the same core
  
  // for resource vector
  int   aiCPUutil[MAX_THREAD_NUMBER]; // CPU utilization from topper
  long double   aiMemFootprint[MAX_THREAD_NUMBER]; // memory footprint in % from topper
  int   prevIdle[MAX_THREAD_NUMBER]; // if CPU utilization was updated in the previous interval
  
  long double aldOnlineDiskWrite[MAX_THREAD_NUMBER]; //in KB/s
  long double aldOnlineDiskRead[MAX_THREAD_NUMBER]; //in KB/s
  long double aldOnlineDiskWait[MAX_THREAD_NUMBER]; //in %
  
  long double aldOnlineTrafficSent[MAX_THREAD_NUMBER]; //in KB
  long double aldOnlineTrafficReceived[MAX_THREAD_NUMBER]; //in KB
  
  
  long double aldTotalDiskWrite[MAX_THREAD_NUMBER]; //in KB
  long double aldTotalDiskRead[MAX_THREAD_NUMBER]; //in KB
  
  long double aldTotalTrafficSent[MAX_THREAD_NUMBER]; //in KB
  long double aldTotalTrafficReceived[MAX_THREAD_NUMBER]; //in KB
  
  int   aiActualNodePages[MAX_THREAD_NUMBER][MAX_NUMA_NODES];
  int   aiOldNodePages[MAX_THREAD_NUMBER][MAX_NUMA_NODES];
  int   aiNUMAIntervals[MAX_THREAD_NUMBER];
  
  int   aiCoresLeftFrom[MAX_THREAD_NUMBER]; // the core on which the thread was executing before the migration or run termination
  
  // data to restore the default behavior from mould.log
  //short int mould_moments[MAX_THREAD_NUMBER][MAX_MOULD_TIMES];
  //short int core[MAX_THREAD_NUMBER][MAX_MOULD_TIMES];
  
  int   aiPriorities[MAX_THREAD_NUMBER]; // thread's priority (0 - none, 1 - important, 2 - real time)
  int   aiBindCores[MAX_THREAD_NUMBER]; // core to bind to from the runfile
  int   aiBindNodes[MAX_THREAD_NUMBER]; // node to bind to from the runfile
  
  int interCoreMigrations[MAX_THREAD_NUMBER];
  int interNodeMigrations[MAX_THREAD_NUMBER];
  int pages_migrated[MAX_THREAD_NUMBER];
  
  int   aiBounced[MAX_THREAD_NUMBER]; // the number of times memory can be bounced between the nodes
  
  int mypst[2]; // descriptors for pseudo-terminal
  
  pthread_mutex_t lock;
  
  int put_to_sleep;
  int active_intervals; // number of active intervals (with to_schedule >= 0)
  
  int containerIDs[MAX_THREAD_NUMBER];
  int aiVTIDs[MAX_THREAD_NUMBER];
  int aiRecv_q[MAX_THREAD_NUMBER];
  int aiSend_q[MAX_THREAD_NUMBER];
};

int queue_turtles[APP_RUN_SIZE], queue_devils[APP_RUN_SIZE];
int active_devil, active_turtle;
int queue_built;

int numa_pst[MAX_NUMA_NODES*2][2]; // descriptors for pseudo-terminal for NUMA system-wide sessions (2 per every chip) 

int top_pst[2]; // descriptor for pseudo-terminal for top thread
int iotop_pst[2]; // descriptor for pseudo-terminal for iotop thread

int old_solution[MAX_SHARED_CACHES_NUMBER][MAX_CORES_PER_SHARED_CACHE][3];

char*   pszHostname;
struct app sAppRun[APP_RUN_SIZE];
int aiApps[APP_RUN_SIZE]; // indexes in runfile
int iRunAppCount; // number of applications that were chosen to run

int aiComputeBoundTIDs[APP_RUN_SIZE*MAX_THREAD_NUMBER];
int aiRankTIDs[APP_RUN_SIZE*MAX_THREAD_NUMBER];

int timestamp;
int mould_time;

int interNodeMigrations;
int interCoreMigrations;
float devilPerc;

int old_prio_devils, old_non_prio_devils, old_turtles;
int pcache;

// global configuration
short int fSchedulerMode;
short int fTool;
short int all_finished_3_times;
int iTimeToDie; // for the whole scheduler
int shared_caches_number;
int cores_per_shared_cache;
short int iRestart;
short int iRandomLaunch;

int last_time;
int topper_restart;

int last_core_to_bind;

pthread_t aThreads[APP_RUN_SIZE];
pthread_t onlineThread[APP_RUN_SIZE];
pthread_t system_wideThread[MAX_NUMA_NODES*2];
pthread_t topThread;
pthread_t iotopThread;
pthread_t aIbsThread;

// for system-wide sessions
long int sample_instr[MAX_CORE_ID];
long int sample_counters[MAX_CORE_ID][12];
long int chip_wide_counters[MAX_CHIP_ID][32];

int cores[MAX_SHARED_CACHES_NUMBER][MAX_CORES_PER_SHARED_CACHE];
int masks[MAX_SHARED_CACHES_NUMBER][MAX_CORES_PER_SHARED_CACHE];

// function prototypes
int main (int argc, char* argv[]);
void child_term_handler(int sig);
int launchSleeper(int iA);
int sleeper (int iA);
void* onliner (void* iA);
void* system_wider_dram (void* session_id);
void* system_wider (void* session_id);
void* topper ();
void* iotopper ();
int launchApp (int iIndex);
int launchIBSer();
void* IBSer ();
void initializePoolMember(int iA);
void populatePool (char* pszFilename);
int file_select(const struct direct *entry);
int checkThreads(int iA);
int get_col(char * line, int colno, int mode, int in_ppid);
int get_child_id(int ppid, int which);
int get_pid_from_tid(int tid);
char * simply_get_col(char * line, int colno);
char * get_binary_name(int in_pid);
int isNumber(const char str[]);
char * log_to(char * file, char * line);
char * get_vm_name(int pid);
void updateTraffic();


int aiSharedCacheCounters[MAX_SHARED_CACHES_NUMBER][6];
int aiSharedCacheCores[MAX_SHARED_CACHES_NUMBER][MAX_CORES_PER_SHARED_CACHE];

// function prototypes
int doRegularAssignment(int iA, int iT);
int doGreedy(int iA, int iT);
int migrate_memory(pid_t pid, int oldNode, int newNode);
int bindThread(int iA, int iT);
int unbindThread(int iA, int iT);
int doRefreshAssignment(int iA);
int doOptimisticAssignment(int iA);
int cacheToMask(int cache);
int maskToCache(int mask);
int maskToCPU(int mask);
int cpuToMask(int cpu);
int getWinningCache(int iA, int iT);
int doIncrement(int iA, int iT);
int doDecrement(int iA, int iT);
void doMould();
int isTheCacheWinnning(int iA_source, int iT_source, int iA_dest, int iT_dest);
void InitScheduler(void);
int output_assignment();

int distributedIntensity(short int workload_has_changed);
int queueScheduler();
int swapToEvenDRAMMisses();
