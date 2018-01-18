// please see data(2).xls file for details

#include <stdio.h>
#include <string.h>
#include <math.h>

// 0-based
#define RUN_NUMBER 2

#define MAX_APP_NUMBER 32
#define MAX_CORE_NUMBER 32

int main() {
  FILE *file;
  char line[1024], name[1024];
  char names[1024];
  
  int i, j, iA, iT, iN, core, intervals, runN=-1;
  int time_mark;
  double percentage;
  int track_intervals[MAX_APP_NUMBER][MAX_CORE_NUMBER];
  
  for (i=0; i<MAX_APP_NUMBER; i++)
  {
    for (j=0; j<MAX_CORE_NUMBER; j++)
    {
      track_intervals[i][j] = 0;
    }
  }
  
  //------------------------------------------------------------
  
  file = fopen("mould.log", "r");
  if(file==NULL) {
    printf("Error: can't open file 'mould.log'.\n");
    return 1;
  }
  
  //------------------------------------------------------------
  
  // reading current line from file
  while ( fgets(line,sizeof(line),file) != NULL )
  {
    //printf(line);
    if ( strstr(line,"intervals") != NULL)
    {
      // "mcf sAppRun[0].aiTIDs[0] (#2) was at 3-th core for 4 intervals"
      sscanf(line, "%d: %s sAppRun[%i].aiTIDs[%i] (#%i) was at %i-th core for %i intervals", &time_mark, &name, &iA, &iT, &iN, &core, &intervals);
      
      //printf("%d: %s[%i][%i](#%i) was at %i-th core for %i intervals\n", time_mark, name, iA, iT, iN, core, intervals);
      
      if (iA >= 0 && core >= 0)
      {
        track_intervals[iA][core] += intervals;
      }
    }
    else if ( strstr(line,"=================") != NULL )
    {
      for (i=0; i<MAX_APP_NUMBER; i++)
      {
        for (j=0; j<MAX_CORE_NUMBER; j++)
        {
          if (track_intervals[i][j] > 0)
          {
            //percentage = track_intervals[i][j] / time_mark *100;
            printf("[%i][%i] %i intervals (%d)\n", i, j, track_intervals[i][j], time_mark);
          }
          track_intervals[i][j] = 0;
        }
      }
      
      runN++;
      //          if (runN > RUN_NUMBER) break;
      time_mark = 0;
      
      printf("\n%s", line);
    }
  }
  
  for (i=0; i<MAX_APP_NUMBER; i++)
  {
    for (j=0; j<MAX_CORE_NUMBER; j++)
    {
      if (track_intervals[i][j] > 0)
      {
        //percentage = track_intervals[i][j] / time_mark *100;
        printf("[%i][%i] %i intervals (%d)\n", i, j, track_intervals[i][j], time_mark);
      }
      track_intervals[i][j] = 0;
    }
  }
  
  return 0;
}
