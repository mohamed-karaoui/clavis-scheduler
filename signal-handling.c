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

#include "scheduler.h"



//=============================================================



int main(int argc, char* argv[]) 
{
  int min_param = 8;
  if (argc <= min_param)
  {
    printf("The user-level scheduler (version 6.5.5)\n");
    printf("Usage: scheduler.out <mode> <tool> <host> <runfile> <multi-threaded/single-threaded> <restart/once> <random/fixed> <benchmark 0> [<benchmark 1> ... <benchmark n>]\n");
    printf("<mode>: 'd' = default OS scheduler, 'b' = bind threads to cores, 'a' = animal scheduler, 'z' = distributed intensity scheduler, 'zo' = distributed intensity scheduler with online-based scheduling, 'p' = pacifier, 'c' = clustering threads for power savings, 'dino' = dino, 'q' = queue-based scheduling, 's' = swap threads to even out memory controller DRAM misses\n");
    printf("<tool>: a tool to collect performance statistics: one of 'pfmon' (works only on kernels <= 2.6.29), 'perf', 'none'\n");
    printf("<host>: hostname: one of 'Xeon_X5365_2', 'Xeon_X5365_4', 'Xeon_E5320', 'Xeon_X5570_woHT', 'Xeon_X5570_HT', 'Xeon_X5650_woHT', 'Xeon_X5650_HT', 'Opteron_2350_4cores', 'Opteron_2350_2cores', 'Opteron_8356_2', 'Opteron_8356_4', 'Opteron_2435_2nodes', 'Opteron_2435_1node', 'Opteron_8435', 'Opteron_6164'\n");
    printf("<runfile>: list of benchmarks to run, when each of it should start to run (in seconds since the start of this program) along with their signatures and invocation strings\n");
    printf("<multi-threaded/single-threaded>: 'mt' = multi-threaded workload, 'st' = single-threaded workload\n");
    printf("<restart/once>: 'restart' = programs restart once finished, 'once' = no restart after finish\n");
    printf("<random/fixed>: 'random' = programs start in random order, 'fixed' = programs start as specified in <runfile>\n");
    printf("<benchmark i>: benchmark index to run, index is a 0-based number from the list in <runfile>.\n");
    //        printf("The scheduler itself always runs on 0-th core.\n");      
    return 0;
  }
  
  struct rlimit mylimit;
  mylimit.rlim_cur = RLIM_INFINITY;
  mylimit.rlim_max = RLIM_INFINITY;
  setrlimit(RLIMIT_CORE, &mylimit);
  
  char*   pszRunfile = 0;
  int iA, i, j, k, need_to_log_numa, numa_maps_count, first_thread_id;
  
  char statline_numa[MAX_LINE_SIZE+5];
  char line_temp[MAX_LINE_SIZE+5];
  char line_temp2[MAX_LINE_SIZE+5];
  char numa_maps_text[512][MAX_LINE_SIZE+5];
  char vm_name_str[MAX_LINE_SIZE+5];
  sprintf(vm_name_str, "\0");
  int numa_node_pages[MAX_NUMA_NODES];
  FILE *procfile;
  FILE *tempfile;
  char * str_begin; char * str_end;
  
  timestamp = time(0);
  
  mould_time = 0;
  last_time = -1;
  topper_restart = 1;
  
  srand (time(NULL)); // don't delete - it's global
  
  char line[MAX_LINE_SIZE + 5];
  if (getcwd(line, sizeof(line)) != NULL)
  {
    sprintf(LOGFILE, "%s/scheduler.log", line);
    sprintf(MOULDFILE, "%s/mould.log", line);
    sprintf(MOULDFILEBM, "%s/mould.bm.log", line);
    sprintf(TIMEFILE, "%s/time.log", line);
    sprintf(PFMONFILE, "%s/pfmon.log", line);
    sprintf(ONLINEFILE, "%s/online.log", line);
    sprintf(NUMAFILE, "%s/numa.log", line);
    sprintf(SYSTEMWIDEFILE, "%s/systemwide.log", line);
    
    sprintf(NETHOGSFILE, "%s/nethogs.log", line);
    gethostname(line_temp, sizeof(line_temp));
    sprintf(CLUSTERREPORTFILE, "%s%s.report", REPORTS_DIRECTORY, line_temp);
  }
  else
  {
    perror("getcwd() error");
  }
  
  signal(SIGCHLD, child_term_handler); // we associate our handler with the signal of terminating child - needed for respawn application after it has finished (see below)
  
  sprintf (line_temp, "\n\n\n\n\n============================ %i ==============================\n\n\n\n\n", timestamp);
  log_to(LOGFILE, line_temp);
  log_to(TIMEFILE, line_temp);
  log_to(MOULDFILE, line_temp);
  log_to(PFMONFILE, line_temp);
  log_to(ONLINEFILE, line_temp);
  log_to(NUMAFILE, line_temp);
  log_to(SYSTEMWIDEFILE, line_temp);
  
  
  // first bind the scheduler to 0-th core
  /*    i=1;
   if (  sched_setaffinity(getpid(), sizeof(i), &(i)) < 0  ){
   int err_code = errno;
   sprintf (line_temp, "*** ERROR: sched_setaffinity failed for the scheduler: %s\n", strerror(err_code));
   log_to(LOGFILE, line_temp);
   printf (line_temp);
   exit(0);
   }*/
  
  iRunAppCount = argc - min_param;
  
  if (strcmp(argv[1], "d") == 0)
  {
    fSchedulerMode = 0;
  }
  else if (strcmp(argv[1], "b") == 0)
  {
    fSchedulerMode = 1;
  }
  else if (strcmp(argv[1], "a") == 0)
  {
    fSchedulerMode = 2;
  }
  else if (strcmp(argv[1], "z") == 0)
  {
    fSchedulerMode = 3;
  }
  else if (strcmp(argv[1], "zo") == 0)
  {
    fSchedulerMode = 4;
  }
  else if (strcmp(argv[1], "bm") == 0)
  {
    fSchedulerMode = 6;
  }
  else if (strcmp(argv[1], "p") == 0)
  {
    fSchedulerMode = 7;
  }
  else if (strcmp(argv[1], "c") == 0)
  {
    fSchedulerMode = 8;
  }
  else if (strcmp(argv[1], "dino") == 0)
  {
    fSchedulerMode = 9;
  }
  else if (strcmp(argv[1], "q") == 0)
  {
    fSchedulerMode = 10;
  }
  else if (strcmp(argv[1], "s") == 0)
  {
    fSchedulerMode = 11;
  }
  else
  {
    sprintf (line_temp, "*** ERROR: Bad <mode>! Must be either 'd' = default OS scheduler, 'b' = bind threads to cores, 'bo' = bind threads to cores with online-based scheduling, 'bm' = bind threads to cores using the mould trace, 'a' = animal scheduler, 'z' = distributed intensity scheduler, 'zo' = distributed intensity scheduler with online-based scheduling, 'nb' = NUMA bind, 'p' = pacifier, 'c' = clustering threads for power savings, 'dino' = dino, 'q' = queue-based scheduling, 's' = swap threads to even out memory controller DRAM misses.\n");
    log_to(LOGFILE, line_temp);
    printf ("%s", line_temp);
    exit(0);
  }
  
  if (strcmp(argv[2], "pfmon") == 0)
  {
    fTool = 0;
  }
  else if (strcmp(argv[2], "perf") == 0)
  {
    fTool = 1;
  }
  else if (strcmp(argv[2], "none") == 0)
  {
    fTool = 2;
  }
  else
  {
    sprintf (line_temp, "*** ERROR: Bad <tool>! Must be either 'pfmon' (works only on kernels <= 2.6.29), 'perf'.\n");
    log_to(LOGFILE, line_temp);
    printf ("%s", line_temp);
    exit(0);
  }
  
  pszHostname = calloc(MAX_LINE_SIZE + 1, 1);
  strncpy(pszHostname, argv[3], MAX_LINE_SIZE);
  if (strcmp(pszHostname, "Xeon_X5365_2") == 0)
  {
    shared_caches_number = 2;
    cores_per_shared_cache = 2;
  }
  else if (strcmp(pszHostname, "Xeon_X5365_4") == 0)
  {
    shared_caches_number = 4;
    cores_per_shared_cache = 2;
  }
  else if (strcmp(pszHostname, "Xeon_E5320") == 0)
  {
    shared_caches_number = 2;
    cores_per_shared_cache = 2;
  }
  else if (strcmp(pszHostname, "Xeon_X5570_woHT") == 0)
  {
    shared_caches_number = 2;
    cores_per_shared_cache = 4;
  }
  else if (strcmp(pszHostname, "Xeon_X5570_HT") == 0)
  {
    shared_caches_number = 2;
    cores_per_shared_cache = 8;
  }
  else if (strcmp(pszHostname, "Xeon_X5650_woHT") == 0)
  {
    shared_caches_number = 2;
    cores_per_shared_cache = 6;
  }
  else if (strcmp(pszHostname, "Xeon_X5650_HT") == 0)
  {
    shared_caches_number = 2;
    cores_per_shared_cache = 12;
  }
  else if (strcmp(pszHostname, "Opteron_2350_4cores") == 0)
  {
    shared_caches_number = 2;
    cores_per_shared_cache = 4;
  }
  else if (strcmp(pszHostname, "Opteron_2350_2cores") == 0)
  {
    shared_caches_number = 2;
    cores_per_shared_cache = 2;
  }
  else if (strcmp(pszHostname, "Opteron_8356_2") == 0)
  {
    shared_caches_number = 2; //only fast nodes for now
    cores_per_shared_cache = 4;
  }
  else if (strcmp(pszHostname, "Opteron_8356_4") == 0)
  {
    shared_caches_number = 4;
    cores_per_shared_cache = 4;
  }
  else if (strcmp(pszHostname, "Opteron_2435_2nodes") == 0)
  {
    shared_caches_number = 2;
    cores_per_shared_cache = 6;
  }
  else if (strcmp(pszHostname, "Opteron_2435_1node") == 0)
  {
    shared_caches_number = 1;
    cores_per_shared_cache = 6;
  }
  else if (strcmp(pszHostname, "Opteron_8435") == 0)
  {
    shared_caches_number = 4;
    cores_per_shared_cache = 6;
  }
  else if (strcmp(pszHostname, "Opteron_6164") == 0)
  {
    shared_caches_number = 4;
    cores_per_shared_cache = 6;
  }
  else if (strcmp(pszHostname, "kvm") == 0)
  {
    shared_caches_number = 1;
    cores_per_shared_cache = 12;
  }
  else
  {
    sprintf (line_temp, "*** ERROR: Bad <host>! Must be either 'Xeon_X5365_2', 'Xeon_X5365_4', 'Xeon_E5320', 'Xeon_X5570_woHT', 'Xeon_X5570_HT', 'Xeon_X5650_woHT', 'Xeon_X5650_HT', 'Opteron_2350_4cores', 'Opteron_2350_2cores', 'Opteron_8356_2', 'Opteron_8356_4', 'Opteron_2435_2nodes', 'Opteron_2435_1node', 'Opteron_8435', 'Opteron_6164', 'kvm'.\n");
    log_to(LOGFILE, line_temp);
    printf ("%s", line_temp);
    exit(0);
  }
  
  pszRunfile = argv[4];
  
  if (strcmp(argv[5], "mt") == 0)
  {
    sprintf(aggregate_results, "");
  }
  else if (strcmp(argv[5], "st") == 0)
  {
    sprintf(aggregate_results, "--aggregate-results ");
  }
  else
  {
    sprintf (line_temp, "*** ERROR: Bad <multi-threaded/single-threaded>! Must be either 'mt' = multi-threaded workload, 'st' = single-threaded workload.\n");
    log_to(LOGFILE, line_temp);
    printf ("%s", line_temp);
    exit(0);
  }
  
  if (strcmp(argv[6], "restart") == 0)
  {
    iRestart = 1;
  }
  else if (strcmp(argv[6], "once") == 0)
  {
    iRestart = 0;
  }
  else
  {
    sprintf (line_temp, "*** ERROR: Bad <restart/once>! Must be either 'restart' = programs restart once finished, 'once' = no restart after finish.\n");
    log_to(LOGFILE, line_temp);
    printf ("%s", line_temp);
    exit(0);
  }
  
  if (strcmp(argv[7], "random") == 0)
  {
    iRandomLaunch = 1;
  }
  else if (strcmp(argv[7], "fixed") == 0)
  {
    iRandomLaunch = 0;
  }
  else
  {
    sprintf (line_temp, "*** ERROR: Bad <random/fixed>! Must be either 'random' = programs start in random order, 'fixed' = programs start as specified in <runfile>.\n");
    log_to(LOGFILE, line_temp);
    printf ("%s", line_temp);
    exit(0);
  }
  
  iTimeToDie = MAX_TIMEOUT + time(0);
  
  short int launched[APP_RUN_SIZE];
  all_finished_3_times = 0;
  
  if (DETECT_ONLINE == 1) iRunAppCount = APP_RUN_SIZE;
  
  for (iA = 0; iA < APP_RUN_SIZE; iA++) initializePoolMember(iA);
  
  if (DETECT_ONLINE == 0)
  {
    // here we form arrays for those applications from <runfile> (and hence from sAppRun) that we want to execute during the current run
    for (iA = 0; iA < iRunAppCount; iA++)
    {
      aiApps[iA] = atoi(argv[min_param + iA]);
      launched[iA] = 0;
    }
    
    // read data about the benchmarks we want to execute during the current run from the config file <runfile> into sAppRun array
    populatePool(pszRunfile);
  }
  
  
  InitScheduler();
  
  
  int iResult;
  // starting system-wide sessions (2 per each socket)
  if (strcmp(pszHostname, "Opteron_8356_2") == 0 || strcmp(pszHostname, "Opteron_8356_4") == 0 || strcmp(pszHostname, "Opteron_2350_4cores") == 0 || strcmp(pszHostname, "Opteron_2350_2cores") == 0 || strcmp(pszHostname, "Opteron_2435_2nodes") == 0 || strcmp(pszHostname, "Opteron_2435_1node") == 0 || strcmp(pszHostname, "Opteron_8435") == 0 || strcmp(pszHostname, "Opteron_6164") == 0 || strcmp(pszHostname, "Xeon_X5570_woHT") == 0 || strcmp(pszHostname, "Xeon_X5570_HT") == 0 || strcmp(pszHostname, "Xeon_X5650_woHT") == 0 || strcmp(pszHostname, "Xeon_X5650_HT") == 0)
  {
    int session, cache;
    if (fSchedulerMode == 11 && fTool == 1)
    {
      for (session = 0; session < shared_caches_number; session++)
      {
        cache = session;
        
        // Create the pseudo terminal (pipes are 4K buffered and hence not really appropriate here)
        if (openpty(&(numa_pst[session][0]),&(numa_pst[session][1]), NULL, NULL, NULL) != 0)
        {
          int err_code = errno;
          sprintf (line_temp, "*** ERROR: numa openpty failed with:\n%s\n", strerror(err_code));
          log_to(LOGFILE, line_temp);
        }
        
        // Create the thread for receivening system-wide session data
        int* pi = calloc(sizeof(int), 1);
        *pi = session;
        if (pthread_create(&system_wideThread[session], 0, system_wider_dram, (void*)pi))
        {
          int err_code = errno;
          sprintf (line_temp, "*** ERROR: System-wide-dram thread spawn failed with:\n%s\n", strerror(err_code));
          log_to(LOGFILE, line_temp);
          return 0;
        }
        
        iResult = vfork();
        
        if (iResult < 0)
        {
          int err_code = errno;
          sprintf (line_temp, "*** ERROR: fork for system-wide-dram pfmon failed with:\n%s\n", strerror(err_code));
          log_to(LOGFILE, line_temp);
        }
        else if (iResult == 0)
        {
          close (numa_pst[session][0]);
          close(1); dup(numa_pst[session][1]) ; // Redirect the stdout of this process to the pseudo terminal
          
          if (fTool == 1) sprintf(line, "perf stat -a -C %i -e r8303e2", cores[cache][0]);
          
          sprintf (line_temp, "%s\n", line);
          log_to(SYSTEMWIDEFILE, line_temp);
          
          char *cmd[] = {"sh", "-c", line, NULL};
          
          // the code of a copy of parent process is being replaced by the code of the new application
          if (execvp("sh", cmd) < 0){
            int err_code = errno;
            sprintf (line_temp, "*** ERROR: exec failed for system-wide-dram pfmon with:\n%s\n", strerror(err_code));
            log_to(LOGFILE, line_temp);
          }
        }
      }
    }
    else if (fTool != 2)
    {
      for (session = 0; session < shared_caches_number*2; session++)
      {
        //usleep(WAIT_PERIOD); //!!! no wait here - otherwise the instr and misses will dissynchronize
        
        cache = floor(session/2);
        
        if (session % 2 != 0)
        {
          if (strcmp(pszHostname, "Xeon_X5570_woHT") == 0 || strcmp(pszHostname, "Xeon_X5570_HT") == 0 || strcmp(pszHostname, "Xeon_X5650_woHT") == 0 || strcmp(pszHostname, "Xeon_X5650_HT") == 0)
          {
            continue;
          }
        }
        
        // Create the pseudo terminal (pipes are 4K buffered and hence not really appropriate here)
        if (openpty(&(numa_pst[session][0]),&(numa_pst[session][1]), NULL, NULL, NULL) != 0)
        {
          int err_code = errno;
          sprintf (line_temp, "*** ERROR: numa openpty failed with:\n%s\n", strerror(err_code));
          log_to(LOGFILE, line_temp);
        }
        
        // Create the thread for receivening system-wide session data
        int* pi = calloc(sizeof(int), 1);
        *pi = session;
        if (pthread_create(&system_wideThread[session], 0, system_wider, (void*)pi))
        {
          int err_code = errno;
          sprintf (line_temp, "*** ERROR: System-wide thread spawn failed with:\n%s\n", strerror(err_code));
          log_to(LOGFILE, line_temp);
          return 0;
        }
        
        iResult = vfork();
        
        if (iResult < 0)
        {
          int err_code = errno;
          sprintf (line_temp, "*** ERROR: fork for system-wide pfmon failed with:\n%s\n", strerror(err_code));
          log_to(LOGFILE, line_temp);
        }
        else if (iResult == 0)
        {
          close (numa_pst[session][0]);
          close(1); dup(numa_pst[session][1]) ; // Redirect the stdout of this process to the pseudo terminal
          
          if (session % 2 == 0)
          {
            if (strcmp(pszHostname, "Xeon_X5570_woHT") == 0)
            {
              if (fTool == 1) sprintf(line, "perf stat -a -C %i,%i,%i,%i -e rc0 -e LLC-load-misses -e LLC-store-misses -e LLC-prefetch-misses", cores[cache][0], cores[cache][1], cores[cache][2], cores[cache][3]);
            }
            else if (strcmp(pszHostname, "Xeon_X5570_HT") == 0)
            {
              if (fTool == 1) sprintf(line, "perf stat -a -C %i,%i,%i,%i,%i,%i,%i,%i -e rc0 -e LLC-load-misses -e LLC-store-misses -e LLC-prefetch-misses", cores[cache][0], cores[cache][1], cores[cache][2], cores[cache][3], cores[cache][4], cores[cache][5], cores[cache][6], cores[cache][7]);
            }
            else if (strcmp(pszHostname, "Xeon_X5650_woHT") == 0)
            {
              if (fTool == 1) sprintf(line, "perf stat -a -C %i,%i,%i,%i,%i,%i -e rc0 -e LLC-load-misses -e LLC-store-misses -e LLC-prefetch-misses", cores[cache][0], cores[cache][1], cores[cache][2], cores[cache][3], cores[cache][4], cores[cache][5]);
            }
            else if (strcmp(pszHostname, "Xeon_X5650_HT") == 0)
            {
              if (fTool == 1) sprintf(line, "perf stat -a -C %i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i -e rc0 -e LLC-load-misses -e LLC-store-misses -e LLC-prefetch-misses", cores[cache][0], cores[cache][1], cores[cache][2], cores[cache][3], cores[cache][4], cores[cache][5], cores[cache][6], cores[cache][7], cores[cache][8], cores[cache][9], cores[cache][10], cores[cache][11]);
            }
            else if (strcmp(pszHostname, "Opteron_2435_2nodes") == 0 || strcmp(pszHostname, "Opteron_2435_1node") == 0 || strcmp(pszHostname, "Opteron_8435") == 0 || strcmp(pszHostname, "Opteron_6164") == 0)
            {
              if (fTool == 0) sprintf(line, "pfmon --system-wide --print-interval=%d --cpu-list=%i -0 -3 --events=RETIRED_INSTRUCTIONS --events=L3_CACHE_MISSES:0x07,L3_CACHE_MISSES:0x17,L3_CACHE_MISSES:0x27,L3_CACHE_MISSES:0x37 --events=L3_CACHE_MISSES:0x47,L3_CACHE_MISSES:0x57 --events=CPU_READ_COMMAND_LATENCY_TO_TARGET_NODE_0_3:0x1F,CPU_READ_COMMAND_LATENCY_TO_TARGET_NODE_0_3:0x2F,CPU_READ_COMMAND_LATENCY_TO_TARGET_NODE_0_3:0x4F,CPU_READ_COMMAND_LATENCY_TO_TARGET_NODE_0_3:0x8F  --events=CPU_READ_COMMAND_REQUESTS_TO_TARGET_NODE_0_3:0x1F,CPU_READ_COMMAND_REQUESTS_TO_TARGET_NODE_0_3:0x2F,CPU_READ_COMMAND_REQUESTS_TO_TARGET_NODE_0_3:0x4F,CPU_READ_COMMAND_REQUESTS_TO_TARGET_NODE_0_3:0x8F --events=MEMORY_CONTROLLER_REQUESTS:READ_REQUESTS,MEMORY_CONTROLLER_REQUESTS:PREFETCH_REQUESTS,MEMORY_CONTROLLER_REQUESTS:WRITE_REQUESTS,MEMORY_CONTROLLER_REQUESTS:READ_REQUESTS_WHILE_WRITES_REQUESTS --switch-timeout=1", PFMON_PERIOD, cores[cache][0]);
              else if (fTool == 1) sprintf(line, "perf stat -a -C %i -e rc0 -e r4008307e1 -e r4008317e1 -e r4008327e1 -e r4008337e1 -e r4008347e1 -e r4008357e1", cores[cache][0]);
            }
            else
            {
              if (fTool == 0) sprintf(line, "pfmon --system-wide --print-interval=%d --cpu-list=%i -0 -3 --events=RETIRED_INSTRUCTIONS --events=L3_CACHE_MISSES:0x17,L3_CACHE_MISSES:0x27,L3_CACHE_MISSES:0x47,L3_CACHE_MISSES:0x87 --events=CPU_READ_COMMAND_LATENCY_TO_TARGET_NODE_0_3:0x1F,CPU_READ_COMMAND_LATENCY_TO_TARGET_NODE_0_3:0x2F,CPU_READ_COMMAND_LATENCY_TO_TARGET_NODE_0_3:0x4F,CPU_READ_COMMAND_LATENCY_TO_TARGET_NODE_0_3:0x8F  --events=CPU_READ_COMMAND_REQUESTS_TO_TARGET_NODE_0_3:0x1F,CPU_READ_COMMAND_REQUESTS_TO_TARGET_NODE_0_3:0x2F,CPU_READ_COMMAND_REQUESTS_TO_TARGET_NODE_0_3:0x4F,CPU_READ_COMMAND_REQUESTS_TO_TARGET_NODE_0_3:0x8F --events=MEMORY_CONTROLLER_REQUESTS:READ_REQUESTS,MEMORY_CONTROLLER_REQUESTS:PREFETCH_REQUESTS,MEMORY_CONTROLLER_REQUESTS:WRITE_REQUESTS,MEMORY_CONTROLLER_REQUESTS:READ_REQUESTS_WHILE_WRITES_REQUESTS --switch-timeout=1", PFMON_PERIOD, cores[cache][0]);
              else if (fTool == 1) sprintf(line, "perf stat -a -C %i -e rc0 -e r4008317e1 -e r4008327e1 -e r4008347e1 -e r4008387e1", cores[cache][0]);
            }
          }
          else
          {
            if (strcmp(pszHostname, "Opteron_2350_2cores") == 0)
            {
              sprintf(line, "pfmon --system-wide --print-interval=%d --cpu-list=%i -0 -3 --events=RETIRED_INSTRUCTIONS", PFMON_PERIOD, cores[cache][1]);
            }
            else if (strcmp(pszHostname, "Opteron_2435_2nodes") == 0 || strcmp(pszHostname, "Opteron_2435_1node") == 0 || strcmp(pszHostname, "Opteron_8435") == 0 || strcmp(pszHostname, "Opteron_6164") == 0)
            {
              if (fTool == 0) sprintf(line, "pfmon --system-wide --print-interval=%d --cpu-list=%i,%i,%i,%i,%i -0 -3 --events=RETIRED_INSTRUCTIONS", PFMON_PERIOD, cores[cache][1], cores[cache][2], cores[cache][3], cores[cache][4], cores[cache][5]);
              else if (fTool == 1) sprintf(line, "perf stat -a -C %i,%i,%i,%i,%i -e rc0", cores[cache][1], cores[cache][2], cores[cache][3], cores[cache][4], cores[cache][5]);
            }
            else
            {
              if (fTool == 0) sprintf(line, "pfmon --system-wide --print-interval=%d --cpu-list=%i,%i,%i -0 -3 --events=RETIRED_INSTRUCTIONS", PFMON_PERIOD, cores[cache][1], cores[cache][2], cores[cache][3]);
              else if (fTool == 1) sprintf(line, "perf stat -a -C %i,%i,%i -e rc0", cores[cache][1], cores[cache][2], cores[cache][3]);
            }
          }
          
          sprintf (line_temp, "%s\n", line);
          log_to(SYSTEMWIDEFILE, line_temp);
          
          char *cmd[] = {"sh", "-c", line, NULL};
          
          // the code of a copy of parent process is being replaced by the code of the new application
          if (execvp("sh", cmd) < 0){
            int err_code = errno;
            sprintf (line_temp, "*** ERROR: exec failed for system-wide pfmon with:\n%s\n", strerror(err_code));
            log_to(LOGFILE, line_temp);
          }
        }
      }
    }
  }
  
  /*
   // starting nethogs to write traffic-generating PIDs into file (checkThreads will then read from it and update sAppRun online)
   if (DETECT_COMMUNICATION_BOUND_VIA_NETHOGS == 1)
   {
   char line2[MAX_LINE_SIZE + 5];
   
   sprintf(line2, "nethogs -d %i -o %s %s > /dev/null 2>&1", NETHOGS_PERIOD, NETHOGSFILE, NETHOGS_INTERFACE);
   
   if (system(line2) < 0)
   {
   int err_code = errno;
   sprintf (line_temp, "*** ERROR: system failed for %s:\n%s\n", line2, strerror(err_code));
   log_to(LOGFILE, line_temp);
   }
   }
   */
  
  // starting iotop thread to get the IO bound threads running on a system
  if (DETECT_IO_BOUND_VIA_IOTOP == 1)
  {
    // Create the pseudo terminal (pipes are 4K buffered and hence not really appropriate here)
    if (openpty(&(iotop_pst[0]),&(iotop_pst[1]), NULL, NULL, NULL) != 0)
    {
      int err_code = errno;
      sprintf (line_temp, "*** ERROR: iotop openpty failed with:\n%s\n", strerror(err_code));
      log_to(LOGFILE, line_temp);
    }
    
    // Create the thread for receivening top data
    if (pthread_create(&iotopThread, 0, iotopper, 0))
    {
      int err_code = errno;
      sprintf (line_temp, "*** ERROR: IOtop thread spawn failed with:\n%s\n", strerror(err_code));
      log_to(LOGFILE, line_temp);
      return 0;
    }
    
    iResult = vfork();
    
    if (iResult < 0)
    {
      int err_code = errno;
      sprintf (line_temp, "*** ERROR: fork for iotop failed with:\n%s\n", strerror(err_code));
      log_to(LOGFILE, line_temp);
    }
    else if (iResult == 0)
    {
      close (iotop_pst[0]);
      close(1); dup(iotop_pst[1]) ; // Redirect the stdout of this process to the pseudo terminal
      
      // IO WAIT is better detected without P, because some thread might wait a lot, but with P it will be not noticable (averaged per all threads?).
      sprintf(line, "iotop -obk -qqq -d %i", PERIOD_IN_SEC);
      
      sprintf (line_temp, "%s\n", line);
      log_to(SYSTEMWIDEFILE, line_temp);
      
      char *cmd[] = {"sh", "-c", line, NULL};
      
      // the code of a copy of parent process is being replaced by the code of the new application
      if (execvp("sh", cmd) < 0){
        int err_code = errno;
        sprintf (line_temp, "*** ERROR: exec failed for iotop with:\n%s\n", strerror(err_code));
        log_to(LOGFILE, line_temp);
      }
    }
  }
  
  
  int iA2, temp_class, temp_comm_snt, temp_comm_rcvd;
  int oldCore, newCore, new_mask, old_mask;
  
  struct bitmask *fromnodes;
  struct bitmask *tonodes;
  
  char* fromnodes_str = calloc(32, 1);
  char* tonodes_str = calloc(32, 1);
  
  if (MIGRATION_TYPE == 2) launchIBSer();
  
  int mode = 1, workload_has_changed, tmp_workload_has_changed;
  
  if (strcmp(pszHostname, "Xeon_X5365_4") == 0 || strcmp(pszHostname, "Xeon_E5320") == 0)
  {
    if (fSchedulerMode == 4 || fSchedulerMode == 8 || fSchedulerMode == 1 || fSchedulerMode == 0 || fSchedulerMode == 9 || fSchedulerMode == 10 || fSchedulerMode == 11)
    {
      for (iA = 0; iA < iRunAppCount; iA++)
      {
        // Create the pseudo terminal (pipes are 4K buffered and hence not really appropriate here)
        if (openpty(&(sAppRun[iA].mypst[0]),&(sAppRun[iA].mypst[1]), NULL, NULL, NULL) != 0)
        {
          int err_code = errno;
          sprintf (line_temp, "*** ERROR: openpty failed for app %i with:\n%s\n", iA, strerror(err_code));
          log_to(LOGFILE, line_temp);
        }
        
        // Create the thread for online sampling - it will update solo miss rate in real time
        int* pi = calloc(sizeof(int), 1);
        *pi = iA;
        if (pthread_create(&onlineThread[iA], 0, onliner, (void*)pi))
        {
          int err_code = errno;
          sprintf (line_temp, "*** ERROR: Online thread spawn failed with:\n%s\n", strerror(err_code));
          log_to(LOGFILE, line_temp);
          return 0;
        }
      }
    }
  }
  
  int count_put_to_sleep = 0;
  
  while (1)
  {
    // starting top thread to get the compute bound threads running on a system
    if (DETECT_ONLINE == 1 && topper_restart == 1)
    {
      // Create the pseudo terminal (pipes are 4K buffered and hence not really appropriate here)
      if (openpty(&(top_pst[0]),&(top_pst[1]), NULL, NULL, NULL) != 0)
      {
        int err_code = errno;
        sprintf (line_temp, "*** ERROR: top openpty failed with:\n%s\n", strerror(err_code));
        log_to(LOGFILE, line_temp);
      }
      
      // Create the thread for receivening top data
      if (pthread_create(&topThread, 0, topper, 0))
      {
        int err_code = errno;
        sprintf (line_temp, "*** ERROR: Top thread spawn failed with:\n%s\n", strerror(err_code));
        log_to(LOGFILE, line_temp);
        return 0;
      }
      
      iResult = vfork();
      
      if (iResult < 0)
      {
        int err_code = errno;
        sprintf (line_temp, "*** ERROR: fork for top failed with:\n%s\n", strerror(err_code));
        log_to(LOGFILE, line_temp);
      }
      else if (iResult == 0)
      {
        close (top_pst[0]);
        close(1); dup(top_pst[1]) ; // Redirect the stdout of this process to the pseudo terminal
        
        sprintf(line, "top -bH -d %i", PERIOD_IN_SEC);
        
        sprintf (line_temp, "%s\n", line);
        log_to(SYSTEMWIDEFILE, line_temp);
        
        char *cmd[] = {"sh", "-c", line, NULL};
        
        // the code of a copy of parent process is being replaced by the code of the new application
        if (execvp("sh", cmd) < 0){
          int err_code = errno;
          sprintf (line_temp, "*** ERROR: exec failed for top with:\n%s\n", strerror(err_code));
          log_to(LOGFILE, line_temp);
        }
      }
      
      topper_restart = 0;
    }
    
    
    if (DETECT_COMM_CONTENTION_VIA_NETSTAT == 1){
      char * col = calloc(64, 1);
      char * vtid_str_end;
      int cur_ctid, recv_q, send_q, vtid, myfileno;
      char vtid_str[MAX_LINE_SIZE+5];
      FILE * vzlist_file;
      FILE * netstat_file;
      char netstat_cmd[MAX_LINE_SIZE+5];
      char netstat_line[MAX_LINE_SIZE+5];
      char vzlist_line[MAX_LINE_SIZE+5];

          sprintf (netstat_cmd, "timeout 1 ssh localhost \"netstat -np | sed -e '1,/Proto Recv-Q/d' | sed -e '/Active UNIX domain sockets/,\\$d' | awk '{print \\$2\\\" \\\"\\$3\\\" \\\"\\$7}'\"");
          netstat_file = popen(netstat_cmd, "r");
          if (netstat_file == NULL) {
            int err_code = errno;
            printf ("*** ERROR: Failed to run command: %s\n%s\n", netstat_cmd, strerror(err_code));
            sprintf (line_temp, "*** ERROR: Failed to run command: %s\n%s\n", netstat_cmd, strerror(err_code));
            log_to(LOGFILE, line_temp);
          }
          
          cur_ctid = 0;
          while (fgets(netstat_line, sizeof(netstat_line), netstat_file) != NULL) {
            col = simply_get_col(netstat_line, 2);
            char *vtid_str_end = strstr(col, "/");
            if (vtid_str_end != NULL){
              strncpy(vtid_str, col, vtid_str_end-col);
              vtid_str[vtid_str_end-col] = '\0';
              vtid = atol(vtid_str);
              
              if (vtid > 0){
                for (iA=0; iA<iRunAppCount; iA++){
                  if (sAppRun[iA].iPID <= 0) continue;
                  for (i=0; i<MAX_THREAD_NUMBER; i++){
                    if (sAppRun[iA].aiTIDs[i] <= 0) continue;
                    if (sAppRun[iA].containerIDs[i] == cur_ctid && sAppRun[iA].aiVTIDs[i] == vtid)
                    {
                      col = simply_get_col(netstat_line, 0);
                      sAppRun[iA].aiRecv_q[i] = atol(col);
                      col = simply_get_col(netstat_line, 1);
                      sAppRun[iA].aiSend_q[i] = atol(col);

                    }
                  }
                }
              }
            }
          }
          myfileno = fileno(netstat_file);
          if (myfileno > 0){
            close(myfileno);
            // hangs here randomly:
            //if (netstat_file != NULL) pclose(netstat_file);
          }

    
    
    
    
    
    
      vzlist_file = popen("timeout 1 vzlist -H -o ctid", "r");
      if (vzlist_file == NULL) {
        int err_code = errno;
        sprintf (line_temp, "*** ERROR: Failed to run command: timeout 5 vzlist -H -o ctid\n%s\n", strerror(err_code));
        log_to(LOGFILE, line_temp);
      }
      while (fgets(vzlist_line, sizeof(vzlist_line), vzlist_file) != NULL) {
        col = simply_get_col(vzlist_line, 0);
        cur_ctid = atol(col);
        if (cur_ctid > 100){
          sprintf (netstat_cmd, "timeout 1 ssh vz%d \"netstat -np | sed -e '1,/Proto Recv-Q/d' | sed -e '/Active UNIX domain sockets/,\\$d' | awk '{print \\$2\\\" \\\"\\$3\\\" \\\"\\$7}'\"", cur_ctid);
          netstat_file = popen(netstat_cmd, "r");
          if (netstat_file == NULL) {
            int err_code = errno;
            printf ("*** ERROR: Failed to run command: %s\n%s\n", netstat_cmd, strerror(err_code));
            sprintf (line_temp, "*** ERROR: Failed to run command: %s\n%s\n", netstat_cmd, strerror(err_code));
            log_to(LOGFILE, line_temp);
          }
          while (fgets(netstat_line, sizeof(netstat_line), netstat_file) != NULL) {
            col = simply_get_col(netstat_line, 2);
            char *vtid_str_end = strstr(col, "/");
            if (vtid_str_end != NULL){
              strncpy(vtid_str, col, vtid_str_end-col);
              vtid_str[vtid_str_end-col] = '\0';
              vtid = atol(vtid_str);
              
              if (vtid > 0){
                for (iA=0; iA<iRunAppCount; iA++){
                  if (sAppRun[iA].iPID <= 0) continue;
                  for (i=0; i<MAX_THREAD_NUMBER; i++){
                    if (sAppRun[iA].aiTIDs[i] <= 0) continue;
                    
                    if (sAppRun[iA].containerIDs[i] == cur_ctid && sAppRun[iA].aiVTIDs[i] == vtid)
                    {
                      col = simply_get_col(netstat_line, 0);
                      sAppRun[iA].aiRecv_q[i] = atol(col);
                      col = simply_get_col(netstat_line, 1);
                      sAppRun[iA].aiSend_q[i] = atol(col);
                    }
                  }
                }
              }
            }
          }
          myfileno = fileno(netstat_file);
          if (myfileno > 0){
            close(myfileno);
            // hangs here randomly:
            //if (netstat_file != NULL) pclose(netstat_file);
          }
        }
      }
      myfileno = fileno(vzlist_file);
      if (myfileno > 0){
        close(myfileno);
        // hangs here randomly:
        //printf("close 3 middle\n");
        //if (vzlist_file != NULL) pclose(vzlist_file);
      }
    }

    // start all benchmarks according to iTimeToStart
    for (iA = 0; iA < iRunAppCount; iA++)
    {
      if (time(0) >= sAppRun[iA].iTimeToStart && DETECT_ONLINE == 0) sleeper(iA);
      
      // put to sleep after a minute
      if (   fSchedulerMode == 10   &&   time(0) >= (sAppRun[iA].iTimeToStart+60)   &&   sAppRun[iA].put_to_sleep == 0   )
      {
        //save missrates from the last 10 intervals
        for(k = 0; k < 10; k++){
          sAppRun[iA].aldOldAppsSignatures[0][k] = sAppRun[iA].aldAppsSignatures[0][k];
        }
        
        kill(sAppRun[iA].iPID, 19);
        printf ("%i: iA == %i put to sleep\n", mould_time, iA);
        
        sAppRun[iA].put_to_sleep = 1;
        count_put_to_sleep++;
      }
      
      workload_has_changed = 0;
      if (sAppRun[iA].iPID > 0)
      {
        // every tick we check the threads of this application and perform the following:
        // if there are new threads in the system then we add their TIDs to the aiTIDs array and perform regular assignment for these new threads
        // if some threads have completed execution during the last tick then we change the value in the aiTIDs array from TID to -TID and perform decrementation of the cache counters for this thread
        // if the main thread of the application (PID == TID) has completed execution during the last tick then we restart this application (because the end of the main thread means that all threads of this multithreaded application will be also terminated)
        tmp_workload_has_changed = checkThreads(iA);
        
        if (sAppRun[iA].to_schedule[0] > 0) sAppRun[iA].active_intervals++;
        
        if (workload_has_changed == 0) workload_has_changed = tmp_workload_has_changed;
      }
    }
    
    if (fSchedulerMode == 10 && count_put_to_sleep == iRunAppCount)
    {
      queueScheduler();
    }
    
    if ( (fSchedulerMode == 3 || fSchedulerMode == 4 || fSchedulerMode == 7 || fSchedulerMode == 8 || fSchedulerMode == 9)/* && workload_has_changed == 1*/)
    {
      distributedIntensity(workload_has_changed);
    }
    
    if (fSchedulerMode == 11)
    {
      swapToEvenDRAMMisses();
      
      sprintf (line_temp, "EventSelect 0E2h Memory Controller DRAM Command Slots Missed (cpu_id 0): %ld\n", chip_wide_counters[0][0]);
      log_to(SYSTEMWIDEFILE, line_temp);
      chip_wide_counters[0][0] = 0;
      
      sprintf (line_temp, "EventSelect 0E2h Memory Controller DRAM Command Slots Missed (cpu_id 1): %ld\n", chip_wide_counters[1][0]);
      log_to(SYSTEMWIDEFILE, line_temp);
      chip_wide_counters[1][0] = 0;
      
      sprintf (line_temp, "EventSelect 0E2h Memory Controller DRAM Command Slots Missed (cpu_id 2): %ld\n", chip_wide_counters[2][0]);
      log_to(SYSTEMWIDEFILE, line_temp);
      chip_wide_counters[2][0] = 0;
      
      sprintf (line_temp, "EventSelect 0E2h Memory Controller DRAM Command Slots Missed (cpu_id 3): %ld\n", chip_wide_counters[3][0]);
      log_to(SYSTEMWIDEFILE, line_temp);
      chip_wide_counters[3][0] = 0;
    }
    
    if (time(0) >= iTimeToDie || all_finished_3_times == 1)
    {
      last_time = 1;
    }
    
    output_assignment();
    
    if (TRUNCATE_RESOURCE_VECTOR == 1)
    {
      tempfile = fopen (CLUSTERREPORTFILE, "w");
      fclose (tempfile);
    }

    for (iA=0; iA<iRunAppCount; iA++) // keep it as a separate "for" loop so that report is generated without delays
    {
      if (sAppRun[iA].iPID > 0 && GENERATE_RESOURCE_VECTOR == 1)
        //if (sAppRun[iA].iPID > 0 && ADD_VM_NAME_TO_RESOURCE_VECTOR == 1 && GENERATE_RESOURCE_VECTOR == 1 && get_vm_name(sAppRun[iA].iPID) == NULL)
      {
        if (ADD_VM_NAME_TO_RESOURCE_VECTOR == 1)
          sprintf(vm_name_str, " %s\0", get_vm_name(sAppRun[iA].iPID));
        
        int   aiTotalCPUutil = 0;
        
        long double aldTotalAppsSignatures = 0;
        
        long double aldTotalOnlineDiskWrite = 0;
        long double aldTotalOnlineDiskRead = 0;
        long double aldMaxOnlineDiskWait = 0;
        
        long double aldTotalOnlineTrafficSent = 0;
        long double aldTotalOnlineTrafficReceived = 0;
        
        int aiTotalRecv_q = 0;
        int aiTotalSend_q = 0;

        first_thread_id = -1;
        for (i=0; i<MAX_THREAD_NUMBER; i++)
        {
          if (sAppRun[iA].iPID > 0 && sAppRun[iA].aiTIDs[i] > 0)
          {
            first_thread_id = i;
            
            aiTotalCPUutil += sAppRun[iA].aiCPUutil[i];
            
            if (sAppRun[iA].aldAppsSignatures[i][0] > 0) aldTotalAppsSignatures += sAppRun[iA].aldAppsSignatures[i][0];
            
            aldTotalOnlineTrafficSent += sAppRun[iA].aldOnlineTrafficSent[i];
            aldTotalOnlineTrafficReceived += sAppRun[iA].aldOnlineTrafficReceived[i];
            
            aldTotalOnlineDiskWrite += sAppRun[iA].aldOnlineDiskWrite[i];
            aldTotalOnlineDiskRead += sAppRun[iA].aldOnlineDiskRead[i];
            
            if (sAppRun[iA].aldOnlineDiskWait[i] > aldMaxOnlineDiskWait) aldMaxOnlineDiskWait = sAppRun[iA].aldOnlineDiskWait[i];
            
            aiTotalRecv_q += sAppRun[iA].aiRecv_q[i];
            aiTotalSend_q += sAppRun[iA].aiSend_q[i];
          }
        }
        
        struct tm *local;
        time_t t;
        
        t = time(NULL);
        local = localtime(&t);
        char * mytime = asctime(local);
        mytime[strlen(mytime)-1] = '\0';
        
        if (first_thread_id >= 0)
        {
          sprintf (line_temp, "%d (%s): %d %s%s %d  CPU %d  MISS RATE %Lf ac_core: %d i-core %i, i-node %i MEM %Lf  TRFC SNT %Lf RCVD %Lf  IO WR %Lf RD %Lf (%d) IO WAIT %Lf NETSTAT RECVQ %d SENDQ %d\n",
                   mould_time, mytime, sAppRun[iA].iPID, sAppRun[iA].pszName, vm_name_str, sAppRun[iA].containerIDs[first_thread_id], aiTotalCPUutil,
                   aldTotalAppsSignatures,  sAppRun[iA].aiActualCores[first_thread_id], sAppRun[iA].interCoreMigrations[first_thread_id],
                   sAppRun[iA].interNodeMigrations[first_thread_id], sAppRun[iA].aiMemFootprint[first_thread_id], aldTotalOnlineTrafficSent, aldTotalOnlineTrafficReceived,
                   aldTotalOnlineDiskWrite, aldTotalOnlineDiskRead, t, aldMaxOnlineDiskWait, aiTotalRecv_q, aiTotalSend_q);
          log_to(CLUSTERREPORTFILE, line_temp);
        }
      }
    }

    // save what applications are executing at what caches atm
    for (iA=0; iA<iRunAppCount; iA++)
    {
      for (i=0; i<MAX_THREAD_NUMBER; i++)
      {
        if (sAppRun[iA].iPID > 0 && sAppRun[iA].aiTIDs[i] > 0)
        {
          for(k = 8; k >= 0; k--){
            sAppRun[iA].aldAppsSignatures[i][k+1] = sAppRun[iA].aldAppsSignatures[i][k];
          }
          
          //sprintf (line_temp, "before %i: %s sAppRun[%i].aiTIDs[%i] (#%i): REAL MISS RATE: %Lf    %Lf    %Lf\n", mould_time, sAppRun[iA].pszName, iA, i, sAppRun[iA].iCount, sAppRun[iA].aldAppsSignatures[i][0], sAppRun[iA].aldActualOnlineMisses[i], sAppRun[iA].aldActualOnlineInstructions[i]);
          //log_to(ONLINEFILE, line_temp);
          
          if (sAppRun[iA].aldActualOnlineMisses[i] > 0 && sAppRun[iA].aldActualOnlineInstructions[i] > 0 && sAppRun[iA].to_schedule[i] > 0)
          {
            // calculating new miss rate
            sAppRun[iA].aldAppsSignatures[i][0] = 10000 * sAppRun[iA].aldActualOnlineMisses[i] / sAppRun[iA].aldActualOnlineInstructions[i];
            
            sprintf (line_temp, "%i: %s sAppRun[%i].aiTIDs[%i]==%i (#%i): REAL MISS RATE: %Lf    %Lf    %Lf\n", mould_time, sAppRun[iA].pszName, iA, i, sAppRun[iA].aiTIDs[i], sAppRun[iA].iCount, sAppRun[iA].aldAppsSignatures[i][0], sAppRun[iA].aldActualOnlineMisses[i], sAppRun[iA].aldActualOnlineInstructions[i]);
            log_to(ONLINEFILE, line_temp);
            
            sAppRun[iA].aldOldOnlineMisses[i] = sAppRun[iA].aldActualOnlineMisses[i];
            sAppRun[iA].aldOldOnlineInstructions[i] = sAppRun[iA].aldActualOnlineInstructions[i];
            
            sAppRun[iA].aldActualOnlineMisses[i] = -1;
            sAppRun[iA].aldActualOnlineInstructions[i] = -1;
          }
          else
          {
            sAppRun[iA].aldAppsSignatures[i][0] = -1;
          }
        }
        
        
        if (sAppRun[iA].aiActualCores[i] != sAppRun[iA].aiOldCores[i] || sAppRun[iA].aiMouldIntervals[i] >= 100 || last_time == 1)
        {
          sAppRun[iA].aiMouldIntervals[i] = 0;
          
          // for dumping the total counters from system-wide session
          if (sAppRun[iA].aiActualCores[i] >= 0 && sAppRun[iA].aiOldCores[i] >=0 ) sAppRun[iA].aiCoresLeftFrom[i] = sAppRun[iA].aiOldCores[i];
        }
        
        if (sAppRun[iA].aiActualCores[i] != sAppRun[iA].aiOldCores[i])
        {
          sAppRun[iA].aiOldCores[i] = sAppRun[iA].aiActualCores[i];
        }
        
        
        if (strcmp(pszHostname, "Opteron_8356_2") == 0 || strcmp(pszHostname, "Opteron_8356_4") == 0 || strcmp(pszHostname, "Opteron_2350_4cores") == 0 || strcmp(pszHostname, "Opteron_2350_2cores") == 0 || strcmp(pszHostname, "Opteron_2435_2nodes") == 0 || strcmp(pszHostname, "Opteron_2435_1node") == 0 || strcmp(pszHostname, "Opteron_8435") == 0 || strcmp(pszHostname, "Opteron_6164") == 0 || strcmp(pszHostname, "Xeon_X5570_woHT") == 0 || strcmp(pszHostname, "Xeon_X5570_HT") == 0 || strcmp(pszHostname, "Xeon_X5650_woHT") == 0 || strcmp(pszHostname, "Xeon_X5650_HT") == 0)
        {
          if (sAppRun[iA].aiTIDs[i] > 0)
          {
            if (LARGE_MEMORY_WORKLOAD == 0)
            {
              if (MPI_WORKLOAD == 1 && iA <= MAX_MPI_APP) sprintf(statline_numa, "/proc/%i/task/%i/numa_maps", sAppRun[iA].aiTIDs[i], sAppRun[iA].aiTIDs[i]);
              else sprintf(statline_numa, "/proc/%i/task/%i/numa_maps", sAppRun[iA].iPID, sAppRun[iA].aiTIDs[i]);
              
              procfile = fopen(statline_numa, "r");
              if(procfile==NULL) {
                sprintf (line_temp, "*** WARNING: can't open numa_maps file %s.\n", statline_numa);
                log_to(LOGFILE, line_temp);
                continue;
              }
              
              numa_maps_count=0;
              
              // we need to do it asap, otherwise there may be segmentation fault in fgets when numa_maps will suddenly be deleted on thread's termination
              while (numa_maps_count < 512 && fgets(numa_maps_text[numa_maps_count], sizeof(numa_maps_text[numa_maps_count]), procfile))
              {
                numa_maps_count++;
              }
              fclose (procfile);
              
              for (j=0; j<MAX_NUMA_NODES; j++){
                numa_node_pages[j] = 0;
              }
              
              for (k=0; k<numa_maps_count; k++)
              {
                for (j=0; j<MAX_NUMA_NODES; j++){
                  sprintf(line_temp, "N%d=\0", j);
                  str_begin = strstr(numa_maps_text[k],line_temp);
                  if (str_begin != NULL)
                  {
                    str_end = strstr(str_begin," ");
                    if (str_end == NULL) str_end = strstr(str_begin,"\n");
                    
                    strncpy(line_temp, str_begin+3, str_end - str_begin-3);
                    line_temp[str_end - str_begin-3] = '\0';
                    
                    numa_node_pages[j] += atoi(line_temp);
                  }
                }
              }
            }
            
            int actualNode = maskToCache(cpuToMask(sAppRun[iA].aiActualCores[i]));
            // repeat memory migration if not all of the memory has migrated in the past (it could happen that chunks of memory were left on the old node)
            if (MIGRATION_TYPE == 1 && sAppRun[iA].aldAppsSignatures[i][0] > DEVIL_HORIZON)
            {
              for (j=0; j<shared_caches_number; j++){
                if (   ++sAppRun[iA].aiBounced[i] <= BOUNCING_LIMIT &&   
                    (   LARGE_MEMORY_WORKLOAD == 1   ||   (j != actualNode   &&   (0.1 * sAppRun[iA].aiActualNodePages[i][actualNode] < sAppRun[iA].aiActualNodePages[i][j]))   )
                    )
                {
                  if (migrate_memory(sAppRun[iA].aiTIDs[i], j, actualNode) == 0)
                  {
                    sprintf (line_temp, "%s sAppRun[%i].aiTIDs[%i] == %i: memory successfully migrated as well from node %i to node %i (additional migration).\n", sAppRun[iA].pszName, iA, i, sAppRun[iA].aiTIDs[i], j, actualNode);
                    log_to(LOGFILE, line_temp);
                  }
                }
              }
            }
          }
        }
        
        sAppRun[iA].aiMouldIntervals[i]++;
        sAppRun[iA].aiNUMAIntervals[i]++;
        
        sAppRun[iA].aldOldOnlineMisses[i] = 0;
        sAppRun[iA].aldOldOnlineInstructions[i] = 0;
      }
    }

    usleep(REFRESH_PERIOD);
    
    if (DETECT_COMMUNICATION_BOUND_VIA_NETHOGS == 1)
    {
      updateTraffic();
    }
    
    if (last_time == 1) break;
    
    mould_time++;
  }
  
  
  char buffer[64];
  buffer[0] = '\0';
  strncat(buffer, argv[0], 64);
  for (i = 1; i < argc; i++)
  {
    strncat(buffer, " \0", 64);
    strncat(buffer, argv[i], 64);
  }
  
  sprintf (line_temp, "---------------------------------------------\n");
  log_to(LOGFILE, line_temp);
  sprintf (line_temp, "Invocation string: %s\n", buffer);
  log_to(LOGFILE, line_temp);
  sprintf (line_temp, "---------------------------------------------\n");
  log_to(LOGFILE, line_temp);
  sprintf (line_temp, "perc: %f, REFRESH_PERIOD: %d\n", devilPerc, REFRESH_PERIOD);
  log_to(LOGFILE, line_temp);
  sprintf (line_temp, "---------------------------------------------\n");
  log_to(LOGFILE, line_temp);
  sprintf (line_temp, "MIGRATION_TYPE: %d, PAGES: %d, BACKWARDS: %d, RANDOM: %d, IBS_INTERVAL: %d, MIGRATE_WHOLE_MEMORY_TYPE: %d\n", MIGRATION_TYPE, PAGES, BACKWARDS, RANDOM, IBS_INTERVAL, MIGRATE_WHOLE_MEMORY_TYPE);
  log_to(LOGFILE, line_temp);
  sprintf (line_temp, "---------------------------------------------\n");
  log_to(LOGFILE, line_temp);
  for (iA=0; iA<iRunAppCount; iA++)
  {
    for (i=0; i<MAX_THREAD_NUMBER; i++)
    {
      if (sAppRun[iA].interNodeMigrations[i] > 0 || sAppRun[iA].interCoreMigrations[i] > 0)
      {
        sprintf (line_temp, "%s sAppRun[%i].aiTIDs[%i] (#%i): internode: %d, intercore: %d\n", sAppRun[iA].pszName, iA, i, sAppRun[iA].iCount, sAppRun[iA].interNodeMigrations[i], sAppRun[iA].interCoreMigrations[i]);
        log_to(LOGFILE, line_temp);
      }
    }
  }
  
  sprintf (line_temp, "---------------------------------------------\n");
  log_to(LOGFILE, line_temp);
  
  for (iA=0; iA<iRunAppCount; iA++)
  {
    for (i=0; i<MAX_THREAD_NUMBER; i++)
    {
      if (sAppRun[iA].pages_migrated[i] > 0)
      {
        sprintf (line_temp, "%s sAppRun[%i].aiTIDs[%i] (#%i): total number of pages migrated: %d\n", sAppRun[iA].pszName, iA, i, sAppRun[iA].iCount, sAppRun[iA].pages_migrated[i]);
        log_to(LOGFILE, line_temp);
      }
    }
  }
  sprintf (line_temp, "---------------------------------------------\n");
  log_to(LOGFILE, line_temp);
  
  int num, mean;
  for (iA=0; iA<iRunAppCount; iA++)
  {
    sprintf (line_temp, "\n%s (active_intervals: %d)\n", sAppRun[iA].pszName, sAppRun[iA].active_intervals);
    log_to(LOGFILE, line_temp);
    mean = 0; num = 0;
    for (i = 0; i < MAX_RUN_TIMES; i++){
      if (sAppRun[iA].aiExecTimes[i] > 0 && sAppRun[iA].aiExecTimes[i] < 100000){
        sprintf (line_temp, "%i ", sAppRun[iA].aiExecTimes[i]);
        log_to(LOGFILE, line_temp);
        mean += sAppRun[iA].aiExecTimes[i];
        num++;
      }
    }
    //sprintf (line_temp, "| total: %i\n", mean);
    //log_to(LOGFILE, line_temp);
    //if (num != 0) mean = mean / num;
    //sprintf (line_temp, "%i out of %i runs\n", mean, num);
    //log_to(LOGFILE, line_temp);
  }
  
  // output the chip-wide counters
  for(i = 0; i < MAX_CHIP_ID; i++){
    sprintf (line_temp, "Beginning data output for chip %d...\n", i);
    log_to(SYSTEMWIDEFILE, line_temp);
    for(j = 0; j < 20; j++){
      sprintf (line_temp, "  %ld  ", chip_wide_counters[i][j]);
      log_to(SYSTEMWIDEFILE, line_temp);
      chip_wide_counters[i][j] = 0;
    }
    sprintf (line_temp, "\n");
    log_to(SYSTEMWIDEFILE, line_temp);
  }
  
  
  // killing all the child processes
  pid_t grp = getpgrp();
  if (!killpg (grp, 9))
  {
    sprintf (line_temp, "Child processes are successfully terminated.\n");
    log_to(LOGFILE, line_temp);
  }
  else
  {
    sprintf (line_temp, "WARNING: There was smth wrong with the termination of the child processes.\n");
    log_to(LOGFILE, line_temp);
  }
  
  sprintf (line_temp, "==========\nMain thread finished.\n==========\n");
  log_to(LOGFILE, line_temp);
  
  return 0;
}



//=============================================================



// respawns <benchmark iA>-th application until <timeout> expires
int sleeper (int iA)
{
  short int iA2, temp_fin;
  int i;
  int temp_pid, tempest_pid;
  int     iResult, child_levels;
  char    line[MAX_LINE_SIZE + 5];
  
  // each time when the application finishes its current run a child_term_handler(int sig) has been invoked and set sAppRun[iA].iPID = -1
  // for a multithreaded application that means the end of the main (first) thread for which iPID == iTID, since with the end of the main thread all other threads will be terminated
  lock(&(sAppRun[iA].lock));
  unlock(&(sAppRun[iA].lock));
  if (sAppRun[iA].iPID == -1){
    temp_fin = 1;
    for (iA2 = 0; iA2 < iRunAppCount; iA2++)
    {
      if (sAppRun[iA2].iCount < SLEEPER_RESTART /* in order to finish three times the aplication needs to be restarted at
       least four times*/)
      {
        temp_fin = 0;
      }
    }
    
    if (temp_fin == 1)
    {
      all_finished_3_times = 1;
    }
    else
    {
      if (iRestart == 1 || sAppRun[iA].iCount == 0)
      {
        temp_pid = launchApp(iA); // get the pid of the sh
        
        // search for the second smallest pid with binary name of (apache2)
        if (LAMP_WORKLOAD == 1 && iA == 1)
        {
          usleep(WAIT_PERIOD);
          usleep(WAIT_PERIOD);
          usleep(WAIT_PERIOD);
          usleep(WAIT_PERIOD);
          
          temp_pid = -1;
          
          // now we need to find the process itself
          for (i=1; i<100000; i++){
            if (get_binary_name(i) != NULL && strcmp(get_binary_name(i), "(apache2)") == 0)
            {
              temp_pid = i;
              printf ("Found first sleep apache2 temp_pid: %d.\n", temp_pid);
              break;
            }
          }
        }
        
        // Step 1. Obtain the immidiate child of temp_pid: either the child or temp_pid itself will be time. In the next step we will determine which one is time.
        i = 0; tempest_pid = -1;
        while(tempest_pid <= 0)
        {
          usleep(WAIT_PERIOD);
          
          if (LAMP_WORKLOAD == 1 && iA == 1) tempest_pid = get_child_id(temp_pid, 1);
          else tempest_pid = get_child_id(temp_pid, 0);
          
          if (i++ > 10)
          {
            sprintf (line_temp, "%s sAppRun[%i] (#%i): ERROR: cannot get the child of temp_pid = %d so smth is wrong! Exiting...\n", sAppRun[iA].pszName, iA, sAppRun[iA].iCount, temp_pid);
            log_to(LOGFILE, line_temp);
            printf ("%s", line_temp);
            
            all_finished_3_times = 1;
            break;
          }
        }
        if (all_finished_3_times == 1) return 1;
        
        // Step 2. Waiting until either temp_pid (like in PARSEC) or its child (like in SPEC) turns into "time" - there could be no other option.
        i = 0;
        while (strcmp(get_binary_name(temp_pid), "(mysqld_safe)") != 0 && strcmp(get_binary_name(tempest_pid), "(mysqld_safe)") != 0 && strcmp(get_binary_name(temp_pid), "(apache2)") != 0 && strcmp(get_binary_name(tempest_pid), "(apache2)") != 0 && strcmp(get_binary_name(temp_pid), "(time)") != 0 && strcmp(get_binary_name(tempest_pid), "(time)") != 0)
        {
          usleep(WAIT_PERIOD);
          if (i++ > 10)
          {
            sprintf (line_temp, "%s sAppRun[%i] (#%i): ERROR: neither temp_pid = %d nor its child cannot turn into time! Exiting...\n", sAppRun[iA].pszName, iA, sAppRun[iA].iCount, temp_pid);
            log_to(LOGFILE, line_temp);
            printf ("%s", line_temp);
            
            all_finished_3_times = 1;
            break;
          }
        }
        if (all_finished_3_times == 1) return 1;
        
        if (strcmp(get_binary_name(tempest_pid), "(apache2)") == 0)
        {
          child_levels = 0;
          temp_pid = tempest_pid;
        }
        else
        {
          if (strcmp(get_binary_name(tempest_pid), "(mysqld_safe)") == 0 || strcmp(get_binary_name(tempest_pid), "(apache2)") == 0 || strcmp(get_binary_name(tempest_pid), "(time)") == 0) child_levels = 1;
          else child_levels = 0; // without sh in the beginning
          if (strcmp(pszHostname, "Opteron_8356_2") == 0 || strcmp(pszHostname, "Opteron_8356_4") == 0 || strcmp(pszHostname, "Opteron_2350_4cores") == 0 || strcmp(pszHostname, "Opteron_2350_2cores") == 0 || strcmp(pszHostname, "Xeon_E5320") == 0 || strcmp(pszHostname, "Opteron_2435_2nodes") == 0 || strcmp(pszHostname, "Opteron_2435_1node") == 0 || strcmp(pszHostname, "Opteron_8435") == 0 || strcmp(pszHostname, "Opteron_6164") == 0 || strcmp(pszHostname, "Xeon_X5570_woHT") == 0 || strcmp(pszHostname, "Xeon_X5570_HT") == 0 || strcmp(pszHostname, "Xeon_X5650_woHT") == 0 || strcmp(pszHostname, "Xeon_X5650_HT") == 0 ||
              fSchedulerMode == 4 || fSchedulerMode == 0 || fSchedulerMode == 7 || fSchedulerMode == 8 || fSchedulerMode == 9 || fSchedulerMode == 11) child_levels += 1; // without pfmon
          else child_levels += 2; // with pfmon
        }
        
        // here DON'T use get_binary_name (see below) !!! execvp will not be finished by the time get_binary_name() will be invoked for a child and as such it will return "(sh)" instead of, say, "                "
        // instead keep looking for the childs until the last one is found
        i = 0;
        while(child_levels != 0)
        {
          tempest_pid = get_child_id(temp_pid, 0);
          if (tempest_pid > 0)
          {
            child_levels--;
            temp_pid = tempest_pid;
          }
          
          usleep(WAIT_PERIOD);
          if (i++ > 10)
          {
            sprintf (line_temp, "%s sAppRun[%i] (#%i): ERROR: waiting for temp_pid = %d to turn into smth! Desperately... Exiting!\n", sAppRun[iA].pszName, iA, sAppRun[iA].iCount, temp_pid);
            log_to(LOGFILE, line_temp);
            printf ("%s", line_temp);
            
            all_finished_3_times = 1;
            break;
          }
          i++;
        }
        if (all_finished_3_times == 1) return 1;
        
        lock(&(sAppRun[iA].lock));
        sAppRun[iA].iPID = temp_pid;
        sAppRun[iA].iCount++;
        
        sprintf (line_temp, "%s sAppRun[%i] (#%i): launched %i-th time (pid = %i)\n", sAppRun[iA].pszName, iA, sAppRun[iA].iCount, sAppRun[iA].iCount, temp_pid);
        log_to(LOGFILE, line_temp);
        unlock(&(sAppRun[iA].lock));
        
        for (i=0; i<MAX_THREAD_NUMBER; i++)
        {
          sAppRun[iA].aiTIDs[i] = -1;
        }
        
        sAppRun[iA].aiExecTimes[sAppRun[iA].iCount-1] = time(0);
        
        
        if (strcmp(pszHostname, "Xeon_X5365_4") == 0 || strcmp(pszHostname, "Xeon_E5320") == 0)
        {
          if (fSchedulerMode == 4 || fSchedulerMode == 8 || fSchedulerMode == 1 || fSchedulerMode == 0 || fSchedulerMode == 9 || fSchedulerMode == 10)
          {
            sprintf(line, "pfmon --attach-task %i --smpl-compact --smpl-entries=1 --long-smpl-periods=5000000000 --smpl-print-counts --follow-all %s -0 -3 --with-header -eUNHALTED_CORE_CYCLES,L2_LINES_IN:SELF,INSTRUCTIONS_RETIRED", temp_pid, aggregate_results);
            //printf("%s\n", line);
            
            iResult = vfork();
            
            if (iResult < 0)
            {
              int err_code = errno;
              sprintf (line_temp, "*** ERROR: fork for online pfmon failed with %s:\n%s\n", sAppRun[iA].pszInvocation, strerror(err_code));
              log_to(LOGFILE, line_temp);
            }
            else if (iResult == 0)
            {
              close (sAppRun[iA].mypst[0]);
              close(1); dup(sAppRun[iA].mypst[1]) ; // Redirect the stdout of this process to the pseudo terminal
              
              char *cmd[] = {"sh", "-c", line, NULL};
              
              // the code of a copy of parent process is being replaced by the code of the new application
              if (execvp("sh", cmd) < 0){
                int err_code = errno;
                sprintf (line_temp, "*** ERROR: exec failed for online pfmon with %s:\n%s\n", sAppRun[iA].pszInvocation, strerror(err_code));
                log_to(LOGFILE, line_temp);
              }
            }
          }
        }
      }
      else
      {
        sAppRun[iA].iCount++;
      }
    }
  }
  
  if ( fSchedulerMode == 2 && mould_time % 8 == 0)
  {
    doRefreshAssignment(iA);
    
    if (mould_time % 16 == 0) doOptimisticAssignment(iA);
  }
  
  return 0;
}



//=============================================================



// collects info for online sampling
void* onliner (void* iA)
{
  short int i;
  char line[MAX_LINE_SIZE + 5];
  char * col = calloc(64, 1);
  int thread_id;
  long double tmp1, tmp2;
  FILE *stream = NULL;
  char line_temp2[MAX_LINE_SIZE+5];
  
  // 0 - initial (first part of the header and small tail after final counters)
  // 1 - sample phase
  // 2 - second part of the header
  // 3 - final counters
  short int pfmon_output_mode = 0;
  
  if (stream != NULL) fclose (stream);
  stream = fdopen (sAppRun[*((int*)iA)].mypst[0], "r");
  
  // online profiling (endless loop with pseudo terminal)
  while (fgets(line, sizeof(line), stream) != NULL)
  {
    col = simply_get_col(line, 0);
    if (strchr(col, '#') == NULL)
    {
      // get the thread id
      // format: 34 18201 18218 3 0x35c33fdae406 17 5000000000 0 0x7f9cba7b718e 0xc266d44 0x619822026
      col = simply_get_col(line, 2);
      thread_id = atoi(col);
      
      // search for where is the thread in the array
      for (i=0; i<MAX_THREAD_NUMBER; i++)
      {
        if (sAppRun[*((int*)iA)].aiTIDs[i] == thread_id)
        {
          col = simply_get_col(line, 9); // new misses
          tmp1 = atof(col);
          col = simply_get_col(line, 10); // new instructions
          tmp2 = atof(col);
          
          // calculating new miss rate
          sAppRun[*((int*)iA)].aldAppsSignatures[i][0] = 10000 * tmp1 / tmp2;
          sAppRun[*((int*)iA)].aldOldOnlineMisses[i] = tmp1;
          sAppRun[*((int*)iA)].aldOldOnlineInstructions[i] = tmp2;
          
          // online data
          sprintf (line_temp2, "%i: %s sAppRun[%i].aiTIDs[%i] (#%i): MISSES: %Lf | INSTRUCTIONS: %Lf ||| WEIGHTED MISSRATE: %Lf\n", mould_time, sAppRun[*((int*)iA)].pszName, *((int*)iA), i, sAppRun[*((int*)iA)].iCount, sAppRun[*((int*)iA)].aldOldOnlineMisses[i], sAppRun[*((int*)iA)].aldOldOnlineInstructions[i], sAppRun[*((int*)iA)].aldAppsSignatures[i][0]);
          log_to(ONLINEFILE, line_temp2);
        }
      }
    }
    else
    {
      // saving header data into pfmon.log
      sprintf (line_temp2, "%s", line);
      log_to(PFMONFILE, line_temp2);
    }
  }
  
  // close the pipe
  if (stream != NULL) fclose (stream);
  
  exit(0);
}



//=============================================================




int get_iA(int cpu_id) {
  int i, iA, lower_limit=0, upper_limit = 0;
  
  // determining application on this core
  for(iA = 0; iA < iRunAppCount; iA++){
    for(i = 0; i < MAX_THREAD_NUMBER; i++){
      //if (iA == 0)
      //{
      //  lower_limit = 28; // 22 initial threads of mysqld + 1 more short thread to create table counter_mem
      //  upper_limit = 28;
      //}
      if (sAppRun[iA].iPID > 0 /*&& i > lower_limit && i < upper_limit*/ && sAppRun[iA].aiTIDs[i] > 0 && sAppRun[iA].to_schedule[i] > 0 && sAppRun[iA].aiActualCores[i] == cpu_id)
      {
        return iA;
      }
    }
  }
  
  return -1;
}

int get_iT(int cpu_id) {
  int i, iA, lower_limit=0, upper_limit = 0;
  
  // determining application on this core
  for(iA = 0; iA < iRunAppCount; iA++){
    for(i = 0; i < MAX_THREAD_NUMBER; i++){
      //if (iA == 0)
      //{
      //  lower_limit = 28; // 22 initial threads of mysqld + 1 more short thread to create table counter_mem
      //  upper_limit = 28;
      //}
      if (sAppRun[iA].iPID > 0 /*&& i > lower_limit && i < upper_limit*/ && sAppRun[iA].aiTIDs[i] > 0 && sAppRun[iA].to_schedule[i] > 0 && sAppRun[iA].aiActualCores[i] == cpu_id)
      {
        return i;
      }
    }
  }
  
  return -1;
}




// collects info on EventSelect 0E2h Memory Controller DRAM Command Slots Missed
void* system_wider_dram (void* session_id)
{
  int i, k, iA, cpu_id, chip_id = -1, iApp, iThread;
  char line[MAX_LINE_SIZE + 5];
  char * col = calloc(64, 1);
  FILE *stream = NULL;
  
  if (stream != NULL) fclose (stream);
  stream = fdopen (numa_pst[*((int*)session_id)][0], "r");
  
  // online profiling (endless loop with pseudo terminal)
  while (fgets(line, sizeof(line), stream) != NULL)
  {
    //        usleep(WAIT_PERIOD);
    //printf ("pipe %i\n", *((int*)session_id));
    //if (*((int*)session_id) <= 1) printf("%s", line);
    
    col = simply_get_col(line, 0);
    
    if (strchr(col, '#') == NULL && strchr(col, '<') == NULL)
    {
      // CPU<ID>
      cpu_id = atoi(col+3);
      
      // chip-wide counters
      col = simply_get_col(line, 1);
      chip_wide_counters[cpu_id][0] += atol(col);
    }
    
    //printf ("line end\n");
  }
  
  printf ("pipe closed in system_wider_dram\n");
  
  // close the pipe
  if (stream != NULL) fclose (stream);
  
  last_time = 1;
}





// collects info from system wide sessions in NUMA memry scheduler project
void* system_wider (void* session_id)
{
  int i, j, k, iA, cpu_id, chip_id = -1, core_id, iApp, iThread;
  char line[MAX_LINE_SIZE + 5];
  char * col = calloc(64, 1);
  FILE *stream = NULL;
  char line_temp2[MAX_LINE_SIZE+5];
  
  if (stream != NULL) fclose (stream);
  stream = fdopen (numa_pst[*((int*)session_id)][0], "r");
  
  // online profiling (endless loop with pseudo terminal)
  while (fgets(line, sizeof(line), stream) != NULL)
  {
    //        usleep(WAIT_PERIOD);
    //printf ("pipe %i\n", *((int*)session_id));
    //if (*((int*)session_id) <= 1) printf("%s", line);
    
    col = simply_get_col(line, 0);
    
    if (strchr(col, '#') == NULL && strchr(col, '<') == NULL)
    {
      // CPU<ID>
      cpu_id = atoi(col+3);
      
      col = simply_get_col(line, 1);
      
      sample_instr[cpu_id] += atol(col);
      if (get_iA(cpu_id) >= 0 && get_iT(cpu_id) >= 0) sAppRun[get_iA(cpu_id)].aldActualOnlineInstructions[get_iT(cpu_id)] += atol(col);
      
      if (strcmp(pszHostname, "Xeon_X5570_woHT") == 0 || strcmp(pszHostname, "Xeon_X5570_HT") == 0 || strcmp(pszHostname, "Xeon_X5650_woHT") == 0 || strcmp(pszHostname, "Xeon_X5650_HT") == 0)
      {
        col = simply_get_col(line, 2);
        sample_counters[cpu_id][0] += atol(col);
        if (get_iA(cpu_id) >= 0 && get_iT(cpu_id) >= 0) sAppRun[get_iA(cpu_id)].aldActualOnlineMisses[get_iT(cpu_id)] += atol(col);
        col = simply_get_col(line, 3);
        sample_counters[cpu_id][0] += atol(col);
        if (get_iA(cpu_id) >= 0 && get_iT(cpu_id) >= 0) sAppRun[get_iA(cpu_id)].aldActualOnlineMisses[get_iT(cpu_id)] += atol(col);
        col = simply_get_col(line, 4);
        sample_counters[cpu_id][0] += atol(col);
        if (get_iA(cpu_id) >= 0 && get_iT(cpu_id) >= 0) sAppRun[get_iA(cpu_id)].aldActualOnlineMisses[get_iT(cpu_id)] += atol(col);
      }
      else
      {
        //if (get_iA(cpu_id) == 5) printf ("%s", line);
        
        chip_id = -1;
        for (i=0; i<shared_caches_number; i++)
        {
          if (cpu_id == cores[i][0])
          {
            chip_id = i;
            for (j=0; j<cores_per_shared_cache; j++)
            {
              k = j+2;
              core_id = cores[chip_id][j];
              
              col = simply_get_col(line, k);
              sample_counters[core_id][0] += atol(col);
              if (get_iA(core_id) >= 0 && get_iT(core_id) >= 0) sAppRun[get_iA(core_id)].aldActualOnlineMisses[get_iT(core_id)] += atol(col);
            }
            break;
          }
        }
      }
      
      if (chip_id >= 0 && fTool == 0)
      {
        // chip-wide counters
        for(i = 0; i < 16; i++){
          col = simply_get_col(line, i+2);
          chip_wide_counters[chip_id][i] += atol(col);
        }
      }
      
      // checking if any thread has been terminated lately
      for(iA = 0; iA < iRunAppCount; iA++){
        for(i = 0; i < MAX_THREAD_NUMBER; i++){
          if (sAppRun[iA].aiCoresLeftFrom[i] != -1)
          {
            // output the total counters
            //sprintf (line_temp2, "%s sAppRun[%i].aiTIDs[%i] (#%i): beginning data output...\n", sAppRun[iA].pszName, iA, i, sAppRun[iA].iCount);
            //log_to(SYSTEMWIDEFILE, line_temp2);
            //sprintf (line_temp2, "INST: %ld\n", sample_instr[sAppRun[iA].aiCoresLeftFrom[i]]);
            //log_to(SYSTEMWIDEFILE, line_temp2);
            sample_instr[sAppRun[iA].aiCoresLeftFrom[i]] = 0;
            for(k = 0; k < 12; k++){
              //sprintf (line_temp2, "  %ld  ", sample_counters[sAppRun[iA].aiCoresLeftFrom[i]][k]);
              //log_to(SYSTEMWIDEFILE, line_temp2);
              sample_counters[sAppRun[iA].aiCoresLeftFrom[i]][k] = 0;
            }
            //sprintf (line_temp2, "\n");
            //log_to(SYSTEMWIDEFILE, line_temp2);
            
            sAppRun[iA].aiCoresLeftFrom[i] = -1;
          }
        }
      }
    }
    
    //printf ("line end\n");
  }
  
  printf ("pipe closed in system_wider\n");
  
  // close the pipe
  if (stream != NULL) fclose (stream);
  
  last_time = 1;
}



//=============================================================



int add_iA_to_sAppRun(int in_pid)
{
  int iA;
  
  // take the empty slot in sAppRun array
  for (iA=0; iA<iRunAppCount; iA++)
  {
    if (sAppRun[iA].iPID < 0) break;
  }
  
  if (sAppRun[iA].iPID >= 0)
  {
    sprintf (line_temp, "*** ERROR: sAppRun is full.\n");
    log_to(LOGFILE, line_temp);
    return -1;
  }
  
  initializePoolMember(iA);
  
  sAppRun[iA].iPID = in_pid;
  
  return iA;
}



// parses info passed on from top output
void* topper ()
{
  int i, iA, iT, cur_tid, cur_pid, cur_ppid, cur_pppid, cur_perc_cpu, cur_mpi_rank, cur_java_process, cur_hadoop_child, line_n, old_process, iBufferSize, iBufferSize2, new_cycle=1, iA_to_get_info, add_to_app;
  long double cur_perc_mem;
  char line[MAX_LINE_SIZE + 5];
  char line_temp[MAX_LINE_SIZE+5];
  char * col = calloc(64, 1);
  char statline[MAX_LINE_SIZE+5];
  struct stat st;
  FILE *stream = NULL;
  FILE *procfile = NULL;
  char*   pszBuffer;
  char cur_hadoop_child_id[MAX_LINE_SIZE+5];
  
  if (stream != NULL) fclose (stream);
  stream = fdopen (top_pst[0], "r");
  
  line_n = 0;
  
  // online profiling (endless loop with pseudo terminal)
  while (stream != NULL && fgets(line, sizeof(line), stream) != NULL)
  {
    // check if the first col is a number (TID), otherwise skip
    col = simply_get_col(line, 0);
    if (isNumber(col) == 0)
    {
      new_cycle = 1;
      continue;
    }
    
    if (new_cycle == 1)
    {
      new_cycle = 0;
      for (i=0; i<(APP_RUN_SIZE*MAX_THREAD_NUMBER); i++)
      {
        aiComputeBoundTIDs[i] = -1;
        aiRankTIDs[i] = -1;
      }
      
      for(iA = 0; iA < iRunAppCount; iA++){
        for(iT = 0; iT < MAX_THREAD_NUMBER; iT++){
          if (sAppRun[iA].prevIdle[iT] == 0)
          {
            sAppRun[iA].aiCPUutil[iT] = 0;
            sAppRun[iA].aiMemFootprint[iT] = 0;
          }
          else
          {
            sAppRun[iA].prevIdle[iT] = 0;
          }
        }
      }
    }
    
    //log_to(SYSTEMWIDEFILE, line);
    
    col = simply_get_col(line, 0);
    cur_tid = atol(col);
    
    col = simply_get_col(line, 1);
    cur_perc_cpu = atol(col);
    
    col = simply_get_col(line, 2);
    cur_perc_mem = atof(col);
    
    // getting more data
    cur_mpi_rank = 0;
    cur_java_process = 0;
    cur_hadoop_child = 0;
    
    col = simply_get_col(line, 3);
    cur_ppid = atol(col);
    sprintf(statline, "/proc/%i/stat", cur_ppid);
    procfile = fopen(statline, "r");
    if(procfile != NULL)
    {
      fgets(line_temp,sizeof(line_temp),procfile);
      fclose (procfile);
      
      // Finding out if the parent of the current pid is mpirun (if it is the first node of the job) or orted (if it is a different node of the job).
      // if so, the current pid is an mpi rank.
      col = simply_get_col(line_temp, 1);
      if (strcmp(col, "(mpirun)") == 0 || strcmp(col, "(orted)") == 0) cur_mpi_rank = 1;
      
      // 1st parent of mpirun:
      cur_pppid = get_col(line_temp, 3, 0, 0);
    }
    

    // get the PID from TID
    cur_pid = get_pid_from_tid(cur_tid);
    if (cur_pid < 0)
    {
      sprintf (line_temp, "*** ERROR: cur_pid < 0 in topper()!\n");
      log_to(LOGFILE, line_temp);
      continue;
    }
    sprintf(statline, "/proc/%i/stat", cur_pid);
    procfile = fopen(statline, "r");
    if(procfile != NULL)
    {
      fgets(line_temp,sizeof(line_temp),procfile);
      fclose (procfile);
      
      // Finding out if the parent of the current pid is mpirun (if it is the first node of the job) or orted (if it is a different node of the job).
      // if so, the current pid is an mpi rank.
      col = simply_get_col(line_temp, 1);
      if (strcmp(col, "(java)") == 0) cur_java_process = 1;
    }
    
    if (cur_java_process == 1){
      sprintf(statline, "/proc/%i/environ", cur_pid);
      procfile = fopen(statline, "rb");
      char *arg = 0;
      size_t size = 0;
      while(procfile != NULL && getdelim(&arg, &size, 0, procfile) != -1)
      {
        // Finding out if the thread id belongs to a Hadoop mapper or reducer JVM
        if(strstr(arg, "hadoop") != NULL){
          char *task_id_str_begin = strstr(arg, "/attempt_");
          if (task_id_str_begin != NULL){
            char *task_id_str_end = strstr(task_id_str_begin+1, "/");
            if (task_id_str_end != NULL){
              strncpy(cur_hadoop_child_id, task_id_str_begin, task_id_str_end - task_id_str_begin);
              cur_hadoop_child_id[task_id_str_end - task_id_str_begin] = '\0';
              cur_hadoop_child = 1;
              break;
            }
          }
          else if (strstr(arg, "datanode") != NULL){
            sprintf(cur_hadoop_child_id, "/datanode");
            cur_hadoop_child = 1;
            break;
          }
          else if (strstr(arg, "tasktracker") != NULL){
            sprintf(cur_hadoop_child_id, "/tasktracker");
            cur_hadoop_child = 1;
          }
        }
      }
      free(arg);
      if (procfile != NULL) fclose (procfile);
    }
    
    iA_to_get_info = -1;
    
    // first update resource vector data for the active threads
    for(iA = 0; iA < iRunAppCount; iA++){
      for(iT = 0; iT < MAX_THREAD_NUMBER; iT++){
        if(sAppRun[iA].aiTIDs[iT] == cur_tid)
        {
          sAppRun[iA].aiCPUutil[iT] = cur_perc_cpu;
          sAppRun[iA].aiMemFootprint[iT] = cur_perc_mem;
          
          sAppRun[iA].prevIdle[iT] = 1;
        }
      }
      
      // for apps detected by iotopper, updateTraffic, etc. (not by topper)
      if(sAppRun[iA].iPID == cur_tid && sAppRun[iA].pszName == NULL)
      {
        iA_to_get_info = iA;
      }
    }
    
    // add to schedule
    add_to_app = 0;
    if (DETECT_ONLINE == 1)
    {
      if (DETECT_COMPUTE_BOUND_VIA_TOP == 1 && cur_perc_cpu > COMPUTE_BOUND_HORIZON)
      {
        for (i=0; i<(APP_RUN_SIZE*MAX_THREAD_NUMBER); i++)
        {
          if (aiComputeBoundTIDs[i] < 0)
          {
            //log_to(SYSTEMWIDEFILE, line);
            aiComputeBoundTIDs[i] = cur_tid; 
            add_to_app = 1;
            break;
          }
        }
      }
      
      if (DETECT_MPI_RANKS == 1 && cur_mpi_rank > 0)
      {
        for (i=0; i<(APP_RUN_SIZE*MAX_THREAD_NUMBER); i++)
        {
          if (aiRankTIDs[i] < 0)
          {
            aiRankTIDs[i] = cur_tid; 
            add_to_app = 1;
            break;
          }
        }
      }

      if (DETECT_HADOOP_CHILDS == 1 && cur_hadoop_child > 0)
      {
        add_to_app = 1; // we add this thread regardless
        // ... but we want to schedule only compute bound with DINO:
        if (cur_perc_cpu > COMPUTE_BOUND_HORIZON){
          for (i=0; i<(APP_RUN_SIZE*MAX_THREAD_NUMBER); i++)
          {
            if (aiComputeBoundTIDs[i] < 0)
            {
              //log_to(SYSTEMWIDEFILE, line);
              aiComputeBoundTIDs[i] = cur_tid; 
              break;
            }
          }
        }
      }
    }
    
    // add to sAppRun array
    if (add_to_app == 1)
    {
      old_process = 0;
      // check if a process is already detected
      for (iA=0; iA<iRunAppCount; iA++)
      {
        if (sAppRun[iA].iPID == cur_pid)
        {
          old_process = 1;
          break;
        }
      }
      
      if (old_process == 0)
      {
        if (iA_to_get_info >= 0)
        {
          sprintf (line_temp, "*** ERROR: iA_to_get_info should not be >= 0 at that point.\n");
          log_to(LOGFILE, line_temp);
        }
        else iA_to_get_info = add_iA_to_sAppRun(cur_pid);
      }
    }
    
    if (iA_to_get_info >= 0)
    {
      sAppRun[iA_to_get_info].iCount = 1;
      sAppRun[iA_to_get_info].aiExecTimes[sAppRun[iA_to_get_info].iCount-1] = time(0);
      
      col = simply_get_col(line, 4);
      iBufferSize = strlen(col);
      if (cur_hadoop_child == 0) iBufferSize2 = 0;
      else iBufferSize2 = strlen(cur_hadoop_child_id);
      pszBuffer = calloc(iBufferSize + iBufferSize2 + 1, 1);
      strncpy(pszBuffer, col, iBufferSize);
      strncpy(pszBuffer+iBufferSize, cur_hadoop_child_id, iBufferSize2);
      sAppRun[iA_to_get_info].pszName = pszBuffer;
      
      sAppRun[iA_to_get_info].iMpirunPID = cur_ppid;
      
      // obtaining iShellPID: 2nd parent of the mpirun (which in turn is parent of the given MPI process)
      // 2nd parent of mpirun
      sprintf(statline, "/proc/%i/stat", cur_pppid);
      procfile = fopen(statline, "r");
      if(procfile != NULL)
      {
        fgets(line_temp,sizeof(line_temp),procfile);
        fclose (procfile);
        sAppRun[iA_to_get_info].iShellPID = get_col(line_temp, 3, 0, 0);
      }
      
      if (cur_mpi_rank > 0)
      {
        sprintf (line_temp, "%i: %s sAppRun[%i]: detected because it is an MPI rank (pid = %i)\n", mould_time, sAppRun[iA_to_get_info].pszName, iA_to_get_info, cur_pid);
        log_to(LOGFILE, line_temp);
      }
      else if (cur_perc_cpu > COMPUTE_BOUND_HORIZON)
      {
        sprintf (line_temp, "%i: %s sAppRun[%i]: detected because is compute bound: %d%% (pid = %i)\n", mould_time, sAppRun[iA_to_get_info].pszName, iA_to_get_info, cur_perc_cpu, cur_pid);
        log_to(LOGFILE, line_temp);
      }
      else if (cur_perc_mem > FOOTPRINT_BOUND_HORIZON)
      {
        sprintf (line_temp, "%i: %s sAppRun[%i]: detected because has big memory footprint: %Lf%% (pid = %i)\n", mould_time, sAppRun[iA_to_get_info].pszName, iA_to_get_info, cur_perc_mem, cur_pid);
        log_to(LOGFILE, line_temp);
      }
      else
      {
        sprintf (line_temp, "%i: %s sAppRun[%i]: detected because it is either I/O or network bound TRAFFIC SNT %Lf RCVD %Lf   IO WRITE %Lf READ %Lf: (pid = %i)\n", mould_time, sAppRun[iA_to_get_info].pszName, iA_to_get_info, sAppRun[iA_to_get_info].aldOnlineTrafficSent[0], sAppRun[iA_to_get_info].aldOnlineTrafficReceived[0], sAppRun[iA_to_get_info].aldOnlineDiskWrite[0], sAppRun[iA_to_get_info].aldOnlineDiskRead[0], cur_pid);
        log_to(LOGFILE, line_temp);
      }
    }
  }
  
  printf ("pipe closed in topper\n");
  
  // close the pipe
  if (stream != NULL) fclose (stream);
  
  topper_restart = 1;
}



//=============================================================



// parses info passed on from iotop output
void* iotopper ()
{
  int i, iA, iT, cur_tid, cur_pid, old_process;
  long double io_read, io_write, io_wait;
  char line[MAX_LINE_SIZE + 5];
  char * col = calloc(64, 1);
  char statline[MAX_LINE_SIZE+5];
  FILE *stream = NULL;
  
  if (stream != NULL) fclose (stream);
  stream = fdopen (iotop_pst[0], "r");
  
  // online profiling (endless loop with pseudo terminal)
  while (stream != NULL && fgets(line, sizeof(line), stream) != NULL)
  {
    col = simply_get_col(line, 0);
    cur_tid = atol(col);
    
    col = simply_get_col(line, 3);
    io_read = atof(col);
    
    col = simply_get_col(line, 5);
    io_write = atof(col);
    
    // the percentage of time the process spent while waiting on I/O:
    col = simply_get_col(line, 9);
    io_wait = atof(col);
    
    // update resource vector data for the active threads
    for(iA = 0; iA < iRunAppCount; iA++){
      for(iT = 0; iT < MAX_THREAD_NUMBER; iT++){
        if(sAppRun[iA].aiTIDs[iT] == cur_tid)
        {
          sAppRun[iA].aldOnlineDiskRead[iT] = io_read;
          sAppRun[iA].aldTotalDiskRead[iT] += io_read * PERIOD_IN_SEC;
          
          sAppRun[iA].aldOnlineDiskWrite[iT] = io_write;
          sAppRun[iA].aldTotalDiskWrite[iT] += io_write * PERIOD_IN_SEC;

          sAppRun[iA].aldOnlineDiskWait[iT] = io_wait;
        }
      }
    }
    
    if ( ADD_IOTOP_TO_APP_ARRAY == 1   &&   (io_read > DISK_TRAFFIC_HORIZON || io_write > DISK_TRAFFIC_HORIZON) )
    {
      // get the PID from TID
      cur_pid = get_pid_from_tid(cur_tid);
      if (cur_pid < 0)
      {
        sprintf (line_temp, "*** ERROR: cur_pid < 0 in iotopper()!\n");
        log_to(LOGFILE, line_temp);
        continue;
      }
      
      old_process = 0;
      // check if a process is already detected
      for (iA=0; iA<iRunAppCount; iA++)
      {
        if (sAppRun[iA].iPID == cur_pid)
        {
          old_process = 1;
          break;
        }
      }
      
      if (old_process == 0)
      {
        add_iA_to_sAppRun(cur_pid);
      }
    }
  }
  
  printf ("pipe closed in iotopper\n");
  
  // close the pipe
  if (stream != NULL) fclose (stream);
  
  last_time = 1;
}



//=============================================================



int launchIBSer()
{
  if (pthread_create(&aIbsThread, 0, IBSer, 0))
  {
    sprintf (line_temp, "IBSer spawn failed.\n");
    log_to(LOGFILE, line_temp);
    return 0;
  }
  
  return 0;
}





//=============================================================





void print_pagenodes(pid_t pid, unsigned long *pageaddrs) {
  /*int pagenodes[PAGES];
   int i;
   int err;
   
   err = numa_move_pages(pid, PAGES, pageaddrs, NULL, pagenodes, 0);
   if (err < 0) {
   perror("move_pages");
   }
   
   for(i=0; i<PAGES; i++) {
   
   if (pagenodes[i] == -ENOENT) 1;//printf("  page #%d: the page does not exist\n", i);
   else if (pagenodes[i] == -EFAULT) 1;//printf("  page #%d: the specified address does not point to a valid mapping\n", i);
   else if (pagenodes[i] == -EPERM) 1;//printf("  page #%d: the page can't be moved (it is mlocked)\n", i);
   else if (pagenodes[i] == -EACCES) 1;//printf("  page #%d: the page is shared by multiple processes and the flag MPOL_MF_MOVE_ALL was not set\n", i);
   else if (pagenodes[i] == -EBUSY) 1;//printf("  page #%d: the page could not be moved - it is busy now\n", i);
   else if (pagenodes[i] == -ENOMEM) 1;//printf("  page #%d: insufficient memory\n", i);
   else if (pagenodes[i] == -EIO) 1;//printf("  page #%d: the page can't be written\n", i);
   else if (pagenodes[i] == -EINVAL) 1;//printf("  page #%d: the page can't be moved because the file system\n", i);
   else printf("  page %d is on node %d\n", i, pagenodes[i]);
   
   }*/
}

void migrate_pagenodes(pid_t pid, const int iA, const int iT, unsigned long *pageaddrs, const int dest_node) {
  int pagenodes[PAGES];
  int i;
  int err;
  unsigned long pageaddrs_to_migrate[PAGES];
  int pagenodes_to_migrate_to[PAGES];
  
  
  for(i=0; i<PAGES; i++) {
    pagenodes_to_migrate_to[i] = dest_node;
  }
  
  err = numa_move_pages(pid, PAGES, (void **)pageaddrs, pagenodes_to_migrate_to, pagenodes, MPOL_MF_MOVE);
  if (err < 0) {
    perror("move_pages");
  }
  for(i=0; i<PAGES; i++) {
    if (pagenodes[i] == -ENOENT) 1;//printf("  page #%d: the page does not exist\n", i);
    else if (pagenodes[i] == -EFAULT) 1;//printf("  page #%d: the specified address does not point to a valid mapping\n", i);
    else if (pagenodes[i] == -EPERM) 1;//printf("  page #%d: the page can't be moved (it is mlocked)\n", i);
    else if (pagenodes[i] == -EACCES) 1;//printf("  page #%d: the page is shared by multiple processes and the flag MPOL_MF_MOVE_ALL was not set\n", i);
    else if (pagenodes[i] == -EBUSY) 1;//printf("  page #%d: the page could not be moved - it is busy now\n", i);
    else if (pagenodes[i] == -ENOMEM) 1;//printf("  page #%d: insufficient memory\n", i);
    else if (pagenodes[i] == -EIO) 1;//printf("  page #%d: the page can't be written\n", i);
    else if (pagenodes[i] == -EINVAL) 1;//printf("  page #%d: the page can't be moved because the file system\n", i);
    else 
    {
      //printf("  page #%d is on node #%d\n", i, final_pagenodes[i]);
      if (pagenodes[i] != pagenodes_to_migrate_to[i])
      {
        //printf("page %d (%lu) is on node %d instead of supposed node %d\n", i, pageaddrs[i], pagenodes[i], pagenodes_to_migrate_to[i]);
      }
      else
      {
        sAppRun[iA].pages_migrated[iT]++;
        //printf("page %d (%lu) was successfully migrated from node %d to node %d\n", i, pageaddrs_to_migrate[i], pagenodes_to_migrate_from[i], pagenodes_to_migrate_to[i]);
        //printf("page %d (%lu) was successfully migrated to node %d\n", i, pageaddrs[i], pagenodes_to_migrate_to[i]);
      }
    }
  }
  
  
  
  
  
  
  
  
  
  
  
  /*
   int count = 0;
   int pagenodes_to_migrate_from[PAGES];
   
   // first obtain the subset of pages that indeed need to be moved (i.e. those that exist and resides on the node, different from dest_node)
   err = numa_move_pages(pid, PAGES, pageaddrs, NULL, pagenodes, 0);
   if (err < 0) {
   //perror("move_pages");
   }
   for(i=0; i<PAGES; i++) {
   if (pagenodes[i] == -ENOENT) 1;//printf("  page #%d: the page does not exist\n", i);
   else if (pagenodes[i] == -EFAULT) 1;//printf("  page #%d: the specified address does not point to a valid mapping\n", i);
   else if (pagenodes[i] == -EPERM) 1;//printf("  page #%d: the page can't be moved (it is mlocked)\n", i);
   else if (pagenodes[i] == -EACCES) 1;//printf("  page #%d: the page is shared by multiple processes and the flag MPOL_MF_MOVE_ALL was not set\n", i);
   else if (pagenodes[i] == -EBUSY) 1;//printf("  page #%d: the page could not be moved - it is busy now\n", i);
   else if (pagenodes[i] == -ENOMEM) 1;//printf("  page #%d: insufficient memory\n", i);
   else if (pagenodes[i] == -EIO) 1;//printf("  page #%d: the page can't be written\n", i);
   else if (pagenodes[i] == -EINVAL) 1;//printf("  page #%d: the page can't be moved because the file system\n", i);
   else
   {
   //printf("  page #%d is on node #%d\n", i, pagenodes[i]);
   if (pagenodes[i] != dest_node)
   {
   //printf("  %d %d\n", pagenodes[i], dest_node);
   pageaddrs_to_migrate[count] = pageaddrs[i];
   pagenodes_to_migrate_to[count] = dest_node;
   //pagenodes_to_migrate_from[count] = pagenodes[i];
   count++;
   }
   }
   }
   
   if (count == 0) return;
   
   unsigned long final_pageaddrs_to_migrate[count];
   int final_pagenodes_to_migrate_to[count];
   //int final_pagenodes_to_migrate_from[count];
   int final_pagenodes[count];
   for(i=0; i<count; i++) {
   final_pageaddrs_to_migrate[i] = pageaddrs_to_migrate[i];
   final_pagenodes_to_migrate_to[i] = pagenodes_to_migrate_to[i];
   //final_pagenodes_to_migrate_from[i] = pagenodes_to_migrate_from[i];
   final_pagenodes[i] = -1;
   }
   
   err = numa_move_pages(pid, count, final_pageaddrs_to_migrate, final_pagenodes_to_migrate_to, final_pagenodes, MPOL_MF_MOVE);
   if (err < 0) {
   //perror("move_pages");
   }
   for(i=0; i<count; i++) {
   if (final_pagenodes[i] == -ENOENT) 1;//printf("  page #%d: the page does not exist\n", i);
   else if (final_pagenodes[i] == -EFAULT) 1;//printf("  page #%d: the specified address does not point to a valid mapping\n", i);
   else if (final_pagenodes[i] == -EPERM) 1;//printf("  page #%d: the page can't be moved (it is mlocked)\n", i);
   else if (final_pagenodes[i] == -EACCES) 1;//printf("  page #%d: the page is shared by multiple processes and the flag MPOL_MF_MOVE_ALL was not set\n", i);
   else if (final_pagenodes[i] == -EBUSY) 1;//printf("  page #%d: the page could not be moved - it is busy now\n", i);
   else if (final_pagenodes[i] == -ENOMEM) 1;//printf("  page #%d: insufficient memory\n", i);
   else if (final_pagenodes[i] == -EIO) 1;//printf("  page #%d: the page can't be written\n", i);
   else if (final_pagenodes[i] == -EINVAL) 1;//printf("  page #%d: the page can't be moved because the file system\n", i);
   else 
   {
   //printf("  page #%d is on node #%d\n", i, final_pagenodes[i]);
   if (final_pagenodes[i] != final_pagenodes_to_migrate_to[i])
   {
   //printf("page %d (%lu) is on node %d instead of supposed node %d\n", i, final_pageaddrs_to_migrate[i], final_pagenodes[i], final_pagenodes_to_migrate_to[i]);
   }
   else
   {
   sAppRun[iA].pages_migrated[iT]++;
   //printf("page %d (%lu) was successfully migrated from node %d to node %d\n", i, pageaddrs_to_migrate[i], pagenodes_to_migrate_from[i], pagenodes_to_migrate_to[i]);
   //printf("page %d (%lu) was successfully migrated to node %d\n", i, final_pageaddrs_to_migrate[i], final_pagenodes_to_migrate_to[i]);
   }
   }
   }*/
}

// collects info from IBS MSR registers
void* IBSer ()
{
  int numberCores = cores_per_shared_cache * shared_caches_number;
  uint32_t reg_cntl[numberCores];
  uint64_t data_cntl[numberCores];
  uint32_t reg_opdata3[numberCores];
  uint64_t data_opdata3[numberCores];
  uint32_t reg_addr[numberCores];
  uint64_t data_addr[numberCores];
  int i, c, r, fd[numberCores];
  int cpu, iA, iT;
  pid_t pid;
  unsigned long nodemask = 3;
  unsigned int highbit = 63, lowbit = 0, bits;
  unsigned long arg;
  char *endarg;
  char *pat;
  int width;
  char msr_file_name[64];
  
  //unsigned long *buffer;
  unsigned long pageaddrs[PAGES];
  int status[PAGES];
  unsigned long pagesize;
  unsigned long maxnode;
  int dest_node;
  int nodes[PAGES];
  
  cpu = 0;
  
  width = 1;    /* Default */
  pat = "%d  %*llx\n";
  
  pagesize = getpagesize();
  
  cpu = 0;
  for(cpu = 0; cpu < numberCores; cpu++)
  {
    reg_cntl[cpu] = 0xC0011033;
    reg_opdata3[cpu] = 0xC0011037;
    reg_addr[cpu] = 0xC0011038;
    
    sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
    fd[cpu] = open(msr_file_name, O_RDWR);
    
    if (fd[cpu] < 0) {
      if (errno == ENXIO) {
        fprintf(stderr, "rdmsr: No CPU %d\n", cpu);
      } else if (errno == EIO) {
        fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n", cpu);
      } else {
        perror("rdmsr: open");
      }
    }
    
    // initializing IBS first
    if (pread(fd[cpu], &data_cntl[cpu], sizeof data_cntl[cpu], reg_cntl[cpu]) != sizeof data_cntl[cpu]) {
      if (errno == EIO) {
        fprintf(stderr, "rdmsr: CPU %d cannot read "
                "MSR 0x%08"PRIx32"\n",
                cpu, reg_cntl[cpu]);
      } else {
        perror("rdmsr: pread");
      }
    }
    
    data_cntl[cpu] |= 0x200F0;
    
    //printf(pat, cpu, width, data_cntl[cpu]);
    
    if (pwrite(fd[cpu], &data_cntl[cpu], sizeof data_cntl[cpu], reg_cntl[cpu]) != sizeof data_cntl[cpu]) {
      if (errno == EIO) {
        fprintf(stderr,
                "wrmsr: CPU %d cannot set MSR "
                "0x%08"PRIx32" to 0x%016"PRIx64"\n",
                cpu, reg_cntl[cpu], data_cntl[cpu]);
      } else {
        perror("wrmsr: pwrite");
      }
    }
    
    // check
    data_cntl[cpu] = 0;
    
    if (pread(fd[cpu], &data_cntl[cpu], sizeof data_cntl[cpu], reg_cntl[cpu]) != sizeof data_cntl[cpu]) {
      if (errno == EIO) {
        fprintf(stderr, "rdmsr: CPU %d cannot read "
                "MSR 0x%08"PRIx32"\n",
                cpu, reg_cntl[cpu]);
      } else {
        perror("rdmsr: pread");
      }
    }
    
    data_cntl[cpu] &= 0x200F0;
    
    if (data_cntl[cpu] != 0x200F0)
    {
      printf(pat, cpu, width, data_cntl[cpu]);
      printf("!!!\n");
      
      // killing all the child processes
      pid_t grp = getpgrp();
      if (!killpg (grp, 9))
      {
        //sprintf (line_temp, "Child processes are successfully terminated.\n");
        //log_to(LOGFILE, line_temp);
      }
      else
      {
        //sprintf (line_temp, "WARNING: There was smth wrong with the termination of the child processes.\n");
        //log_to(LOGFILE, line_temp);
      }
      
      exit(1);
    }
  }
  
  /*  // check2
   for(i = 0; i < numberCores; i++)
   {
   data_cntl[i] = 0;
   
   if (pread(fd[i], &data_cntl[i], sizeof data_cntl[i], reg_cntl[i]) != sizeof data_cntl[i]) {
   if (errno == EIO) {
   fprintf(stderr, "rdmsr: CPU %d cannot read "
   "MSR 0x%08"PRIx32"\n",
   i, reg_cntl[i]);
   } else {
   perror("rdmsr: pread");
   }
   }
   
   data_cntl[i] &= 0x200F0;
   
   printf ("%d  %d\n", i, data_cntl[i]);
   }*/
  
  
  while (1)
  {
    usleep(IBS_INTERVAL);
    
    // reading opdata3 and linear address
    for(cpu = 0; cpu < numberCores; cpu++)
    {
      if (pread(fd[cpu], &data_opdata3[cpu], sizeof data_opdata3[cpu], reg_opdata3[cpu]) != sizeof data_opdata3[cpu]) {
        if (errno == EIO) {
          fprintf(stderr, "rdmsr: CPU %d cannot read "
                  "MSR 0x%08"PRIx32"\n",
                  cpu, reg_opdata3[cpu]);
        } else {
          perror("rdmsr: pread");
        }
      }
      
      data_opdata3[cpu] &= 0x20080;
      
      // The linear address in MSRC001_1038 is valid for the load or store operation and it is a cache miss
      if (data_opdata3[cpu] == 0x20080)
      {
        if (pread(fd[cpu], &data_addr[cpu], sizeof data_addr[cpu], reg_addr[cpu]) != sizeof data_addr[cpu]) {
          if (errno == EIO) {
            fprintf(stderr, "rdmsr: CPU %d cannot read "
                    "MSR 0x%08"PRIx32"\n",
                    cpu, reg_addr[cpu]);
          } else {
            perror("rdmsr: pread");
          }
        }
        //printf(pat, cpu, width, data_addr[cpu]);
        
        //if (!mbind(&(data_addr[cpu]), 1024*1024*1024, MPOL_INTERLEAVE, &nodemask, 2, MPOL_MF_MOVE))
        //{
        //        perror("mbind: ");
        //}
        
        iA = get_iA(cpu); iT = get_iT(cpu);
        
        //printf("not yet2 %d %d %d!\n", cpu, iA, iT);
        
        if (iA >= 0 && iT >= 0)
        {
          pid = sAppRun[iA].aiTIDs[iT];
          dest_node = maskToCache(cpuToMask(cpu));
          
          //maxnode = numa_max_node();
          //printf("numa_max_node %lu\n", maxnode);
          
          //buffer = malloc(PAGES * pagesize * sizeof(unsigned long));
          //printf("move pages to node %lu\n", maxnode);
          
          // get the page address
          data_addr[cpu] = (data_addr[cpu] >> 12) << 12;
          for(i=0; i<PAGES; i++)
          {
            if (BACKWARDS == 1)
            {
              pageaddrs[i] = (unsigned long) (data_addr[cpu] + (i-PAGES/2)*pagesize);
            }
            else if (RANDOM == 1)
            {
              r = (rand() % 8192);
              pageaddrs[i] = (unsigned long) (data_addr[cpu] + r*pagesize);
            }
            else
            {
              pageaddrs[i] = (unsigned long) (data_addr[cpu] + i*pagesize);
            }
            //printf("%#lx\n", pageaddrs[i]);
          }
          
          //printf("before touching the pages\n");
          //print_pagenodes(pid, pageaddrs);
          
          migrate_pagenodes(pid, iA, iT, pageaddrs, dest_node);
          
          //print_pagenodes(pid, pageaddrs);
        }
        
      }
      /*else
       {
       printf("no addr\n");
       }*/
      
      // clear IbsOpVal bit for the next sample
      if (pread(fd[cpu], &data_cntl[cpu], sizeof data_cntl[cpu], reg_cntl[cpu]) != sizeof data_cntl[cpu]) {
        if (errno == EIO) {
          fprintf(stderr, "rdmsr: CPU %d cannot read "
                  "MSR 0x%08"PRIx32"\n",
                  cpu, reg_cntl[cpu]);
        } else {
          perror("rdmsr: pread");
        }
      }
      
      //printf(pat, cpu, width, data_cntl[cpu]);
      
      data_cntl[cpu] |= 0x200F0;
      data_cntl[cpu] &= 0xFFFFFFFFFFFBFFFF;
      
      //printf(pat, cpu, width, data_cntl[cpu]);
      
      if (pwrite(fd[cpu], &data_cntl[cpu], sizeof data_cntl[cpu], reg_cntl[cpu]) != sizeof data_cntl[cpu]) {
        if (errno == EIO) {
          fprintf(stderr,
                  "wrmsr: CPU %d cannot set MSR "
                  "0x%08"PRIx32" to 0x%016"PRIx64"\n",
                  cpu, reg_cntl[cpu], data_cntl[cpu]);
        } else {
          perror("wrmsr: pwrite");
        }
      }
    }
  }
  
  close(fd[cpu]);
}



//=============================================================



// the handler for the signal of terminating child - needed for respawn application after it has finished
// for a multithreaded application that means the end of the main (first) thread for which iPID == iTID
// we are not doing decrementation of cache counters for completed thread here (it is being done in checkThreads function instead) because according to the observations only main thread of the multithreaded application will send a signal SIGCHLD to parent process once it has finished  
void child_term_handler(int sig)
{
  pid_t pid;
  //  int iA, pid_was_set=0;
  
  pid = wait(NULL);
  
  /*  while (!pid_was_set)
   {
   for (iA = 0; iA < iRunAppCount; iA++)
   {
   if (pid == sAppRun[iA].iPID)
   {
   sAppRun[iA].iPID = -1;
   pid_was_set=1;
   }
   }
   if (!pid_was_set) usleep(WAIT_PERIOD);
   }
   */
  sprintf (line_temp, "Pid %d exited.\n", pid);
  log_to(LOGFILE, line_temp);
}



//=============================================================



int launchApp(int iA)
{
  int core_to_bind_to = sAppRun[iA].aiBindCores[0];
  int memory_node = sAppRun[iA].aiBindNodes[0];
  
  if (sAppRun[iA].pszRunDir != NULL)
  {
    if (chdir(sAppRun[iA].pszRunDir) < 0)
    {
      int err_code = errno;
      sprintf (line_temp, "*** ERROR: directory was NOT changed to %s\n%s\n", sAppRun[iA].pszRunDir, strerror(err_code));
      log_to(LOGFILE, line_temp);
    }
    else
    {
      sprintf (line_temp, "%s sAppRun[%i] (#%i): directory was succesfully changed to %s\n", sAppRun[iA].pszName, iA, sAppRun[iA].iCount, sAppRun[iA].pszRunDir);
      log_to(LOGFILE, line_temp);
    }
  }
  
  int     iResult, i;
  char    line[MAX_LINE_SIZE+5];
  
  if (fSchedulerMode == 0)
  {
    if (strcmp(pszHostname, "Opteron_8356_2") == 0 || strcmp(pszHostname, "Opteron_8356_4") == 0 || strcmp(pszHostname, "Opteron_2350_4cores") == 0 || strcmp(pszHostname, "Opteron_2350_2cores") == 0 || strcmp(pszHostname, "Opteron_2435_2nodes") == 0 || strcmp(pszHostname, "Opteron_2435_1node") == 0 || strcmp(pszHostname, "Opteron_8435") == 0 || strcmp(pszHostname, "Opteron_6164") == 0 || strcmp(pszHostname, "Xeon_X5570_woHT") == 0 || strcmp(pszHostname, "Xeon_X5570_HT") == 0 || strcmp(pszHostname, "Xeon_X5650_woHT") == 0 || strcmp(pszHostname, "Xeon_X5650_HT") == 0)
    {
      sprintf(line, "/usr/bin/time -f \"%%e - %%U user %%S system %%E elapsed - that was %%C \" -o %s --append %s", TIMEFILE, sAppRun[iA].pszInvocation);
      //            sprintf(line, "/usr/bin/time -f \"%%e - %%U user %%S system %%E elapsed - that was %%C \" -o %s --append pfmon --follow-all %s -0 -3 --events=L3_CACHE_MISSES:ALL,RETIRED_INSTRUCTIONS --outfile=%s --append --with-header %s", TIMEFILE, aggregate_results, PFMONFILE, sAppRun[iA].pszInvocation);
    }
    else if (strcmp(pszHostname, "Xeon_X5365_2") == 0)
    {
      sprintf(line, "/usr/bin/time -f \"%%e - %%U user %%S system %%E elapsed - that was %%C \" -o %s --append pfmon --follow-all %s -0 -3 --events=L2_LINES_IN:SELF,INSTRUCTIONS_RETIRED --outfile=%s --append --with-header taskset -c 1,3,4,6 %s", TIMEFILE, aggregate_results, PFMONFILE, sAppRun[iA].pszInvocation);
    }
    else if (strcmp(pszHostname, "Xeon_X5365_4") == 0 || strcmp(pszHostname, "Xeon_E5320") == 0)
    {
      //sprintf(line, "/usr/bin/time -f \"%%e - %%U user %%S system %%E elapsed - that was %%C \" -o %s --append pfmon --follow-all %s -0 -3 --events=L2_LINES_IN:SELF,INSTRUCTIONS_RETIRED --outfile=%s --append --with-header %s", TIMEFILE, aggregate_results, PFMONFILE, sAppRun[iA].pszInvocation);
      sprintf(line, "/usr/bin/time -f \"%%e - %%U user %%S system %%E elapsed - that was %%C \" -o %s --append %s", TIMEFILE, sAppRun[iA].pszInvocation);
    }
  }
  else if (fSchedulerMode == 1 || fSchedulerMode == 10 || fSchedulerMode == 11)
  {
    if (strcmp(pszHostname, "Opteron_8356_2") == 0 || strcmp(pszHostname, "Opteron_8356_4") == 0 || strcmp(pszHostname, "Opteron_2350_4cores") == 0 || strcmp(pszHostname, "Opteron_2350_2cores") == 0 || strcmp(pszHostname, "Opteron_2435_2nodes") == 0 || strcmp(pszHostname, "Opteron_2435_1node") == 0 || strcmp(pszHostname, "Opteron_8435") == 0 || strcmp(pszHostname, "Opteron_6164") == 0 || strcmp(pszHostname, "Xeon_X5570_woHT") == 0 || strcmp(pszHostname, "Xeon_X5570_HT") == 0 || strcmp(pszHostname, "Xeon_X5650_woHT") == 0 || strcmp(pszHostname, "Xeon_X5650_HT") == 0)
    {
      if (memory_node >= 0) sprintf(line, "/usr/bin/time -f \"%%e - %%U user %%S system %%E elapsed - that was %%C \" -o %s --append taskset -c %i numactl --membind=%d -- %s", TIMEFILE, core_to_bind_to, memory_node, sAppRun[iA].pszInvocation);
      else sprintf(line, "/usr/bin/time -f \"%%e - %%U user %%S system %%E elapsed - that was %%C \" -o %s --append taskset -c %i %s", TIMEFILE, core_to_bind_to, sAppRun[iA].pszInvocation);
    }
    else
    {
      sprintf(line, "/usr/bin/time -f \"%%e - %%U user %%S system %%E elapsed - that was %%C \" -o %s --append taskset -c %i %s", TIMEFILE, core_to_bind_to, sAppRun[iA].pszInvocation);
    }
  }
  else if (fSchedulerMode == 4 || fSchedulerMode == 7 || fSchedulerMode == 8 || fSchedulerMode == 9)
  {
    if (strcmp(pszHostname, "Opteron_8356_2") == 0 || strcmp(pszHostname, "Opteron_8356_4") == 0 || strcmp(pszHostname, "Opteron_2350_4cores") == 0 || strcmp(pszHostname, "Opteron_2350_2cores") == 0 || strcmp(pszHostname, "Opteron_2435_2nodes") == 0 || strcmp(pszHostname, "Opteron_2435_1node") == 0 || strcmp(pszHostname, "Opteron_8435") == 0 || strcmp(pszHostname, "Opteron_6164") == 0 || strcmp(pszHostname, "Xeon_X5570_woHT") == 0 || strcmp(pszHostname, "Xeon_X5570_HT") == 0 || strcmp(pszHostname, "Xeon_X5650_woHT") == 0 || strcmp(pszHostname, "Xeon_X5650_HT") == 0)
    {
      //sprintf(line, "/usr/bin/time -f \"%%e - %%U user %%S system %%E elapsed - that was %%C \" -o %s --append numactl --interleave=all -- %s", TIMEFILE, sAppRun[iA].pszInvocation);
      sprintf(line, "/usr/bin/time -f \"%%e - %%U user %%S system %%E elapsed - that was %%C \" -o %s --append %s", TIMEFILE, sAppRun[iA].pszInvocation);
    }
    else
    {
      sprintf(line, "/usr/bin/time -f \"%%e - %%U user %%S system %%E elapsed - that was %%C \" -o %s --append %s", TIMEFILE, sAppRun[iA].pszInvocation);
    }
  }
  else
  {
    if (strcmp(pszHostname, "Opteron_8356_2") == 0 || strcmp(pszHostname, "Opteron_2350_4cores") == 0 || strcmp(pszHostname, "Opteron_2350_2cores") == 0 || strcmp(pszHostname, "Opteron_2435_2nodes") == 0 || strcmp(pszHostname, "Opteron_2435_1node") == 0 || strcmp(pszHostname, "Opteron_8435") == 0 || strcmp(pszHostname, "Opteron_6164") == 0 || strcmp(pszHostname, "Xeon_X5570_woHT") == 0 || strcmp(pszHostname, "Xeon_X5570_HT") == 0 || strcmp(pszHostname, "Xeon_X5650_woHT") == 0 || strcmp(pszHostname, "Xeon_X5650_HT") == 0)
    {
      //            sprintf(line, "/usr/bin/time -f \"%%e - %%U user %%S system %%E elapsed - that was %%C \" -o %s --append pfmon --follow-all %s -0 -3 --events=L3_CACHE_MISSES:ALL,RETIRED_INSTRUCTIONS --outfile=%s --append --with-header %s", TIMEFILE, aggregate_results, PFMONFILE, sAppRun[iA].pszInvocation);
      sprintf(line, "/usr/bin/time -f \"%%e - %%U user %%S system %%E elapsed - that was %%C \" -o %s --append pfmon --follow-all %s -0 -3 --events=RETIRED_INSTRUCTIONS --outfile=%s --append --with-header %s", TIMEFILE, aggregate_results, PFMONFILE, sAppRun[iA].pszInvocation);
    }
    else
    {
      sprintf(line, "/usr/bin/time -f \"%%e - %%U user %%S system %%E elapsed - that was %%C \" -o %s --append pfmon --follow-all %s -0 -3 --events=L2_LINES_IN:SELF,INSTRUCTIONS_RETIRED --outfile=%s --append --with-header %s", TIMEFILE, aggregate_results, PFMONFILE, sAppRun[iA].pszInvocation);
    }
  }
  
  char *cmd[] = {"sh", "-c", line, NULL};
  
  sprintf (line_temp, "%s sAppRun[%i] (#%i): invocation string is %s\n", sAppRun[iA].pszName, iA, sAppRun[iA].iCount, line);
  log_to(LOGFILE, line_temp);
  
  iResult = vfork();
  
  if (iResult < 0)
  {
    int err_code = errno;
    sprintf (line_temp, "*** ERROR: fork failed for %s:\n%s\n", sAppRun[iA].pszInvocation, strerror(err_code));
    log_to(LOGFILE, line_temp);
  }
  else if (iResult)
  {
    return iResult;
  }
  
  /*    if (fSchedulerMode == 1)
   {
   int core_to_bind_to = (int)sAppRun[iA].aldAppsSignatures[0][0], mask = -1;
   
   mask = 1 << core_to_bind_to;
   
   sAppRun[iA].aiSharedCacheMasks[0] = mask;
   bindThread(iA, 0);
   }*/
  
  // the code of a copy of parent process is being replaced by the code of the new application
  if (execvp("sh", cmd) < 0){
    int err_code = errno;
    sprintf (line_temp, "*** ERROR: exec failed for %s:\n%s\n", sAppRun[iA].pszInvocation, strerror(err_code));
    log_to(LOGFILE, line_temp);
  }
  
  return 0;
}



//=============================================================
//======================== tools ==============================
//=============================================================


void initializePoolMember(int iA)
{
  int i,j;
  
  sAppRun[iA].pszName = NULL;
  sAppRun[iA].iTimeToStart = 0;
  sAppRun[iA].pszInvocation = NULL;
  sAppRun[iA].pszRunDir = NULL;
  sAppRun[iA].iCount = 0;
  sAppRun[iA].iPID = -1;
  sAppRun[iA].iMpirunPID = -1;
  sAppRun[iA].iShellPID = -1;
  sAppRun[iA].put_to_sleep = 0;
  sAppRun[iA].active_intervals = 0;
  
  for (i = 0; i < MAX_THREAD_NUMBER; i++){
    for (j = 0; j < MAX_SIG_COUNTERS; j++){
      sAppRun[iA].aldAppsSignatures[i][j] = -1;
      sAppRun[iA].aldOldAppsSignatures[i][j] = -1;
    }
    sAppRun[iA].aiTIDs[i] = -1;
    sAppRun[iA].aiSharedCacheMasks[i] = 0;
    sAppRun[iA].aiOldSharedCacheMasks[i] = 0;
    sAppRun[iA].aiMigrated[i] = 0;
    
    sAppRun[iA].aiActualCores[i] = -1;
    sAppRun[iA].aiOldCores[i] = -1;
    sAppRun[iA].aiMouldIntervals[i] = 0;
    
    sAppRun[iA].aiCPUutil[i] = 0;
    sAppRun[iA].aiMemFootprint[i] = 0;
    sAppRun[iA].prevIdle[i] = 0;
    
    for (j = 0; j < MAX_NUMA_NODES; j++){
      sAppRun[iA].aiActualNodePages[i][j] = -1;
      sAppRun[iA].aiOldNodePages[i][j] = -1;
    }
    sAppRun[iA].aiNUMAIntervals[i] = 0;
    
    sAppRun[iA].aldOldOnlineMisses[i] = -1;
    sAppRun[iA].aldOldOnlineInstructions[i] = -1;
    
    sAppRun[iA].aldActualOnlineMisses[i] = -1;
    sAppRun[iA].aldActualOnlineInstructions[i] = -1;
    
    sAppRun[iA].aldOnlineDiskWrite[i] = 0;
    sAppRun[iA].aldOnlineDiskRead[i] = 0;
    sAppRun[iA].aldOnlineDiskWait[i] = 0;
    
    sAppRun[iA].aldOnlineTrafficSent[i] = 0;
    sAppRun[iA].aldOnlineTrafficReceived[i] = 0;
    
    
    sAppRun[iA].aldTotalDiskWrite[i] = 0;
    sAppRun[iA].aldTotalDiskRead[i] = 0;
    
    sAppRun[iA].aldTotalTrafficSent[i] = 0;
    sAppRun[iA].aldTotalTrafficReceived[i] = 0;
    
    sAppRun[iA].aiCoresLeftFrom[i] = -1;
    
    //for (j = 0; j < MAX_MOULD_TIMES; j++){
    //    sAppRun[iA].mould_moments[i][j] = -1;
    //    sAppRun[iA].core[i][j] = -1;
    //}
    
    sAppRun[iA].aiPriorities[i] = -1;
    sAppRun[iA].aiBindCores[i] = -1;
    sAppRun[iA].aiBindNodes[i] = -1;
    
    sAppRun[iA].pages_migrated[i] = 0;
    
    sAppRun[iA].aiBounced[i] = 0;
    
    sAppRun[iA].phase_change[i] = 0;
    sAppRun[iA].cpu_util_change[i] = 0;
    sAppRun[iA].old_jiffies[i] = 0;
    sAppRun[iA].to_schedule[i] = 0;
    
    sAppRun[iA].containerIDs[i] = -1;
    sAppRun[iA].aiVTIDs[i] = -1;
    sAppRun[iA].aiRecv_q[i] = 0;
    sAppRun[iA].aiSend_q[i] = 0;
  }
  
  for (i = 0; i < MAX_RUN_TIMES; i++){
    sAppRun[iA].aiExecTimes[i] = -1;
  }
  
  pthread_mutex_init(&sAppRun[iA].lock, 0);
}



// reads data from the runfile into an array
void populatePool(char* pszFilename)
{
  FILE*   file;
  char    line[MAX_LINE_SIZE + 1];
  char    signature_str[MAX_LINE_SIZE + 1];
  char*   pchReader;
  char*   pchDelimiter;
  int temp_run_times[APP_RUN_SIZE];
  int iBufferSize, tnum, i, j, runfile_no, iA, was_chosen = 0;
  long double animal_class;
  long double memory_node;
  char*   pszBuffer;
  short int ini_interval;
  
  for (i = 0; i < APP_RUN_SIZE; i++){
    temp_run_times[i]=-1;
  }
  
  queue_built = 0;
  active_devil = -1, active_turtle = -1;
  for (i = 0; i < APP_RUN_SIZE; i++){
    queue_turtles[i] = -1;
    queue_devils[i] = -1;
  }
  
  for(i = 0; i < shared_caches_number; i++){
    for(j = 0; j < cores_per_shared_cache; j++){
      old_solution[i][j][0] = -1;
      old_solution[i][j][1] = -1;
      old_solution[i][j][2] = -1;
    }
  }
  
  line[MAX_LINE_SIZE] = 0;
  
  file = fopen(pszFilename, "r");
  
  if (!file)
  {
    sprintf (line_temp, "*** ERROR: Bad filename.\n");
    log_to(LOGFILE, line_temp);
    printf ("%s", line_temp);
    exit(0);
  }
  
  interNodeMigrations = 0;
  interCoreMigrations = 0;
  devilPerc = 0.3; // 0.3
  
  old_prio_devils = 0, old_non_prio_devils = 0, old_turtles = 0;
  
  runfile_no = 0;
  
  while (fgets(line, sizeof(line), file))
  {
    if (strlen(line) > MAX_LINE_SIZE - 1)
    {
      sprintf (line_temp, "Line in file is too long.\n");
      log_to(LOGFILE, line_temp);
      exit(0);
    }
    
    if (strstr(line,"***thread") == NULL && strstr(line,"***numa thread") == NULL && strstr(line,"***priority thread") == NULL && strstr(line,"***rundir") == NULL)
    {
      was_chosen = 0;
      for (iA = 0; iA < iRunAppCount; iA++)
      {
        if (aiApps[iA] == runfile_no){
          was_chosen = 1;
          break;
        }
      }
      
      runfile_no++;
      
      if (was_chosen == 0) continue;
      
      pchReader = line;
      
      //extract name
      pchDelimiter = strchr(pchReader, ' ');
      *pchDelimiter = 0;
      
      iBufferSize = strlen(pchReader);
      pszBuffer = calloc(iBufferSize + 1, 1);
      strncpy(pszBuffer, pchReader, iBufferSize);
      sAppRun[iA].pszName = pszBuffer;
      
      //extract time to start this application
      pchReader = pchDelimiter + 1;
      
      if (iRandomLaunch == 1)
      {
        do
        {
          ini_interval = 0;
          //randomly
          i = (rand() % iRunAppCount)*(REFRESH_PERIOD/1000000+1) + 5;
          for (j = 0; j < APP_RUN_SIZE; j++){
            //                    if ( (temp_run_times[j] >= 0) && (abs(temp_run_times[j] - i) < 5) )
            if (temp_run_times[j] > 0 && temp_run_times[j] == i)
            {
              ini_interval = 1;
            }
          }
        } while (ini_interval == 1);
      }
      else
      {
        i = atoi(pchReader);
      }
      
      sprintf (line_temp, "%s sAppRun[%i] will be launched %i seconds later.\n", sAppRun[iA].pszName, iA, i);
      log_to(LOGFILE, line_temp);
      
      temp_run_times[iA] = i;
      sAppRun[iA].iTimeToStart = i + time(0);//atoi(pchReader) + time(0);
      
      //extract invocation
      pchDelimiter = strchr(pchReader, ' ');
      pchReader = pchDelimiter + 1;
      iBufferSize = strlen(pchReader);
      pszBuffer = calloc(iBufferSize + 1, 1);
      strncpy(pszBuffer, pchReader, iBufferSize);
      sAppRun[iA].pszInvocation = pszBuffer;
      
      //sprintf (line_temp, "%s, %i, %s\n", sAppRun[iA].pszName, sAppRun[iA].iTimeToStart, sAppRun[iA].pszInvocation);
      //log_to(LOGFILE, line_temp);
    }
    else if (strstr(line,"***thread") != NULL && was_chosen != 0)// this line contains a thread signature (because it begins with "thread")
    {
      /*          sscanf(line, "***thread %d [%s]", &tnum, signature_str);
       
       char *col, *brkt;
       
       col = strtok_r(signature_str," \t", &brkt);
       printf(" %s ", col);
       while (col != NULL)
       {
       col = strtok_r(NULL, " \t", &brkt);
       printf(" %s ", col);
       }
       
       */
      
      
      
      
      //          sAppRun[iA].aldAppsSignatures[tnum][0] = animal_class;
      
      sscanf(line, "***thread %d [%Lf]", &tnum, &animal_class);
      sAppRun[iA].aiBindCores[tnum] = animal_class;
      
      
      //sprintf (line_temp, "thread: sAppRun[%i].aldAppsSignatures[%i][0] == %Lf\n", iA, tnum, sAppRun[iA].aldAppsSignatures[tnum][0]);
      //log_to(LOGFILE, line_temp);
    }
    else if (strstr(line,"***numa thread") != NULL && was_chosen != 0)// this line contains a thread signature along with memory node to run it on (because it begins with "thread numa")
    {
      sscanf(line, "***numa thread %d [%Lf, %Lf]", &tnum, &animal_class, &memory_node);
      sAppRun[iA].aiBindCores[tnum] = animal_class;
      sAppRun[iA].aiBindNodes[tnum] = memory_node;
    }
    else if (strstr(line,"***priority thread") != NULL && was_chosen != 0)// this line contains a thread's priority
    {
      sscanf(line, "***priority thread %d [%Lf]", &tnum, &animal_class);
      sAppRun[iA].aiPriorities[tnum] = animal_class;
    }
    else if (strstr(line,"***rundir") != NULL && was_chosen != 0)// this line contains run directory for the application
    {
      //extract command which will be executed proir to the invocation string
      pchReader = line + strlen("***rundir ");
      iBufferSize = strlen(pchReader);
      pszBuffer = calloc(iBufferSize + 1, 1);
      strncpy(pszBuffer, pchReader, iBufferSize);
      // replacing '\n' with '\0' (otherwise chdir won't work)
      i=0;
      while ( pszBuffer[i] != '\n' ) i++;
      pszBuffer[i] = '\0';
      sAppRun[iA].pszRunDir = pszBuffer;
      
      //sprintf (line_temp, "rundir: sAppRun[%i].pszRunDir == %s\n", iA, sAppRun[iA].pszRunDir);
      //log_to(LOGFILE, line_temp);
    }
  }
  fclose(file);
}



//=============================================================




void updateTraffic()
{
  FILE*   file;
  char    line[MAX_LINE_SIZE + 1];
  char * col = calloc(64, 1);
  int cur_tid, cur_pid, i, j, old_process, iA, vnetID, cur_vnetID;
  long double net_rcvd, net_snt;
  long int ld_net_rcvd, ld_net_snt;
  char vm_name_str[MAX_LINE_SIZE+5];
  
  if (ADD_VM_NAME_TO_RESOURCE_VECTOR == 1)
  {
    for(i = 0; i < iRunAppCount; i++){
      if (sAppRun[i].iPID > 0)
      {
        // get the vm name
        if (get_vm_name(sAppRun[i].iPID) != NULL)
          sprintf(vm_name_str, "%s\0", get_vm_name(sAppRun[i].iPID));
        else
          continue;
        
        // get the virtual interface
        sprintf(line, "/var/run/libvirt/qemu/%s.xml", vm_name_str);
        file = fopen(line, "r");
        if(file==NULL) {
          sprintf (line_temp, "*** WARNING: can't open vm xml file %s.\n", line);
          log_to(LOGFILE, line_temp);
          continue;
        }
        
        while (fgets(line, sizeof(line), file))
        {
          if (sscanf(line, " <target dev='vnet%d'/>", &vnetID) == 1) break;
        }
        fclose (file);
        
        // get the traffic on the given vnet interface
        sprintf(line, "/proc/net/dev");
        file = fopen(line, "r");
        if(file==NULL) {
          sprintf (line_temp, "*** WARNING: can't open file %s.\n", line);
          log_to(LOGFILE, line_temp);
          continue;
        }
        
        while (fgets(line, sizeof(line), file))
        {
          if (sscanf(line, " vnet%d: %ld %*ld %*ld %*ld %*ld %*ld %*ld %*ld %ld %*ld %*ld %*ld %*ld %*ld %*ld %*ld", &cur_vnetID, &ld_net_rcvd, &ld_net_snt) == 3 && cur_vnetID == vnetID) break;
        }
        fclose (file);
        
        //printf("vnet of %s: vnet%d, rcv: %ld, tx: %ld\n", vm_name_str, vnetID, ld_net_rcvd, ld_net_snt);
        
        sAppRun[i].aldOnlineTrafficSent[0] = ld_net_snt - sAppRun[i].aldTotalTrafficSent[0];
        sAppRun[i].aldTotalTrafficSent[0] += sAppRun[i].aldOnlineTrafficSent[0];
        
        sAppRun[i].aldOnlineTrafficReceived[0] = ld_net_rcvd - sAppRun[i].aldTotalTrafficReceived[0];
        sAppRun[i].aldTotalTrafficReceived[0] += sAppRun[i].aldOnlineTrafficReceived[0];
      }
    }
  }
  else
  {
    file = fopen(NETHOGSFILE, "r");
    
    if (!file)
    {
      sprintf (line_temp, "*** ERROR: Bad nethogs filename.\n");
      log_to(LOGFILE, line_temp);
      printf ("%s", line_temp);
      return;
    }
    
    while (fgets(line, sizeof(line), file))
    {
      col = simply_get_col(line, 0);
      cur_tid = atol(col);
      
      col = simply_get_col(line, 1);
      net_snt = atof(col);
      
      col = simply_get_col(line, 2);
      net_rcvd = atof(col);
      
      for(i = 0; i < iRunAppCount; i++){
        for(j = 0; j < MAX_THREAD_NUMBER; j++){
          if (sAppRun[i].aiTIDs[j] == cur_tid)
          {
            sAppRun[i].aldOnlineTrafficSent[j] = net_snt;
            sAppRun[i].aldTotalTrafficSent[j] += sAppRun[i].aldOnlineTrafficSent[j];
            
            sAppRun[i].aldOnlineTrafficReceived[j] = net_rcvd;
            sAppRun[i].aldTotalTrafficReceived[j] += sAppRun[i].aldOnlineTrafficReceived[j];
            
            //printf("%d    sent: %Lf    receive: %Lf\n", sAppRun[i].aiTIDs[j], sAppRun[i].aldOnlineTrafficSent[j], sAppRun[i].aldOnlineTrafficReceived[j]);
          }
        }
      }
      
      if ( ADD_NETHOGS_TO_APP_ARRAY == 1   &&   (net_snt > NETWORK_TRAFFIC_HORIZON || net_rcvd > NETWORK_TRAFFIC_HORIZON) )
      {
        // get the PID from TID
        cur_pid = get_pid_from_tid(cur_tid);
        if (cur_pid < 0)
        {
          sprintf (line_temp, "*** ERROR: cur_pid < 0 in updateTraffic()!\n");
          log_to(LOGFILE, line_temp);
          break;
        }
        
        old_process = 0;
        // check if a process is already detected
        for (iA=0; iA<iRunAppCount; iA++)
        {
          if (sAppRun[iA].iPID == cur_pid)
          {
            old_process = 1;
            break;
          }
        }
        
        if (old_process == 0)
        {
          add_iA_to_sAppRun(cur_pid);
        }
      }
    }
    
    fclose (file);
  }
}




//=============================================================



int checkThreads(int iA){
  int i=0, j, k, i_scan, curiTID, main_thread_terminated=0, curpid, curtid, curpsr, containerID, VTID, count, need_to_log_numa, jiffy_delta, local_old_jiffies, max_was_scheduled, candidate_to_schedule;
  short int old_thread, workload_has_changed = 0;
  char statline[MAX_LINE_SIZE+5];
  char statline_numa[MAX_LINE_SIZE+5];
  char statusline[MAX_LINE_SIZE+5];
  int numa_node_pages[MAX_NUMA_NODES];
  FILE *procfile;
  struct direct **files;
  char * str_begin; char * str_end;
  
  short int   activeTIDs[MAX_THREAD_NUMBER];
  for (i=0; i<MAX_THREAD_NUMBER; i++)
  {
    activeTIDs[i] = 0;
  }
  
  sprintf(statline, "/proc/%i/task/", sAppRun[iA].iPID);
  count = scandir(statline, &files, file_select, NULL);
  
  if  (count <= 0){
    // if the main thread of the application (PID == TID) has completed execution during the last tick then we restart this application (because the end of the main thread means that all threads of this multithreaded application will be also terminated)
    main_thread_terminated = 1;
  }
  else {
    i_scan=0;
    while (   ( MPI_WORKLOAD == 1 && iA <= MAX_MPI_APP && get_child_id(sAppRun[iA].iPID, i_scan) != 0 )   ||   ( (MPI_WORKLOAD == 0 || iA > MAX_MPI_APP) && i_scan < count )   ) {
      old_thread = 0;
      if (MPI_WORKLOAD == 0 || iA > MAX_MPI_APP)
      {
        curpid = sAppRun[iA].iPID;
        curtid = atoi(files[i_scan]->d_name); // get tid
      }
      else
      {
        curpid = curtid = get_child_id(sAppRun[iA].iPID, i_scan); // get tid
      }
      
      // get the assigned core
      sprintf(statline, "/proc/%i/task/%d/stat", curpid, curtid);
      procfile = fopen(statline, "r");
      if(procfile==NULL) {
        sprintf (line_temp, "*** WARNING: can't open stat file %s.\n", statline);
        log_to(LOGFILE, line_temp);
        if  (--count <= 0){
          // if the main thread of the application (PID == TID) has completed execution during the last tick then we restart this application (because the end of the main thread means that all threads of this multithreaded application will be also terminated)
          main_thread_terminated = 1;
          break;
        }
        continue;
      }
      
      fgets(statline,sizeof(statline),procfile);
      fclose (procfile);
      
      // check if zombie
      if (get_col(statline, 2, 1, 0))
      {
        sprintf (line_temp, "*** WARNING: wait for zombie %i has started.\n", curtid);
        log_to(LOGFILE, line_temp);
        
        waitpid(curtid, NULL, 0);
        
        sprintf (line_temp, "*** WARNING: wait for zombie %i has finished.\n", curtid);
        log_to(LOGFILE, line_temp);
        
        if  (--count <= 0){
          // if the main thread of the application (PID == TID) has completed execution during the last tick then we restart this application (because the end of the main thread means that all threads of this multithreaded application will be also terminated)
          main_thread_terminated = 1;
          break;
        }
        continue;
      }
      
      curpsr = get_col(statline, 38, 0, 0);
      if (curpsr < 0)
      {
        if  (--count <= 0){
          // if the main thread of the application (PID == TID) has completed execution during the last tick then we restart this application (because the end of the main thread means that all threads of this multithreaded application will be also terminated)
          main_thread_terminated = 1;
          break;
        }
        continue;
      }
      
      containerID = -1;
      VTID = -1;
      if (ADD_CONTAINER_ID_TO_RESOURCE_VECTOR == 1)
      {
        containerID = get_col(statline, 52, 0, 0);
        
        sprintf(statusline, "/proc/%i/task/%d/status", curpid, curtid);
        procfile = fopen(statusline, "r");
        if(procfile==NULL) {
          sprintf (line_temp, "*** WARNING: can't open status file %s.\n", statusline);
          log_to(LOGFILE, line_temp);
          if  (--count <= 0){
            // if the main thread of the application (PID == TID) has completed execution during the last tick then we restart this application (because the end of the main thread means that all threads of this multithreaded application will be also terminated)
            main_thread_terminated = 1;
            break;
          }
          continue;
        }
        
        while (fgets(statusline, sizeof(statusline), procfile) != NULL) {
          if (strcmp(simply_get_col(statusline, 0), "VPid:") == 0)
          {
            VTID = atol(simply_get_col(statusline, 1));
            break;
          }
        }
        fclose (procfile);

        if (containerID < 0 || VTID < 0)
        {
          if  (--count <= 0){
            // if the main thread of the application (PID == TID) has completed execution during the last tick then we restart this application (because the end of the main thread means that all threads of this multithreaded application will be also terminated)
            main_thread_terminated = 1;
            break;
          }
          continue;
        }
      }
      
      
      local_old_jiffies = 0; curiTID = -1;
      // check if current thread was spawned before
      for (i=0; i<MAX_THREAD_NUMBER; i++)
      {
        if (sAppRun[iA].aiTIDs[i] == curtid)
        {
          old_thread = 1;
          activeTIDs[i] = 1;
          sAppRun[iA].aiActualCores[i] = curpsr;
          sAppRun[iA].containerIDs[i] = containerID;
          sAppRun[iA].aiVTIDs[i] = VTID;
          //local_old_jiffies = sAppRun[iA].old_jiffies[i];
          curiTID = i;
          break;
        }
      }
      
      //jiffy_delta = get_col(statline, 13, 0, 0) - local_old_jiffies;
      //if (jiffy_delta != 0) printf("jiffy_delta %i   curtid %i   local_old_jiffies %i\r\n", jiffy_delta, curtid, local_old_jiffies);
      candidate_to_schedule = 0;
      for (i=0; i<(APP_RUN_SIZE*MAX_THREAD_NUMBER); i++)
      {
        if (aiComputeBoundTIDs[i] == curtid || aiRankTIDs[i] == curtid)
        {
          candidate_to_schedule = 1;
          break;
        }
      }
      
      //if (jiffy_delta < 10)
      if (candidate_to_schedule == 0)
      {
        if (curiTID >= 0)
        {
          // do not consider this thread as a valid one since it is not CPU intensive
          sAppRun[iA].cpu_util_change[curiTID]++;
          if (sAppRun[iA].cpu_util_change[curiTID] == 5)
          {
            sAppRun[iA].cpu_util_change[curiTID] = 0;
            sAppRun[iA].to_schedule[curiTID] = abs(sAppRun[iA].to_schedule[curiTID]) - 2*abs(sAppRun[iA].to_schedule[curiTID]);
          }
        }
        else sAppRun[iA].to_schedule[curiTID] = 0;
      }
      else
      {
        if (curiTID >= 0)
        {
          sAppRun[iA].cpu_util_change[curiTID] = 0;
          if (sAppRun[iA].to_schedule[curiTID] == 0)
          {
            // find out on which place this thread started
            max_was_scheduled = 0;
            for (i=0; i<MAX_THREAD_NUMBER; i++)
            {
              if (abs(sAppRun[iA].to_schedule[i]) > max_was_scheduled) max_was_scheduled = abs(sAppRun[iA].to_schedule[i]);
            }
            sAppRun[iA].to_schedule[curiTID] = max_was_scheduled + 1;
          }
          else
          {
            sAppRun[iA].to_schedule[curiTID] = abs(sAppRun[iA].to_schedule[curiTID]);
          }
        }
      }
      
      
      // if its a new thread then adding its TID to the aiTIDs array and perform regular assignment for it
      if (old_thread == 0)
      {
        for (i=0; i<MAX_THREAD_NUMBER; i++)
        {
          // looking for a free spot...
          if (sAppRun[iA].aiTIDs[i] == -1)
          {
            sAppRun[iA].aiTIDs[i] = curtid;
            sAppRun[iA].aiActualCores[i] = curpsr;
            sAppRun[iA].containerIDs[i] = containerID;
            sAppRun[iA].aiVTIDs[i] = VTID;
            
            activeTIDs[i] = 1;
            curiTID = i;
            
            sprintf (line_temp, "%s sAppRun[%i].aiTIDs[%i] (#%i): thread starts: sAppRun[%i].aiTIDs[%i] == %i\n", sAppRun[iA].pszName, iA, i, sAppRun[iA].iCount, iA, i, sAppRun[iA].aiTIDs[i]);
            log_to(LOGFILE, line_temp);
            log_to(NUMAFILE, line_temp);
            log_to(MOULDFILE, line_temp);
            log_to(SYSTEMWIDEFILE, line_temp);
            
            if (fSchedulerMode == 1)
            {
              //int core_to_bind_to = (int)sAppRun[iA].aldAppsSignatures[i][0], mask = -1;
              //sAppRun[iA].aiSharedCacheMasks[i] = cpuToMask(core_to_bind_to);
              
              if (last_core_to_bind++ >= shared_caches_number*cores_per_shared_cache) last_core_to_bind = 0;
              if (sAppRun[iA].aiBindCores[i] < 0) sAppRun[iA].aiBindCores[i] = last_core_to_bind;
              sAppRun[iA].aiSharedCacheMasks[i] = cpuToMask(last_core_to_bind);
              bindThread(iA, i);
            }
            else if (fSchedulerMode == 11)
            {
              sAppRun[iA].aiSharedCacheMasks[i] = cpuToMask(sAppRun[iA].aiBindCores[i]);
              bindThread(iA, i);
            }
            else if (fSchedulerMode == 2) doRegularAssignment(iA, i);
            break;
          }
        }
        workload_has_changed = 1;
      }
      
      //if (jiffy_delta < 0) sAppRun[iA].old_jiffies[curiTID] = 0;
      // else sAppRun[iA].old_jiffies[curiTID] = local_old_jiffies + jiffy_delta;
      
      i_scan++;
    }
  }
  
  if (iRestart == 1 || sAppRun[iA].iCount == 1 || DETECT_ONLINE == 1)
  {
    if (main_thread_terminated)
    {
      sAppRun[iA].aiExecTimes[sAppRun[iA].iCount-1] = time(0) - sAppRun[iA].aiExecTimes[sAppRun[iA].iCount-1];
      
      sprintf (line_temp, "%s sAppRun[%i] (#%i): end of a run was detected: sAppRun[%i].iPID == %i. Execution time: %i sec.\n", sAppRun[iA].pszName, iA, sAppRun[iA].iCount, iA, sAppRun[iA].iPID, sAppRun[iA].aiExecTimes[sAppRun[iA].iCount-1]);
      log_to(LOGFILE, line_temp);
      log_to(NUMAFILE, line_temp);
      log_to(MOULDFILE, line_temp);
      log_to(SYSTEMWIDEFILE, line_temp);
      
      sAppRun[iA].iPID = -1;
      
      if (active_devil == iA) active_devil = -1;
      if (active_turtle == iA) active_turtle = -1;
      //printf ("%i: iA == %i finished\n", mould_time, iA);
      
      for (i=0; i<MAX_THREAD_NUMBER; i++)
      {
        if (sAppRun[iA].aiTIDs[i] > 0)
        {
          sprintf (line_temp, "%s sAppRun[%i].aiTIDs[%i] [mpirun was %d] (#%i): thread finishes due to application's termination: sAppRun[%i].aiTIDs[%i] == %i. Total sent: %Lf, total received: %Lf. Total disk wrote: %Lf, total disk read: %Lf.\n", sAppRun[iA].pszName, iA, i, sAppRun[iA].iMpirunPID, sAppRun[iA].iCount, iA, i, sAppRun[iA].aiTIDs[i], sAppRun[iA].aldTotalTrafficSent[i], sAppRun[iA].aldTotalTrafficReceived[i], sAppRun[iA].aldTotalDiskWrite[i], sAppRun[iA].aldTotalDiskRead[i]);
          log_to(LOGFILE, line_temp);
          log_to(NUMAFILE, line_temp);
          log_to(MOULDFILE, line_temp);
          log_to(SYSTEMWIDEFILE, line_temp);
          
          if (fSchedulerMode == 2) doDecrement(iA, i);
          
          sAppRun[iA].aiTIDs[i] = -2;
          sAppRun[iA].phase_change[i] = 0;
          sAppRun[iA].cpu_util_change[i] = 0;
          sAppRun[iA].old_jiffies[i] = 0;
          sAppRun[iA].to_schedule[i] = 0;
          
          sAppRun[iA].aiCoresLeftFrom[i] = sAppRun[iA].aiActualCores[i];
          
          workload_has_changed = 1;
        }
        
        sAppRun[iA].aiSharedCacheMasks[i] = 0;
        sAppRun[iA].aiActualCores[i] = -1;
        for (j = 0; j < MAX_NUMA_NODES; j++){
          sAppRun[iA].aiActualNodePages[iA][j] = -1;
        }
      }
    }
    else
    {
      // check for completed threads and if any then changing aiTIDs[iA][i] from tid to -tid as an indication that this thread has terminated
      for (i=0; i<MAX_THREAD_NUMBER; i++)
      {
        if (activeTIDs[i] == 0 && sAppRun[iA].aiTIDs[i] > 0)
        {
          sprintf (line_temp, "%s sAppRun[%i].aiTIDs[%i] [mpirun was %d] (#%i): thread finishes: sAppRun[%i].aiTIDs[%i] == %i. Total sent: %Lf, total received: %Lf. Total disk wrote: %Lf, total disk read: %Lf.\n", sAppRun[iA].pszName, iA, i, sAppRun[iA].iMpirunPID, sAppRun[iA].iCount, iA, i, sAppRun[iA].aiTIDs[i], sAppRun[iA].aldTotalTrafficSent[i], sAppRun[iA].aldTotalTrafficReceived[i], sAppRun[iA].aldTotalDiskWrite[i], sAppRun[iA].aldTotalDiskRead[i]);
          log_to(LOGFILE, line_temp);
          log_to(NUMAFILE, line_temp);
          log_to(MOULDFILE, line_temp);
          log_to(SYSTEMWIDEFILE, line_temp);
          
          sAppRun[iA].aiTIDs[i] -= 2*sAppRun[iA].aiTIDs[i];
          sAppRun[iA].phase_change[i] = 0;
          sAppRun[iA].cpu_util_change[i] = 0;
          //sAppRun[iA].old_jiffies[i] = 0; // don't nullify here - 0 only when the whole program terminates
          sAppRun[iA].to_schedule[i] = 0;
          
          sAppRun[iA].aiSharedCacheMasks[i] = 0;
          sAppRun[iA].aiActualCores[i] = -1;
          for (j = 0; j < MAX_NUMA_NODES; j++){
            sAppRun[iA].aiActualNodePages[iA][j] = -1;
          }
          
          if (fSchedulerMode == 2) doDecrement(iA, i);
          
          workload_has_changed = 1;
        }
      }
    }
  }
  
  return workload_has_changed;
}


