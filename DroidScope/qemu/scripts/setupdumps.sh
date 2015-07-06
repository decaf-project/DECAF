##
 # Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>
 #
 # This library is free software; you can redistribute it and/or 
 # modify it under the terms of the GNU Lesser General Public
 # License as published by the Free Software Foundation; either
 # version 2 of the License, or (at your option) any later version.
 #
 # This library is distributed in the hope that it will be useful,
 # but WITHOUT ANY WARRANTY; without even the implied warranty of
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 # Lesser General Public License for more details.
 #
 # You should have received a copy of the GNU Lesser General Public
 # License along with this library; if not, see <http://www.gnu.org/licenses/>.
##

###
# @author Lok Yan
###

#LOK: Added an error when objdump seg faults - this is a bug in binutils-multiarch in Ubuntu before Quantzal

#!/bin/bash

#assumes that these are full paths
process_sub_dir() #basedirname, dirname, targetdirname, dumpcmd, extension, cmdoption, listfilepath, parsingcommand
{
  local basedirname=$1
  local dirname=$2
  local targetdirname=$3
  local dumpcmd=$4
  local extension=$5
  local cmdoption=$6
  local listfilepath=$7
  local parsecmd=$8
  local needdump=1
  local needsym=1
  pushd . > /dev/null

  cd $dirname
  if ( [[ `pwd` != $dirname ]] ); then
    echo "Could not get to $dirname"
    exit
  fi
  if ( [[ ! -e "$targetdirname" ]] ); then
    echo "The target [$targetdirname] does not exist"
    exit
  fi
 
  for file in `ls`; do
    if ( [[ -d "$file" ]] ); then
      echo "Processing sub-directory $dirname/$file" 
      if ( [[ ! -d "$targetdirname/$file" ]] ); then
        mkdir "$targetdirname/$file"
      fi
      process_sub_dir "$basedirname" "$dirname/$file" "$targetdirname/$file" "$dumpcmd" "$extension" "$cmdoption" "$listfilepath" "$parsecmd"
      continue
    fi
    if ( [[ ${file##*.} == $extension ]] ); then
      echo "Processing $dirname/$file"
      #get the relative file path by taking the current path
      # and subtracting the basedirectory path
      relativefilepath=${dirname##$basedirname}
      # remove the leading / if it exists
      relativefilepath=${relativefilepath##/}
      if ( [[ $relativefilepath ]] ); then
        echo "$relativefilepath/$file" >> $listfilepath
      else
        echo "$file" >> $listfilepath
      fi

      needdump=1
      needsy=1

      if ( [[ $cmdoption == "u" || $cmdoption == "s" ]] ); then
        if ( [[ -e $targetdirname/$file.dump ]] ); then
          echo "  Skipping $dumpcmd on $file"
          needdump=0
        fi
      fi

      if ( [[ needddump != 0 ]] ); then
        #It turns out that some versions of objdump multi-arch
        #  causes a seg fault in vfprintf. this is a check for
        #  that
        $dumpcmd "$file" > "$targetdirname/$file.dump"
        #first save the return code
        returncode=$?
        #then do the comparison
        if ( [[ $returncode != 0 ]] ); then
          echo "ERROR: Return value is [$returncode] and not 0"
          echo "ERROR: Command was:"
          echo "  $dumpcmd \"$file\" > \"$targetdirname/$file.dump\""
          echo "I suggest trying the objdump version in the Android toolchains"
          echo " e.g. ln -s ~/Android/android-gingerbread/prebuild/linux-x86/toolchain/arm-eabi-4.4.3/bin/arm-eabi-objdump objdump"
          echo "      PATH=`pwd`:$PATH"
          echo "You might want to run the script with r this time around to check if it works"
          exit
        fi
      fi 

      if ( [[ $cmdoption == "s" ]] ); then
        if ( [[ -e $targetdirname/$file.sym ]] ); then
          echo "  Skipping $parsecmd on $file"
          needsym=0
        fi
      fi

      if ( [[ needsym != 0 ]] ); then
        #Now that the dump file is generated we will try to parse it
        $parsecmd "$targetdirname/$file.dump" "$targetdirname/$file.sym"
        returncode=$?
        if ( [[ $returncode != 0 ]] ); then
          echo "ERROR: Parsing failed - stopping here"
          exit
        fi
      fi
    fi
  done

  popd > /dev/null
}

##########################
# Help menu
##########################
if ( [[ $1 == "-h" || $1 == "--help" ]] ); then
  echo "Usage $0 [OPTION]"
  echo " -h : This message"
  echo "    : no option -> run the full script"
  echo "  r : skip the adb pull part and only compile the dumps"
  echo "  u : like r, but only generate dumps that do not exist" 
  echo "  s : like u, but only generate the symbols that do not exist" 
  echo "  X : all others will check to see if dexdump and objdump are good"
  exit
fi

#########################
# main
#########################
scriptdir=`pwd`

######################################################
### add 'adb','objdump' and 'dexdump' to PATH
PATH=`pwd`:$PATH
######################################################

if ( [[ $1 == "" ]] ); then 
  #setup the new dumps directory
  olddumps="dumps_"
  olddumps+=`date +%s`

  if ( [[ -d dumps ]] ); then
    oldfilecount=`ls -1 dumps | wc -l`
    if ( [[ $oldfilecount > 0 ]] ); then
      mv dumps $olddumps
      mkdir dumps
    fi
  else
    mkdir dumps
  fi

  #now that the new directory has been created, see if adb exists
  adbtoolsthere=`which adb`
  if ( [[ ! $adbtoolsthere ]] ); then
    echo "Can't find adb. Make sure its in your path"
    exit
  fi
    
  #now check to see if the device is connected
  ret=`adb devices | grep "emulator.*device"`
  if ( [[ ! $ret ]] ); then
    echo "Can't seem to find the device. Make sure it is connected"
    exit
  fi
  
  #now go into the dumps directory and pull the libraries down
  cd dumps
  mkdir lib
  cd lib
  adb pull /system/lib/ .
  cd ..
  mkdir app
  cd app
  adb pull /system/app/ .
  cd ..
  mkdir framework 
  cd framework 
  adb pull /system/framework/ .
  cd ..

else
  cd dumps
fi

#now that we have the libraries, its time to generate the dumps,
# but first check that dexdump and objdump are good
  adbtoolsthere=`which dexdump`
  if ( [[ ! $adbtoolsthere ]] ); then
    echo "Can't find dexdump. Make sure its in your path"
    echo "You might want to run the script with 1 this time around too."
    exit
  fi
  
  objdumpexists=`objdump -i | grep arm`
  if ( [[ ! $objdumpexists ]] ); then
    echo "objdump doesn't seem to support arm. Make sure binutils-multiarch is installed"
    echo "Or make sure that objdump points to the version in the android toolchain."
    echo " e.g. ln -s ~/Android/android-gingerbread/prebuild/linux-x86/toolchain/arm-eabi-4.4.3/bin/arm-eabi-objdump objdump"
    echo "      PATH=`pwd`:$PATH"
    echo "You might want to run the script with r this time around too."
    exit
  fi

if ( ! [[ $1 == "" || $1 == "r" || $1 == "u" || $1 == "s" ]] ); then
  echo "dexdump and objdump seems okay"
  exit
fi

basedir=`pwd`

if ( [[ ! -d "out" ]] ); then
  mkdir out
fi

if ( [[ ! -d "out/lib" ]] ); then
  mkdir "$basedir/out/lib"
fi
echo "//Native Library List" > "$basedir/native_lib_list.txt"
process_sub_dir "$basedir/lib" "$basedir/lib" "$basedir/out/lib" "objdump -d" "so" "$1" "$basedir/native_lib_list.txt" "python $scriptdir/parseObjdumpSymbols.py"

if ( [[ ! -d "out/app" ]] ); then
  mkdir "$basedir/out/app"
fi
echo "//Dalvik odex List" > "$basedir/odex_list.txt"
process_sub_dir "$basedir/app" "$basedir/app" "$basedir/out/app" "dexdump -d" "odex" "$1" "$basedir/odex_list.txt" "python $scriptdir/parseDexdumpSymbols.py"

if ( [[ ! -d "out/framework" ]] ); then
  mkdir "$basedir/out/framework"
fi
echo "//Dalvik odex List" > "$basedir/odex_list.txt"
process_sub_dir "$basedir/framework" "$basedir/framework" "$basedir/out/framework" "dexdump -d" "odex" "$1" "$basedir/odex_list.txt" "python $scriptdir/parseDexdumpSymbols.py"
