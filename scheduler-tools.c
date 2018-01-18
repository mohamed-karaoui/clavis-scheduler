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
//======================== tools ==============================
//=============================================================



//=============================================================



void InitScheduler(void)
{
  int i, j;
  for (i=0; i<shared_caches_number; i++)
  {
    for (j=0; j<6; j++)
    {
      aiSharedCacheCounters[i][j] = 0;
    }
    for (j=0; j<cores_per_shared_cache; j++)
    {
      aiSharedCacheCores[i][j] = 0;
    }
  }
  
  for (i=0; i<MAX_CORE_ID; i++)
  {
    sample_instr[i] = 0;
    for (j=0; j<12; j++)
    {
      sample_counters[i][j] = 0;
    }
  }
  
  for (i=0; i<MAX_CHIP_ID; i++)
  {
    for (j=0; j<32; j++)
    {
      chip_wide_counters[i][j] = 0;
    }
  }
  
  for (i=0; i<(APP_RUN_SIZE*MAX_THREAD_NUMBER); i++)
  {
    aiComputeBoundTIDs[i] = -1;
    aiRankTIDs[i] = -1;
  }
  
  last_core_to_bind = -1;
  
  if (strcmp(pszHostname, "Xeon_X5365_2") == 0)
  {
    // Xeon_X5365  2 chips
    cores[0][0] = 1;
    cores[0][1] = 3;
    cores[1][0] = 4;
    cores[1][1] = 6;
  }
  else if (strcmp(pszHostname, "Xeon_X5365_4") == 0)
  {
    // cores are in that exact order for power clustering (fSchedulerMode == 8) - so that caches on the same physical package will be together
    // Xeon_X5365  4 chips
    cores[0][0] = 0;
    cores[0][1] = 2;
    cores[1][0] = 4;
    cores[1][1] = 6;
    cores[2][0] = 1;
    cores[2][1] = 3;
    cores[3][0] = 5;
    cores[3][1] = 7;
  }
  else if (strcmp(pszHostname, "Xeon_E5320") == 0)
  {
    // Xeon_E5320 2 chips
    cores[0][0] = 0;
    cores[0][1] = 1;
    cores[1][0] = 2;
    cores[1][1] = 3;
  }
  else if (strcmp(pszHostname, "Opteron_2350_4cores") == 0)
  {
    // Opteron_2350_4cores  2 chips
    cores[0][0] = 0;
    cores[0][1] = 2;
    cores[0][2] = 4;
    cores[0][3] = 6;
    cores[1][0] = 1;
    cores[1][1] = 3;
    cores[1][2] = 5;
    cores[1][3] = 7;
  }
  else if (strcmp(pszHostname, "Opteron_2350_2cores") == 0)
  {
    // Opteron_2350_4cores  2 chips
    cores[0][0] = 0;
    cores[0][1] = 2;
    //cores[0][2] = 4;
    //cores[0][3] = 6;
    cores[1][0] = 1;
    cores[1][1] = 3;
    //cores[1][2] = 5;
    //cores[1][3] = 7;
  }
  else if (strcmp(pszHostname, "Opteron_8356_2") == 0)
  {
    // Opteron_8356  2 chips
    cores[0][0] = 3;
    cores[0][1] = 7;
    cores[0][2] = 11;
    cores[0][3] = 15;
    cores[1][0] = 2;
    cores[1][1] = 6;
    cores[1][2] = 10;
    cores[1][3] = 14;
  }
  else if (strcmp(pszHostname, "Opteron_8356_4") == 0)
  {
    // Opteron_8356  4 chips
    cores[0][0] = 0;
    cores[0][1] = 4;
    cores[0][2] = 8;
    cores[0][3] = 12;
    cores[1][0] = 3;
    cores[1][1] = 7;
    cores[1][2] = 11;
    cores[1][3] = 15;
    cores[2][0] = 2;
    cores[2][1] = 6;
    cores[2][2] = 10;
    cores[2][3] = 14;
    cores[3][0] = 1;
    cores[3][1] = 5;
    cores[3][2] = 9;
    cores[3][3] = 13;
  }
  else if (strcmp(pszHostname, "Opteron_2435_2nodes") == 0)
  {
    // Opteron_2435_2nodes 2 chips
    cores[0][0] = 0;
    cores[0][1] = 2;
    cores[0][2] = 4;
    cores[0][3] = 6;
    cores[0][4] = 8;
    cores[0][5] = 10;
    cores[1][0] = 1;
    cores[1][1] = 3;
    cores[1][2] = 5;
    cores[1][3] = 7;
    cores[1][4] = 9;
    cores[1][5] = 11;
  }
  else if (strcmp(pszHostname, "Opteron_2435_1node") == 0)
  {
    // Opteron_2435 1 chip 1node
    cores[0][0] = 0;
    cores[0][1] = 2;
    cores[0][2] = 4;
    cores[0][3] = 6;
    cores[0][4] = 8;
    cores[0][5] = 10;
  }
  else if (strcmp(pszHostname, "Opteron_8435") == 0)
  {
    // Opteron_8435 4 chips
    cores[0][0] = 0;
    cores[0][1] = 4;
    cores[0][2] = 8;
    cores[0][3] = 12;
    cores[0][4] = 16;
    cores[0][5] = 20;
    cores[1][0] = 3;
    cores[1][1] = 7;
    cores[1][2] = 11;
    cores[1][3] = 15;
    cores[1][4] = 19;
    cores[1][5] = 23;
    cores[2][0] = 2;
    cores[2][1] = 6;
    cores[2][2] = 10;
    cores[2][3] = 14;
    cores[2][4] = 18;
    cores[2][5] = 22;
    cores[3][0] = 1;
    cores[3][1] = 5;
    cores[3][2] = 9;
    cores[3][3] = 13;
    cores[3][4] = 17;
    cores[3][5] = 21;
  }
  else if (strcmp(pszHostname, "Opteron_6164") == 0)
  {
    // Opteron_6164 4 chips
    cores[0][0] = 0;
    cores[0][1] = 1;
    cores[0][2] = 2;
    cores[0][3] = 3;
    cores[0][4] = 4;
    cores[0][5] = 5;
    cores[1][0] = 6;
    cores[1][1] = 7;
    cores[1][2] = 8;
    cores[1][3] = 9;
    cores[1][4] = 10;
    cores[1][5] = 11;
    cores[2][0] = 18;
    cores[2][1] = 19;
    cores[2][2] = 20;
    cores[2][3] = 21;
    cores[2][4] = 22;
    cores[2][5] = 23;
    cores[3][0] = 12;
    cores[3][1] = 13;
    cores[3][2] = 14;
    cores[3][3] = 15;
    cores[3][4] = 16;
    cores[3][5] = 17;
  }
  else if (strcmp(pszHostname, "Xeon_X5570_woHT") == 0)
  {
    // Xeon_X5570_woHT 2 chips HT is off
    cores[0][0] = 0;
    cores[0][1] = 1;
    cores[0][2] = 2;
    cores[0][3] = 3;
    cores[1][0] = 4;
    cores[1][1] = 5;
    cores[1][2] = 6;
    cores[1][3] = 7;
  }
  else if (strcmp(pszHostname, "Xeon_X5570_HT") == 0)
  {
    // Xeon_X5570_woHT 2 chips HT is on
    cores[0][0] = 0;
    cores[0][1] = 1;
    cores[0][2] = 2;
    cores[0][3] = 3;
    cores[0][4] = 4;
    cores[0][5] = 5;
    cores[0][6] = 6;
    cores[0][7] = 7;
    cores[1][0] = 8;
    cores[1][1] = 9;
    cores[1][2] = 10;
    cores[1][3] = 11;
    cores[1][4] = 12;
    cores[1][5] = 13;
    cores[1][6] = 14;
    cores[1][7] = 15;
  }
  else if (strcmp(pszHostname, "Xeon_X5650_woHT") == 0)
  {
    // Xeon_X5650_woHT 2 chips HT is off
    cores[0][0] = 0;
    cores[0][1] = 2;
    cores[0][2] = 4;
    cores[0][3] = 6;
    cores[0][4] = 8;
    cores[0][5] = 10;
    cores[1][0] = 1;
    cores[1][1] = 3;
    cores[1][2] = 5;
    cores[1][3] = 7;
    cores[1][4] = 9;
    cores[1][5] = 11;
  }
  else if (strcmp(pszHostname, "Xeon_X5650_HT") == 0)
  {
    // Xeon_X5650_woHT 2 chips HT is on
    cores[0][0] = 0;
    cores[0][1] = 2;
    cores[0][2] = 4;
    cores[0][3] = 6;
    cores[0][4] = 8;
    cores[0][5] = 10;
    cores[0][6] = 12;
    cores[0][7] = 14;
    cores[0][8] = 16;
    cores[0][9] = 18;
    cores[0][10] = 20;
    cores[0][11] = 22;
    cores[1][0] = 1;
    cores[1][1] = 3;
    cores[1][2] = 5;
    cores[1][3] = 7;
    cores[1][4] = 9;
    cores[1][5] = 11;
    cores[1][6] = 13;
    cores[1][7] = 15;
    cores[1][8] = 17;
    cores[1][9] = 19;
    cores[1][10] = 21;
    cores[1][11] = 23;
  }
  
  
  for (i=0; i<MAX_SHARED_CACHES_NUMBER; i++)
  {
    for (j=0; j<MAX_CORES_PER_SHARED_CACHE; j++)
    {
      masks[i][j] = cpuToMask(cores[i][j]);
    }
  }
}



//=============================================================



static inline int val_to_char(int v)
{
  if (v >= 0 && v < 10)
    return '0' + v;
  else if (v >= 10 && v < 16)
    return ('a' - 10) + v;
  else 
    return -1;
}


static char * cpuset_to_str(cpu_set_t *mask, char *str)
{
  int base;
  char *ptr = str;
  char *ret = 0;
  
  for (base = CPU_SETSIZE - 4; base >= 0; base -= 4) {
    char val = 0;
    if (CPU_ISSET(base, mask))
      val |= 1;
    if (CPU_ISSET(base + 1, mask))
      val |= 2;
    if (CPU_ISSET(base + 2, mask))
      val |= 4;
    if (CPU_ISSET(base + 3, mask))
      val |= 8;
    if (!ret && val)
      ret = ptr;
    *ptr++ = val_to_char(val);
  }
  *ptr = 0;
  return ret ? ret : ptr - 1;
}


int migrate_memory(pid_t pid, int oldNode, int newNode)
{
  if (pid <= 0 || oldNode < 0 || newNode < 0) return 1;
  
  if (MIGRATE_WHOLE_MEMORY_TYPE == 1)
  {
    struct bitmask *fromnodes;
    struct bitmask *tonodes;
    
    char* fromnodes_str = calloc(32, 1);
    char* tonodes_str = calloc(32, 1);
    
    sprintf(fromnodes_str, "%i", oldNode);
    sprintf(tonodes_str, "%i", newNode);
    
    fromnodes = numa_parse_nodestring(fromnodes_str);
    if (!fromnodes) {
      printf ("<%s> is invalid\n", fromnodes_str);
    }
    tonodes = numa_parse_nodestring(tonodes_str);
    if (!tonodes) {
      printf ("<%s> is invalid\n", tonodes_str);
    }
    
    //printf("%d %#lx %#lx\n", pid, *(fromnodes->maskp), *(tonodes->maskp));
    
    if (numa_migrate_pages(pid, fromnodes, tonodes) < 0)
    {
      //perror("migrate_pages");
      int err_code = errno;
      sprintf (line_temp, "*** ERROR: page migration failed for process %i: %s\n", pid, strerror(err_code));
      log_to(LOGFILE, line_temp);
      return -1;
    }
    else
    {
      sprintf (line_temp, "page migration succeeded for process %i\n", pid);
      log_to(LOGFILE, line_temp);
    }
  }
  else
  {
    char* invoke_str2 = calloc(32, 1);
    sprintf(invoke_str2, "migratepages %i %i %i\0", pid, oldNode, newNode);
    
    //printf("migratepages %i %i %i\n", sAppRun[iA].aiTIDs[iT], oldNode, newNode);
    
    char *cmd[] = {"sh", "-c", invoke_str2, NULL};
    
    int iResult = vfork();
    
    if (iResult < 0)
    {
      int err_code = errno;
      sprintf (line_temp, "*** ERROR: fork failed for process %i:\n%s\n", pid, strerror(err_code));
      log_to(LOGFILE, line_temp);
      
      return iResult;
    }
    else if (iResult)
    {
      return 0;
    }
    
    // the code of a copy of parent process is being replaced by the code of the new application
    if (execvp("sh", cmd) < 0){
      int err_code = errno;
      sprintf (line_temp, "*** ERROR: exec failed for process %i:\n%s\n", pid, strerror(err_code));
      log_to(LOGFILE, line_temp);
    }
  }
  
  return 0;
}



//=============================================================



// binds the thread to the core using the mask
int bindThread(int iA, int iT)
{
  //usleep(REFRESH_PERIOD);
  
  /*    char* invoke_str = calloc(32, 1);
   sprintf(invoke_str, "/usr/bin/taskset -p 0x%X %i\0", sAppRun[iA].aiSharedCacheMasks[iT], sAppRun[iA].aiTIDs[iT]);
   while (system(invoke_str) < 0)
   {
   int err_code = errno;
   printf ("*** ERROR: system(%s) failed for process %i with the intended mask %i: %s\n", invoke_str, sAppRun[iA].aiTIDs[iT], sAppRun[iA].aiSharedCacheMasks[iT], strerror(err_code));
   usleep(REFRESH_PERIOD);
   }
   else
   {
   sAppRun[iA].aiMigrated[iT]++;
   return 0;
   }
   */
  /*    int core_to_bind_to = (int)sAppRun[iA].aldAppsSignatures[iT][0];
   cpu_set_t set;
   CPU_ZERO(&set);
   
   int i;
   
   for(i = 0; i < CPU_SETSIZE; i++){
   if (i == core_to_bind_to)
   {
   CPU_SET(i, &set);
   }
   else
   {
   CPU_CLR(i, &set);
   }                
   }
   */
  
  
  /*    
   char mstr[1 + CPU_SETSIZE / 4];
   cpu_set_t check_mask;
   
   if (sched_getaffinity(sAppRun[iA].aiTIDs[iT], sizeof (check_mask), &check_mask) < 0) {
   perror("sched_getaffinity");
   printf("failed to get pid %d's affinity\n");
   }
   else
   {
   printf ("New cpu affinity mask check %s\n", cpuset_to_str(&check_mask, mstr));
   }         
   
   int set_mempolicy(MPOL_INTERLEAVE, 3, 2);
   */
  
  int oldCore = sAppRun[iA].aiActualCores[iT];
  
  if (sAppRun[iA].aiTIDs[iT] <= 0 || sAppRun[iA].aiSharedCacheMasks[iT] <= 0) return -1;
  
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(maskToCPU(sAppRun[iA].aiSharedCacheMasks[iT]), &mask);
  
  if (  sched_setaffinity(sAppRun[iA].aiTIDs[iT], sizeof(mask), &mask) < 0  ){
    //    if (  sched_setaffinity(sAppRun[iA].aiTIDs[iT], sizeof(sAppRun[iA].aiSharedCacheMasks[iT]), &(sAppRun[iA].aiSharedCacheMasks[iT])) < 0  ){
    int err_code = errno;
    sprintf (line_temp, "*** ERROR: sched_setaffinity failed for process %i with the intended mask %i: %s\n", sAppRun[iA].aiTIDs[iT], sAppRun[iA].aiSharedCacheMasks[iT], strerror(err_code));
    log_to(LOGFILE, line_temp);
    return -1;
  }
  else
  {
    sAppRun[iA].aiMigrated[iT]++;
    //sprintf (line_temp, "%s sAppRun[%i].aiTIDs[%i] == %i: migrated %i-th time. New cpu affinity mask 0x%X\n", sAppRun[iA].pszName, iA, iT, sAppRun[iA].aiTIDs[iT], sAppRun[iA].aiMigrated[iT], sAppRun[iA].aiSharedCacheMasks[iT]);
    //log_to(LOGFILE, line_temp);
    
    int newCore = maskToCPU(sAppRun[iA].aiSharedCacheMasks[iT]);
    sAppRun[iA].aiActualCores[iT] = newCore;
    
    int oldNode = maskToCache(cpuToMask(oldCore));
    int newNode = maskToCache(sAppRun[iA].aiSharedCacheMasks[iT]);
    
    if (oldNode != newNode) sAppRun[iA].interNodeMigrations[iT]++;
    if (oldCore != newCore) sAppRun[iA].interCoreMigrations[iT]++;
    
    //if (oldNode == newNode && oldCore == newCore) return 0; // don't do that: unnecessary bindings are trimmed in DistributedIntensity and Queue manager, here it will remove some needed ones (like during respawn we still need to pin)
    
    sprintf (line_temp, "%s sAppRun[%i].aiTIDs[%i] == %i: oldCore %d oldNode %d     -    newCore %d newNode %d\n", sAppRun[iA].pszName, iA, iT, sAppRun[iA].aiTIDs[iT], oldCore, oldNode, newCore, newNode);
    log_to(LOGFILE, line_temp);
    
    if (   MIGRATION_TYPE == 1 && (strcmp(pszHostname, "Opteron_2350_4cores") == 0 || strcmp(pszHostname, "Opteron_2350_2cores") == 0 || strcmp(pszHostname, "Opteron_8356_2") == 0 || strcmp(pszHostname, "Opteron_8356_4") == 0 || strcmp(pszHostname, "Opteron_2435_2nodes") == 0 || strcmp(pszHostname, "Opteron_2435_1node") == 0 || strcmp(pszHostname, "Opteron_8435") == 0 || strcmp(pszHostname, "Opteron_6164") == 0 || strcmp(pszHostname, "Xeon_X5570_woHT") == 0 || strcmp(pszHostname, "Xeon_X5570_HT") == 0 || strcmp(pszHostname, "Xeon_X5650_woHT") == 0 || strcmp(pszHostname, "Xeon_X5650_HT") == 0)   )
    {
      //if (oldNode != newNode) // commented, see above
      //{
      if (migrate_memory(sAppRun[iA].aiTIDs[iT], oldNode, newNode) == 0)
      {
        sprintf (line_temp, "%s sAppRun[%i].aiTIDs[%i] == %i: memory successfully migrated as well from node %i to node %i.\n", sAppRun[iA].pszName, iA, iT, sAppRun[iA].aiTIDs[iT], oldNode, newNode);
        log_to(LOGFILE, line_temp);
      }
      //}
      
      sAppRun[iA].aiBounced[iT] = 0;
    }
  }
  
  return 0;
}



//=============================================================



// unbinds a thread from a particular core
int unbindThread(int iA, int iT)
{
  if (sAppRun[iA].aiTIDs[iT] <= 0 || sAppRun[iA].aiSharedCacheMasks[iT] <= 0) return -1;
  
  int i;
  cpu_set_t mask;
  CPU_ZERO(&mask);
  for (i = 0; i < shared_caches_number*cores_per_shared_cache; i++)
    CPU_SET(i, &mask);
  
  if (  sched_setaffinity(sAppRun[iA].aiTIDs[iT], sizeof(mask), &mask) < 0  ){
    int err_code = errno;
    sprintf (line_temp, "*** ERROR: sched_setaffinity unbind failed for process %i: %s\n", sAppRun[iA].aiTIDs[iT], strerror(err_code));
    log_to(LOGFILE, line_temp);
    return -1;
  }
  else
  {
    int oldCore = sAppRun[iA].aiActualCores[iT];
    int oldNode = maskToCache(cpuToMask(oldCore));

    sprintf (line_temp, "%s sAppRun[%i].aiTIDs[%i] == %i: oldCore %d oldNode %d     -    unbind\n", sAppRun[iA].pszName, iA, iT, sAppRun[iA].aiTIDs[iT], oldCore, oldNode);
    log_to(LOGFILE, line_temp);
  }
  return 0;
}



//=============================================================



// shows the current assignment of threads if its changed since the previous run 
int output_assignment()
{
  int iA, iT, change = 0;
  
  for(iA = 0; iA < iRunAppCount; iA++){
    for(iT = 0; iT < MAX_THREAD_NUMBER; iT++){
      if (sAppRun[iA].aiSharedCacheMasks[iT] != sAppRun[iA].aiOldSharedCacheMasks[iT])
      {
        change = 1;
        break;
      }
    }
    if (change == 1) break;
  }
  
  if (change == 1)
  {
    sprintf (line_temp, "=============================================================\n");
    log_to(LOGFILE, line_temp);
    sprintf (line_temp, "=== mould_time: %i\n", mould_time);
    log_to(LOGFILE, line_temp);
    
    for(iA = 0; iA < iRunAppCount; iA++){
      for(iT = 0; iT < MAX_THREAD_NUMBER; iT++){
        if(sAppRun[iA].aiTIDs[iT] > 0 && sAppRun[iA].to_schedule[iT] > 0){
          sprintf (line_temp, "=== %s sAppRun[%i].aiTIDs[%i] == %i, prioritized: %d, to schedule: %d; (#%i): migrations (core %i, node %i), mask 0x%X (actual core: %d), slot missrate: %Lf\n", sAppRun[iA].pszName, iA, iT, sAppRun[iA].aiTIDs[iT], sAppRun[iA].aiPriorities[sAppRun[iA].to_schedule[iT]-1], sAppRun[iA].to_schedule[iT], sAppRun[iA].iCount, sAppRun[iA].interCoreMigrations[iT], sAppRun[iA].interNodeMigrations[iT], sAppRun[iA].aiSharedCacheMasks[iT], sAppRun[iA].aiActualCores[iT], sAppRun[iA].aldOldAppsSignatures[iT][0]);
          log_to(LOGFILE, line_temp);
        }
        
        sAppRun[iA].aiOldSharedCacheMasks[iT] = sAppRun[iA].aiSharedCacheMasks[iT];
      }
    }
    
    sprintf (line_temp, "=============================================================\n");
    log_to(LOGFILE, line_temp);
  }
  
  return 0;
}



//=============================================================



// determine the mask
int cacheToMask(int cache)
{
  int mask = -1, j, min_threads = 36000, min_core = -1;
  
  for (j=0; j<cores_per_shared_cache; j++)
  {
    if (min_threads > aiSharedCacheCores[cache][j])
    {
      min_threads = aiSharedCacheCores[cache][j];
      min_core = j;
    }
  }
  
  mask = cacheCoreToMask(cache, min_core);
  
  return mask;
}



//=============================================================



// determine the mask
int cacheCoreToMask(int cache, int min_core)
{
  return masks[cache][min_core];
}



//=============================================================



// get the current cache
int maskToCache(int mask)
{
  int i, j, cache = -1;
  
  for (i=0; i<shared_caches_number; i++)
  {
    for (j=0; j<cores_per_shared_cache; j++)
    {
      if (mask == masks[i][j]) return i;
    }
  }
  
  return cache;
}



//=============================================================



int maskToCore(int mask)
{
  int i, j, core = -1;
  
  for (i=0; i<shared_caches_number; i++)
  {
    for (j=0; j<cores_per_shared_cache; j++)
    {
      if (mask == masks[i][j]) return j;
    }
  }
  
  return core;
}



//=============================================================



// get the current CPU
int maskToCPU(int mask)
{
  int cpu = -1;
  
  if (mask == 1) cpu = 0;
  else if (mask == 2) cpu = 1;
  else if (mask == 4) cpu = 2;
  else if (mask == 8) cpu = 3;
  else if (mask == 16) cpu = 4;
  else if (mask == 32) cpu = 5;
  else if (mask == 64) cpu = 6;
  else if (mask == 128) cpu = 7;
  else if (mask == 256) cpu = 8;
  else if (mask == 512) cpu = 9;
  else if (mask == 1024) cpu = 10;
  else if (mask == 2048) cpu = 11;
  else if (mask == 4096) cpu = 12;
  else if (mask == 8192) cpu = 13;
  else if (mask == 16384) cpu = 14;
  else if (mask == 32768) cpu = 15;
  else if (mask == 65536) cpu = 16;
  else if (mask == 131072) cpu = 17;
  else if (mask == 262144) cpu = 18;
  else if (mask == 524288) cpu = 19;
  else if (mask == 1048576) cpu = 20;
  else if (mask == 2097152) cpu = 21;
  else if (mask == 4194304) cpu = 22;
  else if (mask == 8388608) cpu = 23;
  
  return cpu;
}



//=============================================================



// get the current CPU
int cpuToMask(int cpu)
{
  int mask = -1;
  
  if (cpu>=0) mask = 1 << cpu;
  
  return mask;
}



//=============================================================



// this function extracts colno-th (0-based) field from ps output
// every thread of one multithreaded application will have the same pid but different tid. The first (main) thread of an application will have pid == tid
// mode == 0 - get int value for the col; mode == 1 - check if the process is a zombie; mode == 2 - check if the process is a child of a process with pid == in_ppid and if so return its pid
int get_col(char * line, int colno, int mode, int in_ppid) {
  int i=0, child_pid, ppid;
  char *col, *brkt;
  char buffer[MAX_LINE_SIZE];
  strcpy (buffer, line);
  
  col = strtok_r(buffer," \t", &brkt);
  while (col != NULL && i < colno)
  {
    if (mode == 2 && i == 0 && col) child_pid = atoi(col);
    col = strtok_r(NULL, " \t", &brkt);
    i++;
  }
  
  if (mode == 0)
  {
    if (col) i = atoi(col);
    else
    {
      i=-1;
    }
    
    return i;
  }
  else if (mode == 1)
  {
    if (col && *col == 'Z') return 1;
  }
  else if (mode == 2)
  {
    if (col) ppid = atoi(col);
    if (ppid == in_ppid) return child_pid;
  }
  
  return 0;
}



//=============================================================



// this function returns the name of the process binary given its pid
char * get_binary_name(int in_pid) {
  char statline[MAX_LINE_SIZE+5];
  FILE *procfile;
  char *result=NULL;
  char * buffer;
  
  sprintf(statline, "/proc/%i/stat", in_pid);
  procfile = fopen(statline, "r");
  if(procfile==NULL) {
    return NULL;
  }
  if (fgets(statline,sizeof(statline),procfile) == NULL) {
    fclose (procfile);
    return NULL;
  }
  fclose (procfile);
  
  buffer = simply_get_col(statline, 1);
  
  result = calloc(strlen(buffer) + 1, 1);
  strncpy (result, buffer, strlen(buffer));
  
  return result;
}



//=============================================================



// this function extracts colno-th (0-based) field from the line
char * simply_get_col(char * line, int colno) {
  if (line == NULL) return line;
  char temp_line[MAX_LINE_SIZE+1];
  if (strlen(line) > MAX_LINE_SIZE)
  {
    printf("strlen(line) %i\r\n", strlen(line));
    return NULL;
  }
  
  strncpy (temp_line, line, strlen(line));
  if (strlen(line) < strlen(temp_line)) temp_line[strlen(line)] = '\0';
  
  int i=0;
  char *col=NULL, *brkt=NULL, *result=NULL;
  char buffer[MAX_LINE_SIZE+1];
  strncpy (buffer, temp_line, strlen(temp_line));
  if (strlen(temp_line) < strlen(buffer)) buffer[strlen(temp_line)] = '\0';
  
  if (strchr(temp_line, ' ') == NULL && strchr(temp_line, '\t') == NULL) col = temp_line;
  else
  {
    col = strtok_r(buffer," \t", &brkt);
    while (col != NULL && i < colno)
    {
      col = strtok_r(NULL, " \t", &brkt);
      i++;
    }
  }
  
  if (col!=NULL)
  {
    result = calloc(strlen(col) + 1, 1);
    strncpy(result, col, strlen(col));
  }
  
  return result;
}



//=============================================================



// get which-th child of the process with ppid
int get_child_id(int ppid, int which) {
  int child_pid = 0;
  FILE *procfile;
  int i, iA, k = 0;
  char  statline[MAX_LINE_SIZE+5];
  short int found = 0;
  
  // now we need to find the process itself
  for (i=(ppid+1); i<(ppid+1000); i++){
    sprintf(statline, "/proc/%i/stat", i);
    procfile = fopen(statline, "r");
    if(procfile==NULL)
    {
      continue;
    }
    fgets(statline,sizeof(statline),procfile);
    fclose (procfile);
    
    child_pid = get_col(statline, 3, 2, ppid);
    
    for (iA = 0; iA < iRunAppCount; iA++)
    {
      if (sAppRun[iA].iPID == child_pid)
      {
        child_pid = 0;
        break;
      }
    }
    
    if (child_pid > 0 && k++ == which)
    {
      found = 1;
      break; // we found the child!
    }
  }
  
  if (found == 1) return child_pid;
  else return 0;
}



//=============================================================



// get pid of the given tid
int get_pid_from_tid(int tid) {
  FILE *procfile;
  int i, result_pid, min_pid = (tid > 1000)?(tid-1000):0;
  char  statline[MAX_LINE_SIZE+5];
  char * col = calloc(64, 1);
  //char line_temp[MAX_LINE_SIZE+5];
  
  // now we need to find the process itself, pid is always <= tid.
  for (i=tid; i>min_pid; i--){
    sprintf(statline, "/proc/%i/task/%d/status", i, tid);
    procfile = fopen(statline, "r");
    if(procfile!=NULL) {
      // will exist for every thread (even though is not shown in proc!)
      fgets(statline,sizeof(statline),procfile);
      fgets(statline,sizeof(statline),procfile);
      // third string will contain TGID == true PID (not TID)
      fgets(statline,sizeof(statline),procfile);
      fclose (procfile);
      
      col = simply_get_col(statline, 1);
      result_pid = atol(col);
      
      //sprintf (line_temp, "*** 111 /proc/%i/task/%d/stat %d pid returned by get_pid_from_tid!\n", i, tid, result_pid);
      //log_to(LOGFILE, line_temp);
      
      return result_pid;
    }
  }
  
  return -1;
}



//=============================================================



int isNumber(const char str[]){
  
  char *ok;
  
  strtod(str,&ok);
  
  return !isspace(*str) && strlen(ok)==0;
}



//=============================================================



// this function appends the line into the specified logfile
char * log_to(char * file, char * line) {
  if (line == NULL || file == NULL) return NULL;
  
  FILE *logfile = fopen (file, "a");
  if (logfile == NULL)
  {
    printf ("log_to null instead of %s, the line was: %s\n", file, line);
    printf ("Error: %s.\n", strerror(errno));
    return NULL;
  }
  fprintf (logfile, "%s", line);
  fclose (logfile);
  
  return line;
}



//=============================================================



// used with scandir throughout the code
int file_select(const struct direct *entry){
  if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) return 0;
  else return 1;
}



//=============================================================



// this function returns the name of the vm with the given PID
char * get_vm_name(int pid) {
  int i_scan, count, temp_pid;
  char statline[MAX_LINE_SIZE+5];
  struct direct **files;
  FILE*   file;
  char * dot_ptr = NULL;
  char *result=NULL;
  
  count = scandir("/var/run/libvirt/qemu/", &files, file_select, NULL);
  if  (count > 0){
    i_scan=0;
    while (i_scan < count) {
      if (strstr(files[i_scan]->d_name, ".pid") != NULL)
      {
        sprintf(statline, "/var/run/libvirt/qemu/%s", files[i_scan]->d_name);
        file = fopen(statline, "r");
        if (file) 
        {
          fgets(statline, sizeof(statline), file);
          fclose (file);
          temp_pid = atoi(statline);
          if (temp_pid == pid)
          {
            dot_ptr = strchr(files[i_scan]->d_name, '.');
            if (dot_ptr != NULL) *dot_ptr = '\0';
            
            result = calloc(strlen(files[i_scan]->d_name) + 1, 1);
            strncpy(result, files[i_scan]->d_name, strlen(files[i_scan]->d_name));
            break;
          }
        }
      }
      i_scan++;
    }
  }
  
  return result;
}
