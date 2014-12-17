/*
Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>

DECAF is based on QEMU, a whole-system emulator. You can redistribute
and modify it under the terms of the GNU GPL, version 3 or later,
but it is made available WITHOUT ANY WARRANTY. See the top-level
README file for more details.

For more information about DECAF and other softwares, see our
web site at:
http://sycurelab.ecs.syr.edu/

If you have any questions about DECAF,please post it on
http://code.google.com/p/decaf-platform/
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

void FindFunNameFromStrTAB(unsigned long offset, FILE *stream, FILE *result)
{
     int c;
     int i=0;
     char str[100];
     fseek(stream,offset,SEEK_SET);
     do
     {
        c=fgetc(stream);
        str[i]=c;
        i++; 
     }while(c!=0);
     
     fprintf(result,"%s\n",str);
     
}




int main(int argc, char *argv[])
{
  int ret;
  FILE *fp , *result;
  unsigned long ImageBase,StrTAB,SymTAB,l;
  char Temp1[300]="readelf -l ";
  char Temp2[300]="readelf -d ";
  char Temp3[300]="readelf -d ";
  char Temp[20];
  unsigned int SymAddr,FuncAddr;
 
   if (argc!=3)
  {
      printf("help: memfuncdump [Mem_Dump_File_path]  [Result_File_Name_Path] \n");
      exit(1);
  }
  
  strcat(Temp1,argv[1]);
  strcat(Temp1,"|grep LOAD|awk '{print $3}'>temp1.out");
  strcat(Temp2,argv[1]);
  strcat(Temp2,"|grep STRTAB|awk '{print $3}'>temp2.out");
  strcat(Temp3,argv[1]);
  strcat(Temp3,"|grep SYMTAB|awk '{print $3}'>temp3.out");


  ret = system(Temp1);
  if(ret==-1)
  {
        printf("\nRun 1 failure~\n");
  }
  fp = fopen("temp1.out","r");

  if(fp==NULL)
  {
       printf("Can't open file ! \n");
       exit(1);
  }
  else
  {
       fgets(Temp,11,fp);
       ImageBase=strtoul(Temp,NULL,16); 
  }
  fclose(fp);

  ret = system(Temp2);
  if(ret==-1)
  {
        printf("\nRun 2 failure~\n");
  }
  fp = fopen("temp2.out","r");

  if(fp==NULL)
  {
       printf("Can't open file ! \n");
       exit(1);
  }
  else
  {
       fgets(Temp,11,fp);
       StrTAB=strtoul(Temp,NULL,16); 
  }
  fclose(fp);

  ret = system(Temp3);
  if(ret==-1)
  {
        printf("\nRun 3 failure~\n");
  }
  fp = fopen("temp3.out","r");

  if(fp==NULL)
  {
       printf("Can't open file ! \n");
       exit(1);
  }
  else
  {
       fgets(Temp,11,fp);
       SymTAB=strtoul(Temp,NULL,16); 
  }
  fclose(fp);

 
  ret = system("rm -f temp*.out");
  if(ret==-1)
  {
        printf("\nRun 4 failure~\n");
  }
 


  fp=fopen(argv[1],"r");
  result = fopen(argv[2],"w");
  if(fp==NULL&&result==NULL)
  {
       printf("Can't open file ! \n");
       exit(1);
  }
  else
  {   
      fprintf(result,"Address   \tFunction Name\n");

      for( l = SymTAB-ImageBase ; l < StrTAB-ImageBase ; l+=16)
      {
           fseek(fp,l,SEEK_SET);
           fread(&SymAddr,1,4,fp);
           fread(&FuncAddr,1,4,fp);

           if(SymAddr!=0)
           {
              fprintf(result,"0X%08x\t",FuncAddr);
              FindFunNameFromStrTAB((StrTAB-ImageBase+SymAddr),fp,result); 
           }
      
          else
          {
              fprintf(result,"0X%08x\tNULL\n",FuncAddr);
          }
       }
       fclose(fp);
       fclose(result);

     
  }

  return EXIT_SUCCESS;
}
