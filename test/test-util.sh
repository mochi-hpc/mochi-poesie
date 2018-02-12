#
# General test script utilities
#

if [ -z "$TIMEOUT" ] ; then
    echo expected TIMEOUT variable defined to its respective command
    exit 1
fi

if [ -z "$MKTEMP" ] ; then
    echo expected MKTEMP variable defined to its respective command
    exit 1
fi

TMPDIR=/dev/shm
export TMPDIR
mkdir -p $TMPDIR
TMPBASE=$(${MKTEMP} --tmpdir -d test-XXXXXX)

if [ ! -d $TMPBASE ];
then
  echo "Temp directory doesn't exist: $TMPBASE"
  exit 3
fi

function run_to ()
{
    maxtime=${1}s
    shift
    ${TIMEOUT} --signal=9 $maxtime "$@"
}

function test_start_server ()
{
    startwait=${1:-15}
    maxtime=${2:-120}

    run_to ${maxtime} bin/poesie-server-daemon -f $TMPBASE/poesie.addr na+sm ${@:3} &
    # wait for server to start
    sleep ${startwait}

    svr_addr=`cat $TMPBASE/poesie.addr`
}
