package bridge

//go:generate cmake -E make_directory build/$GOARCH
//go:generate cmake -E chdir build/$GOARCH cmake $CMAKE_ARGS ../../native
//go:generate cmake -E chdir build/$GOARCH cmake --build .
//go:generate go run timestamp/build.go build/$GOARCH/libnative.a build/$GOARCH/liblwip.a build.go

/*
#cgo CFLAGS: -I${SRCDIR}/native

#cgo amd64 LDFLAGS: -L${SRCDIR}/build/amd64 -lnative -llwip
#cgo 386 LDFLAGS: -L${SRCDIR}/build/386 -lnative -llwip
#cgo arm64 LDFLAGS: -L${SRCDIR}/build/arm64 -lnative -llwip
#cgo arm LDFLAGS: -L${SRCDIR}/build/arm -lnative -llwip

#include "init.h"
*/
import "C"

import "errors"

var ErrUnsupported = errors.New("unsupported")
var ErrNative = errors.New("native error")

func init() {
	C.init_lwip()
}
