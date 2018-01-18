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





//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++ Animalistic scheduling algorithm +++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



const int   aiSharedCacheCountersSymValues[5][5] = { {0,0,1,3,1}, {0,20,14,16,8}, {1,14,8,9,4}, {3,16,9,8,6}, {1,8,4,6,2} };
//                                                   turtle(0)       sdevil(1)       idevil(2)       rabbit(3)        sheep(4)



//=============================================================



int doRegularAssignment(int iA, int iT)
{
  doGreedy(iA, iT);
  bindThread(iA, iT);
  
  return 0;
}



//=============================================================



// preparing system for scheduling:
//  - generating new mask for a thread
//  - decrement for containing cache (if not regular assignment)
//  - assign new mask
//  - increment for winning cache
int doGreedy(int iA, int iT)
{
  int cache_won, containing_cache;
  
  cache_won = getWinningCache(iA, iT);
  containing_cache = maskToCache(sAppRun[iA].aiSharedCacheMasks[iT]);
  //printf ("cache_won %i containing_cache %i\n", cache_won, containing_cache);
  
  if (cache_won == containing_cache) return 0;
  
  // decrement for the containing cache (in case of refresh assignment)
  if (containing_cache >= 0) doDecrement(iA, iT);
  
  // assign new mask
  sAppRun[iA].aiSharedCacheMasks[iT] = cacheToMask(cache_won);
  //printf ("for process %i with the intended mask %i: cache_won %i\n", sAppRun[iA].aiTIDs[iT], sAppRun[iA].aiSharedCacheMasks[iT], cache_won);
  
  // increment for the winning cache
  doIncrement(iA, iT);
  
  return 1;
}



//=============================================================



int doRefreshAssignment(int iA)
{
  int i;
  for (i=0; i<MAX_THREAD_NUMBER; i++)
  {
    if (sAppRun[iA].aiTIDs[i] > 0 && doGreedy(iA, i)) bindThread(iA, i);
  }
  
  return 0;
}



//=============================================================


int doOptimisticAssignment(int iA)
{
  int i, partner_iA=iA, partner_iT, iT, temp;
  
  for (iT=0; iT<MAX_THREAD_NUMBER; iT++)
  {
    if (sAppRun[iA].aiTIDs[iT] > 0)
    {
      partner_iT = -1;
      
      while (partner_iA == iA)
      {
        partner_iA = rand() % iRunAppCount;
      }
      
      i = rand() % MAX_THREAD_NUMBER;
      for (; i>=0; i--)
      {
        if (sAppRun[partner_iA].aiTIDs[i] > 0)
        {
          partner_iT = i;
          break;
        }
      }
      
      if (partner_iT < 0) continue;
      
      if ( isTheCacheWinnning(iA, iT, partner_iA, partner_iT) + isTheCacheWinnning(partner_iA, partner_iT, iA, iT) < 0 )
      {
        sprintf (line_temp, "doOptimisticAssignment is launched for threads sAppRun[%i].aiTIDs[%i] (%s) and sAppRun[%i].aiTIDs[%i] (%s)\n", iA, iT,  sAppRun[iA].pszName, partner_iA, partner_iT, sAppRun[partner_iA].pszName);
        log_to(LOGFILE, line_temp);
        
        doDecrement(iA, iT);
        doDecrement(partner_iA, partner_iT);
        
        temp = sAppRun[iA].aiSharedCacheMasks[iT];
        sAppRun[iA].aiSharedCacheMasks[iT] = sAppRun[partner_iA].aiSharedCacheMasks[partner_iT];
        sAppRun[partner_iA].aiSharedCacheMasks[partner_iT] = temp;
        
        doIncrement(iA, iT);
        doIncrement(partner_iA, partner_iT);
        
        bindThread(iA, iT);
        bindThread(partner_iA, partner_iT);
      }
    }
  }
  
  return 0;
}



//=============================================================



int getWinningCache(int iA, int iT)
{
  int cache, min_class_counter_after_load_balancing = 1000000, cur_class_counter_after_load_balancing = -1, containing_class_counter_after_load_balancing = -1, cache_won = -1, count, mincount = 10000, containing_count = -1;
  int signature_class = sAppRun[iA].aldAppsSignatures[iT][0];
  int containing_cache = maskToCache(sAppRun[iA].aiSharedCacheMasks[iT]);
  
  for (cache=0; cache < shared_caches_number; cache++)
  {
    if (cache != containing_cache)
    {
      count = aiSharedCacheCounters[cache][5];
    }
    else
    {
      count = aiSharedCacheCounters[cache][5] - 1;
    }
    
    if (mincount > count) mincount = count;
  }
  
  for (cache=0; cache < shared_caches_number; cache++)
  {
    if (cache != containing_cache)
    {
      cur_class_counter_after_load_balancing = aiSharedCacheCounters[cache][signature_class];// - aiSharedCacheCounters[cache][5];
      count = aiSharedCacheCounters[cache][5];
    }
    else
    {
      cur_class_counter_after_load_balancing = aiSharedCacheCounters[cache][signature_class] - aiSharedCacheCountersSymValues[signature_class][signature_class];
      count = aiSharedCacheCounters[cache][5] - 1;
      containing_class_counter_after_load_balancing = cur_class_counter_after_load_balancing;
      containing_count = count;
      //          cur_class_counter_after_load_balancing = aiSharedCacheCounters[cache][signature_class] - aiSharedCacheCountersSymValues[signature_class][signature_class] - aiSharedCacheCounters[cache][5] + 1;
    }
    
    if (cur_class_counter_after_load_balancing < min_class_counter_after_load_balancing && count == mincount)
    {
      min_class_counter_after_load_balancing = cur_class_counter_after_load_balancing;
      cache_won = cache;
    }
  }
  
  // this is needed in order to prevent unnecessary migration
  if (containing_class_counter_after_load_balancing == min_class_counter_after_load_balancing && containing_count == mincount) cache_won = containing_cache;
  
  return cache_won;
}



//=============================================================



// determines if the shared cache of the thread dest is better for the thread source than its current cache
int isTheCacheWinnning(int iA_source, int iT_source, int iA_dest, int iT_dest)
{
  int source_class_counter, dest_class_counter;
  
  int source_containing_cache = maskToCache(sAppRun[iA_source].aiSharedCacheMasks[iT_source]);
  int source_signature_class = sAppRun[iA_source].aldAppsSignatures[iT_source][0];
  
  int dest_containing_cache = maskToCache(sAppRun[iA_dest].aiSharedCacheMasks[iT_dest]);
  int dest_signature_class = sAppRun[iA_dest].aldAppsSignatures[iT_dest][0];
  
  if (source_containing_cache == dest_containing_cache) return 0;
  
  source_class_counter = aiSharedCacheCounters[source_containing_cache][source_signature_class] - aiSharedCacheCountersSymValues[source_signature_class][source_signature_class];
  
  dest_class_counter = aiSharedCacheCounters[dest_containing_cache][source_signature_class] - aiSharedCacheCountersSymValues[source_signature_class][dest_signature_class];
  
  return (dest_class_counter - source_class_counter);
}



//=============================================================



// we need to increment counters of winning cache before placing the thread to it
int doIncrement(int iA, int iT)
{
  int mask = sAppRun[iA].aiSharedCacheMasks[iT];
  int counter, core = 0, cache_won = maskToCache(mask);
  int signature_class = sAppRun[iA].aldAppsSignatures[iT][0];
  
  //updating counters
  for (counter=0; counter<5; counter++)
  {
    aiSharedCacheCounters[cache_won][counter] += aiSharedCacheCountersSymValues[signature_class][counter];
  }
  aiSharedCacheCounters[cache_won][5]++; // load balancing: +1 thread in this shared cache
  
  core = maskToCore(mask);
  
  aiSharedCacheCores[cache_won][core]++;
  
  return 0;
}



//=============================================================



// we need to decrement counters of containing cache after thread has completed execution
int doDecrement(int iA, int iT)
{
  int mask = sAppRun[iA].aiSharedCacheMasks[iT];
  int counter, core = 0, containing_cache = maskToCache(mask);
  int signature_class = sAppRun[iA].aldAppsSignatures[iT][0];
  
  //updating counters
  for (counter=0; counter<5; counter++)
  {
    aiSharedCacheCounters[containing_cache][counter] -= aiSharedCacheCountersSymValues[signature_class][counter];
  }
  aiSharedCacheCounters[containing_cache][5]--; // load balancing: -1 thread in this shared cache
  
  core = maskToCore(mask);
  
  aiSharedCacheCores[containing_cache][core]--;
  
  return 0;
}





//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++ DI, DIO, DIP, DINO scheduling algorithms +++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



//==============================================================================================



//This is the function that is called when a change in the thread population occurs
int distributedIntensity(short int workload_has_changed){
  /* ============= Read In the Active Threads ================================ */
  //go through a global array and get the number of active threads
  int activeThreads = 0;
  int i, j, k, m, l, p, s, i2, j2, p2, pile_up, prio_devils = 0, non_prio_devils = 0, non_prio_super_devils = 0, turtles = 0, iA, iT, idle_turtles, lower_limit = 0, upper_limit = 0;
  for(i = 0; i < iRunAppCount; i++){
    for(j = 0; j < MAX_THREAD_NUMBER; j++){
      if(sAppRun[i].aiTIDs[j] > 0 && sAppRun[i].to_schedule[j] > 0){
        activeThreads++;
      }
    }
  }
  
  //if the number of active threads is zero or one return zero
  if(activeThreads < 2){
    return 0;
  }
  
  int difference, cache, core, passes;
  
  //the number of cores is:
  int numberCores = cores_per_shared_cache * shared_caches_number;
  
  //create an array to store the active threads
  int numberSlots = numberCores;
  
  //if(activeThreads < numberCores) return 1;
  
  if(activeThreads > numberCores){
    numberSlots = activeThreads;
    printf("%d\n", activeThreads);
  }
  
  long double slotArrayMissRates[numberSlots], oldMissrate, actualMissrate, averageMissrate;
  int slotArray[numberSlots][5];
  
  
  //fill the array with the active threads
  int count = 0;
  for(i = 0; i < iRunAppCount; i++){
    for(j = 0; j < MAX_THREAD_NUMBER; j++){
      //if (i == 0)
      //{
      //  lower_limit = 28; // 22 initial threads of mysqld + 1 more short thread to create table counter_mem
      //  upper_limit = 28;
      //}
      if(sAppRun[i].aiTIDs[j] > 0 && sAppRun[i].to_schedule[j] > 0){// && j > lower_limit && j < upper_limit){
        //store the miss rate (round it to prevent sudden migrations due to slight miss rate
        if (sAppRun[i].aldOldAppsSignatures[j][0] < 0) oldMissrate = 0;
        else oldMissrate = sAppRun[i].aldOldAppsSignatures[j][0];
        
        if (sAppRun[i].aldAppsSignatures[j][0] < 0) actualMissrate = 0;
        else actualMissrate = sAppRun[i].aldAppsSignatures[j][0];
        
        averageMissrate = 0;
        sAppRun[i].phase_change[j] = 0;
        
        //slotArrayMissRates[count] = actualMissrate;
        slotArrayMissRates[count] = oldMissrate;
        
        if (fSchedulerMode == 9)
        {
          for(k = 9; k >= 0; k--){
            averageMissrate += sAppRun[i].aldAppsSignatures[j][k];
            if (   oldMissrate > DEVIL_HORIZON   &&   oldMissrate < SUPER_DEVIL_HORIZON   &&   ( sAppRun[i].aldAppsSignatures[j][k] > SUPER_DEVIL_HORIZON || sAppRun[i].aldAppsSignatures[j][k] < DEVIL_HORIZON )   )
            {
              sAppRun[i].phase_change[j]++;
            }
            else if (   oldMissrate >= SUPER_DEVIL_HORIZON   &&   ( sAppRun[i].aldAppsSignatures[j][k] < SUPER_DEVIL_HORIZON )   )
            {
              sAppRun[i].phase_change[j]++;
            }
            // it does not really matter how the turtle's missrate changes as long as it remains small (below the devil limit) - we can keep it the same as before
            else if (   oldMissrate <= DEVIL_HORIZON   &&   ( sAppRun[i].aldAppsSignatures[j][k] > DEVIL_HORIZON )   )
            {
              sAppRun[i].phase_change[j]++;
            }
          }
        }
        else
        {
          for(k = 9; k >= 0; k--){
            averageMissrate += sAppRun[i].aldAppsSignatures[j][k];
            if (   oldMissrate > DEVIL_HORIZON   &&   ( sAppRun[i].aldAppsSignatures[j][k] > (1+devilPerc)*oldMissrate || sAppRun[i].aldAppsSignatures[j][k] < (1-devilPerc)*oldMissrate )   )
            {
              sAppRun[i].phase_change[j]++;
            }
            
            // it does not really matter how the turtle's missrate changes as long as it remains small (below the devil limit) - we can keep it the same as before
            if (   oldMissrate <= DEVIL_HORIZON   &&   ( sAppRun[i].aldAppsSignatures[j][k] > DEVIL_HORIZON )   )
            {
              sAppRun[i].phase_change[j]++;
            }
          }
        }
        
        averageMissrate = averageMissrate * 0.1;
        
        if (sAppRun[i].phase_change[j] >= 5 && abs(averageMissrate-oldMissrate) > 30)
        {
          slotArrayMissRates[count] = averageMissrate;
        }
        
        sAppRun[i].aldOldAppsSignatures[j][0] = slotArrayMissRates[count];
        slotArrayMissRates[count] = 10 * roundl(0.1 * slotArrayMissRates[count]);
        
        slotArray[count][0] = i; //store the index of the application
        slotArray[count][1] = j; //store the index of the thread
        if (sAppRun[i].aiPriorities[sAppRun[i].to_schedule[j]-1] == 1)
        {
          slotArray[count][2] = sAppRun[i].aiPriorities[sAppRun[i].to_schedule[j]-1]; //store the priority of the thread (only for devils)
          slotArray[count][3] = 1;
          slotArray[count][4] = -1;
          prio_devils++;
        }
        else
        {
          slotArray[count][2] = -1;
          slotArray[count][4] = -1;
          
          if (slotArrayMissRates[count] > DEVIL_HORIZON)
          {
            slotArray[count][3] = 2;
            non_prio_devils++;
          }
          else
          {
            slotArray[count][3] = 3;
            turtles++;
          }
          
          if (fSchedulerMode == 9 && slotArrayMissRates[count] > SUPER_DEVIL_HORIZON)
          {
            slotArray[count][3] = 5;
            non_prio_devils--;
            non_prio_super_devils++;
          }
          
        }
        count++; //increment the counter
      }
    }
  }
  
  //handle the case where the number of active threads is smaller than the number of cores
  //fill empty slots as having miss rate -1
  if(count < numberSlots){
    for(i = count; i < numberSlots; i++){
      slotArrayMissRates[i] = -1;
      slotArray[i][0] = -1;
      slotArray[i][1] = -1;
      slotArray[i][2] = -1;
      slotArray[i][3] = 3; // we're using free spots as turtles
      slotArray[i][4] = -1;
    }
  }
  
  /* =============== Now we sort the array of active threads ========================== */
  
  if (fSchedulerMode != 9) // dino doesn't need sorting
  {
    short int swapped;
    j = numberSlots;
    do
    {
      swapped = 0;
      j--;
      for(i = 0; i < j; i++){
        if(slotArrayMissRates[i] > slotArrayMissRates[i+1]){
          long double miss_temp = slotArrayMissRates[i];
          slotArrayMissRates[i] = slotArrayMissRates[i+1];
          slotArrayMissRates[i+1] = miss_temp;
          
          int temp[5] = {slotArray[i][0], slotArray[i][1], slotArray[i][2], slotArray[i][3], slotArray[i][4]};
          for(k = 0; k < 5; k++){
            slotArray[i][k] = slotArray[i+1][k];
            slotArray[i+1][k] = temp[k];
          }
          swapped = 1;
        }
      }
    } while (swapped == 1);
  }
  
  /* =================== Now we group the threads together and bind them to cores ======================================== */
  
  int caches[shared_caches_number][cores_per_shared_cache][2];
  int classes[shared_caches_number][cores_per_shared_cache];
  
  for(i = 0; i < shared_caches_number; i++){
    for(j = 0; j < cores_per_shared_cache; j++){
      caches[i][j][0] = -1;
      caches[i][j][1] = -1;
      classes[i][j] = -1;
    }
  }
  
  if (old_prio_devils != prio_devils || old_non_prio_devils != non_prio_devils || old_turtles != turtles)
  {
    workload_has_changed = 1;
    old_prio_devils = prio_devils;
    old_non_prio_devils = non_prio_devils;
    old_turtles = turtles;
  }
  if (fSchedulerMode == 7)
  {
    if (workload_has_changed)
    {
      if (prio_devils > 0)
      {
        int tuple = 0;
        for(i = 0; i < numberSlots; i++){
          if (slotArray[i][3] == 1) // pdevil
          {
            slotArray[i][2] = -2;
            slotArray[i][4] = tuple++;
          }
        }
        
        passes = 1; tuple = 0;
        for(i = 0; i < numberSlots; i++){
          if (slotArray[i][3] == 3) // turtle
          {
            slotArray[i][2] = -2;
            slotArray[i][4] = tuple++;
            if (tuple == prio_devils)
            {
              tuple = 0;
              if (++passes == cores_per_shared_cache) break;
            }
          }
        }
        
        // tuples are already sorted in descending order according to the number of turtles in every tuple
        
        core = 0;
        pcache = 0;
        int pcache_has_pdevil = 0; int old_pcache_has_pdevil = 0;
        for(tuple=0; tuple < prio_devils; tuple++){
          // doing this in reverse so that pdevils will be the first ones in every tuple
          for(i = (numberSlots-1); i >= 0; i--){
            if (slotArray[i][4] == tuple)
            {
              if (slotArray[i][3] == 1) pcache_has_pdevil = 1;
              
              caches[pcache][core++][0] = i;
              if (core == cores_per_shared_cache)
              {
                core = 0;
                pcache++;
                old_pcache_has_pdevil = pcache_has_pdevil;
                pcache_has_pdevil = 0;
              }
            }
          }
        }
        
        if (core == 0)
        {
          core = cores_per_shared_cache;
          pcache--;
          pcache_has_pdevil = old_pcache_has_pdevil;
        }
        
        if (pcache_has_pdevil == 0)
        {
          // first releasing turtles that are not in the cache with pdevil
          for(k = 0; k < core; k++){
            slotArray[caches[pcache][k][0]][2] = -1;
            caches[pcache][k][0] = -1;
          }
          pcache--;
        }
        else if (core != cores_per_shared_cache)
        {
          for(i = 0; i < numberSlots; i++)
          {
            if (slotArray[i][2] != -2) // the most peaceful app
            {
              slotArray[i][2] = -2;
              caches[pcache][core++][0] = i;
              if (core == cores_per_shared_cache)
              {
                break;
              }
            }
          }
        }
      }
      else
      {
        pcache = -1;
      }
      
      if (pcache < (shared_caches_number-1))
      {
        // spread the remaining apps (non-prioritized devils and perhaps some turtles across caches that do not contain pdevils)
        int all_taken = 0;
        pile_up = 0;
        while (all_taken == 0)
        {
          all_taken = 1;
          for(i = 0; i < numberSlots; i++){
            if (slotArray[i][2] != -2 /*thread is not taken yet*/)
            {
              all_taken = 0;
              //printf ("%d\n", i);
            }
          }
          
          /*if (all_taken == 0)
           {
           for(i = 0; i < shared_caches_number; i++){
           for(j = 0; j < cores_per_shared_cache; j++){
           printf ("cache %d  core %d:   %d\n", i, j , caches[i][j][0]);
           }
           }
           }*/
          
          cache = pcache+1;
          for(i = (numberSlots-1); i >= 0; i--){
            if (slotArray[i][2] != -2 /*thread is not taken yet*/)
            {
              // search for a free spot in the current cache
              for(k = 0; k < cores_per_shared_cache; k++){
                if (caches[cache][k][pile_up] < 0)
                {
                  caches[cache][k][pile_up] = i;
                  slotArray[i][2] = -2;
                  break;
                }
              }
              cache++;
            }
            if (   cache == shared_caches_number   )
            {
              if (   k == (cores_per_shared_cache-1)   ) pile_up = 1;
              break;
            }
          }
        }
      }
      
      difference = 2;
    }
    else
    {
      difference = 1;
    }
  }
  // dino
  else if (fSchedulerMode == 9)
  {
    int limit_caches = shared_caches_number;
    int copy_non_prio_devils = non_prio_devils, copy_non_prio_super_devils = non_prio_super_devils, copy_turtles = turtles, cur_class = 0, cur_order = 0;
    
    //printf("non_prio_devils %i   non_prio_super_devils %d   turtles %d\n", non_prio_devils, non_prio_super_devils, turtles);
    //for(s = 0; s < count; s++){
    //  printf("initial slotArray[%i][2] %i class %d\n", s, slotArray[s][2], slotArray[s][3]);
    //}
    
    cache = 0;
    for(i = 0; i < (shared_caches_number*cores_per_shared_cache); i++){
      if (copy_non_prio_super_devils-- > 0) cur_class = 5;
      else if (copy_non_prio_devils-- > 0) cur_class = 2;
      else if (copy_turtles-- > 0) cur_class = 3;
      else break;
      
      // search for a free spot in the current cache
      for(k = 0; k < cores_per_shared_cache; k++){
        if (classes[cache][k] < 0)
        {
          classes[cache][k] = cur_class;
          break;
        }
      }
      
      if (cur_order == 0) cache++;
      else cache--;
      
      if (cache == limit_caches)
      {
        cur_order = 1;
        cache--;
      }
      
      if (cache == -1)
      {
        cur_order = 0;
        cache++;
      }
    }
    
    for(i = 0; i < shared_caches_number; i++){
      for(j = 0; j < cores_per_shared_cache; j++){
        if (old_solution[i][j][0] == classes[i][j])
        {
          for(s = 0; s < count; s++){
            if (slotArray[s][0] == old_solution[i][j][1] && slotArray[s][1] == old_solution[i][j][2] && slotArray[s][3] == old_solution[i][j][0])
            {
              caches[i][j][0] = s;
              slotArray[s][2] = -2;
              //printf("match[%i][%i]   %i   class  %d\n", i, j, caches[i][j][0], classes[i][j]);
              break;
            }
          }
        }
      }
    }
    
    for(s = 0; s < count; s++){
      if (slotArray[s][2] != -2)
      {
        //printf("slotArray[%i][2] %i class %d\n", s, slotArray[s][2], slotArray[s][3]);
        for(i = 0; i < shared_caches_number; i++){
          for(j = 0; j < cores_per_shared_cache; j++){
            //printf("classes[%i][%i] %i\n", i, j, classes[i][j]);
            if (caches[i][j][0] < 0 && slotArray[s][3] == classes[i][j])
            {
              caches[i][j][0] = s;
              slotArray[s][2] = -2;
              //printf("caches[%i][%i]   %i\n", i, j, caches[i][j][0]);
              break;
            }
          }
          if (slotArray[s][2] == -2) break;
        }
      }
    }
    
    for(i = 0; i < shared_caches_number; i++){
      for(j = 0; j < cores_per_shared_cache; j++){
        if (caches[i][j][0] >= 0)
        {
          old_solution[i][j][0] = classes[i][j];
          old_solution[i][j][1] = slotArray[caches[i][j][0]][0];
          old_solution[i][j][2] = slotArray[caches[i][j][0]][1];
        }
        else
        {
          old_solution[i][j][0] = -1;
          old_solution[i][j][1] = -1;
          old_solution[i][j][2] = -1;
        }
      }
    }
  }
  // di
  else if (fSchedulerMode == 3 || fSchedulerMode == 4 || fSchedulerMode == 8)
  {
    //if (mould_time % 60 != 0 && workload_has_changed == 0) return 1;
    //if (mould_time % 60 != 0) return 1;
    
    /*if (strcmp(pszHostname, "Xeon_X5365_2") == 0 || strcmp(pszHostname, "Xeon_X5365_4") == 0 || strcmp(pszHostname, "Xeon_E5320") == 0 || strcmp(pszHostname, "Opteron_2350_2cores") == 0)
     {
     int cache = 0;
     //go through each pair that needs to be scheduled together and assign masks
     for(i = 0; i < numberSlots/2; i+=(cores_per_shared_cache/2)){
     //int cache = i % shared_caches_number; //get the cache to which they are assigned
     
     if (slotArray[i][0] >= 0 && slotArray[i][1] >= 0)
     {
     //set the mask of the aggressive thread
     if (sAppRun[slotArray[i][0]].aiSharedCacheMasks[slotArray[i][1]] == masks[cache][0])
     {
     slotArray[i][0] = -2;    
     slotArray[i][1] = -2;    
     }
     else
     {
     sAppRun[slotArray[i][0]].aiSharedCacheMasks[slotArray[i][1]] = masks[cache][0];
     }
     }
     
     if (slotArray[(numberSlots-1)-i][0] >= 0 && slotArray[(numberSlots-1)-i][1] >= 0)
     {
     //set the mask of the passive thread
     if (sAppRun[slotArray[(numberSlots-1)-i][0]].aiSharedCacheMasks[slotArray[(numberSlots-1)-i][1]] == masks[cache][1])
     {
     slotArray[(numberSlots-1)-i][0] = -2;    
     slotArray[(numberSlots-1)-i][1] = -2;    
     }
     else
     {
     sAppRun[slotArray[(numberSlots-1)-i][0]].aiSharedCacheMasks[slotArray[(numberSlots-1)-i][1]] = masks[cache][1];
     }
     }
     
     cache++;
     }
     } else
     // machines with 4 cores per cache
     if (strcmp(pszHostname, "Opteron_8356_2") == 0 || strcmp(pszHostname, "Opteron_8356_4") == 0 || strcmp(pszHostname, "Opteron_2350_4cores") == 0 || strcmp(pszHostname, "Opteron_2435_2nodes") == 0 || strcmp(pszHostname, "Opteron_2435_1node") == 0 || strcmp(pszHostname, "Opteron_8435") == 0 || strcmp(pszHostname, "Opteron_6164") == 0)
     {*/
    
    
    cache = 0;
    int caches_to_spread_prio, all_taken;
    
    // This is Sergey B's new implementation for working with priorities
    if (prio_devils < shared_caches_number || non_prio_devils > 0)
    {
      // spread the prio threads evenly across at most (shared_caches_number - 1) nodes
      if (prio_devils < shared_caches_number) caches_to_spread_prio = prio_devils;
      else caches_to_spread_prio = shared_caches_number - 1;
    }
    else
    {
      // spread the prio threads evenly across the whole system (there is no need to keep one node for the non prio devils)
      caches_to_spread_prio = shared_caches_number;
    }
    
    // spread the prio threads
    if (prio_devils > 0)
    {
      cache = 0;
      for(i = (numberSlots-count); i < numberSlots; i++){
        if (slotArray[i][2] == 1)
        {
          // search for a free spot in the current cache
          for(k = 0; k < cores_per_shared_cache; k++){
            if (caches[cache][k][0] < 0)
            {
              caches[cache][k][0] = i;
              break;
            }
          }
          cache++;
        }
        if (cache == caches_to_spread_prio) cache = 0;
      }
    }
    
    int limit_caches = shared_caches_number;//2;
    
    
    /*if (fSchedulerMode == 8)
     {
     if (non_prio_devils*cores_per_shared_cache < iRunAppCount) limit_caches = (iRunAppCount % 2 == 0)?(iRunAppCount/2):((iRunAppCount+1)/2);
     else limit_caches = non_prio_devils;
     }
     else
     {
     limit_caches = shared_caches_number;
     }*/
    
    
    // now spread the most aggressive threads on nodes without prio threads and then the least aggressive threads on nodes with prio threads
    all_taken = 0;
    int all_caches_are_full;
    
    int emergency_exit = 0;
    
    while (all_taken == 0)
    {
      if (++emergency_exit > 100)
      {
        printf("!!!count %i\r\n", count);
        break;
      }
      
      
      all_taken = 1;
      for(i = (numberSlots-count); i < numberSlots; i++){
        if (slotArray[i][2] != 1 && slotArray[i][2] != -2) // thread is not taken yet
        {
          all_taken = 0;
        }
      }
      
      // search for the most aggressive threads to place on nodes without prio threads
      cache = caches_to_spread_prio;
      for(i = (numberSlots-1); i >= (numberSlots-count); i--){
        if (slotArray[i][2] != 1 && slotArray[i][2] != -2) // thread is not taken yet
        {
          // search for a free spot in the current cache
          for(k = 0; k < cores_per_shared_cache; k++){
            if (caches[cache][k][0] < 0)
            {
              caches[cache][k][0] = i;
              slotArray[i][2] = -2;
              break;
            }
          }
          cache++;
        }
        if (cache == limit_caches) break;
      }
      
      // search for the least aggressive threads to place on nodes with prio threads
      if (prio_devils > 0)
      {
        cache = 0;
        for(i = (numberSlots-count); i < numberSlots; i++){
          if (slotArray[i][2] != 1 && slotArray[i][2] != -2)
          {
            // search for a free spot in the current cache
            for(k = 0; k < cores_per_shared_cache; k++){
              if (caches[cache][k][0] < 0)
              {
                caches[cache][k][0] = i;
                slotArray[i][2] = -2;
                break;
              }
            }
            cache++;
          }
          if (cache == caches_to_spread_prio) break;
        }
      }
    }
    
    /*
     // This is Sergey B's implementation (not brute force)
     int cache = 0;
     //go through each pair that needs to be scheduled together and assign masks 
     for(i = 0; i < numberSlots/2; i+=(cores_per_shared_cache/2)){
     //int cache = i % shared_caches_number; //get the cache to which they are assigned
     
     if (slotArray[i][0] >= 0 && slotArray[i][1] >= 0)
     {
     //set the mask of the aggressive thread
     if (sAppRun[slotArray[i][0]].aiSharedCacheMasks[slotArray[i][1]] == masks[cache][0])
     {
     slotArray[i][0] = -2;    
     slotArray[i][1] = -2;    
     }
     else
     {
     sAppRun[slotArray[i][0]].aiSharedCacheMasks[slotArray[i][1]] = masks[cache][0];
     }
     }
     
     if (slotArray[i+1][0] >= 0 && slotArray[i+1][1] >= 0)
     {
     //set the mask of the next aggressive thread
     if (sAppRun[slotArray[i+1][0]].aiSharedCacheMasks[slotArray[i+1][1]] == masks[cache][1])
     {
     slotArray[i+1][0] = -2;    
     slotArray[i+1][1] = -2;    
     }
     else
     {
     sAppRun[slotArray[i+1][0]].aiSharedCacheMasks[slotArray[i+1][1]] = masks[cache][1];
     }
     }
     
     if (slotArray[(numberSlots-1)-i-1][0] >= 0 && slotArray[(numberSlots-1)-i-1][1] >= 0)
     {
     //set the mask of the next passive thread
     if (sAppRun[slotArray[(numberSlots-1)-i-1][0]].aiSharedCacheMasks[slotArray[(numberSlots-1)-i-1][1]] == masks[cache][2])
     {
     slotArray[(numberSlots-1)-i-1][0] = -2;    
     slotArray[(numberSlots-1)-i-1][1] = -2;    
     }
     else
     {
     sAppRun[slotArray[(numberSlots-1)-i-1][0]].aiSharedCacheMasks[slotArray[(numberSlots-1)-i-1][1]] = masks[cache][2];
     }
     }
     
     if (slotArray[(numberSlots-1)-i][0] >= 0 && slotArray[(numberSlots-1)-i][1] >= 0)
     {
     //set the mask of the passive thread
     if (sAppRun[slotArray[(numberSlots-1)-i][0]].aiSharedCacheMasks[slotArray[(numberSlots-1)-i][1]] == masks[cache][3])
     {
     slotArray[(numberSlots-1)-i][0] = -2;    
     slotArray[(numberSlots-1)-i][1] = -2;    
     }
     else
     {
     sAppRun[slotArray[(numberSlots-1)-i][0]].aiSharedCacheMasks[slotArray[(numberSlots-1)-i][1]] = masks[cache][3];
     }
     }
     
     cache++;
     }
     )*/
  }
  
  int thread_class1, thread_class2;
  if ( !(fSchedulerMode == 7 && !workload_has_changed) )
  {
    // checking if there are threads of the same multithreaded application with the same class and if so, try to schedule them on the same cache
    /*for(i = 0; i < shared_caches_number; i++){
     for(j = 0; j < cores_per_shared_cache; j++){
     k = caches[i][j][0];
     if (k >= 0 && slotArray[k][0] >= 0 && slotArray[k][1] >= 0)
     {
     if (slotArray[k][3] == 1 || slotArray[k][3] == 2)
     {
     thread_class1 = 1;
     thread_class2 = 2;
     }
     else
     {
     thread_class1 = 3;
     thread_class2 = 3;
     }
     
     // check if there are slots in the same cache that are not taken by the threads of the same class and the same app
     for(l = 0; l < cores_per_shared_cache; l++){
     if (l == j) continue;
     
     p = caches[i][l][0];
     
     if (   p >= 0 && slotArray[p][0] != slotArray[k][0] && (slotArray[p][3] == thread_class1 || slotArray[p][3] == thread_class2)   )
     {
     // if there is, search for the thread of the same app and the same class in some other cache, if there is one, swap it
     for(i2 = (i+1); i2 < shared_caches_number; i2++){
     for(j2 = 0; j2 < cores_per_shared_cache; j2++){
     p2 = caches[i2][j2][0];
     
     if (   p2 >= 0 && slotArray[p2][0] == slotArray[k][0] && (slotArray[p2][3] == thread_class1 || slotArray[p2][3] == thread_class2)   )
     {
     caches[i][l][0] = p2;
     caches[i2][j2][0] = p;
     
     // break;
     i2 = shared_caches_number;
     j2 = cores_per_shared_cache;
     }
     }
     }
     }
     }
     }
     }
     }*/
    
    // assigning masks
    for(i = 0; i < shared_caches_number; i++){
      for(j = 0; j < cores_per_shared_cache; j++){
        for(m = 0; m < 2; m++){
          k = caches[i][j][m];
          if (k >= 0 && slotArray[k][0] >= 0 && slotArray[k][1] >= 0)
          {
            if (sAppRun[slotArray[k][0]].aiSharedCacheMasks[slotArray[k][1]] == masks[i][j])
            {
              slotArray[k][0] = -2;
              slotArray[k][1] = -2;
            }
            else
            {
              sAppRun[slotArray[k][0]].aiSharedCacheMasks[slotArray[k][1]] = masks[i][j];
            }
          }
        }
      }
    }
  }
  
  if (fSchedulerMode == 7 && prio_devils > 0)
  {
    int no_shuffle = 1;
    
    for(i = 0; i < shared_caches_number; i++){
      for(j = 0; j < cores_per_shared_cache; j++){
        caches[i][j][0] = -1;
        caches[i][j][1] = -1;
      }
    }
    
    while (no_shuffle)
    {
      int CP[shared_caches_number], CP_all[shared_caches_number], CP_devils[shared_caches_number], cpmin, cpmax, Cmin_turtles = 0, Cmax_turtles = 0;
      int cpmin_passes, cpmax_passes, passes_min, passes_max;
      
      for(i = 0; i < shared_caches_number; i++){
        CP[i] = 0;
        CP_all[i] = 0;
        CP_devils[i] = 0;
      }
      
      // counting the number of turtles in each cache with pdevils
      for(i = 0; i < numberSlots; i++){
        if (slotArray[i][0] >= 0 && slotArray[i][1] >= 0)
        {
          iA = slotArray[i][0];
          iT = slotArray[i][1];
          cache = maskToCache(sAppRun[iA].aiSharedCacheMasks[iT]);
          core = maskToCore(sAppRun[iA].aiSharedCacheMasks[iT]);
          
          //printf("%d %d\n", cache, core);
          if (cache < 0 || core < 0) continue;
          
          caches[cache][core][0] = i;
          
          if (cache <= pcache)
          {
            CP_all[cache]++;
            
            if (slotArray[i][3] == 2) // devil
            {
              CP_devils[cache]++;
              //printf("cache %d   i %d", cache, i);
            }
            
            if (slotArray[i][3] == 3) // real turtle
            {
              if (CP[cache] == Cmin_turtles)
              {
                for(m = 0; m <= pcache; m++){
                  if (m != cache && CP[m] == Cmin_turtles)
                  {
                    Cmin_turtles--;
                    break;
                  }
                }
                
                Cmin_turtles++;
              }
              
              if (CP[cache] == Cmax_turtles) Cmax_turtles++;
              
              CP[cache]++;
            }
          }
        }
      }
      
      // free spots are also counted as turtles
      for(k = 0; k <= pcache; k++){
        if (CP_all[k] < cores_per_shared_cache)
        {
          idle_turtles = cores_per_shared_cache - CP_all[k];
          if (CP[k] == Cmin_turtles)
          {
            for(m = 0; m <= pcache; m++){
              if (m != k && CP[m] == Cmin_turtles)
              {
                Cmin_turtles -= idle_turtles;
                break;
              }
            }
            
            Cmin_turtles += idle_turtles;
          }
          
          if (CP[k] == Cmax_turtles) Cmax_turtles += idle_turtles;
          
          CP[k] += idle_turtles;
        }
      }
      
      if ( (Cmax_turtles - Cmin_turtles) >= difference )
      {
        no_shuffle = (difference==1?0:1);
        
        cpmin = -1; cpmax = -1;
        cpmin_passes = rand() % shared_caches_number; cpmax_passes = rand() % shared_caches_number;
        passes_min = 0; passes_max = 0;
        cache = 0;
        
        while (cpmin < 0 || cpmax < 0)
        {
          //printf("Cmax_turtles %d   Cmin_turtles %d   passes_min %d   cpmin_passes %d   passes_max %d   cpmax_passes %d\n", Cmax_turtles, Cmin_turtles, passes_min, cpmin_passes, passes_max, cpmax_passes);
          if (CP[cache] == Cmin_turtles)
          {
            if (passes_min == cpmin_passes) cpmin = cache;
            else passes_min++;
          }
          
          if (CP[cache] == Cmax_turtles)
          {
            if (passes_max == cpmax_passes) cpmax = cache;
            else passes_max++;
          }
          
          if (++cache > pcache) cache = 0;
        }
        
        if (cpmin < 0 || cpmax < 0)
        {
          printf("!!! error cpmin/cpmax,  cpmin %d   cpmax %d\n", cpmin, cpmax);
          printf("pcache %d    Cmax_turtles %d   Cmin_turtles %d   passes_min %d   cpmin_passes %d   passes_max %d   cpmax_passes %d\n", pcache, Cmax_turtles, Cmin_turtles, passes_min, cpmin_passes, passes_max, cpmax_passes);
          continue;
        }
        
        // swap
        if (CP_devils[cpmin] > 0) cpmin_passes = rand() % CP_devils[cpmin];
        else cpmin_passes = rand() % (cores_per_shared_cache - CP_devils[cpmin] - CP[cpmin]);
        
        cpmax_passes = rand() % CP[cpmax];
        passes_min = 0, passes_max = 0;
        int core_min = -1, core_max = -1;
        
        for(m = 0; m < cores_per_shared_cache; m++){
          if (caches[cpmin][m][0] >= 0)
          {
            if (CP_devils[cpmin] > 0 && slotArray[ caches[cpmin][m][0] ][3] == 2)
            {
              if (passes_min == cpmin_passes) core_min = m;
              else passes_min++;
            }
            else if (CP_devils[cpmin] == 0 && slotArray[ caches[cpmin][m][0] ][3] == 1)
            {
              if (passes_min == cpmin_passes) core_min = m;
              else passes_min++;
            }
          }
          
          if (caches[cpmax][m][0] < 0 || slotArray[ caches[cpmax][m][0] ][3] == 3)
          {
            if (passes_max == cpmax_passes) core_max = m;
            else passes_max++;
          }
        }
        
        if (core_min < 0 || core_max < 0)
        {
          printf("!!! error core_min/core_max,  core_min %d   core_max %d\n", core_min, core_max);
          printf("cpmin %d   cpmax %d\n", cpmin, cpmax);
          printf("pcache %d    Cmax_turtles %d   Cmin_turtles %d   passes_min %d   cpmin_passes %d   passes_max %d   cpmax_passes %d\n", pcache, Cmax_turtles, Cmin_turtles, passes_min, cpmin_passes, passes_max, cpmax_passes);
          printf("CP_devils[cpmin] %d    CP[cpmin] %d   CP[cpmax] %d\n", CP_devils[cpmin], CP[cpmin], CP[cpmax]);
          
          for(i = 0; i < shared_caches_number; i++){
            for(j = 0; j < cores_per_shared_cache; j++){
              printf ("cache %d  core %d:   %d\n", i, j , caches[i][j][0]);
              if (caches[i][j][0] >=0)
              {
                printf ("%d\n", slotArray[ caches[i][j][0] ][3]);
              }
            }
          }
          
          continue;
        }
        
        //int temp_mask = cacheCoreToMask(cpmin, core_min); //sAppRun[ slotArray[caches[cpmin][core_min][0]][0] ].aiSharedCacheMasks[ slotArray[caches[cpmin][core_min][0]][1] ];
        
        if (caches[cpmax][core_max][0] >=0 && slotArray[caches[cpmax][core_max][0]][0] >=0 && slotArray[caches[cpmax][core_max][0]][1] >= 0)
        {
          sAppRun[ slotArray[caches[cpmin][core_min][0]][0] ].aiSharedCacheMasks[ slotArray[caches[cpmin][core_min][0]][1] ] = cacheCoreToMask(cpmax, core_max); //sAppRun[ slotArray[caches[cpmax][core_max][0]][0] ].aiSharedCacheMasks[ slotArray[caches[cpmax][core_max][0]][1] ];
          sAppRun[ slotArray[caches[cpmax][core_max][0]][0] ].aiSharedCacheMasks[ slotArray[caches[cpmax][core_max][0]][1] ] = cacheCoreToMask(cpmin, core_min);
        }
        else
        {
          sAppRun[ slotArray[caches[cpmin][core_min][0]][0] ].aiSharedCacheMasks[ slotArray[caches[cpmin][core_min][0]][1] ] = cacheCoreToMask(cpmax, core_max);
        }
      }
      else
      {
        no_shuffle = 0;
      }
    }
  }
  
  //do the actual binding
  for(i = 0; i < numberSlots; i++){
    if (slotArray[i][0] >= 0 && slotArray[i][1] >= 0)
    {
      //printf("%d %d     %d\n", slotArray[i][0], slotArray[i][1], sAppRun[slotArray[i][0]].aiSharedCacheMasks[slotArray[i][1]]);
      if(bindThread(slotArray[i][0], slotArray[i][1]) < 0){
        return 0; //an error occured
      }
    }
  }

  //do the actual unbinding
  for(i = 0; i < iRunAppCount; i++){
    for(j = 0; j < MAX_THREAD_NUMBER; j++){
      if(sAppRun[i].aiTIDs[j] > 0 && sAppRun[i].to_schedule[j] <= 0){
        sAppRun[i].aiSharedCacheMasks[j] = 0;
        if(unbindThread(i, j) < 0){
          return 0; //an error occured
        }
      }
    }
  }
  
  return 1; //indicating success
}





//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++ Queue-based scheduling algorithm +++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



//==============================================================================================



int queueScheduler(){
  int iA, i, j, k;
  int turtle_intervals, devil_intervals;
  int devils_gone, turtles_gone;
  
  // the first time, all applications are sleeping, we build the queue
  if (queue_built == 0)
  {
    for(iA = 0; iA < iRunAppCount; iA++){
      turtle_intervals = 0;
      devil_intervals = 0;
      for(k = 0; k < 10; k++){
        printf ("iA %i, k %d, missrate: %Lf\n", iA, k, sAppRun[iA].aldOldAppsSignatures[0][k]);
        if (sAppRun[iA].aldOldAppsSignatures[0][k] > DEVIL_HORIZON)
        {
          devil_intervals++;
        }
        else
        {
          turtle_intervals++;
        }
      }
      
      if (devil_intervals >= turtle_intervals)
      {
        for(i = 0; i < iRunAppCount; i++){
          if (queue_devils[i] < 0)
          {
            queue_devils[i] = iA;
            break;
          }
        }
      }
      else
      {
        for(i = 0; i < iRunAppCount; i++){
          if (queue_turtles[i] < 0)
          {
            queue_turtles[i] = iA;
            break;
          }
        }
      }
    }
    
    queue_built = 1;
    
    // output the queue
    for(i = 0; i < iRunAppCount; i++){
      if (queue_devils[i] >= 0) printf ("queue_devils[%i] %i\n", i, queue_devils[i]);
      if (queue_turtles[i] >= 0) printf ("queue_turtles[%i] %i\n", i, queue_turtles[i]);
    }
    
    printf ("active_devil %i   active_turtle %i\n", active_devil, active_turtle);
  }
  
  devils_gone = 1; turtles_gone = 1;
  for(i = 0; i < iRunAppCount; i++){
    if (queue_devils[i] >= 0) devils_gone = 0;
    if (queue_turtles[i] >= 0) turtles_gone = 0;
  }
  
  if (active_devil < 0 && devils_gone == 0)
  {
    nextAnimal(0, 1);
  }
  
  if (active_devil < 0 && devils_gone == 1)
  {
    nextAnimal(1, 1);
  }
  
  if (active_turtle < 0 && turtles_gone == 0)
  {
    nextAnimal(1, 2);
  }
  
  if (active_turtle < 0 && turtles_gone == 1)
  {
    nextAnimal(0, 2);
  }
}



//==============================================================================================



// animal_type: 0 - devil, 1 - turtle
int nextAnimal(int animal_type, int mask){
  int i;
  
  if (animal_type == 0)
  {
    for(i = 0; i < iRunAppCount; i++){
      if (queue_devils[i] >= 0)
      {
        active_devil = queue_devils[i];
        kill(sAppRun[active_devil].iPID, 18);
        printf ("%i: devil iA == %i (%d) from sleep\n", mould_time, active_devil, sAppRun[active_devil].iPID);
        
        sAppRun[active_devil].aiSharedCacheMasks[0] = mask;
        if(bindThread(active_devil, 0) < 0){
          printf ("bindThread error for active_devil %d\n", active_devil);
          return 0; //an error occured
        }
        queue_devils[i] = -1;
        break;
      }
    }
  }
  else
  {
    for(i = 0; i < iRunAppCount; i++){
      if (queue_turtles[i] >= 0)
      {
        active_turtle = queue_turtles[i];
        kill(sAppRun[active_turtle].iPID, 18);
        printf ("%i: turtle iA == %i (%d) from sleep\n", mould_time, active_turtle, sAppRun[active_turtle].iPID);
        sAppRun[active_turtle].aiSharedCacheMasks[0] = mask;
        if(bindThread(active_turtle, 0) < 0){
          printf ("bindThread error for active_turtle %d\n", active_turtle);
          return 0; //an error occured
        }
        queue_turtles[i] = -1;
        break;
      }
    }
  }
}





//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++ Swap threads to even out memory controller DRAM misses +++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



//==============================================================================================



int swapToEvenDRAMMisses(){
  long int min_chip_wide_counter = -1, max_chip_wide_counter = -1;
  int i, j, r, app1, app2, th1, th2, core1, core2, app1activeThreads = 0, app2activeThreads = 0, min_chip_id = -1, max_chip_id = -1;
  
  for(i = 0; i < shared_caches_number; i++){
    if (min_chip_wide_counter > chip_wide_counters[i][0] || min_chip_wide_counter < 0)
    {
      min_chip_wide_counter = chip_wide_counters[i][0];
      min_chip_id = i;
    }
    if (max_chip_wide_counter < chip_wide_counters[i][0])
    {
      max_chip_wide_counter = chip_wide_counters[i][0];
      max_chip_id = i;
    }
  }
  
  //printf("min_chip_wide_counter %ld   max_chip_wide_counter %ld\n", min_chip_wide_counter, max_chip_wide_counter);
  //printf("min_chip_id %d   max_chip_id %d\n", min_chip_id, max_chip_id);
  
  // choose randomly two threads and swap if there is a difference across the chips
  if (min_chip_id >= 0 && max_chip_id >= 0 && min_chip_id != max_chip_id && (max_chip_wide_counter - min_chip_wide_counter) > 100000000)
  {
    if (iRunAppCount > 0)
    {
      app1 = (rand() % iRunAppCount);
      app2 = (rand() % iRunAppCount);
    }
    else return -1;
    
    //printf("app1 %d   app2 %d\n", app1, app2);
    
    for(j = 0; j < MAX_THREAD_NUMBER; j++){
      if(sAppRun[app1].aiTIDs[j] > 0 && sAppRun[app1].to_schedule[j] > 0){
        app1activeThreads++;
      }
    }
    
    if (app2 != app1)
    {
      for(j = 0; j < MAX_THREAD_NUMBER; j++){
        if(sAppRun[app2].aiTIDs[j] > 0 && sAppRun[app2].to_schedule[j] > 0){
          app2activeThreads++;
        }
      }
    }
    else app2activeThreads = app1activeThreads;
    
    //printf("app1activeThreads %d   app2activeThreads %d\n", app1activeThreads, app2activeThreads);
    
    if (app1activeThreads > 0)
    {
      th1 = (rand() % app1activeThreads);
    }
    else return -1;
    
    if (app2activeThreads > 0)
    {
      th2 = (rand() % app2activeThreads);
    }
    else return -1;
    
    core1 = sAppRun[app1].aiBindCores[th1];
    core2 = sAppRun[app2].aiBindCores[th2];
    
    sAppRun[app1].aiSharedCacheMasks[th1] = cpuToMask(core2);
    sAppRun[app2].aiSharedCacheMasks[th2] = cpuToMask(core1);
    
    //printf("th1 %d core1 %d  th2 %d core2 %d\n", th1, core1, th2, core2);
    
    
    bindThread(app1, th1);
    bindThread(app2, th2);
    
    sAppRun[app1].aiBindCores[th1] = core2;
    sAppRun[app2].aiBindCores[th2] = core1;
  }
  
  return 0;
}


