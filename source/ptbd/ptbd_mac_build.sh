
APP_NAME="PTBDDatabase"

BUILD_DIR='../../build'

if [ ! -d $BUILD_DIR ]; then
    mkdir $build_dir
fi

pushd $BUILD_DIR

if [ ! -f zlib.a ]; then
    ZLIB_SOURCES='../source/external/zlib/adler32.c ../source/external/zlib/compress.c ../source/external/zlib/crc32.c ../source/external/zlib/deflate.c ../source/external/zlib/infback.c ../source/external/zlib/inffast.c ../source/external/zlib/inflate.c ../source/external/zlib/inftrees.c  ../source/external/zlib/trees.c ../source/external/zlib/zutil.c'
    clang -c $ZLIB_SOURCES -O2
    ar rc zlib.a adler32.o compress.o crc32.o deflate.o infback.o inffast.o inflate.o inftrees.o trees.o zutil.o
fi

if [ ! -f sqlite3.a ]; then
    clang -c ../source/external/sqlite3.c -O2
    ar rc sqlite3.a sqlite3.o
fi

if [ ! -f libxlsxwriter.a ]; then
    XLSX_WRITER_SOURCES='../source/external/libxlsxwriter/*.c ../source/external/libxlsxwriter/third_party/md5/md5.c ../source/external/libxlsxwriter/third_party/minizip/*.c ../source/external/libxlsxwriter/third_party/tmpfileplus/tmpfileplus.c'
    clang -c $XLSX_WRITER_SOURCES -I ../source/external/libxlsxwriter -I ../source/external/zlib -O2
    ar rc libxlsxwriter.a app.o chart.o chartsheet.o comment.o content_types.o core.o custom.o drawing.o format.o hash_table.o metadata.o packager.o relationships.o shared_strings.o styles.o table.o theme.o utility.o vml.o workbook.o worksheet.o xmlwriter.o md5.o ioapi.o iowin32.o mztools.o unzip.o zip.o tmpfileplus.o
fi

source ../source/renderer_2d/renderer_2d_mac_compile_shaders.sh
clang -g -I ../source -ObjC -D BuildModeDebug=1 -D BuildEnableAsserts=1 -D BuildZLib=1 -D BuildFontProviderSTBTruetype=1 -D ApplicationName=$APP_NAME ../source/ptbd/ptbd_main.c zlib.a sqlite3.a -o ${APP_NAME}_debug -pthread -lm -framework Cocoa -framework UniformTypeIdentifiers -framework Metal -framework MetalKit -framework QuartzCore

popd
