#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_APP_NUMBER 8
#define MAX_RUN_NUMBER 300000
#define MAX_LINE_SIZE 1024

char*  names[MAX_APP_NUMBER];
long double data[MAX_APP_NUMBER][MAX_RUN_NUMBER];
int  indexes[MAX_APP_NUMBER];

int main() {
  FILE *file;
  FILE *resfile;
  char line[MAX_LINE_SIZE], temp1[MAX_LINE_SIZE], name[MAX_LINE_SIZE];
  
  long double average1, sum1, deviation1, percentage1, average2, sum2, deviation2, percentage2;
  
  int apps = -1, i, j, k, app_i = -1, iBufferSize;
  int iApp;
  long double tmp;
  char*  pszBuffer;
  
  for (i=0; i<MAX_APP_NUMBER; i++)
  {
    indexes[i] = 0;
  }
  
  //------------------------------------------------------------
  
  file = fopen("online.log", "r");
  if(file==NULL) {
    printf("Error: can't open file 'pfmon.log'.\n");
    return 1;
  }
  
  //------------------------------------------------------------
  
  resfile = fopen ("online.log.parsed", "wt");
  
  // reading current line from file
  while ( fgets(line,sizeof(line),file) != NULL )
  {
    if ( strstr(line,"============================") != NULL)
    {
      for (i=0; i<apps; i++)
      {
        sum1 = 0; deviation1 = 0; percentage1 = 0;
        
        //fprintf(resfile, names[i]);
        //fprintf(resfile, "\n");
        for (j=0; j<indexes[i]; j++)
        {
          //fprintf(resfile, "%Lf %Lf\n", data[i][j][0], data[i][j][1]);
          
          sum1 += data[i][j];
        }
        
        average1 = sum1 / indexes[i];
        
        for (j=0; j<indexes[i]; j++)
        {
          deviation1 += (data[i][j] - average1) * (data[i][j] - average1);
        }
        
        deviation1 = sqrt(deviation1/(indexes[i]-1)); // here (count-1) but not just count because Excel calculates deviation dividing by (count-1) and we want to directly compare with it
        
        percentage1 = deviation1/average1 * 100;
        
        //fprintf(resfile, "-------\n");
        fprintf(resfile, "%i: %Lf %Lf\n", i, average1, percentage1);
        //fprintf(resfile, "\n");
      }
      
      apps = -1;
      for (i=0; i<MAX_APP_NUMBER; i++)
      {
        indexes[i] = 0;
      }
      fprintf(resfile, line);
    }
    else if ( strlen(line) > 1)
    {
      // sphinx2 sAppRun[5].aiTIDs[0] (#1): MISSES: 103787281.000000 | INSTRUCTIONS: 10287082072.000000 | CYCLES: 14000622127.000000 ||| WEIGHTED MISSRATE: 0.008452
      sscanf(line, "%*s sAppRun[%d].aiTIDs[%*d] (#%*d): MISSES: %*Lf | INSTRUCTIONS: %*Lf | CYCLES: %*Lf ||| WEIGHTED MISSRATE: %Lf", &app_i, &tmp);
      data[app_i][indexes[app_i]] = tmp;
      indexes[app_i]++;
      
      if ((app_i+1) > apps) apps = app_i+1;
    }
  }
  
  for (i=0; i<apps; i++)
  {
    sum1 = 0; deviation1 = 0; percentage1 = 0;
    
    //fprintf(resfile, names[i]);
    //fprintf(resfile, "\n");
    for (j=0; j<indexes[i]; j++)
    {
      //fprintf(resfile, "%Lf %Lf\n", data[i][j][0], data[i][j][1]);
      
      sum1 += data[i][j];
    }
    
    average1 = sum1 / indexes[i];
    
    for (j=0; j<indexes[i]; j++)
    {
      deviation1 += (data[i][j] - average1) * (data[i][j] - average1);
    }
    
    deviation1 = sqrt(deviation1/(indexes[i]-1)); // here (count-1) but not just count because Excel calculates deviation dividing by (count-1) and we want to directly compare with it
    
    percentage1 = deviation1/average1 * 100;
    
    //fprintf(resfile, "-------\n");
    fprintf(resfile, "%i: %Lf %Lf\n", i, average1, percentage1);
    //fprintf(resfile, "\n");
  }
  fprintf(resfile, "\n");
  
  fclose (resfile);
  fclose (file);
  
  return 0;
}
