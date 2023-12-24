CLEAN=0
for arg in "$@"; do
  case $arg in
    --clean)
      CLEAN=1
      shift
      ;;
    *)
      echo "Warning: Unrecognized argument '$arg'"
      exit 1
      ;;
  esac
done

if [ $CLEAN -eq 1 ]; then
    rm -rf build
    mkdir build
fi

APPLE=0
GENERATOR="Unix Makefiles"
if [ "$(uname)" == "Darwin" ]; then
    APPLE=1
    GENERATOR="Ninja"
fi


cd build
cmake -G "$GENERATOR" ../test
cmake --build .

./zooTestDebugTest
./zooTestDebug

