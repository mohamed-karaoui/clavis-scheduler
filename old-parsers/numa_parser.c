#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_APP_NUMBER 20
#define MAX_CHIP_NUMBER 4
#define MAX_LINE_SIZE 1024

char*  names[MAX_APP_NUMBER];
long double memory_mult_time[MAX_APP_NUMBER][MAX_CHIP_NUMBER];
long int  total_time[MAX_APP_NUMBER];

int main() {
  FILE *file;
  FILE *resfile;
  char line[MAX_LINE_SIZE], temp1[MAX_LINE_SIZE], name[MAX_LINE_SIZE];
  
  int count;
  long double average1, sum1, deviation1, percentage1, average2, sum2, deviation2, percentage2, cur_time;
  
  int apps = 0, i, j, k, app_i = -1, iBufferSize, cur_counter = 0;
  long int  cur_memory[MAX_CHIP_NUMBER];
  long double tmp;
  char*  pszBuffer;
  
  for (i=0; i<MAX_APP_NUMBER; i++)
  {
    total_time[i] = 0;
    for (j=0; j<MAX_CHIP_NUMBER; j++)
    {
      memory_mult_time[i][j] = 0;
    }
  }
  
  //------------------------------------------------------------
  
  file = fopen("numa.log", "r");
  if(file==NULL) {
    printf("Error: can't open file 'pfmon.log'.\n");
    return 1;
  }
  
  //------------------------------------------------------------
  
  resfile = fopen ("numa.log.parsed", "wt");
  
  // reading current line from file
  while ( fgets(line,sizeof(line),file) != NULL )
  {
    if ( strstr(line,"============================") != NULL)
    {
      for (i=0; i<apps; i++)
      {
        count = 0;
        sum1 = 0; deviation1 = 0; percentage1 = 0;
        sum2 = 0; deviation2 = 0; percentage2 = 0;
        
        //fprintf(resfile, names[i]);
        //fprintf(resfile, "\n");
        for (j=0; j<indexes[i]; j++)
        {
          //fprintf(resfile, "%Lf %Lf\n", data[i][j][0], data[i][j][1]);
          
          count++;
          sum1 += data[i][j][0];
          sum2 += data[i][j][1];
        }
        
        average1 = sum1 / count;
        average2 = sum2 / count;
        
        for (j=0; j<indexes[i]; j++)
        {
          deviation1 += (data[i][j][0] - average1) * (data[i][j][0] - average1);
          deviation2 += (data[i][j][1] - average2) * (data[i][j][1] - average2);
        }
        
        deviation1 = sqrt(deviation1/(count-1)); // here (count-1) but not just count because Excel calculates deviation dividing by (count-1) and we want to directly compare with it
        deviation2 = sqrt(deviation2/(count-1)); // here (count-1) but not just count because Excel calculates deviation dividing by (count-1) and we want to directly compare with it
        
        percentage1 = deviation1/average1 * 100;
        percentage2 = deviation2/average2 * 100;
        
        //fprintf(resfile, "-------\n");
        fprintf(resfile, "%Lf %Lf\n", average1, average2);
        //fprintf(resfile, "%Lf %Lf\n", deviation1, deviation2);
        //fprintf(resfile, "%Lf %Lf\n", percentage1, percentage2);
        //fprintf(resfile, "\n");
      }
      //fprintf(resfile, "\n");
      //fprintf(resfile, "\n");
      
      apps = 0;
      cur_counter = 0;
      for (i=0; i<MAX_APP_NUMBER; i++)
      {
        indexes[i] = 0;
      }
      fprintf(resfile, line);
    }
    else if ( strstr(line,"# pfmon command line: pfmon --follow-all") != NULL )
    {
      // 4: namd2 sAppRun[7].aiTIDs[0] (#1): 2 11812 0 0
      sscanf(line, "%d: %s sAppRun[%*d].aiTIDs[%*d] (#%*d): %d %d %d %d", &temp1, &name);
      
      app_i = -1;
      for (i=0; i<apps; i++)
      {
        if ( strstr(names[i], name) != NULL ) app_i = i;
      }
      if (app_i < 0)
      {
        app_i = apps++;
        
        iBufferSize = strlen(name);
        pszBuffer = calloc(iBufferSize + 1, 1);
        strncpy(pszBuffer, name, iBufferSize);
        names[app_i] = pszBuffer;
      }
    }
    else if ( strstr(line,"#") == NULL && strlen(line) > 1)
    {
      //     148864651 L2_LINES_IN:SELF
      sscanf(line, "%Lf %*s", &tmp);
      data[app_i][indexes[app_i]][cur_counter] = tmp;
      if (cur_counter == 0)
      {
        cur_counter = 1;
      }
      else
      {
        cur_counter = 0;
        indexes[app_i]++;
      }
    }
  }
  
  for (i=0; i<apps; i++)
  {
    count = 0;
    sum1 = 0; deviation1 = 0; percentage1 = 0;
    sum2 = 0; deviation2 = 0; percentage2 = 0;
    
    //fprintf(resfile, names[i]);
    //fprintf(resfile, "\n");
    for (j=0; j<indexes[i]; j++)
    {
      //fprintf(resfile, "%Lf %Lf\n", data[i][j][0], data[i][j][1]);
      
      count++;
      sum1 += data[i][j][0];
      sum2 += data[i][j][1];
    }
    
    average1 = sum1 / count;
    average2 = sum2 / count;
    
    for (j=0; j<indexes[i]; j++)
    {
      deviation1 += (data[i][j][0] - average1) * (data[i][j][0] - average1);
      deviation2 += (data[i][j][1] - average2) * (data[i][j][1] - average2);
    }
    
    deviation1 = sqrt(deviation1/(count-1)); // here (count-1) but not just count because Excel calculates deviation dividing by (count-1) and we want to directly compare with it
    deviation2 = sqrt(deviation2/(count-1)); // here (count-1) but not just count because Excel calculates deviation dividing by (count-1) and we want to directly compare with it
    
    percentage1 = deviation1/average1 * 100;
    percentage2 = deviation2/average2 * 100;
    
    //fprintf(resfile, "-------\n");
    fprintf(resfile, "%Lf %Lf\n", average1, average2);
    //fprintf(resfile, "%Lf %Lf\n", deviation1, deviation2);
    //fprintf(resfile, "%Lf %Lf\n", percentage1, percentage2);
    //fprintf(resfile, "\n");
  }
  //fprintf(resfile, "\n");
  
  fclose (resfile);
  fclose (file);
  
  return 0;
}
