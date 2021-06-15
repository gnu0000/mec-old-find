/*
 *
 * find.c
 * Monday, 8/12/1996.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <process.h>
#include <time.h>
#include <ctype.h>
#include <conio.h>
#include <GnuType.h>
#include <GnuDir.h>
#include <GnuReg.h>
#include <GnuArg.h>
#include <GnuMisc.h>
#include <GnuStr.h>
#include <GnuMem.h>

#define MAXREGEX 50

BOOL bRECURSE, bLOCALDRIVES;
BOOL bDIRONLY, bNETDRIVES;
BOOL bBEFORE,  bAFTER,   bDEBUG;

PSZ  pszSHOW = "D  T  S  PN" ;
PSZ  pszCMD  = NULL;

FDATE fBEFORE, fAFTER;




PCHAR pcRows = (PCHAR) 0x00000484;
INT   uRow = 0;

void prnt (PSZ psz, ...)
   {
   va_list vlst;

   if (uRow++ == *pcRows)
      {
      printf ("===<Press a key to continue>===");
      getch ();
      printf ("\r                               \r");
      uRow = 1;
      }
   va_start (vlst, psz);
   vprintf (psz, vlst);
   va_end (vlst);
   }


/*
 *
 *
 */
void Usage (void)
   {
   prnt ("FIND   File find utility using true REGEX v1.0   by GNU            %s\n", __DATE__);
   prnt ("\n");
   prnt ("USAGE: FIND [options] filespec [filespec ...]\n");
   prnt ("\n");
   prnt ("WHERE: filespec is any legal regular expression (see below)\n");
   prnt ("       [options] are 0 or more of:\n");
   prnt ("         /r ...... recurse directories\n");
   prnt ("         /e ...... every local hard disk drive (implies /r)\n");
   prnt ("         /n ...... every networked or removable drive (implies /r)\n");
   prnt ("         /d ...... look for dir names rather than files (implies /r)\n");
   prnt ("         /b=date . files before this date. Use almost any format\n");
   prnt ("         /a=date . files after this date.   21Apr 4/21/96 etc...\n");
   prnt ("         /c=str .. Exec this cmd each file (replace '~' with name)\n");
   prnt ("         /s=str .. Show these fields in listing (see below, default=\"D T S PN\")\n");
   prnt ("                    D=date,T=time,S=size,P=path,N=name <SPC>=space\n");
   prnt ("\n");
   prnt ("REGULAR EXPRESSION CHARS: (some chars require the regex to be quoted)\n");
   prnt ("         * .... 0 or more of any          @ .... 0 or more of prev expr\n");
   prnt ("         ? .... any 1 char                {} ... define expression group\n");
   prnt ("         [] ... any char in group         | .... prev OR next expr\n");
   prnt ("         + .... 1 or more of prev expr    \\c ... Match any char 'c'\n");
   prnt ("\n");
   prnt ("EXAMPLES:\n");
   prnt (" find *  ....................... matches all files (including *.* files)\n");
   prnt (" find /en [a-j]*.c ............. all files starting with a thru j on all drives\n");
   prnt (" find \"*.{EBL}|{ZIP}\" .......... all .EBL and .ZIP files (needs quotes)\n");
   prnt (" find a{bc}+* .................. files like abc* abcbc* abcbcbc*\n");
   prnt (" find /s=\"pn s d t a\" * ........ make list look like a dir cmd\n");
   prnt (" find /enc=\"4dos/c del ~\" *.z .. delete all matching files on all drives!!!\n");
   prnt (" find /a=21Apr ................. All files on or after April 21st of this year\n");
   exit (0);
   }

/**************************************************************************/
/*                                                                        */
/* Date parsing functions                                                 */
/*                                                                        */
/**************************************************************************/


CHAR *MON[] = {"XXX","Jan","Feb","Mar","Apr","May","Jun",
               "Jul","Aug","Sep","Oct","Nov","Dec", NULL};

UINT MonthToNum (PSZ pszMon, BOOL bShort)
   {
   UINT i;

   for (i=1; i<13; i++)
      if (!strnicmp (pszMon, MON[i], 3))
         return i;
   return 0;
   }

/*
 * Parses date strings of the forms:
 * null              (todays date)
 * MonDD             Jan21
 * DDMon[YY[YY]]     21Jan    21Jan96    21Jan1996
 * MMDDYY[YY]        012196   01211996
 * M[M]/D[D]/YY[YY]] 1/1/90   01/21/96   01/21/1996 01-21-96
 */
FDATE ParseDate (PSZ pszDate)
   {
   struct tm *ptm;
   time_t tim;
   FDATE  fdate;
   PSZ    p, p2;
   UINT   uLen, uYear;
   CHAR   cTmp;

   /*--- default fields to today ---*/
   tim = time (NULL);
   ptm  = localtime (&tim);
   fdate.day	= ptm->tm_mday ;
   fdate.month = ptm->tm_mon+1;
   fdate.year	= ptm->tm_year - 80;

   /*--- Empty string - use todays date ---*/
   if (!pszDate || !*pszDate) 
      return fdate;

   uLen = strlen (pszDate);

   /*--- Format: MonDD ---*/
   if (isalpha (*pszDate))
      {
      if (!(fdate.month = MonthToNum (pszDate, TRUE)))
         Error ("Invalid date format: %s", pszDate);
      for (p=pszDate+3; isalpha(*p); p++) // skip over month
         ;
      if (!(fdate.day = atoi (p)))
         Error ("Invalid date format: %s", pszDate);
      return fdate;
      }

   /*--- Format DDMon[YY[YY]] ---*/
   if (isdigit(pszDate[0]) && isdigit(pszDate[1]) && isalpha(pszDate[2]))
      {
      fdate.day = atoi (pszDate);
      if (!(fdate.month = MonthToNum (pszDate+2, TRUE)))
         Error ("Invalid date format: %s", pszDate);
      if (uLen <= 5)
         return fdate;
      if (!(uYear = atoi (pszDate+5)))
         Error ("Invalid year in date format: %s", pszDate);
      fdate.year = (uYear > 100 ? uYear-1980 : (uYear > 79 ? uYear-80 : uYear+20));
      return fdate;
      }

   /*--- Format: MMDDYY[YY] ---*/
   if (uLen >= 6 &&  
            isdigit(pszDate[0])   && isdigit(pszDate[1]) &&
            isdigit(pszDate[2])   && isdigit(pszDate[3]) &&
            isdigit(pszDate[4])   && isdigit(pszDate[5]))
      {
      cTmp = pszDate[2]; pszDate[2]='\0'; fdate.month = atoi (pszDate + 0); pszDate[2] = cTmp;
      cTmp = pszDate[4]; pszDate[4]='\0'; fdate.day   = atoi (pszDate + 2); pszDate[4] = cTmp;
      uYear = atoi (pszDate + 4);
      fdate.year = (uYear > 100 ? uYear-1980 : (uYear > 79 ? uYear-80 : uYear+20));
      return fdate;
      }

   /*--- Format: MM/DD[/YY[YY]], MM-DD-[YY[YY]] ---*/
   if ((p = strchr (pszDate, '-')) || (p = strchr (pszDate, '/')))
      {
      cTmp = *p; *p = '\0'; fdate.month = atoi (pszDate); *p = cTmp;
      if ((p2 = strchr (p+1, '-')) || (p2 = strchr (p+1, '/')))
         {
         cTmp = *p2; *p2 = '\0'; fdate.day = atoi (p+1); *p2 = cTmp;
         uYear = atoi (p2 + 1);
         fdate.year = (uYear > 100 ? uYear-1980 : (uYear > 79 ? uYear-80 : uYear+20));
         return fdate;
         }
      fdate.day = atoi (p+1);
      return fdate;
      }
   Error ("Invalid date format: %s", pszDate);
   }


/**************************************************************************/
/*                                                                        */
/*                                                                        */
/*                                                                        */
/**************************************************************************/

/*
 *
 *
 */
int ExecFile (PSZ pszCmd, PSZ pszDir, PFINFO pfo)
   {
   CHAR sz[512];
   INT  iRet;
   PSZ  psz, *ppszArgs;

   /*--- replace '~' with file path/name in cmd line ---*/
   for (psz = sz; *pszCmd; pszCmd++)
      {
      if (*pszCmd == '~')
         {
         strcpy (psz, pszDir);
         psz += strlen (pszDir);
         strcpy (psz, pfo->szName);
         psz += strlen (pfo->szName);
         }
      else
         {
         if (*pszCmd == '/')
            *psz++ = ' ';
         *psz++ = *pszCmd;
         }
      }
   *psz = '\0';

   printf ("Executing '%s'\n", sz);
   ppszArgs = StrMakePPSZ (sz, " \t", FALSE, FALSE, NULL);
   if (bDEBUG)
      {
      for (iRet=0; ppszArgs[iRet]; iRet++)
         printf ("param #%d: %s\n", iRet, ppszArgs[iRet]);
      }
   iRet = spawnvp(P_WAIT,ppszArgs[0],ppszArgs);
   MemFreePPSZ (ppszArgs, 0);
   if (iRet)
      printf ("Exec return: %d\n", iRet);
   return(iRet);
   }


/*
 *
 *
 */
void PrintFile (PSZ pszDir, PFINFO pfo)
   {
   UINT i;
   CHAR sz[64];

   for (i=0; pszSHOW[i]; i++)
      {
      switch (toupper(pszSHOW[i]))
         {
         case 'D':
            printf ("%s", StrFDateString (sz, pfo->fDate));
            break;
         case 'T':
            printf ("%s", StrFTimeString (sz, pfo->fTime));
            break;
         case 'S':
            printf ("%7lu", pfo->ulSize);
            break;
         case 'P':
            printf ("%s", pszDir);
            break;
         case 'N':
            printf ("%-13s", pfo->szName);
            break;
         case ' ':
            printf ("%c", pszSHOW[i]);
            break;
         case 'A':
            printf ("%c%c%c%c%c", 
               (pfo->cAttr & FILE_HIDDEN    ? 'H' : '_'),
               (pfo->cAttr & FILE_READONLY  ? 'R' : '_'),
               (pfo->cAttr & FILE_SYSTEM    ? 'S' : '_'),
               (pfo->cAttr & FILE_ARCHIVED  ? 'A' : '_'),
               (pfo->cAttr & FILE_DIRECTORY ? 'D' : '_'));
            break;
         default:
            printf ("%c", pszSHOW[i]);
         }
      }
   printf ("\n");
   }


/**************************************************************************/
/*                                                                        */
/*                                                                        */
/*                                                                        */
/**************************************************************************/

INT CompareDate (FDATE fDate1, FDATE fDate2)
   {
   PUSHORT pus1, pus2;

   pus1 = (PVOID)&fDate1;
   pus2 = (PVOID)&fDate2;
   return (*pus1 > *pus2 ? 1 : (*pus1 < *pus2 ? -1 : 0));
   }

/*
 *
 *
 */
UINT FindMatches (PRX *prxMatch, PSZ pszDir) 
   {
   CHAR   sz [256];
   PFINFO pfo, pfoList;
   UINT   i, uMatch = 0;

   /*--- normal files ---*/
   sprintf (sz, "%s%s", pszDir, "*.*");
   pfoList = DirFindAll (sz, (bDIRONLY ? FILE_DIRECTORY : FILE_NORMAL));
   for (pfo = pfoList; pfo; pfo = pfo->next)
      {
      for (i=0; prxMatch[i]; i++)
         {
         if (bBEFORE && CompareDate (pfo->fDate, fBEFORE) > 0)
            continue;
         if (bAFTER  && CompareDate (pfo->fDate, fAFTER)  < 0)
            continue;
         if(!_regMatch (pfo->szName, prxMatch[i]))
            continue;

         if (pszCMD)
            ExecFile (pszCMD, pszDir, pfo);
         else
            PrintFile (pszDir, pfo);
         uMatch++;
         break;
         }
      }
   DirFindAllCleanup (pfoList);

   if (!bRECURSE)
      return uMatch;

   /*--- dirs ---*/
   pfoList = DirFindAll (sz, FILE_DIRECTORY);
   for (pfo = pfoList; pfo; pfo = pfo->next)
      {
      sprintf (sz, "%s%s\\", pszDir, pfo->szName);
      uMatch += FindMatches (prxMatch , sz);
      }
   DirFindAllCleanup (pfoList);
   }


/*
 *
 *
 */
UINT SearchDrive (PRX *prxMatch, CHAR cDrive)
   {
   CHAR sz [16];

   sprintf (sz, "%c:\\", cDrive);
   return  FindMatches (prxMatch, sz);
   }

/*
 *
 *
 */
UINT SearchAllDrives (PRX *prxMatch)
   {
   UINT uStat, i, uMatch = 0;
   BOOL bRemote;

   for (i=3; i<27; i++)
      {
      if (!DirGetDriveInfo (i, &uStat)) // drive is: i - 1 + 'A'
         continue;

      bRemote = ((uStat & DI_SUBST) || (uStat & DI_REMOTE));
      if ((!bNETDRIVES && bRemote) || (!bLOCALDRIVES && !bRemote))
         continue;

      uMatch += SearchDrive (prxMatch, (CHAR)(i - 1 + 'A'));
      }
   return uMatch;
   }



/*
 *
 *
 */
int main (int argc, char *argv[])
   {
   UINT i, j, uMatches;
   PRX  prxMatch [MAXREGEX];
   BOOL bHasDriveSpec = FALSE;
   PSZ  pszArg;
   
   if (ArgBuildBlk (" ?- ^r- ^e- ^n- ^d- ^x- ^s% ^c% ^a? ^b?"))
      Error ("%s", ArgGetErr ());
   if (ArgFillBlk (argv))
      Error ("%s", ArgGetErr ());
   if (ArgIs ("?") || (!ArgIs (NULL) && !ArgIs ("a") && !ArgIs ("b")))
      Usage ();

   bDEBUG       = ArgIs ("x");
   bRECURSE     = ArgIs ("r");
   bLOCALDRIVES = ArgIs ("e");
   bNETDRIVES   = ArgIs ("n");
   bDIRONLY     = ArgIs ("d");
   bBEFORE      = ArgIs ("b");
   bAFTER       = ArgIs ("a");

   if (bLOCALDRIVES || bNETDRIVES)
      bRECURSE = TRUE;

   if (ArgIs ("s"))
      pszSHOW = ArgGet ("s", 0);
   if (ArgIs ("c"))
      pszCMD = ArgGet ("c", 0);
   if (bAFTER)
      fAFTER = ParseDate (ArgGet ("a", 0));
   if (bBEFORE)
      fBEFORE = ParseDate (ArgGet ("b", 0));

   RegCaseSensitive (FALSE);

   for (i=j=0; i < ArgIs (NULL) && j+1 < MAXREGEX; i++)
      {
      pszArg = ArgGet (NULL, i);
      if (pszArg [1] == ':') // a drive spec not a file
         {
         bHasDriveSpec = TRUE;
         continue;
         }
      prxMatch[j++] = _regParseRegex (pszArg);
      if (RegIsError ())
         Error ("%s : %s", RegIsError (), pszArg);
      }
   if (!j)
      prxMatch[j++] = _regParseRegex ("*"); // no param given
   prxMatch[j] = NULL;

   if (bLOCALDRIVES || bNETDRIVES)
      uMatches = SearchAllDrives (prxMatch);
   else if (bHasDriveSpec)
      {
      for (i=0; i < ArgIs (NULL)&& i+1 < MAXREGEX; i++)
         {
         pszArg = ArgGet (NULL, i);
         if (pszArg [1] == ':') // a drive spec
            uMatches += SearchDrive (prxMatch, *pszArg);
         }
      }
   else
      uMatches = FindMatches (prxMatch, "");
   return 0;
   }

