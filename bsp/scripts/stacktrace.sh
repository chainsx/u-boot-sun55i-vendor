#!/bin/bash

set -e

if [ $# -eq 0 ] || [ ! -f "$1" ]; then
    echo "Usage: "
    echo "    ./scripts/stacktrace.sh <file>"
    echo
    echo "Param:"
    echo "    <file>: stacktrace info file"
    echo "Example:"
    echo "    ./scripts/stacktrace.sh ./dump.txt"
    exit 1
fi

INPUT_FILE=$1
TPL_SPL=$2
BRANDY_DIR=$(pwd)/..
UBOOT_DIR=$(pwd)/../u-boot-2023

SYM_FILE=${UBOOT_DIR}/u-boot.sym
ELF_FILE=${UBOOT_DIR}/u-boot
ADDR2LINE_TOOLS=$(cat "${UBOOT_DIR}/.crosscompile.txt")addr2line
OBJDUMP_TOOLS=$(cat "${UBOOT_DIR}/.crosscompile.txt")objdump
if [ -z "${SYM_FILE}" ] || [ ! -f "${SYM_FILE}" ]; then
    echo "ERROR: No ${SYM_FILE}"
    exit 1
fi

if [ -z "${ADDR2LINE_TOOLS}" ] || [ ! -f "${ADDR2LINE_TOOLS}" ]; then
    echo "ERROR: No ${ADDR2LINE_TOOLS}"
    exit 1
fi

echo "UBOOT_DIR is $UBOOT_DIR"

echo "SYMBOL File: ${SYM_FILE}"
echo "ADDR2_LINE File: ${ADDR2LINE_TOOLS}"
echo
# Parse PC and LR
echo "Call trace:"
grep '\[< ' ${INPUT_FILE} | grep '>\]' | grep [PC,LR] | while read line
do
	echo -n " ${line}  "

	frame_pc_str=`echo ${line} | awk '{ print "0x"$3 }'`
	frame_pc_dec=`echo ${line} | awk '{ print strtonum("0x"$3); }'`
	frame_pc_hex=`echo "obase=16;${frame_pc_dec}"|bc |tr '[A-Z]' '[a-z]'`

	f_pc_dec=`cat ${SYM_FILE} | sort | awk '/\.text/ { if (strtonum("0x"$1) > '${frame_pc_str}') { print fpc; exit; } fpc=strtonum("0x"$1); }'`
	f_pc_hex=`echo "obase=16;${f_pc_dec}"|bc |tr '[A-Z]' '[a-z]'`
	f_offset_dec=$((frame_pc_dec-f_pc_dec))
	f_offset_hex=`echo "obase=16;${f_offset_dec}"|bc |tr '[A-Z]' '[a-z]'`

	cat ${SYM_FILE} | sort |
	awk -v foffset=${f_offset_hex} '/\.text/ {
		if (strtonum("0x"$1) > '${frame_pc_str}') {
			printf("%s+0x%s/0x%x      ", fname, foffset, fsize);
			exit
		}
		fname=$NF;
		fsize=strtonum("0x"$5);
		fpc=strtonum("0x"$1);
	}'

	func_path=$(${ADDR2LINE_TOOLS} -e ${ELF_FILE} ${frame_pc_str} | sed 's|.*/brandy/||')
	echo ${func_path}
done
echo

# Parse stack
echo "Stack:"
grep '\[< ' ${INPUT_FILE} | grep '>\]' | grep -v [PC,LR] | while read line
do
	echo -n "       ${line}  "

	frame_pc_str=`echo ${line} | awk '{ print "0x"$2 }'`
	frame_pc_dec=`echo ${line} | awk '{ print strtonum("0x"$2); }'`
	frame_pc_hex=`echo "obase=16;${frame_pc_dec}"|bc |tr '[A-Z]' '[a-z]'`

	f_pc_dec=`cat ${SYM_FILE} | sort | awk '/\.text/ { if (strtonum("0x"$1) > '${frame_pc_str}') { print fpc; exit; } fpc=strtonum("0x"$1); }'`
	f_pc_hex=`echo "obase=16;${f_pc_dec}"|bc |tr '[A-Z]' '[a-z]'`
	f_offset_dec=$((frame_pc_dec-f_pc_dec))
	f_offset_hex=`echo "obase=16;${f_offset_dec}"|bc |tr '[A-Z]' '[a-z]'`

	cat ${SYM_FILE} | sort |
	awk -v foffset=${f_offset_hex} '/\.text/ {
		if (strtonum("0x"$1) > '${frame_pc_str}') {
			printf("%s+0x%s/0x%x      ", fname, foffset, fsize);
			exit
		}
		fname=$NF;
		fsize=strtonum("0x"$5);
		fpc=strtonum("0x"$1);
	}'

	func_path=$(${ADDR2LINE_TOOLS} -e ${ELF_FILE} ${frame_pc_str} | sed 's|.*/brandy/||')
	echo ${func_path}
done
echo

# PC instruction
echo "PC Surrounding Instructions:"
line=`grep '\[< ' ${INPUT_FILE} | grep '>\]' | grep [PC]`
frame_pc_str=`echo ${line} | awk '{ print "0x"$3 }'`
frame_pc_dec=`echo ${line} | awk '{ print strtonum("0x"$3); }'`
# Handle Thumb instr
if [ `expr ${frame_pc_dec} % 2` -ne 0 ];then
	frame_pc_dec=`expr ${frame_pc_dec} - 1`
fi
frame_pc_hex=`echo "obase=16;${frame_pc_dec}"|bc |tr '[A-Z]' '[a-z]'`
PC_INSTR=$(${OBJDUMP_TOOLS} -d -C -Mintel --start-address=${frame_pc_str} --stop-address=$((frame_pc_str+16)) ${ELF_FILE})
echo "${PC_INSTR}"
echo

set +e
