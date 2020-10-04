#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define MAX(A, B) ((A) > (B) ? (A) : (B))


#ifdef _MSC_VER
#define ANSILIBS
#define casecmp stricmp
#else
#define casecmp strcasecmp
#endif

int nmodels = 32;

struct stressdef
{
  char *name;			/* rhythm designator */
  char *meter;
  int nseg;			/* number of segments; */
  int nval;			/* number of values; */
  int vel[16];			/* segment velocities */
  float expcoef[16];		/* segment expander coefficient */
} stresspat[48];


/* most of these externals link to variables in genmidi.c */
extern int segnum, segden, nseg;
extern float fdursum[32], fdur[32];
extern int ngain[32];
extern float maxdur;
extern int time_num, time_denom;
extern int verbose;
extern int beatmodel, stressmodel;
extern char timesigstring[16];	/* from parseabc.c */
extern int *checkmalloc(int size);

void reduce (int *, int *);

void
init_stresspat ()
{
  if ( stresspat[0].name != NULL) {  /* [SDG] 2020-06-03 */
      printf("stresspat already initialized\n");
      return;
      } 
  stresspat[0].name = "Hornpipe";
  stresspat[0].meter = "4/4";
  stresspat[0].nseg = 8;
  stresspat[0].nval = 2;
  stresspat[0].vel[0] = 110;
  stresspat[0].vel[1] = 90;
  stresspat[0].expcoef[0] = (float) 1.4;
  stresspat[0].expcoef[1] = (float) 0.6;

  stresspat[1].name = "Hornpipe";
  stresspat[1].meter = "C|";
  stresspat[1].nseg = 8;
  stresspat[1].nval = 2;
  stresspat[1].vel[0] = 110;
  stresspat[1].vel[1] = 90;
  stresspat[1].expcoef[0] = (float) 1.4;
  stresspat[1].expcoef[1] = (float) 0.6;

  stresspat[2].name = "Hornpipe";
  stresspat[2].meter = "2/4";
  stresspat[2].nseg = 8;
  stresspat[2].nval = 2;
  stresspat[2].vel[0] = 110;
  stresspat[2].vel[1] = 90;
  stresspat[2].expcoef[0] = (float) 1.4;
  stresspat[2].expcoef[1] = (float) 0.6;

  stresspat[3].name = "Hornpipe";
  stresspat[3].meter = "9/4";
  stresspat[3].nseg = 9;
  stresspat[3].nval = 3;
  stresspat[3].vel[0] = 110;
  stresspat[3].vel[1] = 80;
  stresspat[3].vel[2] = 100;
  stresspat[3].expcoef[0] = (float) 1.4;
  stresspat[3].expcoef[1] = (float) 0.6;
  stresspat[3].expcoef[2] = (float) 1.0;

  stresspat[4].name = "Hornpipe";
  stresspat[4].meter = "3/2";
  stresspat[4].nseg = 12;
  stresspat[4].nval = 2;
  stresspat[4].vel[0] = 110;
  stresspat[4].vel[1] = 90;
  stresspat[4].expcoef[0] = (float) 1.4;
  stresspat[4].expcoef[1] = (float) 0.6;

  stresspat[5].name = "Hornpipe";
  stresspat[5].meter = "12/8";
  stresspat[5].nseg = 12;
  stresspat[5].nval = 3;
  stresspat[5].vel[0] = 110;
  stresspat[5].vel[1] = 80;
  stresspat[5].vel[2] = 100;
  stresspat[5].expcoef[0] = (float) 1.4;
  stresspat[5].expcoef[1] = (float) 0.6;
  stresspat[5].expcoef[2] = (float) 1.0;

  stresspat[6].name = "Double hornpipe";
  stresspat[6].meter = "6/2";
  stresspat[6].nseg = 12;
  stresspat[6].nval = 6;
  stresspat[6].vel[0] = 110;
  stresspat[6].vel[1] = 80;
  stresspat[6].vel[2] = 80;
  stresspat[6].vel[3] = 105;
  stresspat[6].vel[4] = 80;
  stresspat[6].vel[5] = 80;
  stresspat[6].expcoef[0] = (float) 1.2;
  stresspat[6].expcoef[1] = (float) 0.9;
  stresspat[6].expcoef[2] = (float) 0.9;
  stresspat[6].expcoef[3] = (float) 1.2;
  stresspat[6].expcoef[4] = (float) 0.9;
  stresspat[6].expcoef[5] = (float) 0.9;

  stresspat[7].name = "Reel";
  stresspat[7].meter = "4/4";
  stresspat[7].nseg = 8;
  stresspat[7].nval = 2;
  stresspat[7].vel[0] = 120;
  stresspat[7].vel[1] = 60;
  stresspat[7].expcoef[0] = (float) 1.1;
  stresspat[7].expcoef[1] = (float) 0.9;

  stresspat[8].name = "Reel";
  stresspat[8].meter = "C|";
  stresspat[8].nseg = 8;
  stresspat[8].nval = 8;
  stresspat[8].vel[0] = 80;
  stresspat[8].vel[1] = 60;
  stresspat[8].vel[2] = 120;
  stresspat[8].vel[3] = 60;
  stresspat[8].vel[4] = 80;
  stresspat[8].vel[5] = 60;
  stresspat[8].vel[6] = 120;
  stresspat[8].vel[7] = 60;
  stresspat[8].expcoef[0] = (float) 1.1;
  stresspat[8].expcoef[1] = (float) 0.9;
  stresspat[8].expcoef[2] = (float) 1.1;
  stresspat[8].expcoef[3] = (float) 0.9;
  stresspat[8].expcoef[4] = (float) 1.1;
  stresspat[8].expcoef[5] = (float) 0.9;
  stresspat[8].expcoef[6] = (float) 1.1;
  stresspat[8].expcoef[7] = (float) 0.9;

  stresspat[9].name = "Reel";
  stresspat[9].meter = "2/4";
  stresspat[9].nseg = 8;
  stresspat[9].nval = 2;
  stresspat[9].vel[0] = 120;
  stresspat[9].vel[1] = 60;
  stresspat[9].expcoef[0] = (float) 1.1;
  stresspat[9].expcoef[1] = (float) 0.9;

  stresspat[10].name = "Slip Jig";
  stresspat[10].meter = "9/8";
  stresspat[10].nseg = 9;
  stresspat[10].nval = 3;
  stresspat[10].vel[0] = 110;
  stresspat[10].vel[1] = 70;
  stresspat[10].vel[2] = 80;
  stresspat[10].expcoef[0] = (float) 1.4;
  stresspat[10].expcoef[1] = (float) 0.5;
  stresspat[10].expcoef[2] = (float) 1.1;

  stresspat[11].name = "Double Jig";
  stresspat[11].meter = "6/8";
  stresspat[11].nseg = 6;
  stresspat[11].nval = 3;
  stresspat[11].vel[0] = 110;
  stresspat[11].vel[1] = 70;
  stresspat[11].vel[2] = 80;
  stresspat[11].expcoef[0] = (float) 1.2;
  stresspat[11].expcoef[1] = (float) 0.7;
  stresspat[11].expcoef[2] = (float) 1.1;

  stresspat[12].name = "Single Jig";
  stresspat[12].meter = "6/8";
  stresspat[12].nseg = 6;
  stresspat[12].nval = 3;
  stresspat[12].vel[0] = 110;
  stresspat[12].vel[1] = 80;
  stresspat[12].vel[2] = 90;
  stresspat[12].expcoef[0] = (float) 1.2;
  stresspat[12].expcoef[1] = (float) 0.7;
  stresspat[12].expcoef[2] = (float) 1.1;

  stresspat[13].name = "Slide";
  stresspat[13].meter = "6/8";
  stresspat[13].nseg = 6;
  stresspat[13].nval = 3;
  stresspat[13].vel[0] = 110;
  stresspat[13].vel[1] = 80;
  stresspat[13].vel[2] = 90;
  stresspat[13].expcoef[0] = (float) 1.3;
  stresspat[13].expcoef[1] = (float) 0.8;
  stresspat[13].expcoef[2] = (float) 0.9;

  stresspat[14].name = "Jig";
  stresspat[14].meter = "6/8";
  stresspat[14].nseg = 6;
  stresspat[14].nval = 3;
  stresspat[14].vel[0] = 110;
  stresspat[14].vel[1] = 80;
  stresspat[14].vel[2] = 90;
  stresspat[14].expcoef[0] = (float) 1.2;
  stresspat[14].expcoef[1] = (float) 0.7;
  stresspat[14].expcoef[2] = (float) 1.1;

  stresspat[15].name = "Ragtime";
  stresspat[15].meter = "12/8";
  stresspat[15].nseg = 12;
  stresspat[15].nval = 3;
  stresspat[15].vel[0] = 110;
  stresspat[15].vel[1] = 70;
  stresspat[15].vel[2] = 90;
  stresspat[15].expcoef[0] = (float) 1.4;
  stresspat[15].expcoef[1] = (float) 0.6;
  stresspat[15].expcoef[2] = (float) 1.0;

  stresspat[16].name = "Strathspey";
  stresspat[16].meter = "C";
  stresspat[16].nseg = 8;
  stresspat[16].nval = 2;
  stresspat[16].vel[0] = 120;
  stresspat[16].vel[1] = 80;
  stresspat[16].expcoef[0] = (float) 1.0;
  stresspat[16].expcoef[1] = (float) 1.0;

  stresspat[17].name = "Fling";
  stresspat[17].meter = "C";
  stresspat[17].nseg = 8;
  stresspat[17].nval = 2;
  stresspat[17].vel[0] = 110;
  stresspat[17].vel[1] = 90;
  stresspat[17].expcoef[0] = (float) 1.4;
  stresspat[17].expcoef[1] = (float) 0.6;

  stresspat[18].name = "Set Dance";
  stresspat[18].meter = "4/4";
  stresspat[18].nseg = 8;
  stresspat[18].nval = 2;
  stresspat[18].vel[0] = 110;
  stresspat[18].vel[1] = 90;
  stresspat[18].expcoef[0] = (float) 1.4;
  stresspat[18].expcoef[1] = (float) 0.6;

  stresspat[19].name = "Set Dance";
  stresspat[19].meter = "C|";
  stresspat[19].nseg = 8;
  stresspat[19].nval = 2;
  stresspat[19].vel[0] = 110;
  stresspat[19].vel[1] = 90;
  stresspat[19].expcoef[0] = (float) 1.4;
  stresspat[19].expcoef[1] = (float) 0.6;

  stresspat[20].name = "Waltz";
  stresspat[20].meter = "3/4";
  stresspat[20].nseg = 3;
  stresspat[20].nval = 3;
  stresspat[20].vel[0] = 110;
  stresspat[20].vel[1] = 70;
  stresspat[20].vel[2] = 70;
  stresspat[20].expcoef[0] = (float) 1.04;
  stresspat[20].expcoef[1] = (float) 0.98;
  stresspat[20].expcoef[2] = (float) 0.98;

  stresspat[21].name = "Slow March";
  stresspat[21].meter = "C|";
  stresspat[21].nseg = 8;
  stresspat[21].nval = 3;
  stresspat[21].vel[0] = 115;
  stresspat[21].vel[1] = 85;
  stresspat[21].vel[2] = 100;
  stresspat[21].vel[3] = 85;
  stresspat[21].expcoef[0] = (float) 1.1;
  stresspat[21].expcoef[1] = (float) 0.9;
  stresspat[21].expcoef[2] = (float) 1.1;
  stresspat[21].expcoef[3] = (float) 0.9;

  stresspat[22].name = "Slow March";
  stresspat[22].meter = "C";
  stresspat[22].nseg = 8;
  stresspat[22].nval = 3;
  stresspat[22].vel[0] = 115;
  stresspat[22].vel[1] = 85;
  stresspat[22].vel[2] = 100;
  stresspat[22].vel[3] = 85;
  stresspat[22].expcoef[0] = (float) 1.1;
  stresspat[22].expcoef[1] = (float) 0.9;
  stresspat[22].expcoef[2] = (float) 1.1;
  stresspat[22].expcoef[3] = (float) 0.9;

  stresspat[23].name = "March";
  stresspat[23].meter = "C|";
  stresspat[23].nseg = 8;
  stresspat[23].nval = 2;
  stresspat[23].vel[0] = 115;
  stresspat[23].vel[1] = 85;
  stresspat[23].expcoef[0] = (float) 1.1;
  stresspat[23].expcoef[1] = (float) 0.9;

  stresspat[24].name = "March";
  stresspat[24].meter = "C";
  stresspat[24].nseg = 8;
  stresspat[24].nval = 4;
  stresspat[24].vel[0] = 115;
  stresspat[24].vel[1] = 85;
  stresspat[24].vel[2] = 100;
  stresspat[24].vel[3] = 85;
  stresspat[24].expcoef[0] = (float) 1.1;
  stresspat[24].expcoef[1] = (float) 0.9;
  stresspat[24].expcoef[2] = (float) 1.1;
  stresspat[24].expcoef[3] = (float) 0.9;

  stresspat[25].name = "March";
  stresspat[25].meter = "6/8";
  stresspat[25].nseg = 8;
  stresspat[25].nval = 3;
  stresspat[25].vel[0] = 110;
  stresspat[25].vel[1] = 70;
  stresspat[25].vel[2] = 80;
  stresspat[25].expcoef[0] = (float) 1.1;
  stresspat[25].expcoef[1] = (float) 0.95;
  stresspat[25].expcoef[2] = (float) 0.95;

  stresspat[26].name = "March";
  stresspat[26].meter = "2/4";
  stresspat[26].nseg = 8;
  stresspat[26].nval = 2;
  stresspat[26].vel[0] = 115;
  stresspat[26].vel[1] = 85;
  stresspat[26].expcoef[0] = (float) 1.1;
  stresspat[26].expcoef[1] = (float) 0.9;

  stresspat[27].name = "Polka k1";
  stresspat[27].meter = "3/4";
  stresspat[27].nseg = 3;
  stresspat[27].nval = 3;
  stresspat[27].vel[0] = 90;
  stresspat[27].vel[1] = 110;
  stresspat[27].vel[2] = 100;
  stresspat[27].expcoef[0] = (float) 0.75;
  stresspat[27].expcoef[1] = (float) 1.25;
  stresspat[27].expcoef[2] = (float) 1.00;

  stresspat[28].name = "Polka";
  stresspat[28].meter = "4/4";
  stresspat[28].nseg = 8;
  stresspat[28].nval = 2;
  stresspat[28].vel[0] = 110;
  stresspat[28].vel[1] = 90;
  stresspat[28].expcoef[0] = (float) 1.4;
  stresspat[28].expcoef[1] = (float) 0.6;

  stresspat[29].name = "saucy";
  stresspat[29].meter = "3/4";
  stresspat[29].nseg = 6;
  stresspat[29].nval = 6;
  stresspat[29].vel[0] = 115;
  stresspat[29].vel[1] = 85;
  stresspat[29].vel[2] = 120;
  stresspat[29].vel[3] = 85;
  stresspat[29].vel[4] = 115;
  stresspat[29].vel[5] = 85;
  stresspat[29].expcoef[0] = (float) 1.2;
  stresspat[29].expcoef[1] = (float) 0.8;
  stresspat[29].expcoef[2] = (float) 1.3;
  stresspat[29].expcoef[3] = (float) 0.7;
  stresspat[29].expcoef[4] = (float) 1.1;
  stresspat[29].expcoef[5] = (float) 0.9;

  stresspat[30].name = "Slip jig";
  stresspat[30].meter = "3/4";
  stresspat[30].nseg = 9;
  stresspat[30].nval = 3;
  stresspat[30].vel[0] = 110;
  stresspat[30].vel[1] = 80;
  stresspat[30].vel[2] = 100;
  stresspat[30].expcoef[0] = (float) 1.3;
  stresspat[30].expcoef[1] = (float) 0.7;
  stresspat[30].expcoef[2] = (float) 1.0;

  stresspat[31].name = "Tango";
  stresspat[31].meter = "2/4";
  stresspat[31].nseg = 8;
  stresspat[31].nval = 4;
  stresspat[31].vel[0] = 110;
  stresspat[31].vel[1] = 90;
  stresspat[31].vel[2] = 90;
  stresspat[31].vel[3] = 100;
  stresspat[31].expcoef[0] = (float) 1.6;
  stresspat[31].expcoef[1] = (float) 0.8;
  stresspat[31].expcoef[2] = (float) 0.8;
  stresspat[31].expcoef[3] = (float) 0.8;

}



int
stress_locator (char *rhythmdesignator, char *timesigstring)
{
  int i;
  for (i = 0; i < nmodels; i++)
    {
      if (casecmp (rhythmdesignator, stresspat[i].name) == 0 &&
	  casecmp (timesigstring, stresspat[i].meter) == 0)
	{
	  return i;
	}
    }
  return -1;
}

/* [SS] 2015-12-31 load_stress_parameters  returns 0 or -1 */
int
load_stress_parameters (char *rhythmdesignator)
{
  int n, i, index, nval;
  int qnotenum, qnoteden;
  maxdur = 0;
  for (n = 0; n < 32; n++)
    {
      fdur[n] = 0.0;
      ngain[n] = 0;
    }
  fdursum[0] = fdur[0];
  if (strlen (rhythmdesignator) < 2)
    {
      beatmodel = 0;
      return -1;
    }
  index = stress_locator (rhythmdesignator, timesigstring);
  if (index == -1)
    {
      printf ("**warning** rhythm designator %s %s is not one of\n",
	      rhythmdesignator, timesigstring);
      for (i = 0; i < nmodels; i++)
	{
	  printf ("%s %s ", stresspat[i].name, stresspat[i].meter);
	  if (i % 5 == 4)
	    printf ("\n");
	}
      printf ("\n");
      beatmodel = 0;
      return -1;
    }

  if (stressmodel == 0)
    beatmodel = 2;
  else
    beatmodel = stressmodel;

  nseg = stresspat[index].nseg;
  nval = stresspat[index].nval;

  segnum = time_num;
  segden = time_denom * nseg;
  reduce (&segnum, &segden);
/* compute number of segments in quarter note */
  qnotenum = segden;
  qnoteden = segnum * 4;
  reduce (&qnotenum, &qnoteden);

  for (n = 0; n < nseg + 1; n++)
    {
      i = n % nval;
      ngain[n] = stresspat[index].vel[i];
      fdur[n] = stresspat[index].expcoef[i];
      if (verbose)
	printf ("%d %f\n", ngain[n], fdur[n]);
      maxdur = MAX (maxdur, fdur[n]);
      if (n > 0)
	fdursum[n] =
	  fdursum[n - 1] + fdur[n - 1] * (float) qnoteden / (float) qnotenum;
/* num[],denom[] use quarter note units = 1/1, segment units are usually
   half that size, so we divide by 2.0
*/
    }
/*printf("maxdur = %f\n",maxdur);*/
return 0;
}



/* [SS] 2013-04-10 */
void
read_custom_stress_file (char *filename)
{
  FILE *inhandle;
  char name[32];
  char meter[6];
  char str[4];
  int index;
  int nseg, nval;
  int gain;
  float expand;
  int i, j;
  init_stresspat ();
  inhandle = fopen (filename, "r");
  if (inhandle == NULL)
  {
    printf ("Failed to open file %s\n", filename);
    exit (1);
  }
  if (verbose > 0) printf("reading %s\n",filename);
  while (!feof (inhandle))
  {
    if (feof (inhandle))
      break;
    j = fscanf (inhandle, "%31s", name); /* [SDG] 2020-06-03 */
    if (j == -1)
      break;
    j = fscanf (inhandle, "%5s", meter); /* [SDG] 2020-06-03 */
    index = stress_locator (&name[0], &meter[0]);
    if (verbose > 1) printf ("%s %s index = %d\n",name,meter,index);
    
    j = fscanf (inhandle, "%d %d", &nseg, &nval);
    if (verbose > 2) printf ("j = %d nseg = %d nval = %d\n", j, nseg, nval);
    if (j != 2) exit(-1);
    
    if (nval > 16) {
      printf("stresspat.c: nval = %d is too large for structure %s\n",nval,name);
      exit(-1);
    }
    
    /* copy model to stresspat[] */
    if (index <0) {
      /*creating new model, must include name and meter */
      index = nmodels;
      if (index > 47)
      {
        printf ("used up all available space for stress models\n");
	fclose(inhandle); /* [JA] 2020-09-30 */
        return;
      }
      nmodels++;
      stresspat[index].name =
      (char *) checkmalloc ((strlen (name) + 1) * sizeof (char));
      stresspat[index].meter =
      (char *) checkmalloc ((strlen (meter) + 1) * sizeof (char));
      strcpy(stresspat[index].name, name); /* [RZ] 2013-12-25 */
      strcpy(stresspat[index].meter, meter); /* [RZ] 2013-12-25 */
    }
    
    stresspat[index].nseg = nseg;
    stresspat[index].nval = nval;
    for (i = 0; i < nval; i++)
    {
      j = fscanf (inhandle, "%d %f", &gain, &expand);
      if(verbose > 2) printf ("%d %f\n", gain, expand);
      if (j != 2) exit(1);
      if (feof (inhandle))
        break;
      stresspat[index].vel[i] = gain;      /* [RZ] 2013-12-25 */
      stresspat[index].expcoef[i] = expand; /* [RZ] 2013-12-25 */
    }
    if (fgets (str, 3, inhandle) == NULL) break; /* [SDG] 2020-06-03 */
  }
  fclose(inhandle); /* [JA] 2020-09-30 */
}
