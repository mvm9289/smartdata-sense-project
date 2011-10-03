#!/bin/bash


#---- GLOBAL VARS ----

ARGS=("$@")

DEBUG_ALL=0
DEBUG_NET=0
DEBUG_STATE=0
DEBUG_CFS=0
DEBUG_SENSOR=0
DEBUG_EVENT=0

DEF_SINK=0
SINK_ADDR1=111
SINK_ADDR2=111

DEF_ADDR=0
ADDR1=1
ADDR2=1

DEF_FILE=0
FILE=""

UPLOAD=0



#---- FUNCTIONS ----

function printUsage {
    echo ""
    echo ""
    echo "Usage: ./build.sh [OPTION]"
    echo ""
    echo ""
    echo "  Debugging:"
    echo ""
    echo -e "    -Dall\t\tActive all debugging options."
    echo -e "    -Dnet\t\tActive net debugging options."
    echo -e "    -Dstate\t\tActive state debugging options."
    echo -e "    -Dcfs\t\tActive contiki file system debugging options."
    echo -e "    -Dsensor\t\tActive sensor debugging options."
    echo -e "    -Devent\t\tActive all debugging options."
    echo ""
    echo ""
    echo "  Node address:"
    echo ""
    echo -e "    -S ADDRESS\t\tChoose a sink adsress. For example: -S 1.2"
    echo -e "    -A ADDRESS\t\tChoose a node address. For example: -A 1.1"
    echo ""
    echo ""
    echo "  Files:"
    echo ""
    echo -e "    -F FILENAME\t\tChoose a single file for compile."
    echo -e "    -U\t\tUpload compiled program. If a single file is not defined, this option is ignored."
    echo ""
    echo ""
}

function isAnAddress {
    retVal=0

    addr=($(echo $1 | tr "." "\n"))
    if [ ${#addr[@]} = 2 ] && [[ ${addr[0]} =~ ^[0-9]+$ ]] && [[ ${addr[1]} =~ ^[0-9]+$ ]]
    then
        retVal=1
    fi
}

function fileExists {
    retVal=0

    if [ -f $1 ]
    then
        retVal=1
    fi 
}



#---- ARGUMENTS PARSING ----

ARGS_SIZE=${#ARGS[@]}
for ((i=0;i<$ARGS_SIZE;i++));
do
    arg=${ARGS[${i}]}

    if [ $arg = "-Dall" ]
    then
        DEBUG_ALL=1
    elif [ $arg = "-Dnet" ]
    then
        DEBUG_NET=1
    elif [ $arg = "-Dstate" ]
    then
        DEBUG_STATE=1
    elif [ $arg = "-Dcfs" ]
    then
        DEBUG_CFS=1
    elif [ $arg = "-Dsensor" ]
    then
        DEBUG_SENSOR=1
    elif [ $arg = "-Devent" ]
    then
        DEBUG_EVENT=1
    elif [ $arg = "-S" ]
    then
        DEF_SINK=1
        i=$i+1
        isAnAddress ${ARGS[${i}]}
        if [ $retVal = 1 ]
        then
            SINK_ADDR1=${addr[0]}
            SINK_ADDR2=${addr[1]}
        else
            echo "Error: Bad sink address defined. Please type \"./build.sh -h\" for help."
            exit 0
        fi
    elif [ $arg = "-A" ]
    then
        DEF_ADDR=1
        i=$i+1
        isAnAddress ${ARGS[${i}]}
        if [ $retVal = 1 ]
        then
            ADDR1=${addr[0]}
            ADDR2=${addr[1]}
        else
            echo "Error: Bad node address defined. Please type \"./build.sh -h\" for help."
            exit 0
        fi
    elif [ $arg = "-F" ]
    then
        DEF_FILE=1
        i=$i+1
        fileExist "${ARGS[${i}]}.c"
        if [ $retVal = 1 ]
        then
            FILE=${ARGS[${i}]}
        else
            echo "Error: File does not exist. Please type \"./build.sh -h\" for help."
            exit 0
        fi
    elif [ $arg = "-U" ]
    then
        UPLOAD=1
    elif [ $arg = "-h" ]
    then
        printUsage
        exit 0
    fi
done



#---- MAIN ----

BUILD_OPTIONS=""

if [ $DEBUG_ALL = 1 ]
then
    BUILD_OPTIONS="$BUILD_OPTIONS DEFINES+=DEBUG_MODE"
else
    if [ $DEBUG_NET = 1 ]
    then
        BUILD_OPTIONS="$BUILD_OPTIONS DEFINES+=DEBUG_NET"
    fi
    if [ $DEBUG_STATE = 1 ]
    then
        BUILD_OPTIONS="$BUILD_OPTIONS DEFINES+=DEBUG_STATE"
    fi
    if [ $DEBUG_CFS = 1 ]
    then
        BUILD_OPTIONS="$BUILD_OPTIONS DEFINES+=DEBUG_CFS"
    fi
    if [ $DEBUG_SENSOR = 1 ]
    then
        BUILD_OPTIONS="$BUILD_OPTIONS DEFINES+=DEBUG_SENSOR"
    fi
    if [ $DEBUG_EVENT = 1 ]
    then
        BUILD_OPTIONS="$BUILD_OPTIONS DEFINES+=DEBUG_EVENT"
    fi
fi

if [ $DEF_SINK = 1 ]
then
    BUILD_OPTIONS="$BUILD_OPTIONS DEFINES+=SINK_ADDR1=$SINK_ADDR1 DEFINES+=SINK_ADDR2=$SINK_ADDR2"
fi

if [ $DEF_ADDR = 1 ]
then
    BUILD_OPTIONS="$BUILD_OPTIONS DEFINES+=MY_ADDR1=$ADDR1 DEFINES+=MY_ADDR2=$ADDR2"
fi

if [ $DEF_FILE = 1 ]
then
    FILE_OPTIONS=$FILE

    if [ $UPLOAD = 1 ]
    then
        FILE_OPTIONS="$FILE_OPTIONS.upload"
    fi
fi


make clean
make $BUILD_OPTIONS $FILE_OPTIONS

