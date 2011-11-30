/* 
 * Sharpfin project
 * Copyright (C) by Steve Clarke
 * 2011-11-30 Philipp Schmidt
 *   Added to github 
 *
 * This file is part of the sharpfin project
 *  
 * This Library is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this source files. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define DEBUG(s,t,u) printf(s,t,u)
//#define DEBUG(s,t,y)

// Configuration File Functions
#define CFG_NOCMD 0
#define CFG_LABEL 1
#define CFG_SEARCHFORTAG 2
#define CFG_PRINTUNTILTAG 3
#define CFG_PRINTATTRIBUTE 4
#define CFG_PRINT 5
#define CFG_SEARCHFORSTR 6
#define CFG_PRINTUNTILSTR 7
#define CFG_LOOPUNTILEOF 8
#define CFG_END 9

void readcfg(FILE *fp) ;
int getcommand() ;
char *getdata() ;
void gotoline() ;
void nextline() ;

int currline ;

// Source File Functions
char *getthing(FILE *fp) ;
int istag() ;
char *thing() ;
char *thingattribute(char *attribute) ;
int end() ;
#define iswhite(c) (c==' ' || c=='\n' || c=='\t' || c=='\r')

/*
 * Main
 */
int main(int argc, char *argv[]) {
  FILE *src, *cfg ;
  int finished=(1==0) ;
  if (argc<3) {
    printf("xmlparse parse.cfg filename [arg0 ... ]\n\n") ;
    printf("parse.cfg format\n---------------\n\n") ;
    printf("  :label\n") ;
    printf("  searchfortag tag\n") ;
    printf("  printuntiltag tag\n") ;
    printf("  printattribute attributename\n") ;
    printf("  print string\n") ;
    printf("  searchforstring string\n") ;
    printf("  printuntilstring string\n") ;
    printf("  loopuntileof label\n") ;
    printf("  end\n\n") ;
    printf("  arg0-arg9 can be referred to in print command as $0-$9\n") ;
    printf("  also, $. => space, $$ => $ and $n => \\n\n\n") ;
    return 1 ;
  }
  src=fopen(argv[2],"r") ;
  cfg=fopen(argv[1],"r") ;

  if (src==NULL || cfg==NULL) {
    fprintf(stderr, "xmlparse: unable to open file(s)\n") ;
    return 1 ;
  }
  readcfg(cfg) ;
  do {
    switch(getcommand()) {
    case CFG_LABEL:
    case CFG_NOCMD:
           nextline() ;
           break ;
    case CFG_SEARCHFORTAG:
      do {
        getthing(src) ;
      } while ( (!istag() || strcmp(thing(), getdata())!=0) && !end()) ;
      nextline() ;
      break ;
    case CFG_PRINTUNTILTAG:
      getthing(src) ;
      if (!end() && !istag()) printf("%s",thing()) ;
      while (!end() && !(istag() && strcmp(thing(), getdata())==0)) {
        getthing(src) ;
        if (!istag()) printf(" %s", thing()) ;
      }
      nextline() ;
      break ;
    case CFG_PRINTATTRIBUTE:
      printf("%s", thingattribute(getdata())) ;
      nextline() ;
      break ;
    case CFG_PRINT: {
        char *b ;
        for (b=getdata() ; *b!='\0'; b++) {
          // Argv Entry
          if (*b=='$') {
            if (*(b+1)!='\0') b++ ;
            switch (*b) {
            case '$':
              printf("$") ;
              break ;
            case '0':
              if (argc>=3) printf(argv[3]) ;    
              break ;
            case '1':
              if (argc>=4) printf(argv[4]) ;
              break ;
            case '2':
              if (argc>=5) printf(argv[5]) ;
              break ;
            case '3':
              if (argc>=6) printf(argv[6]) ;
              break ;
            case '4':
              if (argc>=7) printf(argv[7]) ;
              break ;
            case '5':
              if (argc>=8) printf(argv[8]) ;
              break ;
            case '6':
              if (argc>=9) printf(argv[9]) ;
              break ;
            case '7':
              if (argc>=10) printf(argv[10]) ;
              break ;
            case '8':
              if (argc>=11) printf(argv[11]) ;
              break ;
            case '9':
              if (argc>=12) printf(argv[12]) ;
              break ;
            case 'n':
              printf("\n") ;
              break ;
            case '.':
              printf(" ") ;
              break ;
            }
          } else {
            printf("%c", *b) ;
          }
        }
      }
      nextline() ;
      break ;
    case CFG_SEARCHFORSTR:
      do {
        getthing(src) ;
      } while (strcmp(thing(), getdata())==0 && !end()) ;
      nextline() ;
      break ;
    case CFG_PRINTUNTILSTR:
      getthing(src) ;
      while (!end() && strcmp(thing(), getdata())!=0) {
        printf(" %s", thing()) ;
        getthing(src) ;
      }
      nextline() ;
      break ;
    case CFG_LOOPUNTILEOF:
      if (!end()) gotoline() ;
      else nextline() ;
      break ;
    case CFG_END:
      finished=(1==1) ;
      break ;
    }
  } while (!end() && !finished) ;
 fclose(src) ;
 fclose(cfg) ;
 return 0 ;
}

// Source Data Functions
#define THINGLEN 1024
char lastthing[THINGLEN+1] ;
int enddetected ;
int end() {
  return enddetected ;
}

char *getthing(FILE *fp) {
 int i, ch, inquotes=(1==0), endchar, endchar2 ;
 i=0 ;
 if (feof(fp)) {
   enddetected=(1==1) ;
   lastthing[0]='\0' ;
   return lastthing ;
 }
 enddetected=(1==0) ;

 // Skip white space
  do {
    ch=fgetc(fp) ;
  } while (iswhite(ch)) ;

 // Decide where we stop
 if (ch=='<') { endchar='>' ; endchar2='>' ; }
 else { endchar=' ' ; endchar2='<' ; }

 // Process tring
 i=0 ;
 if (ch!=EOF) do {
   if (ch=='\"') inquotes=!inquotes ;
   lastthing[i++]=ch ;
   ch=fgetc(fp) ;
   if (iswhite(ch)) ch=' ' ;
 } while (ch!=EOF && (inquotes || (ch!=endchar && ch!=endchar2)) ) ;
 lastthing[i]='\0' ;
 if (ch=='<') ungetc(ch, fp);
 return (lastthing) ;  
}

int istag() {
  return (lastthing[0]=='<') ;
}

char *thing() {
  static char reply[THINGLEN+1] ;
  int i ;
  if (istag()) {
    strcpy(reply, &lastthing[1]) ;
    for (i=0; i<THINGLEN && (reply[i]!='>' && !iswhite(reply[i])); i++) ;
    reply[i]='\0' ;
  } else {
    strcpy(reply, lastthing) ; 
  }
  return reply ;
}

char *thingattribute(char *attribute) {
  static char reply[THINGLEN+1] ;
  int i, j ;

  if (!istag()) {
    reply[0]='\0' ;
  } else {
    i=0; j=0 ;
    reply[0]='\0' ;
    while (lastthing[i]!='\0' && reply[0]=='\0') {
      while (lastthing[i]!='\0' && lastthing[i]==attribute[j]) {i++ ; j++ ; }
      if (attribute[j]=='\0' && lastthing[i]=='=') {
        i++ ;
        if (lastthing[i]=='\"') {
          i++ ;
          strcpy(reply, &lastthing[i]) ;
          for (j=0; reply[j]!='\"' && reply[j]!='\0'; j++) ;
          reply[j]='\0' ;
        } else {
          strcpy(reply, &lastthing[i]) ;
          for (j=0; !iswhite(reply[j]) && reply[j]!='\0'; j++) ;
          reply[j]='\0' ;
        }
      } else {
        if (lastthing[i]!='\0') i++ ;
      }
    }
  }
  return reply ;
}

// CFG Functions
#define CFGLEN 128
#define CFGLINES 256

int cfgcmd[CFGLINES] ;
char cfgdata[CFGLINES][CFGLEN] ;
void readcfg(FILE *fp) {
  int i, j, line=0 ;
  char buf[CFGLEN*2+1] ;
  for (i=0; i<CFGLINES; i++) cfgdata[i][0]='\0' ;
  while (!feof(fp) && line<CFGLINES) {
    fgets(buf, CFGLEN*2, fp) ;
    // Remove Trailing White Space
    for (i=(strlen(buf)-1); (i>0 && ((buf[i]==' ') || (buf[i]=='\n'))); i--)
	    buf[i]='\0' ;
    //
    // Note:
    //
    // ...command    arguments  
    //    ^      \0  ^
    //    i          j
    //

    // Skip Leading White Space
    for (i=0; buf[i]!='\0' && iswhite(buf[i]); i++) ;

    // Labels are a Special Case
    if (buf[i]==':') {

      cfgcmd[line]=CFG_LABEL ;
      strcpy(cfgdata[line], &buf[i+1]) ;

    } else {

      for (j=i; buf[j]!=' ' && buf[j]!='\0'; j++) ;
      if (buf[j]!='\0') {
        buf[j++]='\0' ;
        while (buf[j]==' ') j++ ;
        strcpy(cfgdata[line], &buf[j]) ;
      }
     
      cfgcmd[line]=(-1) ;
      if (strcmp(&buf[i],"searchfortag")==0) cfgcmd[line]=CFG_SEARCHFORTAG ;
      if (strcmp(&buf[i],"printuntiltag")==0) cfgcmd[line]=CFG_PRINTUNTILTAG ;
      if (strcmp(&buf[i],"printattribute")==0) cfgcmd[line]=CFG_PRINTATTRIBUTE ;
      if (strcmp(&buf[i],"print")==0) cfgcmd[line]=CFG_PRINT ;
      if (strcmp(&buf[i],"printuntilstring")==0) cfgcmd[line]=CFG_PRINTUNTILSTR;
      if (strcmp(&buf[i],"searchforstring")==0) cfgcmd[line]=CFG_SEARCHFORSTR ;
      if (strcmp(&buf[i],"loopuntileof")==0) cfgcmd[line]=CFG_LOOPUNTILEOF ;
      if (strcmp(&buf[i],"end")==0) cfgcmd[line]=CFG_END ;
      if (cfgcmd[line]==(-1)) {
        printf("Invalid Command in CFG file: %s\n", &buf[i]) ;
        exit(1) ;
      }

    }   
    line++ ;
  }
  currline=0 ;
}

int getcommand() {
  if (currline>=(CFGLINES-1))
    return CFG_END ;
  else
    return cfgcmd[currline] ;
}

char *getdata() {
  if (currline>=(CFGLINES-1))
    return "" ;
  else 
    return cfgdata[currline] ;
}

void gotoline() {
  int i, j ;
  j=currline ;
  for (i=0; i<CFGLINES; i++) {
    if (cfgcmd[i]==CFG_LABEL && strcmp(cfgdata[i], cfgdata[j])==0) {
      currline=i+1 ;
      return ;
    }
  }
  currline=CFGLINES-1 ;
}

void nextline() {
  if (currline<CFGLINES) currline++ ;
}
